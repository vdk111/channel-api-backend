#pragma once
#ifndef ONNET_JSON_PARSER_HPP
#define ONNET_JSON_PARSER_HPP

#include "compiled_schema.hpp"
#include "parse_error.hpp"
#include "parser_context.hpp"
#include "json_view.hpp"
#include "json_serializer.hpp"
#include <memory>
#include <string_view>

namespace onnet::json {

enum class ParserBackend {
    Glaze,
    Simdjson
};

struct ParseResult {
    bool success = false;
    ParseError error = ParseError::NONE;
    OnnetJsonView view;
    operator bool() const noexcept { return success; }
};

class OnnetJsonParser {
public:
    explicit OnnetJsonParser(ParserBackend backend = ParserBackend::Glaze);
    ~OnnetJsonParser();
    
    OnnetJsonParser(const OnnetJsonParser&) = delete;
    OnnetJsonParser& operator=(const OnnetJsonParser&) = delete;
    OnnetJsonParser(OnnetJsonParser&&) noexcept;
    OnnetJsonParser& operator=(OnnetJsonParser&&) noexcept;
    
    ParseResult parse(
        const CompiledSchema& schema,
        std::string_view request_json,
        ParserContext& ctx
    ) noexcept;
    
    const char* backendName() const noexcept;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace onnet::json
#endif // ONNET_JSON_PARSER_HPP

