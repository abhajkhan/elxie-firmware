# Testing

- Required green before finishing any task: `tests/run_tests.sh`, `tests/host_compile/check.sh`, and `pnpm -r test` (when packages exist).
- Compiler work: every fixture in `packages/compiler/fixtures/` must compile byte-identical to `tools/elxasm.py` output of the matching `spec/test-vectors/*.elxa`.
- Simulator work: the WASM VM must reproduce `spec/test-vectors/expected/*.trace` exactly.
- UI work: add/extend a Playwright test that drives the flow in the simulator, and attach a screenshot to your summary.
