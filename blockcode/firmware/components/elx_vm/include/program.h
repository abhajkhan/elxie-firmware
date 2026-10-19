// Elxie program image (.elxb) parsing and validation.
// Portable C++ (no Arduino dependencies) so it builds on the robot, on a PC for
// tests, and to WebAssembly for the app simulator.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "opcodes_gen.h"

namespace elx {

// File layout (little-endian):
//   0  'E''L''X''B'
//   4  u8  version
//   5  u8  num_lists     (0..LIMIT_MAX_LISTS)
//   6  u8  num_scripts   (1..LIMIT_MAX_SCRIPTS)
//   7  u8  num_vars      (0..LIMIT_MAX_VARS)
//   8  u16 code_len
//  10  u16 strings_len
//  12  char name[16]     (UTF-8, NUL padded)
//  28  script table: num_scripts x { u8 event, u8 param, u16 code_offset }
//      code[code_len]
//      strings[strings_len] (NUL-terminated strings, referenced by byte offset)
//      u32 crc32 (IEEE) over every byte before it
constexpr size_t HEADER_SIZE = 28;
constexpr size_t SCRIPT_ENTRY_SIZE = 4;

struct ScriptEntry {
  uint8_t event;
  uint8_t param;
  uint16_t offset;
};

struct Program {
  char name[17];
  uint8_t version;
  uint8_t numLists;
  uint8_t numScripts;
  uint8_t numVars;
  ScriptEntry scripts[LIMIT_MAX_SCRIPTS];
  const uint8_t* code;
  uint16_t codeLen;
  const char* strings;
  uint16_t stringsLen;
};

// Which syscalls/events the running firmware implements. The validator rejects
// programs that use anything else so a newer app can't crash an older robot.
typedef bool (*SupportFn)(uint8_t id);

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0);

// Parse + fully validate. On success `out` points into `buf`, which must stay
// alive while the program runs. Validation guarantees every instruction,
// jump target, variable index and syscall id is legal, so the interpreter can
// skip bounds checks on operands.
LoadError parseProgram(const uint8_t* buf, size_t len, Program& out,
                       SupportFn syscallSupported, SupportFn eventSupported);

// Returns string at byte offset, or nullptr if out of range.
const char* programString(const Program& p, int32_t offset);

}  // namespace elx
