---
description: Run every check the repo has and summarize results
---
1. Run `python3 tools/gen_opcodes.py` and confirm `git diff --exit-code` shows no changes to generated files.
2. Run `tests/run_tests.sh`.
3. Run `tests/host_compile/check.sh`. If ESP-IDF is installed, also run `cd firmware && idf.py build`.
4. If `package.json` exists at the root, run `pnpm -r test`.
5. Report a table: check / result / notes. If anything fails, fix it before reporting done.
