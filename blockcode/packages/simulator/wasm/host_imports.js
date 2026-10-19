// Emscripten JS library: forwards the VM's host calls to Module.host, an object
// the simulator installs before starting (see packages/simulator/src/VmWasm.ts).
// Module.host must implement:
//   nowMs(), random32(), syscall(id, args:Float32Array, strings:Uint8Array) ->
//     { ok, result?, waitKind?, waitValue? }, waitDone(kind, value), event(ev, param),
//   programStart(), programStop(), error(code, script, pc)
mergeInto(LibraryManager.library, {
  js_now_ms: function () { return Module.host.nowMs() >>> 0; },
  js_random32: function () { return Module.host.random32() >>> 0; },
  js_syscall: function (id, argsPtr, argc, resultPtr, kindPtr, valuePtr, strPtr, strLen) {
    var args = HEAPF32.subarray(argsPtr >> 2, (argsPtr >> 2) + argc);
    var strings = HEAPU8.subarray(strPtr, strPtr + strLen);
    var r = Module.host.syscall(id, args, strings);
    if (!r || !r.ok) return 0;
    if (r.result !== undefined) HEAPF32[resultPtr >> 2] = r.result;
    HEAPU8[kindPtr] = r.waitKind || 0;
    HEAPU32[valuePtr >> 2] = (r.waitValue || 0) >>> 0;
    return 1;
  },
  js_wait_done: function (kind, value) { return Module.host.waitDone(kind, value >>> 0) ? 1 : 0; },
  js_event: function (ev, param) { return Module.host.event(ev, param) ? 1 : 0; },
  js_program_start: function () { Module.host.programStart(); },
  js_program_stop: function () { Module.host.programStop(); },
  js_error: function (code, script, pc) { Module.host.error(code, script, pc); },
});
