#include "program.h"

#include <string.h>

namespace elx {

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

LoadError parseProgram(const uint8_t* buf, size_t len, Program& out,
                       SupportFn syscallSupported, SupportFn eventSupported) {
  if (len < HEADER_SIZE + SCRIPT_ENTRY_SIZE + 4) return LoadError::TOO_SMALL;
  if (len > LIMIT_MAX_PROGRAM_BYTES) return LoadError::BAD_LENGTH;
  if (buf[0] != 'E' || buf[1] != 'L' || buf[2] != 'X' || buf[3] != 'B') return LoadError::BAD_MAGIC;
  if (buf[4] != BYTECODE_VERSION) return LoadError::BAD_VERSION;

  memset(&out, 0, sizeof(out));
  out.version = buf[4];
  out.numLists = buf[5];
  out.numScripts = buf[6];
  out.numVars = buf[7];
  out.codeLen = rd16(buf + 8);
  out.stringsLen = rd16(buf + 10);
  memcpy(out.name, buf + 12, 16);
  out.name[16] = 0;

  if (out.numScripts == 0 || out.numScripts > LIMIT_MAX_SCRIPTS) return LoadError::TOO_MANY_SCRIPTS;
  if (out.numVars > LIMIT_MAX_VARS) return LoadError::TOO_MANY_VARS;
  if (out.numLists > LIMIT_MAX_LISTS) return LoadError::BAD_LIST;

  const size_t tableEnd = HEADER_SIZE + (size_t)out.numScripts * SCRIPT_ENTRY_SIZE;
  const size_t expected = tableEnd + out.codeLen + out.stringsLen + 4;
  if (expected != len) return LoadError::BAD_LENGTH;
  if (out.codeLen == 0) return LoadError::BAD_LENGTH;

  if (crc32(buf, len - 4) != rd32(buf + len - 4)) return LoadError::BAD_CRC;

  out.code = buf + tableEnd;
  out.strings = (const char*)(out.code + out.codeLen);
  if (out.stringsLen > 0 && out.strings[out.stringsLen - 1] != 0) return LoadError::BAD_STRINGS;

  // Pass 1: decode every instruction, remember where instructions start.
  static uint8_t starts[LIMIT_MAX_PROGRAM_BYTES / 8];
  memset(starts, 0, sizeof(starts));
  const uint8_t* code = out.code;
  for (uint32_t pc = 0; pc < out.codeLen;) {
    const uint8_t op = code[pc];
    const uint8_t n = OP_LENGTH[op];
    if (n == 0) return LoadError::BAD_OPCODE;
    if (pc + n > out.codeLen) return LoadError::TRUNCATED;
    starts[pc >> 3] |= (uint8_t)(1u << (pc & 7));
    switch (op) {
      case OP_LOAD_VAR:
      case OP_STORE_VAR:
      case OP_CHANGE_VAR:
        if (code[pc + 1] >= out.numVars) return LoadError::BAD_VAR;
        break;
      case OP_LIST_ADD:
      case OP_LIST_GET:
      case OP_LIST_SET:
      case OP_LIST_LEN:
      case OP_LIST_CLEAR:
      case OP_LIST_DELETE:
      case OP_LIST_INSERT:
      case OP_LIST_CONTAINS:
        if (code[pc + 1] >= out.numLists) return LoadError::BAD_LIST;
        break;
      case OP_SYS: {
        const uint8_t id = code[pc + 1];
        if (!(SYSCALL_INFO[id].flags & SYSF_VALID)) return LoadError::BAD_SYSCALL;
        if (syscallSupported && !syscallSupported(id)) return LoadError::BAD_SYSCALL;
        break;
      }
      default:
        break;
    }
    pc += n;
  }
  auto isStart = [&](uint32_t a) { return a < out.codeLen && (starts[a >> 3] & (1u << (a & 7))); };

  // Pass 2: jump targets must land on instruction boundaries.
  for (uint32_t pc = 0; pc < out.codeLen; pc += OP_LENGTH[code[pc]]) {
    const uint8_t op = code[pc];
    if (op == OP_JMP || op == OP_JZ || op == OP_JNZ) {
      if (!isStart(rd16(code + pc + 1))) return LoadError::BAD_JUMP;
    }
  }

  // Script table.
  for (uint8_t i = 0; i < out.numScripts; ++i) {
    const uint8_t* e = buf + HEADER_SIZE + i * SCRIPT_ENTRY_SIZE;
    out.scripts[i].event = e[0];
    out.scripts[i].param = e[1];
    out.scripts[i].offset = rd16(e + 2);
    if (eventSupported && !eventSupported(e[0])) return LoadError::BAD_EVENT;
    if (!isStart(out.scripts[i].offset)) return LoadError::BAD_SCRIPT_OFFSET;
  }
  return LoadError::OK;
}

const char* programString(const Program& p, int32_t offset) {
  if (offset < 0 || offset >= (int32_t)p.stringsLen) return nullptr;
  return p.strings + offset;
}

}  // namespace elx
