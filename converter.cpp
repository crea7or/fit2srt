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

constexpr char* kOutputJsonTag = "json";
constexpr char* kOutputSrtTag = "srt";
constexpr char* kInputStdinTag = "stdin";
constexpr char* kOutputStdoutTag = "stdout";

struct Time {
  int64_t hours{0};
  int64_t minutes{0};
  int64_t seconds{0};
  int64_t milliseconds{0};
};

struct SrtItem {
  SrtItem(int64_t frame, int64_t milliseconds_from, int64_t milliseconds_to, std::string data)
      : frame(frame), milliseconds_from(milliseconds_from), milliseconds_to(milliseconds_to), data(std::move(data)) {}
  int64_t frame{0};
  int64_t milliseconds_from{0};
  int64_t milliseconds_to{0};
  std::string data;
};

Time GetTime(const int64_t milliseconds_total) {
  Time time_struct;
  int64_t ms_remainder = milliseconds_total;
  time_struct.hours = ms_remainder / 3600000;
  ms_remainder = ms_remainder - (time_struct.hours * 3600000);
  time_struct.minutes = ms_remainder / 60000;
  ms_remainder = ms_remainder - (time_struct.minutes * 60000);
  time_struct.seconds = ms_remainder / 1000;
  time_struct.milliseconds = ms_remainder - (time_struct.seconds * 1000);
  return time_struct;
}

struct ValueByType {
  bool Valid() const { return dt != DataType::kTypeMax; };
  int64_t value{0};
  DataType dt{DataType::kTypeMax};
};

ValueByType GetValueByType(const Record& record, const DataType type) {
  ValueByType result;
  if ((record.Valid & DataTypeToMask(type)) != 0) {
    result.dt = type;
    const uint32_t data_type_index = static_cast<uint32_t>(type);
    result.value = record.values[data_type_index];
  }
  return result;
}

std::string NumberToStringPrecision(const int64_t number,
                                    const double divider,
                                    const size_t total_symbols,
                                    const size_t dot_limit) {
  const double double_number = static_cast<double>(number);
  std::string str_result(std::to_string(double_number / divider));
  str_result = str_result.substr(0, total_symbols);
  const size_t dot_string_size = str_result.size();
  const size_t dot_position = str_result.find('.');
  if (dot_position != std::string::npos) {
    const size_t after_dot_position = dot_limit + 1;
    if ((dot_string_size - dot_position) > after_dot_position) {
      str_result = str_result.substr(0, (dot_position + after_dot_position));
    }
  }

  const size_t string_size = str_result.size();
  if (string_size > 0 && str_result.at(string_size - 1) == '.') {
    str_result = str_result.substr(0, string_size - 1);
  }
  return str_result;
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
      ("f,offset", "offset in milliseconds to sync with video", cxxopts::value<int64_t>()->default_value("0"))  //
      ("s,smooth", "smooth values", cxxopts::value<int32_t>()->default_value("0"));                             //
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
    const int64_t offset = cmd_result["offset"].as<int64_t>();
    const int32_t smoothness = cmd_result["smooth"].as<int32_t>();

    if (output_type != kOutputJsonTag && output_type != kOutputSrtTag) {
      SPDLOG_ERROR("unknown output specified: '{}', only srt and .json supported", output_type);
      return 1;
    }

    if (output_type == kOutputJsonTag && (offset != 0 || smoothness != 0)) {
      SPDLOG_WARN("smoothness or offset valid only for .srt output format");
    }

    if (smoothness > 9) {
      SPDLOG_ERROR("smoothness can not be more than 9");
      return 1;
    }

    FitResult fit_result = FitParser(input_fit_file);
    if (fit_result.status != ParseResult::kSuccess) {
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
      for (const auto& header_item : fit_result.header) {
        writer.StartObject();
        writer.Key("data");
        writer.String(header_item.data_tag.data(), static_cast<rapidjson::SizeType>(header_item.data_tag.size()));
        writer.Key("units");
        writer.String(header_item.data_units.data(), static_cast<rapidjson::SizeType>(header_item.data_units.size()));
        writer.EndObject();
      }
      writer.EndArray();
      // records
      writer.Key("records");
      writer.StartArray();

      for (const auto& item : fit_result.result) {
        writer.StartObject();

        for (uint32_t index = kDataTypeFirst; index < kDataTypeMax; ++index) {
          const auto value_by_type = GetValueByType(item, static_cast<DataType>(index));
          if (value_by_type.Valid()) {
            const auto name = DataTypeToName(value_by_type.dt);
            writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
            writer.Int64(value_by_type.value);
          }
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
      int64_t records_count = 0;
      int64_t first_video_timestamp = 0;
      int64_t first_fit_timestamp = 0;
      int64_t ascent = 500 * 5;   // x / 5 - 500
      int64_t descent = 500 * 5;  // x / 5 - 500
      int64_t previous_altitude = 0;
      bool initial_altitude_set = false;
      std::vector<SrtItem> subtitles;

      for (auto& rec : fit_result.result) {
        // convert all timestamps to milliseconds
        constexpr uint32_t type_index = static_cast<uint32_t>(DataType::kTypeTimeStamp);
        rec.values[type_index] *= 1000;
      }

      size_t valid_value_count = 0;
      for (size_t index = 0; index < fit_result.result.size(); ++index) {
        const auto& original_record = fit_result.result[index];

        const auto record_time_by_type = GetValueByType(original_record, DataType::kTypeTimeStamp);
        const int64_t record_timestamp = record_time_by_type.Valid() ? record_time_by_type.value : 0;

        // fit timestamp should not be 0, because it's milliseconds since UTC 00:00 Dec 31 1989
        if (0 == first_fit_timestamp) {
          first_fit_timestamp = record_timestamp;
          if (offset > 0) {
            first_fit_timestamp += offset;
          } else if (offset < 0) {
            first_video_timestamp = std::abs(offset);
            subtitles.emplace_back(records_count++, 0, 0, "< .fit data is not available >");
          }
        }

        if (offset > 0) {
          // positive offset, 'offset' second of the data from .fit file will displayed at the first second of the
          // video
          if (record_timestamp < first_fit_timestamp) {
            continue;
          }
        }

        // smoothness
        std::vector<Record> records_to_process;
        if (valid_value_count > 0 && smoothness > 0) {
          Record start_from = fit_result.result[index - 1];
          Record diff = original_record - start_from;
          diff = diff / (smoothness + 1);
          for (int64_t cur_step = 0; cur_step < smoothness; ++cur_step) {
            start_from = start_from + diff;
            records_to_process.push_back(start_from);
          }
        }

        if (false == initial_altitude_set) {
          // set initial altitude
          const auto altitude_by_type = GetValueByType(original_record, DataType::kTypeAltitude);
          if (altitude_by_type.Valid()) {
            initial_altitude_set = true;
            previous_altitude = altitude_by_type.value;
          }
        }

        records_to_process.push_back(original_record);
        // we use it instead of index > 0
        ++valid_value_count;

        for (auto& record : records_to_process) {
          std::string output;
          const auto dst_by_type = GetValueByType(record, DataType::kTypeDistance);
          if (dst_by_type.Valid()) {
            const auto distance(NumberToStringPrecision(dst_by_type.value, 100000.0, 5, 2));
            output += fmt::format("{:>5} km", distance);
          }

          const auto hr_by_type = GetValueByType(record, DataType::kTypeHeartRate);
          if (hr_by_type.Valid()) {
            output += fmt::format("{:>5} bmp", hr_by_type.value);
          }

          const auto cadence_by_type = GetValueByType(record, DataType::kTypeCadence);
          if (cadence_by_type.Valid()) {
            output += fmt::format("{:>5} rpm", cadence_by_type.value);
          }

          const auto power_by_type = GetValueByType(record, DataType::kTypePower);
          if (power_by_type.Valid()) {
            output += fmt::format("{:>6} w", power_by_type.value);
          }

          const auto altitude_by_type = GetValueByType(record, DataType::kTypeAltitude);
          if (altitude_by_type.Valid()) {
            const int64_t altitude_diff = altitude_by_type.value - previous_altitude;
            if (altitude_diff > 0) {
              ascent += altitude_diff;
            } else {
              descent += altitude_diff;
            }
            previous_altitude = altitude_by_type.value;
            output += fmt::format("{:>5} m", (ascent / 5) - 500);
          }

          const auto speed_by_type = GetValueByType(record, DataType::kTypeSpeed);
          if (speed_by_type.Valid()) {
            const auto speed(NumberToStringPrecision(speed_by_type.value, 277.77, 5, 1));
            output += fmt::format("{:>6} km/h", speed);
          }

          const auto temp_by_type = GetValueByType(record, DataType::kTypeTemperature);
          if (temp_by_type.Valid()) {
            output += fmt::format("{:>4} C", temp_by_type.value);
          }

          const auto timestamp_by_type = GetValueByType(record, DataType::kTypeTimeStamp);
          const int64_t current_record_timestamp = timestamp_by_type.Valid() ? timestamp_by_type.value : 0;
          const int64_t milliseconds = (current_record_timestamp - first_fit_timestamp) + first_video_timestamp;
          subtitles.emplace_back(records_count++, milliseconds, milliseconds + 60000, output);
          if (subtitles.size() > 1) {
            subtitles[subtitles.size() - 2].milliseconds_to = milliseconds;
          }
        }
      }

      uint64_t saved_size = 0;
      std::filesystem::remove(output_file);
      std::ofstream output_stream(output_file, std::ios::out | std::ios::app | std::ios::binary);
      for (const auto item : subtitles) {
        const Time time_from(GetTime(item.milliseconds_from));
        const Time time_to(GetTime(item.milliseconds_to));
        const auto file_out(
            fmt::format("{}\n{:0>2d}:{:0>2d}:{:0>2d},{:0>3d} --> {:0>2d}:{:0>2d}:{:0>2d},{:0>3d}\n{}\n\n",
                        item.frame,
                        time_from.hours,
                        time_from.minutes,
                        time_from.seconds,
                        time_from.milliseconds,
                        time_to.hours,
                        time_to.minutes,
                        time_to.seconds,
                        time_to.milliseconds,
                        item.data));
        output_stream.write(file_out.c_str(), file_out.size());
        saved_size += file_out.size();
      }


    } else {
      // checked at the cmd line validation
    }
  } catch (const std::exception& e) {
    // file errors usually
    SPDLOG_ERROR("exception during processing: {}", e.what());
  }

  return 0;
}
