# Windows Node 快速安装指南

使用 ClawDesk MCP 将 Windows 电脑变成 OpenClaw 的 Node。

## 前置要求

- Windows 10/11
- Node.js 18+ (用于 mcporter)
- 网络可达 OpenClaw Gateway

## 安装步骤

### 1. 下载 ClawDesk

```powershell
# 从 GitHub 下载最新版本
curl -L -o WinBridgeAgentPro.zip https://github.com/codyard/WinBridgeAgentPro/releases/latest/download/WinBridgeAgentPro-win32-x64.zip

# 解压到任意目录
Expand-Archive -Path WinBridgeAgentPro.zip -DestinationPath "C:\Program Files\WinBridgeAgentPro"
```

### 2. 启动服务

```powershell
# 进入目录
cd "C:\Program Files\WinBridgeAgentPro"

# 启动（会自动打开仪表板）
.\WinBridgeAgentPro.exe
```

仪表板地址：`http://localhost:35182`

### 3. 配置安全策略（重要）

1. 打开仪表板 `http://localhost:35182`
2. 进入 **Settings** → **Security**
3. 添加命令白名单：

```
echo, dir, cd, type, copy, move, del,
git, npm, node, python, python3, pnpm,
curl, wget, ping, ipconfig
```

4. 保存并重启服务

### 4. 安装 mcporter（OpenClaw 端）

```bash
# 在运行 OpenClaw 的机器上
npm i -g mcporter
```

### 5. 配对 Windows Node

```bash
# 添加配置
mcporter config add --name windows --url http://192.168.31.3:35182

# 验证连接
mcporter list clawdesk
```

### 6. 测试工具

```bash
# 截图
mcporter call clawdesk.take_screenshot

# 摄像头
mcporter call clawdesk.take_camera_photo

# 录屏
mcporter call clawdesk.record_screen duration=5

# 执行命令
mcporter call clawdesk.execute_command command="dir C:\Users"

# 进程列表
mcporter call clawdesk.list_processes
```

## 工具清单

| 类别 | 工具数 | 示例 |
|------|--------|------|
| 文件操作 | 12 | read_file, write_file, list_directory |
| 截图/录屏 | 3 | take_screenshot, record_screen |
| 摄像头 | 2 | take_camera_photo, take_camera_video |
| 浏览器 | 8 | browser_launch, browser_navigate |
| 系统控制 | 6 | shutdown_system, set_clipboard |
| 进程/窗口 | 7 | list_processes, list_windows |
| Shell | 2 | execute_command, run_bat |

**总计：54 个工具**

## 常见问题

### Q: execute_command 被拒绝
A: 在仪表板 Security 设置中添加命令到白名单，然后重启服务

### Q: 摄像头找不到
A: 重启 ClawDesk 服务，让它重新检测硬件

### Q: 录屏失败
A: 确保 Windows Graphics Capture 已启用（设置 → 隐私 → 屏幕捕捉）

### Q: 如何远程访问？
A: 把 `http://localhost:35182` 改成 `http://<IP>:35182`，确保防火墙放行

## 卸载

```powershell
# 停止服务
taskkill /F /IM WinBridgeAgentPro.exe

# 删除目录
Remove-Item -Recurse -Force "C:\Program Files\WinBridgeAgentPro"
```

---

**完成！** 现在你的 Windows 电脑已经是 OpenClaw 的 Node 了。
