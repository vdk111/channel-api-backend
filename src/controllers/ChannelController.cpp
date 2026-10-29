
#include "controllers/ChannelController.h"
#include "services/ChannelService.h"

#include <drogon/drogon.h>

#include <onnet/json/json_parser.hpp>
#include <onnet/json/schema_registry.hpp>
#include <onnet/json/parser_context.hpp>
#include <onnet/json/parse_error.hpp>

#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    // Stores the schemas used by the API.
    onnet::json::OnnetJsonSchemaRegistry g_schemaRegistry;

    // Ensures that schemas are loaded only once.
    std::once_flag g_schemaLoadFlag;

    // Creates an HTTP response containing JSON.
    drogon::HttpResponsePtr makeJsonResponse(
        drogon::HttpStatusCode status,
        const std::string &body)
    {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(status);
        response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        response->setBody(body);
        return response;
    }

    // Escapes special characters before placing text in a JSON response.
    std::string escapeJson(const std::string &value)
    {
        std::string result;

        for (unsigned char ch : value)
        {
            switch (ch)
            {
                case '"':
                    result += "\\\"";
                    break;

                case '\\':
                    result += "\\\\";
                    break;

                case '\n':
                    result += "\\n";
                    break;

                case '\r':
                    result += "\\r";
                    break;

                case '\t':
                    result += "\\t";
                    break;

                case '\b':
                    result += "\\b";
                    break;

                case '\f':
                    result += "\\f";
                    break;

                default:
                    // Escape other JSON control characters.
                    if (ch < 0x20)
                    {
                        const char hex[] = "0123456789abcdef";
                        result += "\\u00";
                        result += hex[(ch >> 4) & 0x0F];
                        result += hex[ch & 0x0F];
                    }
                    else
                    {
                        result += static_cast<char>(ch);
                    }
                    break;
            }
        }

        return result;
    }

    // Creates an HTTP 400 response for an invalid field.
    // logError=false prevents duplicate logs when the parser
    // has already logged its detailed error.
    drogon::HttpResponsePtr makeFieldError(
        const std::string &field,
        const std::string &message,
        bool logError = true)
    {
        if (logError)
        {
            LOG_WARN << "POST /createChannelInfo"
                     << " - validation failed"
                     << " - field: " << field
                     << " - reason: " << message;
        }

        const std::string body =
            "{\"success\":false,"
            "\"error\":\"INVALID_FIELD\","
            "\"field\":\"" + escapeJson(field) + "\","
            "\"message\":\"" + escapeJson(message) + "\"}";

        return makeJsonResponse(drogon::k400BadRequest, body);
    }

    // Checks whether a string contains only whitespace.
    bool isBlank(const std::string &value)
    {
        return value.find_first_not_of(" \t\n\r") == std::string::npos;
    }
}

// Loads the API schemas before the server accepts requests.
void initializeChannelSchemas()
{
    std::call_once(g_schemaLoadFlag, []()
    {
        // Load schemas from the directory configured in CMake.  
        g_schemaRegistry.loadDirectory(CHANNEL_SCHEMA_DIR);

        // Confirm that the required API schema exists.
        const auto *schema =
            g_schemaRegistry.find("POST", "/createChannelInfo");

        // Stop startup if the schema is missing.
        if (schema == nullptr)
        {
            LOG_ERROR << "Required schema not found:"
                      << " POST /createChannelInfo";

            throw std::runtime_error(
                "Schema for POST /createChannelInfo was not found");
        }

        LOG_INFO << "Loaded schema for POST /createChannelInfo";
    });
}

// Handles POST /createChannelInfo requests.
void ChannelController::createChannelInfo(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // Record that a request reached this controller.
    LOG_INFO << "POST /createChannelInfo - request received";

    // STEP 1: Check the HTTP method.
    if (request->method() != drogon::Post)
    {
        LOG_WARN << "POST /createChannelInfo"
                 << " - unsupported HTTP method";

        auto response = makeJsonResponse(
            drogon::k405MethodNotAllowed,
            R"({"success":false,"error":"METHOD_NOT_ALLOWED","message":"Use POST"})");

        response->addHeader("Allow", "POST");
        callback(response);
        return;
    }

    // STEP 2: Find the schema for this API.
    const auto *schema =
        g_schemaRegistry.find("POST", "/createChannelInfo");

    if (schema == nullptr)
    {
        LOG_ERROR << "POST /createChannelInfo - schema not found";

        callback(makeJsonResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"SCHEMA_NOT_FOUND","message":"API schema is unavailable"})"));

        return;
    }

    // STEP 3: Parse and validate JSON using OnnetJsonParser.
    const std::string_view requestBody = request->body();

    // Each worker thread has its own parser context.
    thread_local onnet::json::ParserContext parserContext;

    // Each worker thread has its own parser instance.
    thread_local onnet::json::OnnetJsonParser parser;

    // Clear the previous request's error information.
    parserContext.reset();

    // Parse the body and validate it against the API schema.
    const auto parseResult =
        parser.parse(*schema, requestBody, parserContext);

    // Return an error if parsing or schema validation fails.
    if (!parseResult)
    {
        const auto &error = parserContext.error;

        // Use the parser's field name when available.
        // Use "request" for errors without a specific field.
        const std::string field =
            error.field_name.empty()
                ? "request"
                : error.field_name;

        // Prefer the parser's detailed message.
        const std::string message =
            error.message.empty()
                ? onnet::json::parseErrorToString(error.code)
                : error.message;

        // Log parser diagnostics, including the byte position.
        LOG_WARN << "POST /createChannelInfo"
                 << " - OnnetJsonParser validation failed"
                 << " - error: "
                 << onnet::json::parseErrorToString(error.code)
                 << " - field: " << field
                 << " - message: " << message
                 << " - byte_offset: " << error.byte_offset;

        // The detailed parser error is already logged above.
        callback(makeFieldError(field, message, false));
        return;
    }

    // Access the parsed JSON only after successful validation.
    const auto &json = parseResult.view;

    // STEP 4: Extract and validate channel_id.
    const uint32_t channelId = json.getUInt32("channel_id");

    if (channelId == 0)
    {
        callback(makeFieldError(
            "channel_id",
            "channel_id must be greater than 0"));
        return;
    }

    if (channelId >
        static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    {
        callback(makeFieldError(
            "channel_id",
            "channel_id exceeds the maximum supported value"));
        return;
    }

    // STEP 5: Extract and validate channel_name.
    const std::string channelName =
        std::string(json.getString("channel_name"));

    if (channelName.empty() || isBlank(channelName))
    {
        callback(makeFieldError(
            "channel_name",
            "channel_name cannot be empty"));
        return;
    }

    if (channelName.size() > 100)
    {
        callback(makeFieldError(
            "channel_name",
            "channel_name must not exceed 100 bytes"));
        return;
    }

    // STEP 6: Extract and validate channel_type.
    const uint32_t channelType = json.getUInt32("channel_type");

    if (channelType != 0 && channelType != 1)
    {
        callback(makeFieldError(
            "channel_type",
            "channel_type must be 0 or 1"));
        return;
    }

    // STEP 7: Extract and validate status.
    const uint32_t status = json.getUInt32("status");

    if (status != 0 && status != 1)
    {
        callback(makeFieldError(
            "status",
            "status must be 0 or 1"));
        return;
    }

    // All parsing, schema, and application checks passed.
    LOG_INFO << "POST /createChannelInfo"
             << " - validation successful"
             << " - channel_id: " << channelId;

    // STEP 8: Prepare the validated channel data.
    ChannelData data{
        channelId,
        channelName,
        channelType,
        status
    };

    // STEP 9: Insert the data into PostgreSQL asynchronously.
    // Keep the existing service implementation unchanged.
    insertChannelInfoAsync(
        std::move(data),
        std::move(callback));
}
