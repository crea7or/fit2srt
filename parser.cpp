#include "parser.h"

#include <spdlog/spdlog.h>

#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "fitsdk/fit_convert.h"

DataTagUnit DataTypeToTag(DataType type) {
  switch (type) {
    case DataType::kTypeAltitude:
      return {kAltitudeTag, kAltitudeUnitsTag};
    case DataType::kTypeLatitude:
      return {kLatitudeTag, kLatitudeUnitsTag};
    case DataType::kTypeLongitude:
      return {kLongitudeTag, kLongitudeUnitsTag};
    case DataType::kTypeSpeed:
      return {kSpeedTag, kSpeedUnitsTag};
    case DataType::kTypeDistance:
      return {kDistanceTag, kDistanceUnitsTag};
    case DataType::kTypeHeartRate:
      return {kHeartRateTag, kHeartRateUnitsTag};
    case DataType::kTypePower:
      return {kPowerTag, kPowerUnitsTag};
    case DataType::kTypeCadence:
      return {kCadenceTag, kCadenceUnitsTag};
    case DataType::kTypeTemperature:
      return {kTemperatureTag, kTemperatureUnitsTag};
    case DataType::kTypeTimeStamp:
      return {kTimeStampTag, kTimeStampUnitsTag};
  }
  if (type != DataType::kTypeNone) {
    throw std::runtime_error("Wrong DataType supplied for conversion to the Tag");
  }
  return {"", ""};
}

FitResult FitParser(std::string input_fit_file) {
  // note: fit_result.used_data_types == DataType::kTypeNone have special meaning after returning from this function -
  // error in processing
  FitResult fit_result;
  try {
    uint32_t records_count = 0;
    const uintmax_t buffer_size = 4096;
    char buffer[buffer_size];

    FIT_CONVERT_RETURN fit_status = FIT_CONVERT_CONTINUE;
    FitConvert_Init(FIT_TRUE);

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
        /*
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
        */

        DataEntry new_entry(fit_record_ptr->timestamp);

        std::string output;
        if (fit_record_ptr->distance != FIT_UINT32_INVALID) {
          // FIT_UINT32 distance = 100 * m = cm
          new_entry.values.emplace_back(kDistanceTag, fit_record_ptr->distance);
          fit_result.used_data_types |= DataType::kTypeDistance;
        }

        if (fit_record_ptr->heart_rate != FIT_BYTE_INVALID) {
          // FIT_UINT8 heart_rate = bpm
          new_entry.values.emplace_back(kHeartRateTag, fit_record_ptr->heart_rate);
          fit_result.used_data_types |= DataType::kTypeHeartRate;
        }

        if (fit_record_ptr->cadence != FIT_BYTE_INVALID) {
          // FIT_UINT8 cadence = rpm
          new_entry.values.emplace_back(kCadenceTag, fit_record_ptr->cadence);
          fit_result.used_data_types |= DataType::kTypeCadence;
        }

        if (fit_record_ptr->power != FIT_UINT16_INVALID) {
          // FIT_UINT16 power = watts
          new_entry.values.emplace_back(kPowerTag, fit_record_ptr->power);
          fit_result.used_data_types |= DataType::kTypePower;
        }

        if (fit_record_ptr->altitude != FIT_UINT16_INVALID) {
          // FIT_UINT16 altitude = 5 * m + 500,
          new_entry.values.emplace_back(kAltitudeTag, fit_record_ptr->altitude);
          fit_result.used_data_types |= DataType::kTypeAltitude;
        }

        if (fit_record_ptr->speed != FIT_UINT16_INVALID) {
          // FIT_UINT16 speed = 1000 * m/s = mm/s
          new_entry.values.emplace_back(kSpeedTag, fit_record_ptr->speed);
          fit_result.used_data_types |= DataType::kTypeSpeed;
        }

        if (fit_record_ptr->temperature != FIT_SINT8_INVALID) {
          // FIT_SINT8 temperature = C
          new_entry.values.emplace_back(kTemperatureTag, fit_record_ptr->temperature);
          fit_result.used_data_types |= DataType::kTypeTemperature;
        }

        if (fit_record_ptr->position_lat != FIT_SINT32_INVALID && fit_record_ptr->position_long != FIT_SINT32_INVALID) {
          // FIT_SINT32 position_lat = semicircles
          // FIT_SINT32 position_long = semicircles
          new_entry.values.emplace_back(kLatitudeTag, fit_record_ptr->position_lat);
          new_entry.values.emplace_back(kLongitudeTag, fit_record_ptr->position_long);
          fit_result.used_data_types |= DataType::kTypeLongitude;
          fit_result.used_data_types |= DataType::kTypeLatitude;
        }

        fit_result.result.emplace_back(std::move(new_entry));

        /*
        const uint32_t seconds = (fit_record_ptr->timestamp - first_fit_timestamp) + first_video_timestamp;
        subtitles.emplace_back(records_count++, seconds, seconds + 60, output);
        if (subtitles.size() > 1) {
          subtitles[subtitles.size() - 2].seconds_to = seconds;
        }
        */
      }
    }
    if (fit_status == FIT_CONVERT_ERROR) {
      SPDLOG_ERROR("error decoding file");
      fit_result.used_data_types = DataType::kTypeNone;
    } else if (fit_status == FIT_CONVERT_CONTINUE) {
      SPDLOG_ERROR("unexpected end of file");
      fit_result.used_data_types = DataType::kTypeNone;
    } else if (fit_status == FIT_CONVERT_DATA_TYPE_NOT_SUPPORTED) {
      SPDLOG_ERROR("file is not FIT file");
      fit_result.used_data_types = DataType::kTypeNone;
    } else if (fit_status == FIT_CONVERT_PROTOCOL_VERSION_NOT_SUPPORTED) {
      SPDLOG_ERROR("protocol version not supported");
      fit_result.used_data_types = DataType::kTypeNone;
    }
  } catch (const std::exception& e) {
    // file errors usually
    SPDLOG_ERROR("exception during processing: {}", e.what());
    fit_result.used_data_types = DataType::kTypeNone;
  }
  return fit_result;
}