
#include "services/ChannelQueryService.h"

#include <drogon/drogon.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

// Keep query helper functions private to this source file.
namespace
{
    // Redis cache entries expire after five minutes.
    constexpr int CACHE_TTL_SECONDS = 300;

    // Create an HTTP response containing JSON.
    drogon::HttpResponsePtr makeQueryResponse(
        drogon::HttpStatusCode status,
        const std::string &body)
    {
        // Create a new HTTP response.
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

    // Escape special characters before putting database text into JSON.
    std::string escapeChannelJson(const std::string &value)
    {
        // Store the escaped output.
        std::string result;

        // Process every character in the input string.
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
                        // Hexadecimal digits used for JSON escaping.
                        const char hex[] = "0123456789abcdef";

                        // Start a Unicode escape sequence.
                        result += "\\u00";

                        // Add the upper hexadecimal digit.
                        result += hex[(ch >> 4) & 0x0F];

                        // Add the lower hexadecimal digit.
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

    // Generate the Redis key for a channel.
    std::string makeChannelCacheKey(uint32_t channelId)
    {
        // Give each channel its own cache key.
        return "channel:" + std::to_string(channelId);
    }

    // Build the JSON response for a channel.
    std::string makeChannelJson(
        int channelId,
        const std::string &channelName,
        int channelType,
        int status)
    {
        // Return the channel information in JSON format.
        return
            "{\"success\":true,"
            "\"data\":{"
            "\"channel_id\":" + std::to_string(channelId) + ","
            "\"channel_name\":\"" + escapeChannelJson(channelName) + "\","
            "\"channel_type\":" + std::to_string(channelType) + ","
            "\"status\":" + std::to_string(status) +
            "}}";
    }

    // Handle a successful PostgreSQL SELECT.
    void handleSelectSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // Check whether PostgreSQL returned a matching row.
        if (result.empty())
        {
            // Log that the requested channel does not exist.
            LOG_INFO << "GET /channels"
                     << " - channel not found"
                     << " - channel_id: " << channelId;

            // Return HTTP 404 Not Found.
            (*callback)(makeQueryResponse(
                drogon::k404NotFound,
                R"({"success":false,"error":"CHANNEL_NOT_FOUND","message":"Channel not found"})"
            ));

            // Stop because there is no channel to return.
            return;
        }

        // Access the first row returned by PostgreSQL.
        const auto &row = result[0];

        // Read the channel ID from the database row.
        const int channelIdValue = row["channel_id"].as<int>();

        // Read the channel name from the database row.
        const std::string channelName =
            row["channel_name"].as<std::string>();

        // Read the channel type from the database row.
        const int channelTypeValue = row["channel_type"].as<int>();

        // Read the channel status from the database row.
        const int statusValue = row["status"].as<int>();

        // Convert the database row into a JSON response.
        const std::string body = makeChannelJson(
            channelIdValue,
            channelName,
            channelTypeValue,
            statusValue
        );

        // Generate the Redis key for this channel.
        const std::string key = makeChannelCacheKey(channelId);

        // Get the Redis client configured in main.cpp.
        auto redisClient = drogon::app().getRedisClient();

        // Store the result in Redis with a five-minute expiry.
        redisClient->execCommandAsync(
            // This callback runs when Redis stores the value.
            [channelId](const drogon::nosql::RedisResult &)
            {
                // Log that the cache was populated.
                LOG_INFO << "Redis cache populated"
                         << " - channel_id: " << channelId;
            },

            // This callback runs if Redis cannot store the value.
            [channelId](const std::exception &error)
            {
                // Log the Redis error.
                LOG_ERROR << "Redis cache SETEX failed"
                          << " - channel_id: " << channelId
                          << " - reason: " << error.what();
            },

            // SETEX stores the value and assigns its expiry time.
            "SETEX %s %d %s",

            // Redis key.
            key.c_str(),

            // Cache expiry in seconds.
            CACHE_TTL_SECONDS,

            // JSON response stored as the cache value.
            body.c_str()
        );

        // Log that PostgreSQL supplied the channel.
        LOG_INFO << "GET /channels"
                 << " - channel retrieved from PostgreSQL"
                 << " - channel_id: " << channelId;

        // Return the PostgreSQL result to the client.
        (*callback)(makeQueryResponse(
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
        // Try to extract and log the database exception.
        try
        {
            // Rethrow the exception captured by Drogon.
            if (error)
            {
                std::rethrow_exception(error);
            }

            // Log if no exception details are available.
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId;
        }
        // Handle standard C++ exceptions.
        catch (const std::exception &exception)
        {
            // Log the actual database error.
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        // Handle unknown exceptions.
        catch (...)
        {
            // Log an unknown database error.
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId
                      << " - unknown error";
        }

        // Return HTTP 500 Internal Server Error.
        (*callback)(makeQueryResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"
        ));
    }

    // Query PostgreSQL when Redis does not contain the channel.
    void getChannelFromPostgreSQL(
        uint32_t channelId,
        const std::shared_ptr<HttpResponseCallback> &callback)
    {
        // Define the SQL query used to retrieve a channel.
        const std::string sql =
            "SELECT channel_id, channel_name, channel_type, status "
            "FROM channel_info "
            "WHERE channel_id = $1 "
            "LIMIT 1";

        // Log that the request is being sent to PostgreSQL.
        LOG_INFO << "Redis cache MISS"
                 << " - querying PostgreSQL"
                 << " - channel_id: " << channelId;

        // Execute the SELECT query asynchronously.
        drogon::app().getDbClient()->execSqlAsync(
            // Drogon requires the SQL statement as the first argument.
            sql,

            // This callback runs when PostgreSQL returns a result.
            [callback, channelId](
                const drogon::orm::Result &result)
            {
                // Process the result and populate Redis.
                handleSelectSuccess(result, callback, channelId);
            },

            // This callback runs if PostgreSQL reports an error.
            [callback, channelId](
                const std::exception_ptr &error)
            {
                // Process the database error.
                handleSelectError(error, callback, channelId);
            },

            // Bind the channel ID to SQL parameter $1.
            channelId
        );
    }
}

// Retrieve one channel, checking Redis before PostgreSQL.
void getChannelByIdAsync(
    uint32_t channelId,
    HttpResponseCallback callback)
{
    // Keep the callback alive during asynchronous operations.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Generate the Redis key for the requested channel.
    const std::string key = makeChannelCacheKey(channelId);

    // Get the Redis client configured in main.cpp.
    auto redisClient = drogon::app().getRedisClient();

    // Ask Redis for the channel's cached value.
    redisClient->execCommandAsync(
        // This callback runs when Redis successfully executes GET.
        [sharedCallback, channelId](
            const drogon::nosql::RedisResult &result)
        {
            // Check whether Redis returned a cached value.
            if (result.type() != drogon::nosql::RedisResultType::kNil)
            {
                // Convert the cached Redis value into a string.
                const std::string cachedBody = result.asString();

                // Log that the request was served from Redis.
                LOG_INFO << "Redis cache HIT"
                         << " - channel_id: " << channelId;

                // Return the cached response without querying PostgreSQL.
                (*sharedCallback)(makeQueryResponse(
                    drogon::k200OK,
                    cachedBody
                ));

                // Stop because Redis already supplied the response.
                return;
            }

            // Log that Redis does not contain the requested key.
            LOG_INFO << "Redis cache MISS"
                     << " - channel_id: " << channelId;

            // Retrieve the channel from PostgreSQL.
            getChannelFromPostgreSQL(channelId, sharedCallback);
        },

        // This callback runs if the Redis GET command fails.
        [sharedCallback, channelId](
            const std::exception &error)
        {
            // Log the Redis failure.
            LOG_ERROR << "Redis GET failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << error.what();

            // Continue with PostgreSQL if Redis is unavailable.
            LOG_INFO << "Falling back to PostgreSQL"
                     << " - channel_id: " << channelId;

            // Retrieve the channel from PostgreSQL.
            getChannelFromPostgreSQL(channelId, sharedCallback);
        },

        // Redis GET reads the value for the specified key.
        "GET %s",

        // Redis key to retrieve.
        key.c_str()
    );
}
