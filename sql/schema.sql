
-- Create the channel_info table if it does not already exist.
CREATE TABLE IF NOT EXISTS channel_info
(
    -- Unique identifier for each channel.
    channel_id INTEGER PRIMARY KEY,

    -- Channel name is required and limited to 255 characters.
    channel_name VARCHAR(255) NOT NULL,

    -- Numeric channel type.
    channel_type INTEGER NOT NULL,

    -- Numeric channel status.
    status INTEGER NOT NULL,

    -- Automatically record when the row was created.
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
