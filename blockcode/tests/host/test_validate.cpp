// Validator tests: every malformed image must be rejected before it can run.
//   test_validate path/to/valid.elxb
#include <stdio.h>
#include <string.h>

#include "program.h"

using namespace elx;

static int failures = 0;
static void expect(const char* what, LoadError got, LoadError want) {
  bool ok = got == want;
  printf("%s %-28s got=%u want=%u\n", ok ? "PASS" : "FAIL", what, (unsigned)got, (unsigned)want);
  if (!ok) failures++;
}
static void fixCrc(uint8_t* b, size_t n) {
  uint32_t c = crc32(b, n - 4);
  memcpy(b + n - 4, &c, 4);
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  static uint8_t orig[8192], b[8192];
  FILE* f = fopen(argv[1], "rb");
  if (!f) return 2;
  size_t n = fread(orig, 1, sizeof orig, f);
  fclose(f);
  Program p;
  auto v1 = [](uint8_t id) { return (SYSCALL_INFO[id].flags & SYSF_V1) != 0; };

  memcpy(b, orig, n);
  expect("valid image", parseProgram(b, n, p, v1, nullptr), LoadError::OK);

  memcpy(b, orig, n); b[n - 1] ^= 0xFF;
  expect("corrupted crc", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_CRC);

  memcpy(b, orig, n); b[0] = 'X';
  expect("bad magic", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_MAGIC);

  memcpy(b, orig, n); b[4] = 99; fixCrc(b, n);
  expect("future version", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_VERSION);

  expect("truncated file", parseProgram(orig, n - 3, p, v1, nullptr), LoadError::BAD_LENGTH);
  expect("tiny file", parseProgram(orig, 10, p, v1, nullptr), LoadError::TOO_SMALL);

  const size_t code = HEADER_SIZE + orig[6] * SCRIPT_ENTRY_SIZE;
  memcpy(b, orig, n); b[code] = 0xEE; fixCrc(b, n);
  expect("invalid opcode", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_OPCODE);

  // Find first JMP/JZ/JNZ and aim it into the middle of an instruction.
  memcpy(b, orig, n);
  uint16_t clen = (uint16_t)(b[8] | (b[9] << 8));
  for (uint32_t pc = 0; pc < clen; pc += OP_LENGTH[b[code + pc]]) {
    uint8_t op = b[code + pc];
    if (op == OP_JMP || op == OP_JZ || op == OP_JNZ) {
      b[code + pc + 1] = 1; b[code + pc + 2] = 0;  // offset 1 = inside the first PUSH_I8
      break;
    }
  }
  fixCrc(b, n);
  expect("jump into operand", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_JUMP);

  memcpy(b, orig, n);
  for (uint32_t pc = 0; pc < clen; pc += OP_LENGTH[b[code + pc]])
    if (b[code + pc] == OP_LOAD_VAR) { b[code + pc + 1] = 31; break; }
  fixCrc(b, n);
  expect("variable out of range", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_VAR);

  memcpy(b, orig, n);
  for (uint32_t pc = 0; pc < clen; pc += OP_LENGTH[b[code + pc]])
    if (b[code + pc] == OP_SYS) { b[code + pc + 1] = SYS_I2C_WRITE; break; }  // "later" syscall
  fixCrc(b, n);
  expect("unsupported syscall", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_SYSCALL);

  memcpy(b, orig, n); b[HEADER_SIZE + 2] = 1; b[HEADER_SIZE + 3] = 0; fixCrc(b, n);
  expect("script offset mid-instr", parseProgram(b, n, p, v1, nullptr), LoadError::BAD_SCRIPT_OFFSET);

  printf("%s (%d failures)\n", failures ? "VALIDATOR TESTS FAILED" : "all validator tests passed", failures);
  return failures ? 1 : 0;
}
