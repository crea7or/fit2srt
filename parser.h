#include <string>
#include <vector>

constexpr char* kSpeedTag = "speed";
constexpr char* kSpeedUnitsTag = "m/sec";

constexpr char* kDistanceTag = "distance";
constexpr char* kDistanceUnitsTag = "cm";

constexpr char* kHeartRateTag = "heartrate";
constexpr char* kHeartRateUnitsTag = "bpm";

constexpr char* kAltitudeTag = "altitude";
constexpr char* kAltitudeUnitsTag = "cm";

constexpr char* kPowerTag = "power";
constexpr char* kPowerUnitsTag = "w";

constexpr char* kCadenceTag = "cadence";
constexpr char* kCadenceUnitsTag = "rpm";

constexpr char* kTemperatureTag = "temperature";
constexpr char* kTemperatureUnitsTag = "c";

constexpr char* kTimeStampTag = "timestamp";
constexpr char* kTimeStampUnitsTag = "sec";

constexpr char* kLatitudeTag = "latitude";
constexpr char* kLatitudeUnitsTag = "semicircles";

constexpr char* kLongitudeTag = "longitude";
constexpr char* kLongitudeUnitsTag = "semicircles";

constexpr char* kOutputJsonTag = "json";
constexpr char* kOutputSrtTag = "srt";
constexpr char* kInputStdinTag = "stdin";
constexpr char* kOutputStdoutTag = "stdout";


enum class DataType : uint32_t {
  kTypeNone = 0,                 //
  kTypeSpeed = 0x01 << 0,        //
  kTypeDistance = 0x01 << 1,     //
  kTypeHeartRate = 0x01 << 2,    //
  kTypeAltitude = 0x01 << 3,     //
  kTypePower = 0x01 << 4,        //
  kTypeCadence = 0x01 << 5,      //
  kTypeTemperature = 0x01 << 6,  //
  kTypeTimeStamp = 0x01 << 7,    //
  kTypeLatitude = 0x01 << 8,     //
  kTypeLongitude = 0x01 << 9,    //
};

constexpr DataType operator|(const DataType selfValue, const DataType inValue) {
  return (DataType)(uint32_t(selfValue) | uint32_t(inValue));
}

constexpr DataType operator&(const DataType selfValue, const DataType inValue) {
  return (DataType)(uint32_t(selfValue) & uint32_t(inValue));
}

constexpr DataType& operator|=(DataType& selfValue, DataType inValue) {
  selfValue = (DataType)(uint32_t(selfValue) | uint32_t(inValue));
  return selfValue;
}

// int64_t is used for all values because uint32_t/int32_t is the maximum possible value in the .fit file
// value type <> value data
struct DataPoint {
  DataPoint(const char* type_ptr, int32_t value) : type(type_ptr), value(value) {}
  DataPoint(const char* type_ptr, uint32_t value) : type(type_ptr), value(static_cast<int64_t>(value)) {}
  DataPoint(const DataPoint&) = default;
  DataPoint(DataPoint&&) = default;
  std::string type;
  int64_t value;
};

// single data entry for one record
struct DataEntry {
  explicit DataEntry(uint32_t timestamp) : timestamp(static_cast<int64_t>(timestamp)) {}
  DataEntry(const DataEntry&) = default;
  DataEntry(DataEntry&&) = default;

  int64_t timestamp;
  std::vector<DataPoint> values;
};

struct DataTagUnit {
  DataTagUnit(const char* tag, const char* units)
      : data_tag(tag, std::char_traits<char>::length(tag)),
        data_units_tag(units, std::char_traits<char>::length(units)) {}
  std::string data_tag;
  std::string data_units_tag;
};

struct FitResult {
  // parsed data from .fit
  std::vector<DataEntry> result;

  // header for all available types of data
  DataType used_data_types{DataType::kTypeNone};
};

DataTagUnit DataTypeToTag(DataType type);

FitResult FitParser(std::string input);
