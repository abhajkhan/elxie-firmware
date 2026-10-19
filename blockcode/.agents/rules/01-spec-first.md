# Spec first

- Before implementing a block, syscall, command or message, find it in `spec/opcodes.json` / `spec/03-protocol.md`. If it isn't there, propose the spec change first (in your plan), then implement.
- After editing `spec/opcodes.json`, run `python3 tools/gen_opcodes.py` and commit generated files in the same change.
- Add or update a golden program in `spec/test-vectors/` for every new VM-visible behavior.
