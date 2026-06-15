# Windows Node Quick Install Guide

Use ClawDesk MCP to turn a Windows PC into an OpenClaw node.

## Prerequisites

- Windows 10/11
- Node.js 18+ (for `mcporter`)
- Network access to your OpenClaw Gateway

## Installation Steps

### 1. Download ClawDesk

```powershell
# Download the latest release from GitHub
curl -L -o WinBridgeAgentPro.zip https://github.com/codyard/WinBridgeAgentPro/releases/latest/download/WinBridgeAgentPro-win32-x64.zip

# Extract to any directory
Expand-Archive -Path WinBridgeAgentPro.zip -DestinationPath "C:\Program Files\WinBridgeAgentPro"
```

### 2. Start the Service

```powershell
# Enter the directory
cd "C:\Program Files\WinBridgeAgentPro"

# Start the app (it will open the dashboard)
.\WinBridgeAgentPro.exe
```

Dashboard URL: `http://localhost:35182`

### 3. Configure Security Policy (Important)

1. Open the dashboard at `http://localhost:35182`
2. Go to **Settings** -> **Security**
3. Add command allowlist entries:

```
echo, dir, cd, type, copy, move, del,
git, npm, node, python, python3, pnpm,
curl, wget, ping, ipconfig
```

4. Save and restart the service

### 4. Install mcporter (OpenClaw side)

```bash
# On the machine running OpenClaw
npm i -g mcporter
```

### 5. Pair the Windows Node

```bash
# Add node config
mcporter config add --name windows --url http://192.168.31.3:35182

# Verify connectivity
mcporter list clawdesk
```

### 6. Test Tools

```bash
# Screenshot
mcporter call clawdesk.take_screenshot

# Camera
mcporter call clawdesk.take_camera_photo

# Screen recording
mcporter call clawdesk.record_screen duration=5

# Run command
mcporter call clawdesk.execute_command command="dir C:\Users"

# Process list
mcporter call clawdesk.list_processes
```

## Tool Summary

| Category | Count | Examples |
|------|--------|------|
| File operations | 12 | read_file, write_file, list_directory |
| Screenshot / recording | 3 | take_screenshot, record_screen |
| Camera | 2 | take_camera_photo, take_camera_video |
| Browser | 8 | browser_launch, browser_navigate |
| System control | 6 | shutdown_system, set_clipboard |
| Process / window | 7 | list_processes, list_windows |
| Shell | 2 | execute_command, run_bat |

**Total: 54 tools**

## FAQ

### Q: `execute_command` is denied

A: Add the command to the allowlist in the dashboard Security settings, then restart the service.

### Q: Camera is not found

A: Restart the ClawDesk service so it re-detects hardware.

### Q: Screen recording fails

A: Make sure Windows Graphics Capture is enabled (Settings -> Privacy -> Screen capture).

### Q: How do I access it remotely?

A: Replace `http://localhost:35182` with `http://<IP>:35182` and allow the port through the firewall.

## Uninstall

```powershell
# Stop the service
taskkill /F /IM WinBridgeAgentPro.exe

# Remove the directory
Remove-Item -Recurse -Force "C:\Program Files\WinBridgeAgentPro"
```

---

**Done!** Your Windows PC is now ready to work as an OpenClaw node.
