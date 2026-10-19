---
description: Prepare a firmware release
---
1. Bump `ELX_FW_MINOR` (or MAJOR) in `firmware/main/board_config.h`.
2. Run the `verify` workflow.
3. `cd firmware && idf.py build` — fix any build errors.
4. Write release notes: new syscalls (they appear in the INFO capability bitmap), behavior changes, board_config changes.
