#pragma once
#ifndef ONNET_JSON_VIEW_HPP
#define ONNET_JSON_VIEW_HPP

#include "json_types.hpp"
#include "parser_context.hpp"
#include <cstdint>
#include <string_view>
#include <array>
#include <cstddef>

namespace onnet::json {

// Forward declarations
struct FieldSlot;
struct CompiledSchema;
class ParserContext;
class ObjectView;
class ArrayView;

// ── OnnetJsonView ───────────────────────────────────────────
class OnnetJsonView {
public:
    enum class ArrayItemType : uint8_t {
        String = 0, Int32, UInt32, Int64, UInt64, Number, Boolean, Object
    };
    
    OnnetJsonView() = default;
    
    // FieldId getters (High-performance hot path)
    bool contains(FieldId id) const noexcept;
    bool isNull(FieldId id) const noexcept;
    int32_t          getInt32(FieldId id) const noexcept;
    uint32_t         getUInt32(FieldId id) const noexcept;
    int64_t          getInt64(FieldId id) const noexcept;
    uint64_t         getUInt64(FieldId id) const noexcept;
    double           getNumber(FieldId id) const noexcept;
    bool             getBool(FieldId id) const noexcept;
    std::string_view getString(FieldId id) const noexcept;
    ObjectView       getObject(FieldId id) const noexcept;
    ArrayView        getArray(FieldId id) const noexcept;
    
    // String-key convenience getters (Spec F-032)
    bool             contains(std::string_view name) const noexcept;
    bool             isNull(std::string_view name) const noexcept;
    int32_t          getInt32(std::string_view name) const noexcept;
    uint32_t         getUInt32(std::string_view name) const noexcept;
    int64_t          getInt64(std::string_view name) const noexcept;
    uint64_t         getUInt64(std::string_view name) const noexcept;
    double           getNumber(std::string_view name) const noexcept;
    bool             getBool(std::string_view name) const noexcept;
    std::string_view getString(std::string_view name) const noexcept;
    ObjectView       getObject(std::string_view name) const noexcept;
    ArrayView        getArray(std::string_view name) const noexcept;

    std::array<FieldSlot, ParserContext::MAX_FIELDS_PER_OBJECT>* fields_ = nullptr;
    ParserContext* ctx_ = nullptr;
    const CompiledSchema* schema_ = nullptr;
};

// ── ObjectView ──────────────────────────────────────────────
class ObjectView {
public:
    ObjectView() = default;
    
    // Constructor for safe initialization with const pointers
    ObjectView(const std::array<FieldSlot, ParserContext::MAX_FIELDS_PER_OBJECT>* f, const ParserContext* c, const CompiledSchema* s = nullptr, const std::bitset<128>* pm = nullptr) 
        : fields_(f), ctx_(c), schema_(s), present_mask_(pm) {}
    
    // FieldId getters (High-performance hot path)
    bool contains(FieldId id) const noexcept;
    bool isNull(FieldId id) const noexcept;
    int32_t          getInt32(FieldId id) const noexcept;
    uint32_t         getUInt32(FieldId id) const noexcept;
    int64_t          getInt64(FieldId id) const noexcept;
    uint64_t         getUInt64(FieldId id) const noexcept;
    double           getNumber(FieldId id) const noexcept;
    bool             getBool(FieldId id) const noexcept;
    std::string_view getString(FieldId id) const noexcept;
    ObjectView       getObject(FieldId id) const noexcept;
    ArrayView        getArray(FieldId id) const noexcept;

    // String-key convenience getters (Spec F-032)
    bool             contains(std::string_view name) const noexcept;
    bool             isNull(std::string_view name) const noexcept;
    int32_t          getInt32(std::string_view name) const noexcept;
    uint32_t         getUInt32(std::string_view name) const noexcept;
    int64_t          getInt64(std::string_view name) const noexcept;
    uint64_t         getUInt64(std::string_view name) const noexcept;
    double           getNumber(std::string_view name) const noexcept;
    bool             getBool(std::string_view name) const noexcept;
    std::string_view getString(std::string_view name) const noexcept;
    ObjectView       getObject(std::string_view name) const noexcept;
    ArrayView        getArray(std::string_view name) const noexcept;
    
    const std::array<FieldSlot, ParserContext::MAX_FIELDS_PER_OBJECT>* fields_ = nullptr;
    const ParserContext* ctx_ = nullptr;
    const CompiledSchema* schema_ = nullptr;
    const std::bitset<128>* present_mask_ = nullptr;
};

// ── ArrayView ───────────────────────────────────────────────
class ArrayView {
private:
    const void* slot_ = nullptr;
    const ParserContext* ctx_ = nullptr;
    const CompiledSchema* schema_ = nullptr;

public:
    ArrayView() = default;
    ArrayView(const void* slot, const ParserContext* ctx, const CompiledSchema* s = nullptr) 
        : slot_(slot), ctx_(ctx), schema_(s) {}

    size_t size() const noexcept;
    bool empty() const noexcept;

    std::string_view getString(size_t index) const noexcept;
    int32_t          getInt32(size_t index) const noexcept;
    uint32_t         getUInt32(size_t index) const noexcept;
    int64_t          getInt64(size_t index) const noexcept;
    uint64_t         getUInt64(size_t index) const noexcept;
    double           getNumber(size_t index) const noexcept;
    bool             getBool(size_t index) const noexcept;
    ObjectView       getObject(size_t index) const noexcept;
};

} // namespace onnet::json

#endif // ONNET_JSON_VIEW_HPP