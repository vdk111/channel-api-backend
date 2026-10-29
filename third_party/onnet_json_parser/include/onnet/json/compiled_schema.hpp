#pragma once
#ifndef ONNET_JSON_COMPILED_SCHEMA_HPP
#define ONNET_JSON_COMPILED_SCHEMA_HPP

#include "json_types.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace onnet::json {

// Named format types (replaces regex patterns)
enum class FormatType : uint8_t {
    None = 0,
    Email,
    Uri,      // Future
    Uuid,     // Future
    Ipv4,     // Future
    Date      // Future
};

struct FieldRule {
    FieldId id;
    JsonType type;
    bool required;
    uint64_t name_hash;
    std::string name;
    std::string full_name;
    
    // Nullable support
    bool nullable = false;
    
    // Enum validation
    std::vector<std::string> enum_values;
    std::vector<int64_t> enum_int_values;
    std::vector<double> enum_num_values;
    
    // Named format validation (replaces regex)
    FormatType format_type = FormatType::None;
    
    // Integer constraints
    int64_t min_int = 0;
    int64_t max_int = 0;
    bool has_min_int = false;
    bool has_max_int = false;
    
    // Floating-point constraints
    double min_num = 0.0;
    double max_num = 0.0;
    bool has_min_num = false;
    bool has_max_num = false;
    
    // String constraints
    size_t min_length = 0;
    size_t max_length = SIZE_MAX;
    bool allow_escapes = true;
    
    // Array constraints
    size_t min_items = 0;
    size_t max_items = SIZE_MAX;
    
    // Nested schema (for object/array)
    std::shared_ptr<struct CompiledSchema> nested_schema;
    std::shared_ptr<FieldRule> items_rule;
};

struct FieldLookupTable {
    struct Entry {
        std::string name;
        FieldId id;
    };
    
    std::vector<Entry> entries;
    
    void add(std::string name, FieldId id) {
        entries.push_back({std::move(name), id});
    }
    
    void build() {
        std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.name < b.name; });
    }
    
    FieldId find(std::string_view name) const noexcept {
        if (entries.empty()) return INVALID_FIELD_ID;
        
        size_t lo = 0;
        size_t hi = entries.size();
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            std::string_view mid_name(entries[mid].name);
            if (mid_name < name) {
                lo = mid + 1;
            } else if (mid_name > name) {
                hi = mid;
            } else {
                return entries[mid].id;
            }
        }
        return INVALID_FIELD_ID;
    }
    
    size_t size() const noexcept { return entries.size(); }
};

struct CompiledSchema {
    RouteId route_id;
    std::string api_name;
    std::string version;
    std::string method;
    std::string path;
    
    FieldLookupTable lookup;
    std::vector<FieldRule> fields;
    RequiredFieldMask required_mask = 0;
    
    uint32_t max_body_bytes = 16384;
    uint16_t max_depth = 10;
    bool allow_unknown_fields = false;
};

} // namespace onnet::json

#endif // ONNET_JSON_COMPILED_SCHEMA_HPP
