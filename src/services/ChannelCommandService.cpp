
#include "services/ChannelCommandService.h"

#include <drogon/drogon.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

// Keep helper functions private to this source file.
namespace
{
    // Create an HTTP response containing JSON.
    drogon::HttpResponsePtr makeCommandResponse(
        drogon::HttpStatusCode status,
        const std::string &body)
    {
        // Create a new HTTP response.
        auto response = drogon::HttpResponse::newHttpResponse();

        // Set the HTTP status code.
        response->setStatusCode(status);

        // Set the response content type to JSON.
        response->setContentTypeCode(drogon::CT_APPLICATION_JSON);

        // Set the JSON response body.
        response->setBody(body);

        // Return the completed response.
        return response;
    }

    // Build the Redis key belonging to a channel.
    std::string makeChannelCacheKey(uint32_t channelId)
    {
        // Use a unique key for each channel.
        return "channel:" + std::to_string(channelId);
    }

    // Remove a channel's cached value after a successful write.
    void invalidateChannelCache(uint32_t channelId)
    {
        // Generate the Redis key for this channel.
        const std::string key = makeChannelCacheKey(channelId);

        // Get the Redis client configured in main.cpp.
        auto redisClient = drogon::app().getRedisClient();

        // Execute Redis DEL asynchronously.
        redisClient->execCommandAsync(
            // This callback runs when Redis successfully executes DEL.
            [channelId](const drogon::nosql::RedisResult &)
            {
                // Log successful cache invalidation.
                LOG_INFO << "Redis cache invalidated"
                         << " - channel_id: " << channelId;
            },

            // This callback runs if the Redis command fails.
            [channelId](const std::exception &error)
            {
                // Log the Redis failure.
                LOG_ERROR << "Redis cache invalidation failed"
                          << " - channel_id: " << channelId
                          << " - reason: " << error.what();
            },

            // DEL removes the specified Redis key.
            "DEL %s",
            key.c_str()
        );
    }

    // Process a successful PostgreSQL INSERT.
    void handleInsertSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // An empty RETURNING result means the ID already exists.
        if (result.empty())
        {
            // Log the duplicate ID.
            LOG_WARN << "POST /createChannelInfo"
                     << " - duplicate channel_id: " << channelId;

            // Return HTTP 409 Conflict.
            (*callback)(makeCommandResponse(
                drogon::k409Conflict,
                R"({"success":false,"error":"DUPLICATE_CHANNEL_ID","field":"channel_id","message":"Channel ID already exists"})"
            ));

            // Stop processing this duplicate.
            return;
        }

        // Remove an old cached value after the insert succeeds.
        invalidateChannelCache(channelId);

        // Log the successful database insertion.
        LOG_INFO << "POST /createChannelInfo"
                 << " - database insertion successful"
                 << " - channel_id: " << channelId;

        // Return HTTP 201 Created.
        (*callback)(makeCommandResponse(
            drogon::k201Created,
            R"({"success":true,"message":"Channel information created successfully"})"
        ));
    }

    // Process a failed PostgreSQL INSERT.
    void handleInsertError(
        const std::exception_ptr &error,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // Try to extract and log the database exception.
        try
        {
            // Rethrow the exception captured by Drogon.
            if (error)
            {
                std::rethrow_exception(error);
            }

            // Log if no exception details were provided.
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - no exception details supplied";
        }
        // Handle standard C++ exceptions.
        catch (const std::exception &exception)
        {
            // Log the actual database error.
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        // Handle exceptions that are not standard C++ exceptions.
        catch (...)
        {
            // Log an unknown database error.
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - unknown error";
        }

        // Return HTTP 500 Internal Server Error.
        (*callback)(makeCommandResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"
        ));
    }
}

// Execute the channel creation command asynchronously.
void insertChannelInfoAsync(
    ChannelData data,
    HttpResponseCallback callback)
{
    // Use parameterized SQL to protect against SQL injection.
    const std::string sql =
        "INSERT INTO channel_info "
        "(channel_id, channel_name, channel_type, status) "
        "VALUES ($1, $2, $3, $4) "
        "ON CONFLICT (channel_id) DO NOTHING "
        "RETURNING channel_id";

    // Save the ID for logging and cache invalidation.
    const uint32_t channelId = data.channelId;

    // Keep the callback alive for asynchronous execution.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Execute the INSERT without blocking the HTTP worker thread.
    drogon::app().getDbClient()->execSqlAsync(
        // SQL statement must be the first argument.
        sql,

        // This callback runs when PostgreSQL returns a result.
        [sharedCallback, channelId](
            const drogon::orm::Result &result)
        {
            // Check the INSERT result and send the response.
            handleInsertSuccess(result, sharedCallback, channelId);
        },

        // This callback runs when PostgreSQL reports an error.
        [sharedCallback, channelId](
            const std::exception_ptr &error)
        {
            // Handle the database error.
            handleInsertError(error, sharedCallback, channelId);
        },

        // Bind channel ID to SQL parameter $1.
        data.channelId,

        // Bind channel name to SQL parameter $2.
        data.channelName,

        // Bind channel type to SQL parameter $3.
        data.channelType,

        // Bind channel status to SQL parameter $4.
        data.status
    );
}

