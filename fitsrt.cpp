/*

 MIT License

 Copyright (c) 2021 pavel.sokolov@gmail.com / CEZEO software Ltd. All rights reserved.

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

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "fitsdk/fit_convert.h"
#include "stdio.h"
#include "string.h"

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
 C E Z E O  S O F T W A R E (c) 2022   FIT telemetry to SRT subtitles converter

)%";

constexpr const char kHelp[] = R"%(

usage: fitsrt <input> <output> <offset>

input - path to .fit file to read data from
output - path to .srt file to write subtitle to
offset - optional offset in seconds to sync video and .fit data
* if the offset is positive - 'offset' second of the data from .fit file will be displayed at the first second of the video.
    it is for situations when you started video after starting recording your activity(that generated .fit file)
* if the offset is negative - the first second of .fit data will be displayed at abs('offset') second of the video
    it is for situations when you started your activity (that generated .fit file) after starting the video

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

int main(int argc, char* argv[]) {
  spdlog::set_pattern("[%H:%M:%S.%e] %^[%l]%$ %v");
  std::cout << kBanner << std::endl;

  if (argc < 3) {
    SPDLOG_ERROR(kHelp);
    return 1;
  }

  int32_t offset = 0;
  if (argc == 4) {
    char* end_ptr = nullptr;
    try {
      offset = std::strtol(argv[3], &end_ptr, 10);
    } catch (const std::exception& e) { SPDLOG_ERROR("wring offset param: {}", e.what()); }
  }

  const std::string input_fit_file(argv[1]);
  const std::string output_srt_file(argv[2]);

  uint32_t records_count = 0;
  const uintmax_t buffer_size = 4096;
  char buffer[buffer_size];

  FIT_CONVERT_RETURN fit_status = FIT_CONVERT_CONTINUE;
  FitConvert_Init(FIT_TRUE);

  std::vector<SrtItem> subtitles;

  try {
    std::ifstream input_stream(input_fit_file, std::ios::in | std::ios::app | std::ios::binary);
    const uintmax_t input_file_size = std::filesystem::file_size(input_fit_file);
    uintmax_t was_read = 0;

    uint32_t first_video_timestamp = 0;
    uint32_t first_fit_timestamp = 0;

    SPDLOG_INFO("opening file: {}, size: {} bytes", input_fit_file, input_file_size);

    while ((input_file_size > was_read) && (fit_status == FIT_CONVERT_CONTINUE)) {
      const auto available_bytes = input_file_size - was_read;
      const auto bytes_read = available_bytes < buffer_size ? available_bytes : buffer_size;
      input_stream.read(buffer, bytes_read);
      was_read += bytes_read;

      while (fit_status = FitConvert_Read(buffer, static_cast<FIT_UINT32>(bytes_read)),
             fit_status == FIT_CONVERT_MESSAGE_AVAILABLE) {
        if (FitConvert_GetMessageNumber() != FIT_MESG_NUM_RECORD) {
          continue;
        }

        const FIT_UINT8* fit_message_ptr = FitConvert_GetMessageData();
        const FIT_RECORD_MESG* fit_record_ptr = reinterpret_cast<const FIT_RECORD_MESG*>(fit_message_ptr);

        // fit timestamp should not be 0, because it's seconds since UTC 00:00 Dec 31 1989
        if (0 == first_fit_timestamp) {
          first_fit_timestamp = fit_record_ptr->timestamp;
          if (offset > 0) {
            first_fit_timestamp += offset;
          } else if (offset < 0) {
            first_video_timestamp = std::abs(offset);
            subtitles.emplace_back(records_count++, 0, 0, "< .fit data is not available >");
          }
        }

        if (offset > 0) {
          // positive offset, 'offset' second of the data from .fit file will displayed at the first second of the video
          if (fit_record_ptr->timestamp < first_fit_timestamp) {
            continue;
          }
        }

        std::string output;
        if (fit_record_ptr->distance != FIT_UINT32_INVALID) {
          uint64_t dst = fit_record_ptr->distance;
          dst = dst / 1000;
          auto distance(std::to_string(dst));
          if (distance.size() < 3) {
            distance = "00" + distance;
          }
          distance.insert(distance.size() - 2, 1, '.');
          output += fmt::format(" {:>7} km ", distance);
        }

        if (fit_record_ptr->heart_rate != FIT_BYTE_INVALID) {
          output += fmt::format(" {:>3} bmp ", fit_record_ptr->heart_rate);
        }

        if (fit_record_ptr->cadence != FIT_BYTE_INVALID) {
          output += fmt::format(" {:>3} rpm ", fit_record_ptr->cadence);
        }

        if (fit_record_ptr->accumulated_power != FIT_UINT32_INVALID) {
          output += fmt::format(" {:>4} wt ", fit_record_ptr->accumulated_power);
        }

        if (fit_record_ptr->altitude != FIT_UINT16_INVALID) {
          output += fmt::format(" {:>4} m ", (fit_record_ptr->altitude / 5) - 500);
        }

        if (fit_record_ptr->speed != FIT_UINT16_INVALID) {
          const uint16_t spd = (uint16_t)((double)(fit_record_ptr->speed) / 2.7777);
          auto speed(std::to_string(spd));
          if (speed.size() < 3) {
            speed = "00" + speed;
          }
          speed.insert(speed.size() - 2, 1, '.');
          output += fmt::format(" {:>6} km/h ", speed);
        }

        if (fit_record_ptr->temperature != FIT_SINT8_INVALID) {
          output += fmt::format(" {:>3} C ", fit_record_ptr->temperature);
        }

        const uint32_t seconds = (fit_record_ptr->timestamp - first_fit_timestamp) + first_video_timestamp;
        subtitles.emplace_back(records_count++, seconds, seconds + 60, output);
        if (subtitles.size() > 1) {
          subtitles[subtitles.size() - 2].seconds_to = seconds;
        }
      }
    }
    if (fit_status == FIT_CONVERT_END_OF_FILE) {
      if (subtitles.size() == 0) {
        SPDLOG_ERROR("no subtitles generated");
        return 1;
      }

      uint64_t saved_size = 0;
      std::filesystem::remove(output_srt_file);
      std::ofstream output_stream(output_srt_file, std::ios::out | std::ios::app | std::ios::binary);
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

      SPDLOG_INFO("subtitles saved to: {}, size: {}", output_srt_file, saved_size);
      return 0;

    } else if (fit_status == FIT_CONVERT_ERROR) {
      SPDLOG_ERROR("error decoding file");
    } else if (fit_status == FIT_CONVERT_CONTINUE) {
      SPDLOG_ERROR("unexpected end of file");
    } else if (fit_status == FIT_CONVERT_DATA_TYPE_NOT_SUPPORTED) {
      SPDLOG_ERROR("file is not FIT file");
    } else if (fit_status == FIT_CONVERT_PROTOCOL_VERSION_NOT_SUPPORTED) {
      SPDLOG_ERROR("protocol version not supported");
    }
  } catch (const std::exception& e) {
    // file errors usually
    SPDLOG_ERROR("exception during processing: {}", e.what());
  }

  return 1;
}
