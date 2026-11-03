
#include "services/ChannelService.h"

#include <drogon/drogon.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace
{
    // Redis cache entries expire after five minutes.
    constexpr int CACHE_TTL_SECONDS = 300;

    // Create an HTTP response containing JSON.
    drogon::HttpResponsePtr makeServiceResponse(
        drogon::HttpStatusCode status,
        const std::string &body)
    {
        // Create a new HTTP response object.
        auto response = drogon::HttpResponse::newHttpResponse();

        // Set the HTTP status code.
        response->setStatusCode(status);

        // Tell the client that the response contains JSON.
        response->setContentTypeCode(drogon::CT_APPLICATION_JSON);

        // Attach the JSON response body.
        response->setBody(body);

        // Return the completed response.
        return response;
    }

    // Escape special characters in a database string before JSON output.
    std::string escapeChannelJson(const std::string &value)
    {
        // This string will contain the escaped output.
        std::string result;

        // Process every character in the original string.
        for (unsigned char ch : value)
        {
            // Handle characters that have special meaning in JSON.
            switch (ch)
            {
                case '"':
                    result += "\\\"";
                    break;

                case '\\':
                    result += "\\\\";
                    break;

                case '\b':
                    result += "\\b";
                    break;

                case '\f':
                    result += "\\f";
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

                default:
                    // Escape other control characters.
                    if (ch < 0x20)
                    {
                        const char hex[] = "0123456789abcdef";

                        result += "\\u00";
                        result += hex[(ch >> 4) & 0x0F];
                        result += hex[ch & 0x0F];
                    }
                    else
                    {
                        // Copy ordinary characters unchanged.
                        result += static_cast<char>(ch);
                    }
                    break;
            }
        }

        // Return the JSON-safe string.
        return result;
    }

    // Create the Redis key for a particular channel.
    std::string makeChannelCacheKey(uint32_t channelId)
    {
        // Each channel gets a separate Redis key.
        return "channel:" + std::to_string(channelId);
    }

    // Build the JSON response body for a channel.
    std::string makeChannelJson(
        int channelId,
        const std::string &channelName,
        int channelType,
        int status)
    {
        // Create a JSON object containing the channel information.
        return
            "{\"success\":true,"
            "\"data\":{"
            "\"channel_id\":" + std::to_string(channelId) + ","
            "\"channel_name\":\"" + escapeChannelJson(channelName) + "\","
            "\"channel_type\":" + std::to_string(channelType) + ","
            "\"status\":" + std::to_string(status) +
            "}}";
    }

    // Delete the cached copy of a channel after a successful database insert.
    void invalidateChannelCache(uint32_t channelId)
    {
        // Build the Redis key for the channel.
        const std::string key = makeChannelCacheKey(channelId);

        // Get the configured Redis client.
        auto redisClient = drogon::app().getRedisClient();

        // Delete the channel's cached value asynchronously.
        redisClient->execCommandAsync(
            // Called when Redis successfully executes DEL.
            [channelId](const drogon::nosql::RedisResult &)
            {
                // Log successful cache invalidation.
                LOG_INFO << "Redis cache invalidated"
                         << " - channel_id: " << channelId;
            },

            // Called if the Redis command fails.
            [channelId](const std::exception &error)
            {
                // Log the error without failing the successful database insert.
                LOG_ERROR << "Redis cache invalidation failed"
                          << " - channel_id: " << channelId
                          << " - reason: " << error.what();
            },

            // Redis DEL removes the specified key.
            "DEL %s",
            key.c_str()
        );
    }

    // Handle a successful PostgreSQL INSERT.
    void handleInsertSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // An empty result means the channel ID already exists.
        if (result.empty())
        {
            // Log the duplicate channel ID.
            LOG_WARN << "POST /createChannelInfo"
                     << " - duplicate channel_id: " << channelId;

            // Return HTTP 409 Conflict.
            (*callback)(makeServiceResponse(
                drogon::k409Conflict,
                R"({"success":false,"error":"DUPLICATE_CHANNEL_ID","field":"channel_id","message":"Channel ID already exists"})"
            ));

            // Stop processing the duplicate.
            return;
        }

        // Remove any cached copy after the database insert succeeds.
        invalidateChannelCache(channelId);

        // Log the successful PostgreSQL insertion.
        LOG_INFO << "POST /createChannelInfo"
                 << " - database insertion successful"
                 << " - channel_id: " << channelId;

        // Return HTTP 201 Created.
        (*callback)(makeServiceResponse(
            drogon::k201Created,
            R"({"success":true,"message":"Channel information created successfully"})"
        ));
    }

    // Handle a failed PostgreSQL INSERT.
    void handleInsertError(
        const std::exception_ptr &error,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // Try to extract the database error details.
        try
        {
            // Rethrow the captured exception.
            if (error)
            {
                std::rethrow_exception(error);
            }

            // Log if no exception details are available.
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - no exception details supplied";
        }
        catch (const std::exception &exception)
        {
            // Log the actual database exception.
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        catch (...)
        {
            // Handle an unknown exception.
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - unknown error";
        }

        // Return an HTTP 500 response.
        (*callback)(makeServiceResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"
        ));
    }

    // Handle a successful PostgreSQL SELECT.
    void handleSelectSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // Check whether PostgreSQL returned a matching channel.
        if (result.empty())
        {
            // Log that the channel was not found.
            LOG_INFO << "GET /channels"
                     << " - channel not found"
                     << " - channel_id: " << channelId;

            // Return HTTP 404 Not Found.
            (*callback)(makeServiceResponse(
                drogon::k404NotFound,
                R"({"success":false,"error":"CHANNEL_NOT_FOUND","message":"Channel not found"})"
            ));

            // Stop processing because no channel exists.
            return;
        }

        // Access the first matching database row.
        const auto &row = result[0];

        // Extract the channel ID from PostgreSQL.
        const int channelIdValue = row["channel_id"].as<int>();

        // Extract the channel name from PostgreSQL.
        const std::string channelName =
            row["channel_name"].as<std::string>();

        // Extract the channel type from PostgreSQL.
        const int channelTypeValue = row["channel_type"].as<int>();

        // Extract the channel status from PostgreSQL.
        const int statusValue = row["status"].as<int>();

        // Convert the database values into a JSON response.
        const std::string body = makeChannelJson(
            channelIdValue,
            channelName,
            channelTypeValue,
            statusValue
        );

        // Create the Redis key for this channel.
        const std::string key = makeChannelCacheKey(channelId);

        // Get the configured Redis client.
        auto redisClient = drogon::app().getRedisClient();

        // Store the JSON response in Redis with an expiry time.
        redisClient->execCommandAsync(
            // Called when Redis successfully stores the value.
            [channelId](const drogon::nosql::RedisResult &)
            {
                // Log that the cache was populated.
                LOG_INFO << "Redis cache populated"
                         << " - channel_id: " << channelId;
            },

            // Called if Redis cannot store the value.
            [channelId](const std::exception &error)
            {
                // Log the Redis error; the PostgreSQL response can still be returned.
                LOG_ERROR << "Redis cache SETEX failed"
                          << " - channel_id: " << channelId
                          << " - reason: " << error.what();
            },

            // SETEX stores the JSON and automatically expires it.
            "SETEX %s %d %s",
            key.c_str(),
            CACHE_TTL_SECONDS,
            body.c_str()
        );

        // Log that the channel was retrieved from PostgreSQL.
        LOG_INFO << "GET /channels"
                 << " - channel retrieved from PostgreSQL"
                 << " - channel_id: " << channelId;

        // Return the database result to the client.
        (*callback)(makeServiceResponse(
            drogon::k200OK,
            body
        ));
    }

    // Handle a failed PostgreSQL SELECT.
    void handleSelectError(
        const std::exception_ptr &error,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // Try to extract the database error details.
        try
        {
            // Rethrow the captured exception.
            if (error)
            {
                std::rethrow_exception(error);
            }

            // Log if no exception details are available.
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId;
        }
        catch (const std::exception &exception)
        {
            // Log the actual database exception.
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        catch (...)
        {
            // Handle an unknown exception.
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId
                      << " - unknown error";
        }

        // Return HTTP 500 if PostgreSQL cannot complete the query.
        (*callback)(makeServiceResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"
        ));
    }

    // Query PostgreSQL when Redis does not contain the channel.
    void getChannelFromPostgreSQL(
        uint32_t channelId,
        const std::shared_ptr<HttpResponseCallback> &callback)
    {
        // Define the SQL query.
        const std::string sql =
            "SELECT channel_id, channel_name, channel_type, status "
            "FROM channel_info "
            "WHERE channel_id = $1 "
            "LIMIT 1";

        // Log that the request is going to PostgreSQL.
        LOG_INFO << "Redis cache MISS"
                 << " - querying PostgreSQL"
                 << " - channel_id: " << channelId;

        // Execute the SELECT query asynchronously.
        // IMPORTANT: Drogon requires the SQL string as the first argument.
        drogon::app().getDbClient()->execSqlAsync(
            sql,

            // Called when PostgreSQL returns a result.
            [callback, channelId](
                const drogon::orm::Result &result)
            {
                // Process the database result and populate Redis.
                handleSelectSuccess(result, callback, channelId);
            },

            // Called when PostgreSQL reports an error.
            [callback, channelId](
                const std::exception_ptr &error)
            {
                // Process the database error.
                handleSelectError(error, callback, channelId);
            },

            // Bind the requested channel ID to SQL parameter $1.
            channelId
        );
    }
}

// Insert channel information into PostgreSQL asynchronously.
void insertChannelInfoAsync(
    ChannelData data,
    HttpResponseCallback callback)
{
    // Use parameterized SQL to prevent SQL injection.
    const std::string sql =
        "INSERT INTO channel_info "
        "(channel_id, channel_name, channel_type, status) "
        "VALUES ($1, $2, $3, $4) "
        "ON CONFLICT (channel_id) DO NOTHING "
        "RETURNING channel_id";

    // Save the ID for logging and cache invalidation.
    const uint32_t channelId = data.channelId;

    // Share the HTTP callback with asynchronous handlers.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Execute the INSERT without blocking the HTTP worker.
    // IMPORTANT: Drogon requires the SQL string as the first argument.
    drogon::app().getDbClient()->execSqlAsync(
        sql,

        // Called when PostgreSQL successfully executes the INSERT.
        [sharedCallback, channelId](
            const drogon::orm::Result &result)
        {
            // Check the insert result and invalidate Redis if successful.
            handleInsertSuccess(result, sharedCallback, channelId);
        },

        // Called when PostgreSQL reports an INSERT error.
        [sharedCallback, channelId](
            const std::exception_ptr &error)
        {
            // Return the database error response.
            handleInsertError(error, sharedCallback, channelId);
        },

        // Bind the channel values to SQL parameters.
        data.channelId,
        data.channelName,
        data.channelType,
        data.status
    );
}

// Retrieve one channel, checking Redis before PostgreSQL.
void getChannelByIdAsync(
    uint32_t channelId,
    HttpResponseCallback callback)
{
    // Share the callback between the Redis and PostgreSQL operations.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Create the Redis key for the requested channel.
    const std::string key = makeChannelCacheKey(channelId);

    // Get the configured Redis client.
    auto redisClient = drogon::app().getRedisClient();

    // Ask Redis for the cached channel.
    redisClient->execCommandAsync(
        // Called when Redis successfully executes GET.
        [sharedCallback, channelId](
            const drogon::nosql::RedisResult &result)
        {
            // Check whether Redis returned an existing value.
            if (result.type() != drogon::nosql::RedisResultType::kNil)
            {
                // Extract the cached JSON response.
                const std::string cachedBody = result.asString();

                // Log that the request was served from Redis.
                LOG_INFO << "Redis cache HIT"
                         << " - channel_id: " << channelId;

                // Return the cached JSON without querying PostgreSQL.
                (*sharedCallback)(makeServiceResponse(
                    drogon::k200OK,
                    cachedBody
                ));

                // Stop processing because the cache supplied the response.
                return;
            }

            // Redis returned nil, meaning the key does not exist.
            LOG_INFO << "Redis cache MISS"
                     << " - channel_id: " << channelId;

            // Retrieve the channel from PostgreSQL.
            getChannelFromPostgreSQL(channelId, sharedCallback);
        },

        // Called if the Redis GET command fails.
        [sharedCallback, channelId](
            const std::exception &error)
        {
            // Log the Redis error.
            LOG_ERROR << "Redis GET failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << error.what();

            // Continue using PostgreSQL if Redis is unavailable.
            LOG_INFO << "Falling back to PostgreSQL"
                     << " - channel_id: " << channelId;

            // Retrieve the channel from PostgreSQL.
            getChannelFromPostgreSQL(channelId, sharedCallback);
        },

        // Redis GET reads the cached value for this key.
        "GET %s",
        key.c_str()
    );
}

