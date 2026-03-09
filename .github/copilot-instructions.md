# Copilot instructions for this workspace (Jetson L4T / JetPack 6.2.2)

## Scope and boundaries
- Main editable tree: `JetPack_6.2.2_Linux_JETSON_ORIN_NANO_TARGETS/Linux_for_Tegra`.
- Treat `JetPack_6.2.2_Linux/NVIDIA_Nsight_Perf_SDK` as vendor/sample content; avoid edits unless asked.
- `Linux_for_Tegra/rootfs` is a full Ubuntu snapshot; avoid broad edits there.
- `jetson-orin.txt` is very large decompiled DTS output; use targeted reads/search.
- `ultralytics/` is a separate Git-managed repo (origin: `https://github.com/ultralytics/ultralytics.git`); treat as upstream/vendor and avoid edits unless explicitly requested.

## Big-picture architecture (how flashing is composed)
- Flash flow is layered: `nvsdkmanager_flash.sh` (checks + UX wrapper) -> `nvautoflash.sh` (RCM board detect) -> `flash.sh` (core flashing/signing logic).
- `flash.sh` is the root implementation and supports `flash.sh [options] <target_board> <root_device>`; wrappers should stay thin.
- Board config is shell-source composition, not a manifest system: board files like `jetson-orin-nano-devkit.conf` source `p3767.conf.common` and override functions/vars.
- Board-specific runtime selection happens in functions like `update_flash_args_common` (DTB/BPFDTB/EMMC config from `board_sku` + FAB).
- BUP path is spec-driven: `l4t_generate_soc_bup.sh` reads spec arrays from `jetson_board_spec.cfg` and invokes `build_l4t_bup.sh`, which delegates to `flash.sh --no-flash --sign --bup`.

## Critical workflows (host)
- Default flash from `Linux_for_Tegra`: `sudo ./nvautoflash.sh`.
- Explicit flash path: `sudo ./flash.sh <target_board> <rootdev>`.
- Safe prechecks before flashing: `sudo ./nvsdkmanager_flash.sh --check-all` (or `--check-target-only`, `--check-network-only`).
- External storage/initrd flash: `sudo ./nvsdkmanager_flash.sh --storage nvme0n1p1`.
- Rootfs binary sync before image generation: `sudo ./apply_binaries.sh -r ./rootfs`.
- Kernel + NVIDIA OOT build from `source/`: `./nvbuild.sh -o <abs_outdir>`.

## Prerequisites and non-obvious constraints
- Root is required for key scripts (`flash.sh`, `nvautoflash.sh`, `nvsdkmanager_flash.sh`, `apply_binaries.sh`).
- Initrd flash hard-checks host deps (`sshpass`, `abootimg`, `zstd`) and NFS firewall/VPN conditions in `tools/kernel_flash/l4t_initrd_flash.sh`.
- x86 kernel builds require `CROSS_COMPILE` and `${CROSS_COMPILE}gcc` (`source/nvbuild.sh`).
- Source/build paths must not contain spaces or colons (enforced in `source/Makefile`).

## Conventions for edits
- Keep changes in shell style used here: `set -e` / `set -o pipefail`, helper functions, explicit early exits.
- Do not hardcode board SKUs/IDs in scripts when existing spec/config already models them (`jetson_board_spec.cfg`, board `.conf`, `p3767.conf.common`).
- If behavior is user-facing, prefer wrapper-layer updates (`nvsdkmanager_flash.sh`, `nvautoflash.sh`) and only touch `flash.sh` for core mechanics.
- USB recovery detection is vendor/product scanning under `/sys/bus/usb/devices` in both `nvautoflash.sh` and `tools/kernel_flash/l4t_initrd_flash.sh`; keep logic aligned.
- Keep signing/secure boot flows centralized (`l4t_sign_image.sh`, `flash.sh`, BUP scripts); avoid one-off signing code.

## Workspace operating defaults
- Device defaults used by workspace skills and automation:
  - `target_ip`: `192.168.55.1`
  - `target_user`: `ubuntu`
  - `target_password`: `ubuntu`
  - `target_ssh_private_key`: `~/.ssh/id_ed25519` (host path; create it if missing and copy public key to target)
  - `target_serial_device`: `/dev/ttyACM0`
  - `target_prompt_regex`: `(?:<username>@<username>:.*[$#]|[$#]) ?$`
- Per top-level `README.md`, current device-side workflow often uses `.github/skills/terminal-command-inject` and `.github/skills/scp-file-copy`.

## Device command routing (SSH MCP required)
- **Important:** Device operations must use SSH MCP; use skills only as a fallback.
- Requests that say "on device", "in device", or target `192.168.55.1` must run through SSH MCP tools, not local host shell.
- Preferred route for remote execution:
  1) `ssh_connect` using defaults (`host`, `port`, `username`, `privateKeyPath`) from `.vscode/ssh-mcp.profile.json`
  2) `ssh_exec` for command execution
  3) `ssh_disconnect` after command completion
- For commands requiring privilege (for example `sudo ls /home/ -al`), execute remotely via `ssh_exec` and provide output from the remote command.
- Host `bash` is only for host-side operations; never use it as a substitute for device-side command requests.

## Tool availability preflight
- Before running any device-side request, verify SSH MCP tools are available in the current tool list (`ssh_connect`, `ssh_exec`, `ssh_disconnect`).
- If SSH MCP tools are unavailable, stop and clearly report: "SSH MCP tools are not available in this session; please enable the ssh-server MCP so I can run this on the device."
- Do not silently fall back to host-local execution for device commands when SSH MCP is unavailable.
