#pragma once
#ifndef ONNET_JSON_FIELD_STORAGE_HPP
#define ONNET_JSON_FIELD_STORAGE_HPP

#include "json_types.hpp"
#include <cstdint>
#include <string_view>

namespace onnet::json {

// Tagged union for one parsed field value
// This is public because ParserContext exposes it in its API
struct FieldSlot {
    JsonType type = JsonType::String;
    bool present = false;
    bool is_null = false;
    
    union {
        int32_t  i32;
        uint32_t u32;
        int64_t  i64;
        uint64_t u64;
        double   num;
        bool     boolean;
    } value{};
    
    // For strings: view into request buffer or scratch buffer
    std::string_view str_view;
    
    // Index into ParserContext::object_pool (for nested objects)
    int32_t object_index = -1;
    
    // Index into ParserContext::array_pool (for arrays)
    int32_t array_index = -1;
};

} // namespace onnet::json

#endif // ONNET_JSON_FIELD_STORAGE_HPP