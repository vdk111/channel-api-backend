#pragma once
#ifndef ONNET_JSON_TYPES_HPP
#define ONNET_JSON_TYPES_HPP

#include <cstdint>
#include <string> 
#include <bitset>

namespace onnet::json {

using FieldId = uint16_t;
using RouteId = uint32_t;
using RequiredFieldMask = std::bitset<128>; 

constexpr FieldId INVALID_FIELD_ID = UINT16_MAX;

enum class JsonType : uint8_t {
    String,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Number,
    Boolean,
    Object,
    Array
};

const char* jsonTypeToString(JsonType type) noexcept;
JsonType jsonTypeFromString(const std::string& str);

} // namespace onnet::json

#endif // ONNET_JSON_TYPES_HPP
