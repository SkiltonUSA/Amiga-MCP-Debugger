# Amiga development backup and recovery

The Git branch contains the Mac development tooling, native source, launch
scripts, session memory, hardware report and selected configuration/install
records. It does **not** contain a complete Amiga disk backup.

Start by reading [session memory](amiga-session-memory.md). It records the
latest user confirmations and supersedes “pending” runtime statements in
older installation records. The reports are dated historical evidence.

## What is preserved

- `amiga/`, `scripts/amiga.py`, `tests/amiga/`: pinned build environment,
  upstream patch, probe, network watcher, launch scripts and simulator checks.
- `docs/development-amiga.md`: architecture, setup and debugger limitations.
- `amiga/records/2026-10-07/inventory/`: searchable HTML, PDF, CSV inventories,
  raw observations and source used to build the report.
- Other dated record drawers: firmware identities, USB installation checks,
  application installation paths/checksums, configuration snapshots and the
  successful Dark Forces 640x480 diagnostic log.
- `amiga/records/2026-10-07/manifest.json`: per-file SHA256, local provenance
  and indication of normalized Mac paths. `SHA256SUMS.txt` also covers the
  manifest. Verify from that directory with `shasum -a 256 -c SHA256SUMS.txt`.

Git attributes preserve record bytes, including original CRLF CSV output and
trailing spaces from Amiga commands; these are not source-formatting errors.

Record scripts are preserved as session evidence. Some assume specific paths,
installed Python dependencies, application versions or a live MCP endpoint.
Inspect them before reuse. In particular `extract_iso_data.py` is tied to the
recorded ISO extent map; it is not a general ISO extractor. Report rebuilding
uses the historical observations and does not collect a new hardware audit.

## Recreate the Mac environment

1. Clone the repository and check out this backup branch.
2. Install `uv`, a Docker-compatible runtime, and Mac FS-UAE if emulation is
   required. Supply your own licensed Amiga ROM/OS media.
3. Run `make setup-amiga` to create ignored `.tools/` and `.context/` files.
4. Edit `.context/amiga/settings.json`: hardware host `10.0.0.40`, TCP 2345,
   destination `RAM:SixiesDev`. Choose a container command that exists on the
   new Mac; the captured settings are evidence, not a portable default.
5. Run `make amiga-doctor`, `make amiga-build` and `make test-amiga`. The test
   starts an isolated simulator; it does not connect to or reboot hardware.
6. Run `make amiga-hardware` once the Amiga's bridge is listening. Use the
   generated `.context/amiga/codex-mcp.toml` fragment for the client setup.

The network watcher is separately built from
`amiga/tools/bridge-netwatch/main.c` as described in development-amiga.md;
`make amiga-build` builds the upstream bridge and Sixies probe.

## Recover Amiga files deliberately

The existing installation already has the bridge, USB stack and firmware.
Do not rerun firmware updates or overwrite working preferences just to restore
the Mac workspace. For actual Amiga recovery, compare against current files
first and preserve any newer changes.

`amiga/scripts/` contains the current editable launch scripts. The dated
`roadie/` records preserve before/after User-Startup text. The `scummvm/`
records include `scummvm-mt32.ini` and saved MIDI preferences; `inventory/`
contains the saved AHI preferences. These are captured snapshots, not a fresh
copy of every file on the machine. Copying binary preferences requires normal
Amiga permissions and appropriate reload/reboot handling.

The Dark Forces icon can be configured in Workbench Information: enable only
`640x480` and `SC55`, with stack 65536. Keep the original-icon backup in the
application drawer. A blank loading screen lasts while roughly 72 MB is
staged into ARM memory. All original Dark Forces game data must be supplied
again from the owner's media if recovering a lost disk.

## Deliberate exclusions and remaining backup gap

No ROMs, commercial game files, ISO/disk images, downloaded third-party
executables, firmware images, vendor installer archives, browser profiles,
credentials, generated toolchains or complete conversation transcript are
uploaded. Download origins/checksums and the meaningful session decisions are
preserved instead. Existing local downloads and attachments are left intact.

The Amiga was unreachable during this backup, so no fresh live configuration
or game-save capture was possible. A separate verified backup of `System:`,
`HDD50Gig:` and `SD032G:` is still needed for full hardware recovery. No disk
backup, formatting or repair was performed in this task.
