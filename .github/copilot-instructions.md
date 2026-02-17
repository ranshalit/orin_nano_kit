# Copilot instructions for this workspace (Jetson L4T / JetPack 6.2.2)

## Repository focus and boundaries
- Primary working tree is `JetPack_6.2.2_Linux_JETSON_ORIN_NANO_TARGETS/Linux_for_Tegra` (L4T BSP, flashing, bootloader, rootfs tooling).
- Treat `JetPack_6.2.2_Linux/NVIDIA_Nsight_Perf_SDK` as third-party/vendor sample content; avoid changing it unless explicitly requested.
- `Linux_for_Tegra/rootfs` is a full Ubuntu rootfs snapshot; avoid edits there unless the request explicitly targets rootfs content.
- Top-level `jetson-orin.txt` is a large decompiled DT source artifact; use targeted search/read ranges instead of loading the whole file.

## Big-picture architecture
- Flashing entrypoints are layered wrappers:
  - `nvsdkmanager_flash.sh` (SDKM-friendly flow, storage/network prechecks, external media flow)
  - `nvautoflash.sh` (auto-detect board in RCM and call `flash.sh`)
  - `flash.sh` (core implementation, ~5k lines, accepts `<target_board> <rootdev>`)
- Board selection/config composition is shell-source based:
  - board config files (for example `jetson-orin-nano-devkit.conf`) source shared common config (`p3767.conf.common`)
  - runtime functions like `update_flash_args_common` derive DTB/BCT/EMMC config from EEPROM-derived `board_sku`/FAB.
- Update payload path: `build_l4t_bup.sh` and `l4t_generate_soc_bup.sh` drive BUP generation using board specs in `jetson_board_spec.cfg`.

## Critical workflows (host-side)
- Standard flash from `Linux_for_Tegra`: `sudo ./nvautoflash.sh`.
- Explicit flash: `sudo ./flash.sh <target_board> <rootdev>` (examples are documented in `flash.sh` header).
- SDKM wrapper with checks: `sudo ./nvsdkmanager_flash.sh --check-all`.
- External media flash (initrd flow): `sudo ./nvsdkmanager_flash.sh --storage nvme0n1p1`.
- Apply NVIDIA binaries into rootfs before image generation: `sudo ./apply_binaries.sh -r ./rootfs`.
- Build kernel + NVIDIA OOT modules from `source/`: `./nvbuild.sh -o <abs_outdir>`.

## Build/debug prerequisites that are easy to miss
- Many scripts require root (`flash.sh`, `nvautoflash.sh`, `nvsdkmanager_flash.sh`, `apply_binaries.sh`).
- Initrd flash requires host tools and network state (`sshpass`, `abootimg`, `zstd`, NFS firewall allowance, preferably no VPN) as enforced in `tools/kernel_flash/l4t_initrd_flash.sh`.
- On x86 kernel builds, `source/nvbuild.sh` expects `CROSS_COMPILE` toolchain and checks `${CROSS_COMPILE}gcc` exists.
- Keep output paths free of spaces/colons (enforced by `source/Makefile` and `source/nvbuild.sh`).

## Project conventions and editing patterns
- Shell is the control plane; config is mostly environment variables + sourced `.conf` fragments (not JSON/YAML).
- Follow existing bash style in this repo: `set -e`/`set -o pipefail`, explicit helper functions, early exits with clear error text.
- Do not hardcode board IDs/SKUs in new logic when existing spec/config files already model them (`jetson_board_spec.cfg`, `p3767.conf.common`, board `.conf` files).
- Prefer wrapper scripts (`nvsdkmanager_flash.sh`, `nvautoflash.sh`) for user-facing workflows; only drop into `flash.sh` when adding core behavior.

## Integration points to understand before changes
- USB recovery-device detection is done by vendor/product ID scanning under `/sys/bus/usb/devices` in both `nvautoflash.sh` and `tools/kernel_flash/l4t_initrd_flash.sh`.
- Board spec arrays in `jetson_board_spec.cfg` are consumed by BUP tooling (`l4t_generate_soc_bup.sh`) and should stay schema-compatible (`key=value;...` entries).
- Security/signing flows are centralized in `l4t_sign_image.sh` and secure-boot paths in `flash.sh`/BUP scripts; avoid ad-hoc signing logic.

## Agent operating guidance
- When asked to modify flash behavior, identify whether the request belongs in wrapper layer (`nvsdkmanager_flash.sh` / `nvautoflash.sh`) or core layer (`flash.sh`) before editing.
- For board-specific changes, edit the smallest relevant board `.conf` plus shared common file only when multiple SKUs are affected.
- Validate with script help/check commands first (for example `--help`, `--check-target-only`, `--check-network-only`) before proposing destructive flash commands.

## Target device defaults
- `target_ip`: `192.168.55.1`
- `target_user`: `ubuntu`
- `target_password`: `ubuntu`
- `target_prompt_regex`: `(?:<username>@<username>:.*[$#]|[$#]) ?$`
- `target_serial_device`: `/dev/ttyACM0`
- `target_mac`: `3c:6d:66:62:a2:11`
