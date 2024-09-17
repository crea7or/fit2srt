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

#include "parser.h"

#include <spdlog/spdlog.h>

#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "fitsdk/fit_convert.h"

namespace {

constexpr std::string_view kSpeedTag("speed");
constexpr std::string_view kSpeedUnitsTag("mm/sec");

constexpr std::string_view kDistanceTag("distance");
constexpr std::string_view kDistanceUnitsTag("cm");

constexpr std::string_view kHeartRateTag("heartrate");
constexpr std::string_view kHeartRateUnitsTag("bpm");

constexpr std::string_view kAltitudeTag("altitude");
constexpr std::string_view kAltitudeUnitsTag("cm");

constexpr std::string_view kPowerTag("power");
constexpr std::string_view kPowerUnitsTag("w");

constexpr std::string_view kCadenceTag("cadence");
constexpr std::string_view kCadenceUnitsTag("rpm");

constexpr std::string_view kTemperatureTag("temperature");
constexpr std::string_view kTemperatureUnitsTag("c");

constexpr std::string_view kTimeStampTag("timestamp");
constexpr std::string_view kTimeStampUnitsTag("sec");

constexpr std::string_view kLatitudeTag("latitude");
constexpr std::string_view kLatitudeUnitsTag("semicircles");

constexpr std::string_view kLongitudeTag("longitude");
constexpr std::string_view kLongitudeUnitsTag("semicircles");

constexpr std::string_view kStdinTag("stdin");

struct Buffer final {
 public:
  Buffer(const size_t buffer_size) { buffer_.resize(buffer_size); }

  void SetDataSize(const size_t size) { data_size_ = size; }

  size_t GetDataSize() const { return data_size_; }

  size_t GetBufferSize() const { return buffer_.size(); }

  char* GetDataPtr() { return buffer_.data(); }

 private:
  std::vector<char> buffer_;
  size_t data_size_{0};
};

// for std::cin unique_ptr
struct NoopDeleter final {
  void operator()(...) const {}
};

class DataSource {
 public:
  enum class Type {
    kFile,
    kStdin,
  };

  enum class Status {
    kContinueRead,
    kEndOfFile,
    kError,
  };

  DataSource(Type type) : type_(type) {}

  virtual ~DataSource() = default;

  virtual Status ReadData(Buffer& buffer) = 0;

 protected:
  Status ReadDataInternal(std::istream& stream, Buffer& buffer) {
    try {
      stream.read(buffer.GetDataPtr(), buffer.GetBufferSize());
      buffer.SetDataSize(stream.gcount());
      if (stream.eof()) {
        return Status::kEndOfFile;
      } else if (stream.good()) {
        return Status::kContinueRead;
      }
    } catch (const std::exception& e) {
      SPDLOG_ERROR("input file reading error: {}", e.what());  //
    }
    return Status::kError;
  }

 private:
  Type type_{Type::kFile};
};


class DataSourceFile : public DataSource {
 public:
  DataSourceFile(std::string source_name) : DataSource(DataSource::Type::kFile), source_name_(std::move(source_name)) {
    stream_ = std::make_unique<std::ifstream>(source_name_, std::ios::in | std::ios::app | std::ios::binary);
  }

  Status ReadData(Buffer& buffer) override {
    return ReadDataInternal(*stream_.get(), buffer);  //
  }

 private:
  std::string source_name_;
  std::unique_ptr<std::istream> stream_;
};

class DataSourceStdin : public DataSource {
 public:
  DataSourceStdin() : DataSource(DataSource::Type::kStdin) {}

  Status ReadData(Buffer& buffer) override {
    return ReadDataInternal(std::cin, buffer);  //
  }
};

}  // namespace

uint32_t DataTypeToMask(const DataType type) {
  return 0x01 << static_cast<uint32_t>(type);
}

std::string_view DataTypeToName(const DataType type) {
  switch (type) {
    case DataType::kTypeAltitude:
      return kAltitudeTag;
    case DataType::kTypeLatitude:
      return kLatitudeTag;
    case DataType::kTypeLongitude:
      return kLongitudeTag;
    case DataType::kTypeSpeed:
      return kSpeedTag;
    case DataType::kTypeDistance:
      return kDistanceTag;
    case DataType::kTypeHeartRate:
      return kHeartRateTag;
    case DataType::kTypePower:
      return kPowerTag;
    case DataType::kTypeCadence:
      return kCadenceTag;
    case DataType::kTypeTemperature:
      return kTemperatureTag;
    case DataType::kTypeTimeStamp:
      return kTimeStampTag;
  }
  return "";
}

std::string_view DataTypeToUnit(const DataType type) {
  switch (type) {
    case DataType::kTypeAltitude:
      return kAltitudeUnitsTag;
    case DataType::kTypeLatitude:
      return kLatitudeUnitsTag;
    case DataType::kTypeLongitude:
      return kLongitudeUnitsTag;
    case DataType::kTypeSpeed:
      return kSpeedUnitsTag;
    case DataType::kTypeDistance:
      return kDistanceUnitsTag;
    case DataType::kTypeHeartRate:
      return kHeartRateUnitsTag;
    case DataType::kTypePower:
      return kPowerUnitsTag;
    case DataType::kTypeCadence:
      return kCadenceUnitsTag;
    case DataType::kTypeTemperature:
      return kTemperatureUnitsTag;
    case DataType::kTypeTimeStamp:
      return kTimeStampUnitsTag;
  }
  return "";
}

void HeaderItem(std::vector<DataTagUnit>& header, const uint32_t header_bitmask, const DataType type) {
  const uint32_t type_bitmask = DataTypeToMask(type);
  if ((header_bitmask & type_bitmask) != 0) {
    header.emplace_back(DataTypeToName(type), DataTypeToUnit(type));
  }
}

void ApplyValue(Record& new_record, const DataType data_type, const int64_t value) {
  uint32_t data_type_index = static_cast<uint32_t>(data_type);
  new_record.values[data_type_index] = value;
  new_record.Valid |= DataTypeToMask(data_type);
}

FitResult FitParser(std::string input_fit_file) {
  FitResult fit_result;
  uint32_t used_data_types{0};  // mask of values DataType values: 0x01 << DataType

  try {
    FIT_CONVERT_RETURN fit_status = FIT_CONVERT_CONTINUE;
    FitConvert_Init(FIT_TRUE);

    Buffer data_buffer(4096);
    std::unique_ptr<DataSource> data_source;
    if (kStdinTag == input_fit_file) {
      data_source = std::make_unique<DataSourceStdin>();
    } else {
      data_source = std::make_unique<DataSourceFile>(input_fit_file);
    }

    while ((DataSource::Status::kError != data_source->ReadData(data_buffer)) && (fit_status == FIT_CONVERT_CONTINUE)) {
      while (fit_status = FitConvert_Read(data_buffer.GetDataPtr(), static_cast<FIT_UINT32>(data_buffer.GetDataSize())),
             fit_status == FIT_CONVERT_MESSAGE_AVAILABLE) {
        if (FitConvert_GetMessageNumber() != FIT_MESG_NUM_RECORD) {
          continue;
        }

        const FIT_UINT8* fit_message_ptr = FitConvert_GetMessageData();
        const FIT_RECORD_MESG* fit_record_ptr = reinterpret_cast<const FIT_RECORD_MESG*>(fit_message_ptr);

        Record new_record;
        ApplyValue(new_record, DataType::kTypeTimeStamp, fit_record_ptr->timestamp);

        std::string output;
        if (fit_record_ptr->distance != FIT_UINT32_INVALID) {
          // FIT_UINT32 distance = 100 * m = cm
          ApplyValue(new_record, DataType::kTypeDistance, fit_record_ptr->distance);
        }

        if (fit_record_ptr->heart_rate != FIT_BYTE_INVALID) {
          // FIT_UINT8 heart_rate = bpm
          ApplyValue(new_record, DataType::kTypeHeartRate, fit_record_ptr->heart_rate);
        }

        if (fit_record_ptr->cadence != FIT_BYTE_INVALID) {
          // FIT_UINT8 cadence = rpm
          ApplyValue(new_record, DataType::kTypeCadence, fit_record_ptr->cadence);
        }

        if (fit_record_ptr->power != FIT_UINT16_INVALID) {
          // FIT_UINT16 power = watts
          ApplyValue(new_record, DataType::kTypePower, fit_record_ptr->power);
        }

        if (fit_record_ptr->altitude != FIT_UINT16_INVALID) {
          // FIT_UINT16 altitude = 5 * m + 500
          ApplyValue(new_record, DataType::kTypeAltitude, fit_record_ptr->altitude);
        }

        if (fit_record_ptr->enhanced_altitude != FIT_UINT32_INVALID) {
          // FIT_UINT32 enhanced_altitude = 5 * m + 500
          ApplyValue(new_record, DataType::kTypeAltitude, fit_record_ptr->enhanced_altitude);
        }

        if (fit_record_ptr->speed != FIT_UINT16_INVALID) {
          // FIT_UINT16 speed = 1000 * m/s = mm/s
          ApplyValue(new_record, DataType::kTypeSpeed, fit_record_ptr->speed);
        }

        if (fit_record_ptr->enhanced_speed != FIT_UINT32_INVALID) {
          // FIT_UINT32 enhanced_speed = 1000 * m/s = mm/s
          ApplyValue(new_record, DataType::kTypeSpeed, fit_record_ptr->enhanced_speed);
        }

        if (fit_record_ptr->temperature != FIT_SINT8_INVALID) {
          // FIT_SINT8 temperature = C
          ApplyValue(new_record, DataType::kTypeTemperature, fit_record_ptr->temperature);
        }

        if (fit_record_ptr->position_lat != FIT_SINT32_INVALID && fit_record_ptr->position_long != FIT_SINT32_INVALID) {
          // FIT_SINT32 position_lat = semicircles
          // FIT_SINT32 position_long = semicircles
          ApplyValue(new_record, DataType::kTypeLatitude, fit_record_ptr->position_lat);
          ApplyValue(new_record, DataType::kTypeLongitude, fit_record_ptr->position_long);
        }

        // first apply to global flags
        used_data_types |= new_record.Valid;
        // then move
        fit_result.result.emplace_back(std::move(new_record));
      }
    }

    if (fit_status == FIT_CONVERT_END_OF_FILE) {
      // success
      fit_result.status = ParseResult::kSuccess;
      fit_result.header_flags = used_data_types;

      HeaderItem(fit_result.header, used_data_types, DataType::kTypeAltitude);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeCadence);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeDistance);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeHeartRate);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeLatitude);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeLongitude);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypePower);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeSpeed);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeTemperature);
      HeaderItem(fit_result.header, used_data_types, DataType::kTypeTimeStamp);

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
  return fit_result;
}
