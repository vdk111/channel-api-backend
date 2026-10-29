#pragma once
#ifndef ONNET_JSON_PARSE_ERROR_HPP
#define ONNET_JSON_PARSE_ERROR_HPP

#include "json_types.hpp"  // Provides FieldId and INVALID_FIELD_ID
#include <cstdint>
#include <string>

namespace onnet::json {

enum class ParseError : uint8_t {
    NONE = 0,
    
    // Syntax errors
    INVALID_JSON,
    INVALID_UTF8,
    
    // Structural errors
    UNKNOWN_FIELD,
    MISSING_REQUIRED_FIELD,
    DUPLICATE_KEY,
    MAX_DEPTH_EXCEEDED,
    BODY_TOO_LARGE,
    
    // Type errors
    TYPE_MISMATCH,
    INTEGER_OVERFLOW,
    
    // Constraint errors
    RANGE_VIOLATION,
    STRING_TOO_SHORT,
    STRING_TOO_LONG,
    ESCAPED_STRING_NOT_ALLOWED,
    ARRAY_TOO_SHORT,
    ARRAY_TOO_LONG,
    
    // NEW: Feature-specific errors
    ENUM_VIOLATION,
    PATTERN_VIOLATION,
    NULL_NOT_ALLOWED,
};

const char* parseErrorToString(ParseError error) noexcept;

struct ParseErrorContext {
    ParseError code = ParseError::NONE;
    FieldId field_id = INVALID_FIELD_ID;
    uint64_t byte_offset = 0;
    std::string field_name;
    std::string message;
    
    void clear() noexcept {
        code = ParseError::NONE;
        field_id = INVALID_FIELD_ID;
        byte_offset = 0;
        field_name.clear();
        message.clear();
    }
};

} // namespace onnet::json

#endif // ONNET_JSON_PARSE_ERROR_HPP