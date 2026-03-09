# build

1. used sdk manager for first flash programing
2. then do everything on device (installs, updates, demos, etc)

## previous installs and tests
installed ultralytics, tested with GPU:

source ~/yolo-venv/bin/activate
>>> import ultralytics
>>> import torch
>>> ultralytics.cuda.is_available()

yolo predict model=yolo11n.pt source=0 conf=0.25 save=True project=~/yolo-runs name=camera-test exist_ok=True show=True

# skills
using .github skills of terminal-command-inject and copy scp-copy-files skill for testing and working with the device (target)

## SSH-MCP (installed for this workspace)
- Source: `tools/ssh-mcp` (cloned from `https://github.com/mixelpixx/SSH-MCP`)
- Build output: `tools/ssh-mcp/build/index.js`
- Workspace MCP config: `.vscode/mcp.json`
- Tracked SSH key profile: `.vscode/ssh-mcp.profile.json`

### How SSH MCP server was added
Use this in Copilot CLI: `copilot` -> `/mcp add` (the command is interactive; there is not a documented full inline `/mcp add ...` form).

Enter:
- Name: `ssh-server`
- Command: `node`
- Args: `/media/ranshal/4c73e1ab-4b56-472a-9270-98829a300cb8/jetson/nvidia/tools/ssh-mcp/build/index.js`
- Env: `NODE_NO_WARNINGS=1`

Press `Ctrl+S` to save.

If prompted for SSH defaults in your server flow, use `.vscode/ssh-mcp.profile.json` values:
- `host`: `192.168.55.1`
- `port`: `22`
- `username`: `ubuntu`
- `privateKeyPath`: `~/.ssh/id_ed25519`

### SSH key usage
Use `ssh_connect` with the defaults from `.vscode/ssh-mcp.profile.json`:
- `host`: `192.168.55.1`
- `port`: `22`
- `username`: `ubuntu`
- `privateKeyPath`: `~/.ssh/id_ed25519`
