#pragma once
#ifndef ONNET_JSON_JSON_SERIALIZER_HPP
#define ONNET_JSON_JSON_SERIALIZER_HPP

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <utility>
#include <type_traits>
#include <glaze/glaze.hpp>

namespace onnet::json {

/**
 * @brief Outbound response serialization diagnostic and status result.
 * Matches <<status & diagnostics>> SerializationResult in architecture diagram.
 */
class SerializationResult {
public:
    bool success{false};
    size_t bytes_written{0};
    std::string error_message{};

    SerializationResult() noexcept = default;
    SerializationResult(bool ok, size_t written, std::string_view err = {}) noexcept
        : success(ok), bytes_written(written) {
        try {
            error_message = std::string(err);
        } catch (...) {
            error_message.clear();
        }
    }

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
    [[nodiscard]] bool ok() const noexcept { return success; }
    [[nodiscard]] size_t size() const noexcept { return bytes_written; }
    [[nodiscard]] size_t bytes() const noexcept { return bytes_written; }
    [[nodiscard]] std::string_view error() const noexcept { return error_message; }

    static SerializationResult ok(size_t written) noexcept {
        return SerializationResult(true, written);
    }
    static SerializationResult fail(std::string_view err = {}) noexcept {
        return SerializationResult(false, 0, err);
    }
};

/**
 * @brief Backward-compatible error context struct for existing codebase.
 * Fully interoperable with modern SerializationResult.
 */
struct SerializationError {
    bool has_error{false};
    std::string message{};

    SerializationError() = default;
    SerializationError(bool err, std::string msg) : has_error(err), message(std::move(msg)) {}
    SerializationError(const SerializationResult& res)
        : has_error(!res.ok()), message(std::string(res.error())) {}

    [[nodiscard]] explicit operator bool() const noexcept { return !has_error; }
    [[nodiscard]] bool ok() const noexcept { return !has_error; }
    [[nodiscard]] std::string_view error() const noexcept { return message; }
    operator SerializationResult() const noexcept {
        return has_error ? SerializationResult::fail(message) : SerializationResult::ok(0);
    }
};

/**
 * @brief Outbound reusable contiguous memory buffer for zero-allocation wire serialization.
 * Matches <<reusable memory buffer>> SerializationBuffer in architecture diagram.
 *
 * Implements high-performance memory reuse backed by std::string:
 * - Guaranteed null-termination (safe for C-APIs, syslog, HTTP socket handoff).
 * - Small String Optimization (SSO) for small payloads.
 * - Move-only semantics to prevent accidental deep-heap copies in network pipelines.
 * - Implicit conversion to std::string_view for zero-copy socket dispatch.
 *
 * @note Memory Retention & Shrink Policy:
 *       - Normal Payloads (<= 64 KB): clear() preserves capacity for 0 heap allocations across reuse cycles.
 *       - Outlier Payloads (> 64 KB): clear() automatically shrinks capacity back to 4 KB baseline,
 *         freeing excess memory to the OS.
 *       - Hardware Alignment: Pre-allocations round up to 64-byte hardware cache line boundaries.
 *
 * @warning Thread Safety & Lifetime Contract:
 *       - Thread Safety: SerializationBuffer is NOT thread-safe. A single instance must never be
 *         accessed concurrently across multiple threads. Use thread_local instances for lock-free scaling.
 *       - Async Lifetime: Borrowed string_views/pointers via string_view() or data() remain valid only
 *         until the next call to clear() or serialize(). For async network dispatch across thread
 *         boundaries, transfer ownership using std::move() or create an isolated copy via clone().
 */
class SerializationBuffer {
private:
    std::string buffer_;

public:
    static constexpr size_t DEFAULT_CAPACITY = 4096;      // 4 KB baseline (1 memory page)
    static constexpr size_t MAX_RETAIN_CAPACITY = 65536;  // 64 KB High-Water Mark threshold

    SerializationBuffer() : SerializationBuffer(DEFAULT_CAPACITY) {}

    explicit SerializationBuffer(size_t initial_capacity) {
        if (initial_capacity > 0) {
            reserve(initial_capacity);
        }
    }

    // Move-only semantics: enforces zero-copy across async boundaries
    SerializationBuffer(const SerializationBuffer&) = delete;
    SerializationBuffer& operator=(const SerializationBuffer&) = delete;
    SerializationBuffer(SerializationBuffer&&) noexcept = default;
    SerializationBuffer& operator=(SerializationBuffer&&) noexcept = default;

    [[nodiscard]] SerializationBuffer clone() const {
        SerializationBuffer copy(0);
        copy.buffer_ = buffer_;
        return copy;
    }

    void reserve(size_t capacity) {
        // Chunk pre-allocations in 64-byte blocks with overflow protection
        if (capacity > SIZE_MAX - 63) {
            buffer_.reserve(capacity);
            return;
        }
        size_t chunked_cap = (capacity + 63) & ~size_t(63);
        buffer_.reserve(chunked_cap);
    }

    void clear() noexcept {
        // High-Water Mark Protection:
        // Normal payload (<= 64KB): retain capacity for zero-allocation reuse.
        // Extreme outlier (> 64KB): best-effort reset back to 4 KB baseline to prevent permanent RAM bloat.
        if (buffer_.capacity() <= MAX_RETAIN_CAPACITY) {
            buffer_.clear();
        } else {
            try {
                std::string fresh_buffer;
                size_t chunked_cap = (DEFAULT_CAPACITY + 63) & ~size_t(63);
                fresh_buffer.reserve(chunked_cap);
                buffer_.swap(fresh_buffer);
            } catch (...) {
                buffer_.clear();
            }
        }
    }

    [[nodiscard]] const char* data() const noexcept {
        return buffer_.data();
    }

    [[nodiscard]] const char* c_str() const noexcept {
        return buffer_.c_str();
    }

    [[nodiscard]] size_t size() const noexcept {
        return buffer_.size();
    }

    [[nodiscard]] size_t bytes_written() const noexcept {
        return buffer_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return buffer_.empty();
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return buffer_.capacity();
    }

    [[nodiscard]] std::string_view string_view() const noexcept {
        return std::string_view(buffer_);
    }

    [[nodiscard]] operator std::string_view() const noexcept {
        return std::string_view(buffer_);
    }

    [[nodiscard]] std::string& raw_buffer() noexcept {
        return buffer_;
    }

    [[nodiscard]] const std::string& raw_buffer() const noexcept {
        return buffer_;
    }
};

/**
 * @brief High-performance RFC 8259 compliant outbound JSON serialization facade.
 * Matches <<stateless facade>> OnnetJsonSerializer in architecture diagram.
 *
 * ============================================================================
 * ARCHITECTURAL CONTRACTS:
 * ============================================================================
 *
 * 1. POINTER REJECTION CONTRACT:
 *    - Disallowed (Compile-time): Raw pointers (T*) are strictly rejected via static_assert
 *      to prevent accidental serialization of memory addresses or pointer semantics.
 *      Callers must pass objects by value or dereference them (*ptr).
 *    - Disallowed: Raw character arrays / C-strings (const char*). Callers must use
 *      std::string_view or std::string.
 *    - Allowed: Smart pointers (std::unique_ptr<T>, std::shared_ptr<T>) and std::optional<T>
 *      are supported as nullable wrappers (serializing null when empty, or unwrapping if set).
 *    - Struct Members: Struct member fields should avoid raw pointers; use std::optional<T>
 *      or smart pointers for optional/nullable sub-objects.
 *
 * 2. BUFFER REUSE & LIFECYCLE CONTRACT:
 *    - Overwrite Semantics: All buffer-accepting serialize overloads reset the target buffer
 *      at entry. Existing buffer contents are cleared; the serializer NEVER appends.
 *    - Failure State: If serialization fails or an exception occurs, the output buffer is
 *      guaranteed to be left empty (size() == 0), preventing partial/stale JSON leakage.
 *    - High-Water Mark Policy: Reusable buffers retain capacity up to 64 KB for 0 heap
 *      allocations across cycles. Any outlier > 64 KB automatically shrinks to 4 KB.
 *    - Thread Safety: Buffers are NOT thread-safe. Use thread_local instances or thread-isolated
 *      pools for concurrent serialization pipelines.
 *    - Async Dispatch: String views and data pointers borrowed from a buffer are valid only
 *      until the next serialize() or clear() call. For asynchronous I/O across worker threads,
 *      transfer ownership using std::move() or clone().
 *
 * 3. RFC 8259 TREE HIERARCHY & CIRCULAR REFERENCE CONTRACT:
 *    - RFC 8259 Tree Model: JSON is strictly an acyclic hierarchical tree data format.
 *      It does not support pointer identity, reference anchors, or graph cycles.
 *    - Acyclic DAG Nodes: Multiple smart pointers sharing an identical child node (DAG)
 *      are safely expanded and serialized as independent tree branches.
 *    - Prohibited (Cyclic Graphs): Circular references (e.g., Node A -> Node B -> Node A)
 *      are illegal in JSON and must NEVER be serialized. Attempting to serialize a cyclic
 *      structure causes infinite recursion and stack exhaustion (Undefined Behavior).
 *      Applications must break reference cycles (e.g., using foreign IDs or relational keys)
 *      prior to outbound JSON serialization.
 * ============================================================================
 */
class OnnetJsonSerializer {
private:
    static void resetStringBuffer(std::string& buffer) noexcept {
        if (buffer.capacity() <= SerializationBuffer::MAX_RETAIN_CAPACITY) {
            buffer.clear();
        } else {
            try {
                std::string fresh;
                size_t aligned_cap = (SerializationBuffer::DEFAULT_CAPACITY + 63) & ~size_t(63);
                fresh.reserve(aligned_cap);
                buffer.swap(fresh);
            } catch (...) {
                buffer.clear();
            }
        }
    }

public:
    OnnetJsonSerializer() = delete;
    OnnetJsonSerializer(const OnnetJsonSerializer&) = delete;
    OnnetJsonSerializer& operator=(const OnnetJsonSerializer&) = delete;
    OnnetJsonSerializer(OnnetJsonSerializer&&) = delete;
    OnnetJsonSerializer& operator=(OnnetJsonSerializer&&) = delete;

    /**
     * @brief Primary architecture method: serializes directly into a reusable SerializationBuffer.
     *
     * @tparam T Plain C++ struct, container, or primitive type.
     * @param obj The C++ object to serialize (must be passed by value or reference, not raw pointer).
     * @param out Reusable SerializationBuffer target.
     * @return SerializationResult status, bytes written, and diagnostics.
     *
     * @note Pointer Contract:
     *       - Raw pointers (T*) are rejected at compile-time. Pass by value or dereference (*ptr).
     *       - Smart pointers (std::unique_ptr<T>, std::shared_ptr<T>) are serialized as nullable.
     *       - Raw strings (const char*) are disallowed; use std::string_view or std::string.
     *
     * @warning Buffer Behavior & Concurrency:
     *       - Overwrite: Any existing data in 'out' is wiped at entry. It does NOT append.
     *       - Thread Safety: SerializationBuffer is NOT thread-safe. Use thread_local instances.
     *       - Async Lifetime: Borrowed string_views/pointers remain valid only until the next
     *         call to serialize() or clear(). Use std::move(out) or out.clone() for async socket handoff.
     *       - Failure State: If serialization fails, out.size() is guaranteed to be 0 (empty).
     */
    template <typename T>
    static SerializationResult serialize(const T& obj, SerializationBuffer& out) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        out.clear();
        try {
            auto ec = glz::write_json(obj, out.raw_buffer());
            if (static_cast<bool>(ec)) {
                out.clear(); // Ensure no partial/stale data remains on failure
                try {
                    return SerializationResult::fail(glz::format_error(ec));
                } catch (...) {
                    return SerializationResult::fail("Serialization formatting error");
                }
            }
            return SerializationResult::ok(out.size());
        } catch (const std::exception& e) {
            out.clear(); // Ensure no partial/stale data remains on exception
            try {
                return SerializationResult::fail(e.what());
            } catch (...) {
                return SerializationResult::fail("Serialization exception");
            }
        } catch (...) {
            out.clear(); // Ensure no partial/stale data remains on exception
            return SerializationResult::fail("Unknown serialization exception");
        }
    }

    /**
     * @brief Serialize a C++ object/struct in-place into a caller-provided reusable std::string buffer.
     *
     * @tparam T Plain C++ struct, container, or primitive type.
     * @param obj The object to serialize (must be passed by value or reference, not raw pointer).
     * @param buffer Caller-provided reusable string buffer (overwritten, never appended).
     * @return true on success, false on failure.
     *
     * @note Pointer Contract: Raw pointers (T*) are rejected at compile time.
     * @warning Buffer Behavior: Pre-existing data in 'buffer' is wiped at entry. Auto-shrinks if > 64 KB.
     */
    template <typename T>
    static bool serialize(const T& obj, std::string& buffer) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        resetStringBuffer(buffer); // Reset stale data; enforces 64 KB retention policy
        try {
            auto ec = glz::write_json(obj, buffer);
            if (static_cast<bool>(ec)) {
                resetStringBuffer(buffer); // Ensure no partial/stale data remains on failure
                return false;
            }
            return true;
        } catch (...) {
            resetStringBuffer(buffer); // Ensure no partial/stale data remains on exception
            return false;
        }
    }

    /**
     * @brief Convenience serialization returning a newly allocated std::string.
     *
     * @tparam T Plain C++ struct, container, or primitive type.
     * @param obj The object to serialize.
     * @return std::string containing JSON representation, or empty string on failure.
     */
    template <typename T>
    static std::string serialize(const T& obj) {
        std::string buffer;
        if (serialize(obj, buffer)) {
            return buffer;
        }
        return {};
    }

    /**
     * @brief Backward-compatible alias for single-argument std::string return.
     */
    template <typename T>
    static std::string serializeToString(const T& obj) {
        return serialize(obj);
    }

    /**
     * @brief Serialize with legacy SerializationError diagnostics into reusable std::string buffer.
     *
     * @tparam T Plain C++ struct, container, or primitive type.
     * @param value The object to serialize.
     * @param buffer Caller-provided reusable string buffer.
     * @param err Output error context populated on failure.
     * @return true on success, false on failure.
     */
    template <typename T>
    static bool serialize(const T& value, std::string& buffer, SerializationError& err) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        resetStringBuffer(buffer); // Reset stale data; enforces 64 KB retention policy
        try {
            auto ec = glz::write_json(value, buffer);
            if (static_cast<bool>(ec)) {
                resetStringBuffer(buffer); // Ensure no partial/stale data remains on failure
                err.has_error = true;
                try {
                    err.message = glz::format_error(ec);
                } catch (...) {
                    err.message.clear();
                }
                return false;
            }
            err.has_error = false;
            err.message.clear();
            return true;
        } catch (const std::exception& e) {
            resetStringBuffer(buffer); // Ensure no partial/stale data remains on exception
            err.has_error = true;
            try {
                err.message = e.what();
            } catch (...) {
                err.message.clear();
            }
            return false;
        } catch (...) {
            resetStringBuffer(buffer); // Ensure no partial/stale data remains on exception
            err.has_error = true;
            try {
                err.message = "Unknown serialization exception";
            } catch (...) {
                err.message.clear();
            }
            return false;
        }
    }

    /**
     * @brief Serialize with modern SerializationResult diagnostics into reusable std::string buffer.
     *
     * @tparam T Plain C++ struct, container, or primitive type.
     * @param value The object to serialize.
     * @param buffer Caller-provided reusable string buffer.
     * @param result Output diagnostic result populated on failure/success.
     * @return true on success, false on failure.
     */
    template <typename T>
    static bool serialize(const T& value, std::string& buffer, SerializationResult& result) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        resetStringBuffer(buffer);
        try {
            auto ec = glz::write_json(value, buffer);
            if (static_cast<bool>(ec)) {
                resetStringBuffer(buffer);
                try {
                    result = SerializationResult::fail(glz::format_error(ec));
                } catch (...) {
                    result = SerializationResult::fail("Serialization formatting error");
                }
                return false;
            }
            result = SerializationResult::ok(buffer.size());
            return true;
        } catch (const std::exception& e) {
            resetStringBuffer(buffer);
            try {
                result = SerializationResult::fail(e.what());
            } catch (...) {
                result = SerializationResult::fail("Serialization exception");
            }
            return false;
        } catch (...) {
            resetStringBuffer(buffer);
            result = SerializationResult::fail("Unknown serialization exception");
            return false;
        }
    }

    /**
     * @brief Serialize a C++ object/struct in prettified (indented) RFC 8259 JSON into SerializationBuffer.
     *
     * @tparam T Plain C++ struct, container, or primitive type.
     * @param obj The object to serialize (must be passed by value or reference, not raw pointer).
     * @param out Reusable SerializationBuffer target (overwritten, never appended).
     * @return SerializationResult status, bytes written, and diagnostics.
     *
     * @note Pointer Contract: Raw pointers (T*) are rejected at compile time.
     * @warning Buffer Behavior: Pre-existing data in 'out' is wiped at entry.
     */
    template <typename T>
    static SerializationResult serializePretty(const T& obj, SerializationBuffer& out) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        out.clear();
        try {
            auto ec = glz::write<glz::opts{.prettify = true}>(obj, out.raw_buffer());
            if (static_cast<bool>(ec)) {
                out.clear(); // Ensure no partial/stale data remains on failure
                try {
                    return SerializationResult::fail(glz::format_error(ec));
                } catch (...) {
                    return SerializationResult::fail("Serialization formatting error");
                }
            }
            return SerializationResult::ok(out.size());
        } catch (const std::exception& e) {
            out.clear(); // Ensure no partial/stale data remains on exception
            try {
                return SerializationResult::fail(e.what());
            } catch (...) {
                return SerializationResult::fail("Serialization exception");
            }
        } catch (...) {
            out.clear(); // Ensure no partial/stale data remains on exception
            return SerializationResult::fail("Unknown serialization exception");
        }
    }

    /**
     * @brief Serialize a C++ object/struct in prettified (indented) RFC 8259 JSON into std::string.
     */
    template <typename T>
    static bool serializePretty(const T& value, std::string& buffer) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        resetStringBuffer(buffer); // Reset stale data; enforces 64 KB retention policy
        try {
            auto ec = glz::write<glz::opts{.prettify = true}>(value, buffer);
            if (static_cast<bool>(ec)) {
                resetStringBuffer(buffer); // Ensure no partial/stale data remains on failure
                return false;
            }
            return true;
        } catch (...) {
            resetStringBuffer(buffer); // Ensure no partial/stale data remains on exception
            return false;
        }
    }

    /**
     * @brief Serialize in prettified JSON with legacy SerializationError diagnostics.
     */
    template <typename T>
    static bool serializePretty(const T& value, std::string& buffer, SerializationError& err) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        resetStringBuffer(buffer);
        try {
            auto ec = glz::write<glz::opts{.prettify = true}>(value, buffer);
            if (static_cast<bool>(ec)) {
                resetStringBuffer(buffer);
                err.has_error = true;
                try {
                    err.message = glz::format_error(ec);
                } catch (...) {
                    err.message.clear();
                }
                return false;
            }
            err.has_error = false;
            err.message.clear();
            return true;
        } catch (const std::exception& e) {
            resetStringBuffer(buffer);
            err.has_error = true;
            try {
                err.message = e.what();
            } catch (...) {
                err.message.clear();
            }
            return false;
        } catch (...) {
            resetStringBuffer(buffer);
            err.has_error = true;
            try {
                err.message = "Unknown serialization exception";
            } catch (...) {
                err.message.clear();
            }
            return false;
        }
    }

    /**
     * @brief Serialize in prettified JSON with modern SerializationResult diagnostics.
     */
    template <typename T>
    static bool serializePretty(const T& value, std::string& buffer, SerializationResult& result) noexcept {
        static_assert(!std::is_pointer_v<T> && !std::is_array_v<T>,
            "[OnnetJsonSerializer] Raw pointers and C-arrays are disallowed. Pass by value or use std::string_view.");
        resetStringBuffer(buffer);
        try {
            auto ec = glz::write<glz::opts{.prettify = true}>(value, buffer);
            if (static_cast<bool>(ec)) {
                resetStringBuffer(buffer);
                try {
                    result = SerializationResult::fail(glz::format_error(ec));
                } catch (...) {
                    result = SerializationResult::fail("Serialization formatting error");
                }
                return false;
            }
            result = SerializationResult::ok(buffer.size());
            return true;
        } catch (const std::exception& e) {
            resetStringBuffer(buffer);
            try {
                result = SerializationResult::fail(e.what());
            } catch (...) {
                result = SerializationResult::fail("Serialization exception");
            }
            return false;
        } catch (...) {
            resetStringBuffer(buffer);
            result = SerializationResult::fail("Unknown serialization exception");
            return false;
        }
    }

    /**
     * @brief Convenience pretty serialization returning a newly allocated std::string.
     */
    template <typename T>
    static std::string serializePretty(const T& value) {
        std::string buffer;
        if (serializePretty(value, buffer)) {
            return buffer;
        }
        return {};
    }

    /**
     * @brief Backward-compatible alias for single-argument pretty std::string return.
     */
    template <typename T>
    static std::string serializePrettyToString(const T& value) {
        return serializePretty(value);
    }
};

} // namespace onnet::json

#endif // ONNET_JSON_JSON_SERIALIZER_HPP
