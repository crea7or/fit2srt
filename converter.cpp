/*

 MIT License

 Copyright (c) 2022 pavel.sokolov@gmail.com / CEZEO software Ltd. All rights reserved.

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

// defines for rapidjson
#ifndef RAPIDJSON_HAS_CXX11_RVALUE_REFS
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#endif
#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif

// rapidjson errors handling
#include <stdexcept>

#ifndef RAPIDJSON_ASSERT_THROWS
#define RAPIDJSON_ASSERT_THROWS 1
#endif
#ifdef RAPIDJSON_ASSERT
#undef RAPIDJSON_ASSERT
#endif
#define RAPIDJSON_ASSERT(x) \
  if (x)                    \
    ;                       \
  else                      \
    throw std::runtime_error("Failed: " #x);
// rapidjson errors handling

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <spdlog/spdlog.h>

#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "fitsdk/fit_convert.h"
#include "parser.h"

constexpr const char kBanner[] = R"%(

      .:+oooooooooooooooooooooooooooooooooooooo: `/ooooooooooo/` :ooooo+/-`
   `+dCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOEZshCEZEOCEZEOEZ#doCEZEOEZEZNs.
  :CEZEON#ddddddddddddddddddddddddddddddNCEZEO#h.:hdddddddddddh/.yddddCEZEO#N+
 :CEZEO+.        .-----------.`       `+CEZEOd/   .-----------.        `:CEZEO/
 CEZEO/         :CEZEOCEZEOEZNd.    `/dCEZEO+`   sNCEZEOCEZEO#Ny         -CEZEO
 CEZEO/         :#NCEZEOCEZEONd.   :hCEZEOo`     oNCEZEOCEZEO#Ny         -CEZEO
 :CEZEOo.`       `-----------.`  -yNEZ#Ns.       `.-----------.`       `/CEZEO/
  :CEZEONCEZEOd/.ydCEZEOCEZEOdo.sNCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOEZNEZEZN+
   `+dCEZEOEZEZdoCEZEOCEZEOEZ#N+CEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOEZ#s.
      .:+ooooo/` :+oooooooooo+. .+ooooooooooooooooooooooooooooooooooooo+/.
 C E Z E O  S O F T W A R E (c) 2022   FIT telemetry converter to SRT or JSON

)%";

constexpr const char kHelp[] = R"%(

usage: fitconvert -i input_file -o output_file -t output_type -f offset -s N

-i - path to .fit file to read data from
-o - path to .srt or .json file to write to
-t - export type (optional, default to srt)
-f - offset in milliseconds to sync video and .fit data (optional, for srt export only)
* if the offset is positive - 'offset' second of the data from .fit file will be displayed at the first second of the video.
    it is for situations when you started video after starting recording your activity(that generated .fit file)
* if the offset is negative - the first second of .fit data will be displayed at abs('offset') second of the video
    it is for situations when you started your activity (that generated .fit file) after starting the video
-s - smooth values by inserting N smoothed values between timestamps (optional, for srt export only)
)%";

struct Time {
  uint32_t hours{0};
  uint32_t minutes{0};
  uint32_t seconds{0};
};

struct SrtItem {
  SrtItem(uint32_t frame, uint32_t seconds_from, uint32_t seconds_to, std::string data)
      : frame(frame), seconds_from(seconds_from), seconds_to(seconds_to), data(std::move(data)) {}
  uint32_t frame{0};
  uint32_t seconds_from{0};
  uint32_t seconds_to{0};
  std::string data;
};

Time GetTime(uint32_t seconds_total) {
  Time time_struct;
  uint32_t minutes_total = seconds_total / 60;
  uint32_t hours_total = minutes_total / 60;
  time_struct.hours = hours_total;
  time_struct.minutes = minutes_total - (hours_total * 60);
  time_struct.seconds = seconds_total - ((time_struct.minutes * 60) + (hours_total * 3600));
  return time_struct;
}

void HeaderItem(rapidjson::Writer<rapidjson::StringBuffer>& writer, DataType in_header, DataType check_for) {
  if ((in_header & check_for) == check_for) {
    writer.StartObject();
    const auto tags = DataTypeToTag(check_for);
    writer.Key("data");
    writer.String(tags.data_tag);
    writer.Key("units");
    writer.String(tags.data_units_tag);
    writer.EndObject();
  }
}

int main(int argc, char* argv[]) {
  spdlog::set_pattern("[%H:%M:%S.%e] %^[%l]%$ %v");

  cxxopts::Options cmd_options("FIT converter", "FIT telemetry converter to SRT or JSON");
  cmd_options.add_options()                                              //
      ("i,input", "input FIT file path", cxxopts::value<std::string>())  //
      ("o,output", "output file path", cxxopts::value<std::string>())    //
      ("h,help", "help message")                                         //
      ("t,type",
       "output format to generate (srt or json)",
       cxxopts::value<std::string>()->default_value(kOutputSrtTag))                                             //
      ("f,offset", "offset in milliseconds to sync with video", cxxopts::value<int32_t>()->default_value("0"))  //
      ("s,smooth", "smooth values", cxxopts::value<uint64_t>()->default_value("0"));                            //
  const auto cmd_result = cmd_options.parse(argc, argv);

  if (argc < 3 || cmd_result.count("help") > 0) {
    std::cout << kBanner << std::endl;
    std::cout << kHelp << std::endl;
    return 1;
  }

  try {
    const std::string input_fit_file(cmd_result["input"].as<std::string>());
    const std::string output_file(cmd_result["output"].as<std::string>());
    const std::string output_type(cmd_result["type"].as<std::string>());
    const int32_t offset = cmd_result["offset"].as<int32_t>();
    const uint64_t smothness = cmd_result["smooth"].as<uint64_t>();

    if (output_type != kOutputJsonTag && output_type != kOutputSrtTag) {
      SPDLOG_ERROR("unknown output specified: '{}', only srt and .json supported", output_type);
      return 1;
    }

    if (output_type == kOutputJsonTag && (offset != 0 || smothness != 0)) {
      SPDLOG_WARN("smoothness or offset valid only for .srt output format");
    }

    if (smothness > 10) {
      SPDLOG_ERROR("smoothness can not be more than 10");
      return 1;
    }

    const FitResult fit_result = FitParser(input_fit_file);
    if (fit_result.used_data_types == DataType::kTypeNone) {
      // Special meaning - processing error.The exact error has been reported in the FitParser function.
      return 1;
    }

    if (output_type == kOutputJsonTag) {
      rapidjson::StringBuffer string_buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
      writer.StartObject();
      // header
      writer.Key("header");
      writer.StartArray();
      // header objects
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeAltitude);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeCadence);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeDistance);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeHeartRate);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeLatitude);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeLongitude);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypePower);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeSpeed);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeTemperature);
      HeaderItem(writer, fit_result.used_data_types, DataType::kTypeTimeStamp);
      writer.EndArray();
      // records
      writer.Key("records");
      writer.StartArray();

      for (const auto item : fit_result.result) {
        writer.StartObject();
        writer.Key(kTimeStampTag);
        writer.Int64(item.timestamp);
        for (const auto value : item.values) {
          writer.Key(value.type);
          writer.Int64(value.value);
        }
        writer.EndObject();
      }

      writer.EndArray();
      writer.EndObject();


      std::filesystem::remove(output_file);
      std::ofstream output_stream(output_file, std::ios::out | std::ios::app | std::ios::binary);
      output_stream.write(string_buffer.GetString(), string_buffer.GetSize());
      output_stream.close();

    } else if (output_type == kOutputSrtTag) {
      /*
      uint32_t first_video_timestamp = 0;
      uint32_t first_fit_timestamp = 0;

      uint64_t saved_size = 0;
      std::filesystem::remove(output_file);
      std::ofstream output_stream(output_file, std::ios::out | std::ios::app | std::ios::binary);
      for (const auto item : subtitles) {
        const Time time_from(GetTime(item.seconds_from));
        const Time time_to(GetTime(item.seconds_to));
        const auto file_out(fmt::format("{}\n{:0>2d}:{:0>2d}:{:0>2d},000 --> {:0>2d}:{:0>2d}:{:0>2d},000\n{}\n\n",
                                        item.frame,
                                        time_from.hours,
                                        time_from.minutes,
                                        time_from.seconds,
                                        time_to.hours,
                                        time_to.minutes,
                                        time_to.seconds,
                                        item.data));
        output_stream.write(file_out.c_str(), file_out.size());
        saved_size += file_out.size();
      }
      */
    } else {
      // checked at the cmd line validation
    }
  } catch (const std::exception& e) {
    // file errors usually
    SPDLOG_ERROR("exception during processing: {}", e.what());
  }

  return 0;
}
