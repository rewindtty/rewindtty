# Upload Server Implementation Guide

This guide explains how to implement a server to receive JSON calls from rewindtty.

## Request Format

rewindtty sends JSON data via HTTP POST requests to the configured endpoint.

### Required HTTP Headers
```
Content-Type: application/json
User-Agent: rewindtty-cli/{version}
```

### Session JSON Structure

The JSON payload contains the following fields:

```json
{
  "command": "executed command string",
  "start_time": 1692123456.789,
  "end_time": 1692123466.123,
  "duration": 9.334,
  "chunks": [
    {
      "time": 0.0,
      "size": 25,
      "data": "terminal output"
    },
    {
      "time": 1.234,
      "size": 12,
      "data": "more output"
    }
  ]
}
```

#### Session fields:
- `command`: The command executed in the terminal
- `start_time`: Unix timestamp of session start (double)
- `end_time`: Unix timestamp of session end (double)
- `duration`: Total duration in seconds (calculated as end_time - start_time)
- `chunks`: Array of terminal output chunks

#### Chunk fields:
- `time`: Relative time from session start (double, in seconds)
- `size`: Data size in bytes (double)
- `data`: Terminal output as string

## Server Implementation

### Required Endpoint

The server must expose an endpoint that accepts POST requests. The default URL is configured as:
```
https://upload.rewindtty.dev/upload
```

### Server Response

#### Success Response (HTTP 2xx):
The server must respond with a JSON containing an `id` field:

```json
{
  "id": "unique_session_id"
}
```

The `id` can be either a string or a number. rewindtty will use it to build the player URL:
```
https://play.rewindtty.dev/{id}
```

#### Error Response (HTTP 4xx/5xx):
In case of error, the server can respond with an error message in the response body.

## Configuration

### Custom Upload URL

To configure a custom upload URL or custom player URL, you can make the project with custom `PLAYER_URL` and/or `UPLOAD_URL`.


```bash
make UPLOAD_URL=http://localhost:5500/upload PLAYER_URL=http://localhost:3000
```

## Security Considerations

1. **Input validation**: Always validate received JSON data
2. **Rate limiting**: Implement rate limits to prevent abuse
3. **Authentication**: Consider implementing an authentication system
4. **Maximum size**: Limit the maximum size of the JSON payload
5. **Sanitization**: Sanitize data before saving, especially the `data` field of chunks

## Testing

To test the server, you can use curl:

```bash
curl -X POST https://your-server.com/upload \
  -H "Content-Type: application/json" \
  -H "User-Agent: rewindtty-cli/1.0" \
  -d '{
    "command": "ls -la",
    "start_time": 1692123456.789,
    "end_time": 1692123466.123,
    "duration": 9.334,
    "chunks": [
      {
        "time": 0.0,
        "size": 25,
        "data": "total 8\ndrwxr-xr-x 2 user user 4096 Aug 15 14:30 ."
      }
    ]
  }'
```

The response should be:
```json
{
  "id": "abc123"
}
```