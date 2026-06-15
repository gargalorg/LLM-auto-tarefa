# WinBridgeAgent - MCP Protocol Documentation

[中文文档](MCP.md)

## Overview

WinBridgeAgent implements the Model Context Protocol (MCP) standard, enabling AI assistants to call Windows system functions through a standardized interface.

**Open Source Features**:
- ✅ Bearer token authentication (except `/health` and `/mcp/status`)
- ✅ Complete system operation capabilities
- ✅ Real-time audit logging
- ✅ HTTP and SSE transport support

## MCP Protocol Version

- **Protocol Version (server response)**: 2024-11-05
- **Accepted Client Versions**: 2024-11-05, 2025-03-26
- **Implementation Status**: Full Implementation
- **Release Date**: 2026-02-06
- **Supported Features**: Tool calling (resources/prompts are not implemented)

## Typical Handshake Flow (Client ↔ Server)

Treat MCP as "JSON-RPC over a transport":

1. Establish transport (stdio/HTTP/WebSocket)
2. Capability discovery (initialize/hello)
3. Enumerate tools (tools/list; optionally resources/prompts if supported)
4. Run phase (tools/call; optional logs/events; cancellation/timeout)

For this project (HTTP/LAN use case), recommended sequence:

1. Optional probe (no auth): `GET /mcp/status`
2. Initialize (auth): `POST /mcp` (JSON-RPC `initialize`) or legacy `POST /mcp/initialize`
3. List tools: `tools/list`
4. Call tools: `tools/call`

## Feature Support Matrix

| Feature | Support | Notes |
| --- | --- | --- |
| Tools | ✅ | `tools/list`, `tools/call` |
| Resources | ✅ | `resources/list`, `resources/read` |
| Prompts | ✅ | `prompts/list`, `prompts/get` |

### Tool Categories

| Category | Tool Count | Tool List |
|----------|------------|-----------|
| File Operations | 8 | `read_file`, `write_file`, `list_directory`, `search_files`, `create_directory`, `delete_file`, `move_file`, `copy_file` |
| System Information | 4 | `list_disks`, `get_system_info`, `list_processes`, `list_windows` |
| Clipboard | 2 | `read_clipboard`, `write_clipboard` |
| Screenshot | 3 | `take_screenshot`, `take_region_screenshot`, `take_window_screenshot` |
| Power Management | 3 | `shutdown`, `restart`, `sleep` |
| Command Execution | 2 | `execute_command`, `execute_powershell` |
| Browser | 2 | `list_browser_tabs`, `close_browser_tab` |

**Total**: 24 tools

## Connection Configuration

### Capability Probe (No Auth)

Some clients probe MCP first:

```bash
curl http://<windows-ip>:35182/mcp/status
```

The response includes machine-readable limits (HTTP max body/header, rate limit, SSE limits) and auth/security policy hints.

### Basic Connection

```bash
# Initialize MCP session
curl -X POST \
  -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}' \
  http://<windows-ip>:35182/mcp/initialize
```

### List Available Tools

```bash
curl -X POST \
  -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{}' \
  http://<windows-ip>:35182/mcp/tools/list
```

### Tool Call Examples

```bash
# List directory contents
curl -X POST \
  -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"list_directory","arguments":{"path":"C:\\"}}' \
  http://<windows-ip>:35182/mcp/tools/call

# Read file
curl -X POST \
  -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"read_file","arguments":{"path":"C:\\test.txt"}}' \
  http://<windows-ip>:35182/mcp/tools/call

# Execute command
curl -X POST \
  -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"execute_command","arguments":{"command":"dir"}}' \
  http://<windows-ip>:35182/mcp/tools/call
```

## Tool Details

### File Operation Tools

#### `read_file`
Read file content with support for line ranges and tail reading.

**Parameters**:
- `path` (string, required): File path
- `start` (integer, optional): Start line number (0-based)
- `lines` (integer, optional): Number of lines to read
- `tail` (integer, optional): Number of lines to read from end

#### `write_file`
Write content to file.

**Parameters**:
- `path` (string, required): File path
- `content` (string, required): File content
- `append` (boolean, optional): Whether to append (default false)

#### `list_directory`
List directory contents.

**Parameters**:
- `path` (string, required): Directory path
- `offset` (integer, optional): Pagination start index (default 0)
- `limit` (integer, optional): Max entries to return (default 200, max 2000)

#### `search_files`
Search files by name/metadata/content filters.

**Parameters**:
- `path` (string, optional): Restrict search root path
- `name_query` (string, optional): File name keyword
- `content_query` (string, optional): File content keyword
- `exts` (string[], optional): Allowed extensions (e.g. `[".txt", ".md"]`)
- `days` (number, optional): Only files modified within N days
- `min_size` (number, optional): Minimum file size in bytes
- `max_size` (number, optional): Maximum file size in bytes
- `max` (number, optional): Max results (default 100)

#### `search_file`
Search text in one file.

**Parameters**:
- `path` (string, required): File path
- `query` (string, required): Search keyword

### System Information Tools

#### `list_disks`
Get all disk drive information.

**Returns**: Drive list with drive letter, type, total space, free space, etc.

#### `list_processes`
Get running process list.

**Returns**: Process list with PID, name, path, memory usage, etc.

#### `list_windows`
Get desktop window list.

**Returns**: Window list with handle, title, class name, position, size, etc.

### Clipboard Tools

#### `read_clipboard`
Read clipboard content.

**Returns**: Supports text, image, and file types.

#### `write_clipboard`
Write content to clipboard.

**Parameters**:
- `content` (string, required): Content to write

### Screenshot Tools

#### `take_screenshot`
Capture entire screen.

**Parameters**:
- `format` (string, optional): Image format (png/jpg, default png)
- `quality` (integer, optional): JPEG quality (1-100, default 90)

#### `take_region_screenshot`
Capture screen region.

**Parameters**:
- `x` (integer, required): Start X coordinate
- `y` (integer, required): Start Y coordinate
- `width` (integer, required): Width
- `height` (integer, required): Height
- `format` (string, optional): Image format

#### `take_window_screenshot`
Capture specific window.

**Parameters**:
- `window_title` (string, optional): Window title (partial match)
- `process_name` (string, optional): Process name

### Power Management Tools

#### `shutdown`
Shutdown computer.

#### `restart`
Restart computer.

#### `sleep`
Put computer to sleep.

### Command Execution Tools

#### `execute_command`
Execute system command.

**Parameters**:
- `command` (string, required): Command to execute
- `timeout` (integer, optional): Timeout in seconds (default 30)

#### `execute_powershell`
Execute PowerShell command.

**Parameters**:
- `script` (string, required): PowerShell script
- `timeout` (integer, optional): Timeout in seconds (default 30)

### Browser Tools

#### `list_browser_tabs`
List browser tabs.

**Parameters**:
- `browser` (string, optional): Browser type (chrome/edge/firefox, auto-detect)

#### `close_browser_tab`
Close browser tab.

**Parameters**:
- `tab_id` (string, required): Tab ID
- `browser` (string, optional): Browser type

## Error Handling

Dispatcher-level errors and upgraded tools (such as `list_directory`) use this structured format:

```json
{
  "ok": false,
  "tool": "tool_name",
  "error": {
    "code": "ERROR_CODE",
    "message": "error message",
    "details": {}
  }
}
```

In MCP protocol payload, this JSON is placed in `content[0].text`, and text is UTF-8 encoded.

Legacy shape is still accepted by some clients:

```json
{
  "content": [
    {
      "type": "text",
      "text": "Error message"
    }
  ],
  "isError": true
}
```

Common errors:
- File not found
- Permission denied
- Invalid path
- Command execution failed
- Timeout

## Audit Logging

The open source version provides complete operation auditing. All operations are logged to:

- Log location: `%LOCALAPPDATA%\WinBridgeAgent\logs\audit-YYYY-MM-DD.log`
- Log content: Timestamp, operation type, parameters, result, client information

## Security Notes

The open source version removes commercial security restrictions, but note:

1. **Network Security**: For LAN access, set listen address to `0.0.0.0` in settings
2. **Firewall**: Ensure Windows Firewall allows port 35182 access
3. **Permissions**: Program runs with current user privileges, cannot execute admin operations

## More Information

- Project homepage: https://github.com/codyard/WinBridgeAgent
- MCP Registry: https://registry.modelcontextprotocol.io/v0.1/servers?search=io.github.codyard/winbridgeagent
- Full documentation: README.md / README_zh.md
