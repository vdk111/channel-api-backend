#include "controllers/ChannelController.h"
#include "services/ChannelService.h"

#include <drogon/drogon.h>

#include <onnet/json/json_parser.hpp>
#include <onnet/json/schema_registry.hpp>
#include <onnet/json/parser_context.hpp>
#include <onnet/json/parse_error.hpp>

#include <charconv>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifndef CHANNEL_SCHEMA_DIR
#error "CHANNEL_SCHEMA_DIR was not defined by CMake"
#endif

namespace
{
    // Store API schemas for the lifetime of the server.
    onnet::json::OnnetJsonSchemaRegistry g_schemaRegistry;

    // Ensure schemas are initialized only once.
    std::once_flag g_schemaLoadFlag;

    // Create an HTTP response containing JSON.
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

    // Escape special characters before inserting text into JSON.
    std::string escapeJson(const std::string &value)
    {
        std::string result;

        for (char ch : value)
        {
            if (ch == '"')
            {
                result += "\\\"";
            }
            else if (ch == '\\')
            {
                result += "\\\\";
            }
            else if (ch == '\n')
            {
                result += "\\n";
            }
            else if (ch == '\r')
            {
                result += "\\r";
            }
            else if (ch == '\t')
            {
                result += "\\t";
            }
            else
            {
                result += ch;
            }
        }

        return result;
    }

    // Create a JSON response describing an invalid field.
    drogon::HttpResponsePtr makeFieldError(
        const std::string &field,
        const std::string &message)
    {
        const std::string body =
            "{\"success\":false,"
            "\"error\":\"INVALID_FIELD\","
            "\"field\":\"" + escapeJson(field) + "\","
            "\"message\":\"" + escapeJson(message) + "\"}";

        return makeJsonResponse(drogon::k400BadRequest, body);
    }

    // Check whether a string contains only whitespace.
    bool isBlank(const std::string &value)
    {
        return value.find_first_not_of(" \t\n\r") == std::string::npos;
    }

    // Convert an ID string into a valid channel ID.
    bool parseChannelId(
        const std::string &idText,
        uint32_t &channelId)
    {
        // Reject an empty ID.
        if (idText.empty())
        {
            return false;
        }

        // Identify the beginning and end of the ID text.
        const char *begin = idText.data();
        const char *end = begin + idText.size();

        // Convert the text into an unsigned integer.
        const auto result = std::from_chars(begin, end, channelId);

        // Reject invalid characters, overflow, or trailing characters.
        if (result.ec != std::errc{} || result.ptr != end)
        {
            return false;
        }

        // Channel IDs must be greater than zero.
        if (channelId == 0)
        {
            return false;
        }

        // Ensure the ID fits the PostgreSQL INTEGER column.
        if (channelId >
            static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
        {
            return false;
        }

        return true;
    }

    // Validate an ID and retrieve its channel from PostgreSQL.
    void handleGetChannelById(
        const std::string &idText,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback)
    {
        uint32_t channelId = 0;

        // Reject an invalid ID before querying PostgreSQL.
        if (!parseChannelId(idText, channelId))
        {
            callback(makeJsonResponse(
                drogon::k400BadRequest,
                R"({"success":false,"error":"INVALID_ID","message":"id must be a positive integer within the supported range"})"
            ));

            return;
        }

        // Retrieve the matching channel asynchronously.
        getChannelByIdAsync(channelId, std::move(callback));
    }
}

// Load the schemas before the server starts accepting requests.
void initializeChannelSchemas()
{
    std::call_once(g_schemaLoadFlag, []()
    {
        // Load schema files from the configured directory.
        g_schemaRegistry.loadDirectory(CHANNEL_SCHEMA_DIR);

        // Find the schema for POST /createChannelInfo.
        const auto *schema =
            g_schemaRegistry.find("POST", "/createChannelInfo");

        // Stop startup if the required schema is missing.
        if (schema == nullptr)
        {
            throw std::runtime_error(
                "Schema for POST /createChannelInfo was not found"
            );
        }

        LOG_INFO << "OnnetJsonParser schemas loaded successfully";
    });
}

// Handle POST /createChannelInfo.
void ChannelController::createChannelInfo(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // Only POST requests are allowed.
    if (request->method() != drogon::Post)
    {
        auto response = makeJsonResponse(
            drogon::k405MethodNotAllowed,
            R"({"success":false,"error":"METHOD_NOT_ALLOWED","message":"Use POST"})"
        );

        response->addHeader("Allow", "POST");
        callback(response);

        return;
    }

    // Find the schema for this endpoint.
    const auto *schema =
        g_schemaRegistry.find("POST", "/createChannelInfo");

    // Return an error if the schema is unavailable.
    if (schema == nullptr)
    {
        callback(makeJsonResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"SCHEMA_NOT_FOUND","message":"API schema is unavailable"})"
        ));

        return;
    }

    // Read the incoming JSON body.
    const std::string_view requestBody = request->body();

    // Each worker thread gets its own parser context.
    thread_local onnet::json::ParserContext parserContext;

    // Each worker thread gets its own parser.
    thread_local onnet::json::OnnetJsonParser parser;

    // Clear errors from the previous request.
    parserContext.reset();

    // Parse the JSON and validate it against the schema.
    const auto parseResult =
        parser.parse(*schema, requestBody, parserContext);

    // Reject malformed JSON or schema validation failures.
    if (!parseResult)
    {
        const auto &error = parserContext.error;

        const std::string field =
            error.field_name.empty() ? "request" : error.field_name;

        const std::string message =
            error.message.empty()
                ? onnet::json::parseErrorToString(error.code)
                : error.message;

        callback(makeFieldError(field, message));

        return;
    }

    // Access the validated JSON values.
    const auto &json = parseResult.view;

    // Extract the channel ID.
    const uint32_t channelId = json.getUInt32("channel_id");

    // Reject channel ID zero.
    if (channelId == 0)
    {
        callback(makeFieldError(
            "channel_id",
            "channel_id must be greater than 0"
        ));

        return;
    }

    // Reject IDs that exceed the PostgreSQL INTEGER range.
    if (channelId >
        static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    {
        callback(makeFieldError(
            "channel_id",
            "channel_id exceeds the maximum supported value"
        ));

        return;
    }

    // Extract the channel name.
    const std::string channelName =
        std::string(json.getString("channel_name"));

    // Reject an empty or whitespace-only channel name.
    if (channelName.empty() || isBlank(channelName))
    {
        callback(makeFieldError(
            "channel_name",
            "channel_name cannot be empty"
        ));

        return;
    }

    // Reject names longer than 100 bytes.
    if (channelName.size() > 100)
    {
        callback(makeFieldError(
            "channel_name",
            "channel_name must not exceed 100 bytes"
        ));

        return;
    }

    // Extract the channel type.
    const uint32_t channelType = json.getUInt32("channel_type");

    // Only channel types 0 and 1 are allowed.
    if (channelType != 0 && channelType != 1)
    {
        callback(makeFieldError(
            "channel_type",
            "channel_type must be 0 or 1"
        ));

        return;
    }

    // Extract the channel status.
    const uint32_t status = json.getUInt32("status");

    // Only status values 0 and 1 are allowed.
    if (status != 0 && status != 1)
    {
        callback(makeFieldError(
            "status",
            "status must be 0 or 1"
        ));

        return;
    }

    // Store the validated fields in the service data structure.
    ChannelData data{
        channelId,
        channelName,
        channelType,
        status
    };

    // Insert the channel asynchronously into PostgreSQL.
    insertChannelInfoAsync(
        std::move(data),
        std::move(callback)
    );
}

// Handle GET /channels?id=101.
void ChannelController::getChannelById(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // Read the ID from the query string.
    const std::string idText = request->getParameter("id");

    // Return HTTP 400 if the ID is missing.
    if (idText.empty())
    {
        callback(makeJsonResponse(
            drogon::k400BadRequest,
            R"({"success":false,"error":"INVALID_ID","message":"The id query parameter is required"})"
        ));

        return;
    }

    // Validate the ID and retrieve the channel.
    handleGetChannelById(idText, std::move(callback));
}

// Handle GET /channels/101.
void ChannelController::getChannelByPathId(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &channelIdText)
{
    // The ID is supplied by Drogon's route binding.
    (void)request;

    // Validate the ID and retrieve the channel.
    handleGetChannelById(channelIdText, std::move(callback));
}