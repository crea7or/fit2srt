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

#include <string>
#include <vector>

enum class DataType : uint32_t {
  // always should be zero
  kTypeFirst = 0,
  kTypeSpeed = 0,
  kTypeDistance = 1,
  kTypeHeartRate = 2,
  kTypeAltitude = 3,
  kTypePower = 4,
  kTypeCadence = 5,
  kTypeTemperature = 6,
  kTypeTimeStamp = 7,
  kTypeLatitude = 8,
  kTypeLongitude = 9,
  // always should be at the end
  kTypeMax,
};

inline constexpr uint32_t kDataTypeFirst = static_cast<uint32_t>(DataType::kTypeFirst);
inline constexpr uint32_t kDataTypeMax = static_cast<uint32_t>(DataType::kTypeMax);

struct Record {
  int64_t values[static_cast<uint32_t>(DataType::kTypeMax)]{};
  uint32_t Valid{0};  // mask of values DataType values: 0x01 << DataType
};

constexpr Record operator-(const Record left_value, const Record right_value) {
  Record diff_record;
  diff_record.Valid = left_value.Valid & right_value.Valid;
  for (uint32_t index = kDataTypeFirst; index < kDataTypeMax; ++index) {
    diff_record.values[index] = left_value.values[index] - right_value.values[index];
  }
  return diff_record;
}

constexpr Record operator+(const Record left_value, const Record right_value) {
  Record summ_record;
  summ_record.Valid = left_value.Valid & right_value.Valid;
  for (uint32_t index = kDataTypeFirst; index < kDataTypeMax; ++index) {
    summ_record.values[index] = left_value.values[index] + right_value.values[index];
  }
  return summ_record;
}

constexpr Record operator/(const Record left_value, const int64_t divider) {
  Record divided_record;
  divided_record.Valid = left_value.Valid;
  for (uint32_t index = kDataTypeFirst; index < kDataTypeMax; ++index) {
    divided_record.values[index] = left_value.values[index] / divider;
  }
  return divided_record;
}

struct DataTagUnit {
  DataTagUnit() = default;
  DataTagUnit(std::string_view tag, std::string_view units) : data_tag(std::move(tag)), data_units(std::move(units)) {}

  bool IsValid() const { return false == data_tag.empty() && false == data_units.empty(); }

  std::string_view data_tag;
  std::string_view data_units;
};

enum class ParseResult {
  kSuccess,
  kError,
};

struct FitResult {
  // parsing status
  ParseResult status{ParseResult::kError};

  // parsed data from file
  std::vector<Record> result;

  // header for all available types of data in this file
  std::vector<DataTagUnit> header;
  // header in bitmask format
  uint32_t header_flags{0};
};

std::string_view DataTypeToName(const DataType type);
std::string_view DataTypeToUnit(const DataType type);
uint32_t DataTypeToMask(const DataType type);

FitResult FitParser(std::string input);
