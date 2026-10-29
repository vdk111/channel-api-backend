    #pragma once
#ifndef ONNET_JSON_SCHEMA_REGISTRY_HPP
#define ONNET_JSON_SCHEMA_REGISTRY_HPP

#include "compiled_schema.hpp"
#include "json_types.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace onnet::json {

class OnnetJsonSchemaRegistry {
public:
    bool loadDirectory(std::string_view directory);
    bool loadString(std::string_view json_content);
    
    const CompiledSchema* find(RouteId route) const noexcept;
    const CompiledSchema* find(std::string_view method, std::string_view path) const noexcept;
    
    size_t size() const noexcept;
    
private:
    std::unordered_map<RouteId, std::shared_ptr<CompiledSchema>> by_route_;
    std::unordered_map<std::string, std::shared_ptr<CompiledSchema>> by_method_path_;
};

} // namespace onnet::json

#endif // ONNET_JSON_SCHEMA_REGISTRY_HPP