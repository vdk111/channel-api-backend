#pragma once
#ifndef ONNET_JSON_PARSER_CONTEXT_HPP
#define ONNET_JSON_PARSER_CONTEXT_HPP

#include "parse_error.hpp"
#include "json_types.hpp"
#include "field_storage.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <bitset>

namespace onnet::json {

struct ParserContext {
    ParseErrorContext error;
    std::string scratch_buffer;
    std::string_view request_buffer;
    uint16_t current_depth = 0;

    //  ZERO HEAP ALLOCATION: Optimized limits for stack safety (~41 KB total size)
    static constexpr size_t MAX_ROOT_FIELDS = 128;
    static constexpr size_t MAX_OBJECTS = 12;
    static constexpr size_t MAX_ARRAYS  = 8;
    static constexpr size_t MAX_FIELDS_PER_OBJECT = 128;
    static constexpr size_t MAX_ARRAY_ITEMS = 64;
    
    struct ObjectSlot {
        std::array<FieldSlot, MAX_FIELDS_PER_OBJECT> fields;
        std::bitset<128> present_mask = 0;
        
        void reset() noexcept {
            present_mask.reset();
        }
    };
    
    struct ArraySlot {
        uint8_t item_type = 0;
        size_t item_count = 0;
        
        //  Anonymous union: an array only holds ONE type of items at a time
        // Saves ~6 KB per ArraySlot (drops size from 7 KB to ~1 KB)
        union {
            std::array<std::string_view, MAX_ARRAY_ITEMS> strings;
            std::array<int32_t, MAX_ARRAY_ITEMS>  i32s;   
            std::array<uint32_t, MAX_ARRAY_ITEMS> u32s;   
            std::array<int64_t, MAX_ARRAY_ITEMS>  i64s;
            std::array<uint64_t, MAX_ARRAY_ITEMS> u64s;
            std::array<double, MAX_ARRAY_ITEMS>   nums;
            std::array<bool, MAX_ARRAY_ITEMS>     bools;
            std::array<int32_t, MAX_ARRAY_ITEMS>  object_indices;
        };
        
        ArraySlot() noexcept : item_type(0), item_count(0), strings{} {}
        ~ArraySlot() noexcept = default;
        
        void reset() noexcept {
            item_count = 0;
            item_type = 0;
        }
    };
    
    // Root level fields
    std::array<FieldSlot, MAX_ROOT_FIELDS> root_fields;
    std::bitset<128> root_present_mask = 0;

    ObjectSlot object_pool[MAX_OBJECTS];
    ArraySlot  array_pool[MAX_ARRAYS];
    size_t object_count = 0;
    size_t array_count = 0;
    
    int32_t allocateObject() {
        if (object_count >= MAX_OBJECTS) return -1;
        size_t idx = object_count++;
        object_pool[idx].reset();
        return static_cast<int32_t>(idx);
    }
    
    int32_t allocateArray() {
        if (array_count >= MAX_ARRAYS) return -1;
        size_t idx = array_count++;
        array_pool[idx].reset();
        return static_cast<int32_t>(idx);
    }
    
    void reset() noexcept {
        error.clear();
        scratch_buffer.clear();
        request_buffer = {};
        current_depth = 0;
        object_count = 0;
        array_count = 0;
        root_present_mask.reset();
    }
};

} // namespace onnet::json
#endif


