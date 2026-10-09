# SDL ZZFractal 0.4 Direct hardware acceptance

A4000TX / TF4060 / ZZ9000 XX19c / XACP 1.7, 2026-10-09.

- `comparison.json`: five-run medians on the same 32-bit RTG mode: 0.3 ARM
  3.737539 s; direct ARM 1.807719 s. 2.0675x, 51.6334% less wall time.
- `acceptance.json` and `.log`: final developer build, independent full-frame
  oracle and true-colour display checks, deeper 10.122 s render, legacy modes,
  hidden/overlapping/moved windows, cancellation and quit while paused.
- `baseline.json` and `.log`: exact previously verified 0.3 developer executable,
  SHA-256 `c475ff7e3a39137ac4ab1f11fe0c53a68748019957e844f66c9b041032ac8756`.
- `standalone.log`: exact installed release, default ARM and CPU hashes,
  keyboard A/C/Q, RET1 and exit 0. `standalone.png`: physical window capture.
- `build.json` / `fractal-points.json`: developer identity. Standalone identity
  and executable are under `amiga/distribution/SDLZZFractal-0.4/`.
- `install.json`: installed files match local checksums. `lha-validation.json`:
  native LHA fresh extraction and all ten member checksums. Archive integrity
  also passed native LhA 2.15; Mac-downloaded LHA MD5 matched the Amiga.
- `release-assets.json`: exact release asset sizes and SHA-256 hashes.
- Native tests and component build logs distinguish simulator/host evidence
  from physical execution. Full-frame/cancellation/integrity/deadline tests
  use ASAN/UBSAN. 45 native tests and MCP smoke passed.

Normal direct rendering does not read back the image. The `save`/`pixels`
hooks explicitly do so afterward for validation; timed repeats precede it.
The existing SDK 0.2.0 SDL2 library, Q14 kernel, firmware and cache/MMU policy
are unchanged. The copy uses owned, briefly locked P96 memory and an on-card
blit; no page-flip/vsync or zero-total-Zorro-traffic guarantee is claimed.

A candidate run initially requested the true-colour diagnostic on 8-bit
Workbench; that hook is deliberately unsupported there. The harness was
corrected to use full count/oracle validation for Workbench. Final acceptance
passed. A further full-frame timeout fix was verified by the >10 s render.

The installed standalone was restarted after its clean exit test and left
open for the user. Q releases Core1; the MCP bridge remains available.
