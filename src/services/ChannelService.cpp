#include "services/ChannelService.h"

#include <drogon/drogon.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace
{
    // Create an HTTP response containing JSON.
    drogon::HttpResponsePtr makeServiceResponse(
        drogon::HttpStatusCode status,
        const std::string &body)
    {
        auto response = drogon::HttpResponse::newHttpResponse();

        response->setStatusCode(status);
        response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        response->setBody(body);

        return response;
    }

    // Escape special characters in a database string before JSON output.
    std::string escapeChannelJson(const std::string &value)
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

    // Handle a successful INSERT query.
    void handleInsertSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // An empty result means the ID already exists.
        if (result.empty())
        {
            LOG_WARN << "POST /createChannelInfo"
                     << " - duplicate channel_id: " << channelId;

            (*callback)(makeServiceResponse(
                drogon::k409Conflict,
                R"({"success":false,"error":"DUPLICATE_CHANNEL_ID","field":"channel_id","message":"Channel ID already exists"})"
            ));

            return;
        }

        LOG_INFO << "POST /createChannelInfo"
                 << " - database insertion successful"
                 << " - channel_id: " << channelId;

        (*callback)(makeServiceResponse(
            drogon::k201Created,
            R"({"success":true,"message":"Channel information created successfully"})"
        ));
    }

    // Handle an INSERT database error.
    void handleInsertError(
        const std::exception_ptr &error,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        try
        {
            if (error)
            {
                std::rethrow_exception(error);
            }

            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - no exception details supplied";
        }
        catch (const std::exception &exception)
        {
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        catch (...)
        {
            LOG_ERROR << "Database insertion failed"
                      << " - channel_id: " << channelId
                      << " - unknown error";
        }

        (*callback)(makeServiceResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"
        ));
    }

    // Handle a successful SELECT query.
    void handleSelectSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // No matching row means the channel does not exist.
        if (result.empty())
        {
            LOG_INFO << "GET /channels"
                     << " - channel not found"
                     << " - channel_id: " << channelId;

            (*callback)(makeServiceResponse(
                drogon::k404NotFound,
                R"({"success":false,"error":"CHANNEL_NOT_FOUND","message":"Channel not found"})"
            ));

            return;
        }

        // Read the first matching database row.
        const auto &row = result[0];

        // Extract the values returned by PostgreSQL.
        const int channelIdValue = row["channel_id"].as<int>();
        const std::string channelName =
            row["channel_name"].as<std::string>();
        const int channelTypeValue = row["channel_type"].as<int>();
        const int statusValue = row["status"].as<int>();

        // Build a JSON response from the database values.
        const std::string body =
            "{\"success\":true,"
            "\"data\":{"
            "\"channel_id\":" + std::to_string(channelIdValue) + ","
            "\"channel_name\":\"" + escapeChannelJson(channelName) + "\","
            "\"channel_type\":" + std::to_string(channelTypeValue) + ","
            "\"status\":" + std::to_string(statusValue) +
            "}}";

        LOG_INFO << "GET /channels"
                 << " - channel retrieved"
                 << " - channel_id: " << channelId;

        (*callback)(makeServiceResponse(
            drogon::k200OK,
            body
        ));
    }

    // Handle a failed SELECT query.
    void handleSelectError(
        const std::exception_ptr &error,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        try
        {
            if (error)
            {
                std::rethrow_exception(error);
            }

            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId;
        }
        catch (const std::exception &exception)
        {
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        catch (...)
        {
            LOG_ERROR << "Database SELECT failed"
                      << " - channel_id: " << channelId
                      << " - unknown error";
        }

        (*callback)(makeServiceResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"
        ));
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

    // Keep the ID for logging.
    const uint32_t channelId = data.channelId;

    // Share the callback with the asynchronous handlers.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Execute the INSERT without blocking the HTTP worker.
    drogon::app().getDbClient()->execSqlAsync(
        sql,

        // Called when PostgreSQL successfully executes the query.
        [sharedCallback, channelId](
            const drogon::orm::Result &result)
        {
            handleInsertSuccess(result, sharedCallback, channelId);
        },

        // Called when PostgreSQL reports an error.
        [sharedCallback, channelId](
            const std::exception_ptr &error)
        {
            handleInsertError(error, sharedCallback, channelId);
        },

        // Bind the values to SQL placeholders.
        data.channelId,
        data.channelName,
        data.channelType,
        data.status
    );
}

// Retrieve one channel from PostgreSQL asynchronously.
void getChannelByIdAsync(
    uint32_t channelId,
    HttpResponseCallback callback)
{
    // Select only the channel matching the supplied ID.
    const std::string sql =
        "SELECT channel_id, channel_name, channel_type, status "
        "FROM channel_info "
        "WHERE channel_id = $1 "
        "LIMIT 1";

    // Share the callback with the asynchronous handlers.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Execute the SELECT without blocking the HTTP worker.
    drogon::app().getDbClient()->execSqlAsync(
        sql,

        // Called when PostgreSQL successfully executes the query.
        [sharedCallback, channelId](
            const drogon::orm::Result &result)
        {
            handleSelectSuccess(result, sharedCallback, channelId);
        },

        // Called when PostgreSQL reports an error.
        [sharedCallback, channelId](
            const std::exception_ptr &error)
        {
            handleSelectError(error, sharedCallback, channelId);
        },

        // Bind the requested ID to SQL placeholder $1.
        channelId
    );
}