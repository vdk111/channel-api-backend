
#include "controllers/ChannelController.h" // Include the controller declaration.
#include <drogon/drogon.h>                 // Include Drogon server and logging APIs.
#include <nlohmann/json.hpp>               // Include the nlohmann JSON library.
#include <memory>                          // Include shared_ptr and make_shared.
#include <string>                          // Include std::string.

using json = nlohmann::json;               // Give the JSON type a shorter name.

// Handle requests for POST /createChannelInfo.
void ChannelController::createChannelInfo(
    const drogon::HttpRequestPtr &request, // Read the incoming HTTP request.
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) // Send the HTTP response.
{
    // Create a helper that builds an HTTP response containing JSON.
    // This avoids passing nlohmann::json to Drogon's Json::Value API.
    const auto makeJsonResponse = [](
        const json &data,                  // JSON content to send.
        drogon::HttpStatusCode status)     // HTTP status code to return.
    {
        auto response = drogon::HttpResponse::newHttpResponse(); // Create an empty response.
        response->setStatusCode(status);   // Set the HTTP status code.
        response->addHeader(               // Tell the client the response is JSON.
            "Content-Type", "application/json; charset=utf-8");
        response->setBody(data.dump());    // Serialize JSON into response text.
        return response;                   // Return the completed response.
    };

    // Create a helper for validation and request errors.
    const auto sendError = [&callback, &makeJsonResponse](
        drogon::HttpStatusCode status,     // Status code for this error.
        const std::string &message)        // Human-readable error message.
    {
        LOG_WARN << "HTTP " << static_cast<int>(status)
                 << ": " << message;       // Log the warning on the server.

        json error;                        // Create an error JSON object.
        error["error"] = message;          // Add the error message.

        auto response = makeJsonResponse(error, status); // Build the response.
        callback(response);                // Send the response to the client.
    };

    // Reject methods other than POST before reading or inserting data.
    if (request->method() != drogon::Post)
    {
        LOG_WARN << "Unsupported HTTP method for /createChannelInfo"; // Log the rejection.

        json error;                        // Create the error response body.
        error["error"] = "Method Not Allowed"; // Explain why the request was rejected.
        error["message"] = "Use POST for this endpoint"; // Tell the client the allowed method.

        auto response = makeJsonResponse(error, drogon::k405MethodNotAllowed); // Return 405.
        response->addHeader("Allow", "POST"); // Advertise the allowed method.
        callback(response);                // Send the response to Postman or curl.
        return;                            // Stop; do not access the database.
    }

    // Log that the supported API request arrived.
    LOG_INFO << "Received POST request for /createChannelInfo";

    // Read and copy the request body into an owned string.
    const std::string body(request->body());

    // Prepare a JSON object to hold the parsed request.
    json input;

    // Parse the request body and handle invalid JSON.
    try
    {
        input = json::parse(body);         // Convert JSON text into a JSON value.
    }
    catch (const json::parse_error &)
    {
        sendError(drogon::k400BadRequest, "Invalid JSON request body"); // Return 400.
        return;                            // Stop processing this request.
    }

    // Require the top-level JSON value to be an object.
    if (!input.is_object())
    {
        sendError(drogon::k400BadRequest,
                  "Request body must be a JSON object"); // Log and return 400.
        return;                            // Do not continue validation.
    }

    // Check that all required fields are present.
    if (!input.contains("channel_id") ||
        !input.contains("channel_name") ||
        !input.contains("channel_type") ||
        !input.contains("status"))
    {
        sendError(
            drogon::k400BadRequest,
            "Required fields: channel_id, channel_name, channel_type, status"
        );                                 // Return a useful validation error.
        return;                            // Stop if any field is missing.
    }

    // Check each field's JSON type before extracting its value.
    if (!input["channel_id"].is_number_integer() ||
        input["channel_id"].is_boolean() ||
        !input["channel_name"].is_string() ||
        !input["channel_type"].is_number_integer() ||
        input["channel_type"].is_boolean() ||
        !input["status"].is_number_integer() ||
        input["status"].is_boolean())
    {
        sendError(
            drogon::k400BadRequest,
            "channel_id, channel_type and status must be integers; "
            "channel_name must be a string"
        );                                 // Return 400 for incorrect types.
        return;                            // Do not access invalid values.
    }

    // Extract the integer fields and the channel name from the JSON.
    const auto channelIdValue = input["channel_id"].get<long long>();
    const std::string channelName = input["channel_name"].get<std::string>();
    const auto channelTypeValue = input["channel_type"].get<long long>();
    const auto statusValue = input["status"].get<long long>();

    // Ensure integer values fit the PostgreSQL INTEGER column range.
    if (channelIdValue < 1 ||
        channelIdValue > 2147483647LL ||
        channelTypeValue < -2147483648LL ||
        channelTypeValue > 2147483647LL ||
        statusValue < -2147483648LL ||
        statusValue > 2147483647LL)
    {
        sendError(
            drogon::k400BadRequest,
            "Integer field is outside the supported range"
        );                                 // Report an out-of-range value.
        return;                            // Stop processing the request.
    }

    // Convert the validated values to the C++ int type.
    const int channelId = static_cast<int>(channelIdValue);
    const int channelType = static_cast<int>(channelTypeValue);
    const int status = static_cast<int>(statusValue);

    // Require a channel name between 1 and 255 bytes.
    if (channelName.empty() || channelName.size() > 255)
    {
        sendError(
            drogon::k400BadRequest,
            "channel_name must contain between 1 and 255 characters"
        );                                 // Return a validation error.
        return;                            // Do not insert invalid data.
    }

    // Log successful validation without logging the entire request body.
    LOG_INFO << "Channel request validated; channel_id=" << channelId;

    // Get the PostgreSQL client registered as "default" in main.cpp.
    auto dbClient = drogon::app().getDbClient("default");

    // Keep the response callback alive while the asynchronous query runs.
    auto responseCallback = std::make_shared<
        std::function<void(const drogon::HttpResponsePtr &)>>(
            std::move(callback));

    // Run the database insert asynchronously.
    dbClient->execSqlAsync(
        "INSERT INTO channel_info "          // Select the target table.
        "(channel_id, channel_name, channel_type, status) " // Select columns.
        "VALUES ($1, $2, $3, $4) "           // Use parameterized SQL values.
        "ON CONFLICT (channel_id) DO NOTHING " // Do not overwrite existing rows.
        "RETURNING channel_id",              // Return the ID when inserted.

        // This callback runs when PostgreSQL successfully executes the query.
        [responseCallback, channelId, makeJsonResponse]
        (const drogon::orm::Result &result)
        {
            // No returned row means the channel ID already existed.
            if (result.empty())
            {
                LOG_WARN << "Duplicate channel_id=" << channelId; // Log duplicate.

                json error;                    // Create a short error response. 01 s;bdgu3` 
                error["error"] = "channel_id already exists"; // Explain the conflict.

                auto response = makeJsonResponse(
                    error, drogon::k409Conflict); // Build HTTP 409.
                (*responseCallback)(response);    // Send the response.
                return;                           // Finish this callback.
            }

            // Log successful insertion without printing the whole request.
            LOG_INFO << "Channel created successfully; channel_id=" << channelId;

            // Return only a message, not the submitted channel data.
            json output;                         // Create the success response body.
            output["message"] =
                "Channel information created successfully"; // Set the message.

            // Build the JSON response with HTTP 201 Created.
            auto response = makeJsonResponse(
                output, drogon::k201Created);

            // Send the response to Postman or curl.
            (*responseCallback)(response);
        },

        // This callback runs when PostgreSQL reports a query error.
        [responseCallback, channelId, makeJsonResponse]
        (const drogon::orm::DrogonDbException &exception)
        {
            // Log the actual database error on the server.
            LOG_ERROR << "Database error for channel_id=" << channelId
                      << ": " << exception.base().what();

            // Keep internal database details out of the client response.
            json error;                          // Create an error response body.
            error["error"] = "Database operation failed"; // Use a safe message.

            // Build an HTTP 500 response.
            auto response = makeJsonResponse(
                error, drogon::k500InternalServerError);

            // Send the error response to the client.
            (*responseCallback)(response);
        },

        // Bind these values to SQL placeholders $1, $2, $3 and $4.
        channelId,
        channelName,
        channelType,
        status
    );                                         // Finish the asynchronous query call.
}                                              // End of the controller function.






























/*
#include "controllers/ChannelController.h"

#include <drogon/drogon.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <limits>
#include <memory>
#include <string>

using json = nlohmann::json;

// Create an HTTP response containing serialized JSON.
static drogon::HttpResponsePtr makeJsonResponse(
    const json &data,
    drogon::HttpStatusCode status)
{
    // Create a standard HTTP response.
    auto response = drogon::HttpResponse::newHttpResponse();

    // Set the HTTP status, for example 400 or 201.
    response->setStatusCode(status);

    // Tell the client that the body contains JSON.
    response->setContentTypeCode(drogon::CT_APPLICATION_JSON);

    // Convert nlohmann JSON to text and set the response body.
    response->setBody(data.dump());

    return response;
}

// Handle POST /createChannelInfo.
void ChannelController::createChannelInfo(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // Drogon returns a string_view, so explicitly copy it into a string.
    const auto requestBody = request->body();
    const std::string body(requestBody.data(), requestBody.size());

    json input;

    // Parse the incoming JSON.
    try
    {
        input = json::parse(body);
    }
    catch (const json::parse_error &)
    {
        json error;
        error["error"] = "Invalid JSON request body";

        callback(makeJsonResponse(error, drogon::k400BadRequest));
        return;
    }

    // The body must be a JSON object.
    if (!input.is_object())
    {
        json error;
        error["error"] = "Request body must be a JSON object";

        callback(makeJsonResponse(error, drogon::k400BadRequest));
        return;
    }

    // Check that all required fields are present.
    if (!input.contains("channel_id") ||
        !input.contains("channel_name") ||
        !input.contains("channel_type") ||
        !input.contains("status"))
    {
        json error;
        error["error"] =
            "Required fields: channel_id, channel_name, channel_type, status";

        callback(makeJsonResponse(error, drogon::k400BadRequest));
        return;
    }

    // Check field types before extracting values.
    // Boolean values are not accepted as integers.
    if (!input["channel_id"].is_number_integer() ||
        input["channel_id"].is_boolean() ||
        !input["channel_name"].is_string() ||
        !input["channel_type"].is_number_integer() ||
        input["channel_type"].is_boolean() ||
        !input["status"].is_number_integer() ||
        input["status"].is_boolean())
    {
        json error;
        error["error"] =
            "channel_id, channel_type and status must be integers; "
            "channel_name must be a string";

        callback(makeJsonResponse(error, drogon::k400BadRequest));
        return;
    }

    // Extract integer values as 64-bit integers first.
    const auto idValue = input["channel_id"].get<long long>();
    const auto typeValue = input["channel_type"].get<long long>();
    const auto statusValue = input["status"].get<long long>();

    // Check the PostgreSQL INTEGER range before converting to int.
    constexpr long long intMin =
        std::numeric_limits<int>::min();

    constexpr long long intMax =
        std::numeric_limits<int>::max();

    if (idValue < 1 || idValue > intMax ||
        typeValue < intMin || typeValue > intMax ||
        statusValue < intMin || statusValue > intMax)
    {
        json error;
        error["error"] =
            "channel_id must be positive and integer fields must fit "
            "the PostgreSQL INTEGER range";

        callback(makeJsonResponse(error, drogon::k400BadRequest));
        return;
    }

    // Extract the channel name.
    const std::string channelName =
        input["channel_name"].get<std::string>();

    // Validate the channel name.
    if (channelName.empty() || channelName.size() > 255)
    {
        json error;
        error["error"] =
            "channel_name must contain between 1 and 255 characters";

        callback(makeJsonResponse(error, drogon::k400BadRequest));
        return;
    }

    // Convert the validated numeric values to int.
    const int channelId = static_cast<int>(idValue);
    const int channelType = static_cast<int>(typeValue);
    const int channelStatus = static_cast<int>(statusValue);

    // Get the PostgreSQL client created in main.cpp.
    auto dbClient = drogon::app().getDbClient("default");

    // Keep the response callback alive until the async query completes.
    using Callback =
        std::function<void(const drogon::HttpResponsePtr &)>;

    auto responseCallback =
        std::make_shared<Callback>(std::move(callback));

    // Insert the record using parameterized SQL.
    // Parameter placeholders protect against SQL injection.
    // If the primary key already exists, no row is returned.
    dbClient->execSqlAsync(
        "INSERT INTO channel_info "
        "(channel_id, channel_name, channel_type, status) "
        "VALUES ($1, $2, $3, $4) "
        "ON CONFLICT (channel_id) DO NOTHING "
        "RETURNING channel_id",

        // Called when PostgreSQL successfully executes the query.
        [responseCallback, channelId, channelName,
         channelType, channelStatus]
        (const drogon::orm::Result &result)
        {
            // No returned row means the channel ID already exists.
            if (result.empty())
            {
                json error;
                error["error"] = "channel_id already exists";

                (*responseCallback)(
                    makeJsonResponse(error, drogon::k409Conflict));
                return;
            }

            // Build the successful JSON response.
            json output;
            output["message"] =
                "Channel information created successfully";

            output["data"] = {
                {"channel_id", channelId},
                {"channel_name", channelName},
                {"channel_type", channelType},
                {"status", channelStatus}
            };

            // Return HTTP 201 Created.
            (*responseCallback)(
                makeJsonResponse(output, drogon::k201Created));
        },

        // Called if PostgreSQL reports a query error.
        [responseCallback]
        (const drogon::orm::DrogonDbException &exception)
        {
            // Log the actual error on the server for debugging.
            LOG_ERROR << "createChannelInfo database error: "
                      << exception.base().what();

            // Do not expose database details to the API client.
            json error;
            error["error"] = "Database operation failed";

            (*responseCallback)(
                makeJsonResponse(
                    error,
                    drogon::k500InternalServerError));
        },

        // Values bound to $1, $2, $3 and $4.
        channelId,
        channelName,
        channelType,
        channelStatus
    );
}

*/