// AUTO-GENERATED from spec/opcodes.json by tools/gen_opcodes.py. DO NOT EDIT.
#pragma once
#include <stdint.h>

namespace elx {

constexpr uint8_t BYTECODE_VERSION = 1;
constexpr uint32_t LIMIT_MAX_PROGRAM_BYTES = 4096;
constexpr uint32_t LIMIT_MAX_SCRIPTS = 8;
constexpr uint32_t LIMIT_MAX_VARS = 32;
constexpr uint32_t LIMIT_STACK_DEPTH = 16;
constexpr uint32_t LIMIT_NUM_SLOTS = 8;
constexpr uint32_t LIMIT_TICK_MS = 10;
constexpr uint32_t LIMIT_OP_BUDGET_PER_TICK = 500;
constexpr uint32_t LIMIT_MAX_LISTS = 4;
constexpr uint32_t LIMIT_LIST_CAPACITY = 64;

enum Op : uint8_t {
  OP_NOP = 0,
  OP_PUSH_I8 = 1,
  OP_PUSH_I16 = 2,
  OP_PUSH_F32 = 3,
  OP_POP = 4,
  OP_DUP = 5,
  OP_LOAD_VAR = 8,
  OP_STORE_VAR = 9,
  OP_CHANGE_VAR = 10,
  OP_ADD = 16,
  OP_SUB = 17,
  OP_MUL = 18,
  OP_DIV = 19,
  OP_MOD = 20,
  OP_NEG = 21,
  OP_ABS = 22,
  OP_ROUND = 23,
  OP_RANDOM = 24,
  OP_LT = 32,
  OP_GT = 33,
  OP_EQ = 34,
  OP_LE = 35,
  OP_GE = 36,
  OP_NE = 37,
  OP_AND = 38,
  OP_OR = 39,
  OP_NOT = 40,
  OP_JMP = 48,
  OP_JZ = 49,
  OP_JNZ = 50,
  OP_YIELD = 51,
  OP_END = 52,
  OP_STOP_ALL = 53,
  OP_WAIT = 54,
  OP_SYS = 64,
  OP_LIST_ADD = 72,
  OP_LIST_GET = 73,
  OP_LIST_SET = 74,
  OP_LIST_LEN = 75,
  OP_LIST_CLEAR = 76,
  OP_LIST_DELETE = 77,
  OP_LIST_INSERT = 78,
  OP_LIST_CONTAINS = 79,
};

// Instruction length in bytes (opcode + operands); 0 = invalid opcode.
static const uint8_t OP_LENGTH[256] = {
  1, 2, 3, 5, 1, 1, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
  3, 3, 3, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

enum Event : uint8_t {
  EV_START = 0,
  EV_TOUCHED = 1,
  EV_OBSTACLE = 2,
  EV_LINE = 3,
  EV_EDGE = 4,
  EV_TILTED = 5,
  EV_SHAKEN = 6,
  EV_REMOTE_BUTTON = 7,
  EV_LOUD = 8,
  EV_MAGNET = 9,
  EV_MESSAGE = 10,
  EV_OBJECT_NEAR = 11,
  EV_PICKED_UP = 12,
  EV_PUT_DOWN = 13,
  EV_LONG_PRESS = 14,
  EV_DOUBLE_TAP = 15,
  EV_BATTERY_LOW = 16,
};

enum Sys : uint8_t {
  SYS_DRIVE_TIMED = 16,
  SYS_TURN_DEGREES = 17,
  SYS_DRIVE = 18,
  SYS_SET_MOTORS = 19,
  SYS_STOP_MOTORS = 20,
  SYS_TURN_TO_HEADING = 21,
  SYS_SPIN = 22,
  SYS_DRIVE_ARC = 23,
  SYS_MOVE_UNTIL = 24,
  SYS_FOLLOW_LINE = 25,
  SYS_SET_STOP_MODE = 26,
  SYS_FACE_SHOW = 32,
  SYS_FACE_LOOK = 33,
  SYS_FACE_BLINK = 34,
  SYS_SHOW_TEXT = 35,
  SYS_SHOW_NUMBER = 36,
  SYS_FACE_CLEAR = 37,
  SYS_SHOW_ICON = 38,
  SYS_FACE_WINK = 39,
  SYS_FACE_ANIM = 40,
  SYS_DRAW_PIXEL = 41,
  SYS_DRAW_LINE = 42,
  SYS_DRAW_RECT = 43,
  SYS_DRAW_CIRCLE = 44,
  SYS_LIGHTS_ALL = 48,
  SYS_LIGHT_SET = 49,
  SYS_LIGHT_EFFECT = 50,
  SYS_LIGHTS_BRIGHTNESS = 51,
  SYS_LIGHTS_OFF = 52,
  SYS_RGB = 53,
  SYS_RANDOM_COLOR = 54,
  SYS_LIGHT_BAR = 55,
  SYS_PLAY_NOTE = 64,
  SYS_PLAY_SOUND = 65,
  SYS_REST = 66,
  SYS_SET_TEMPO = 67,
  SYS_PLAY_TONE = 68,
  SYS_STOP_SOUND = 69,
  SYS_PLAY_MELODY = 70,
  SYS_DISTANCE = 80,
  SYS_LINE_VALUE = 81,
  SYS_LINE_POSITION = 82,
  SYS_LINE_DETECTED = 83,
  SYS_EDGE_DETECTED = 84,
  SYS_FLOOR_DEPTH = 85,
  SYS_TILT = 86,
  SYS_IS_SHAKEN = 87,
  SYS_IS_TOUCHED = 88,
  SYS_SOUND_LEVEL = 89,
  SYS_TIMER = 90,
  SYS_RESET_TIMER = 91,
  SYS_HEADING = 92,
  SYS_MAGNET_NEAR = 93,
  SYS_MAGNET_STRENGTH = 94,
  SYS_OBJECT_NEAR = 95,
  SYS_JOYSTICK = 96,
  SYS_REMOTE_BUTTON = 97,
  SYS_REMOTE_BUZZ = 98,
  SYS_JOYSTICK_DRIVE = 99,
  SYS_REACT = 112,
  SYS_SET_MOOD = 113,
  SYS_MOOD = 114,
  SYS_DANCE = 115,
  SYS_SAY_HELLO = 116,
  SYS_ACT_SLEEPY = 117,
  SYS_BROADCAST = 128,
  SYS_LINE_ALL = 160,
  SYS_PICKED_UP = 161,
  SYS_UPSIDE_DOWN = 162,
  SYS_TURNED_ANGLE = 163,
  SYS_RESET_ANGLE = 164,
  SYS_FACING = 165,
  SYS_TOUCH_COUNT = 166,
  SYS_BATTERY_LEVEL = 167,
  SYS_CALIBRATE_LINE = 168,
  SYS_I2C_WRITE = 224,
  SYS_I2C_READ = 225,
  SYS_UART_SEND = 226,
  SYS_UART_AVAILABLE = 227,
  SYS_UART_READ = 228,
};

constexpr uint8_t SYSF_VALID = 1, SYSF_RETURNS = 2, SYSF_BLOCKING = 4, SYSF_V1 = 8;
struct SyscallInfo { uint8_t argc; uint8_t flags; };
static const SyscallInfo SYSCALL_INFO[256] = {
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {3,13}, {2,13}, {2,9}, {2,9}, {0,9}, {1,13}, {2,13}, {3,13},
  {3,13}, {2,13}, {1,9}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {1,9}, {1,9}, {0,9}, {1,9}, {1,9}, {0,9}, {1,9}, {0,13},
  {1,13}, {2,9}, {4,9}, {5,9}, {4,9}, {0,0}, {0,0}, {0,0},
  {1,9}, {2,9}, {2,9}, {1,9}, {0,9}, {3,11}, {0,11}, {3,9},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {2,13}, {1,13}, {1,13}, {1,9}, {2,13}, {0,9}, {1,13}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,11}, {1,11}, {0,11}, {1,11}, {1,11}, {1,11}, {1,11}, {0,11},
  {1,11}, {0,11}, {0,11}, {0,9}, {0,11}, {0,11}, {0,11}, {1,11},
  {1,11}, {1,11}, {1,9}, {1,13}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {1,13}, {1,9}, {0,11}, {1,13}, {0,13}, {0,13}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {1,9}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {1,11}, {0,11}, {0,11}, {0,11}, {0,9}, {1,11}, {0,11}, {0,11},
  {1,13}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {3,1}, {2,3}, {1,1}, {0,3}, {0,3}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
};

// Block dropdown values. Use as elx::enums::face::HAPPY (kept in their own
// namespace so names like 'face' or 'sound' don't clash with firmware objects).
namespace enums {
namespace drive { enum : uint8_t { FORWARD = 1, BACKWARD = 2 }; }
namespace turn { enum : uint8_t { LEFT = 1, RIGHT = 2 }; }
namespace look { enum : uint8_t { CENTER = 0, LEFT = 1, RIGHT = 2, UP = 3, DOWN = 4 }; }
namespace face { enum : uint8_t { NEUTRAL = 0, HAPPY = 1, SAD = 2, ANGRY = 3, SURPRISED = 4, SLEEPY = 5, LOVE = 6, DIZZY = 7 }; }
namespace effect { enum : uint8_t { RAINBOW = 1, PULSE = 2, BLINK = 3, CHASE = 4, POLICE = 5, FIRE = 6, SIGNAL_LEFT = 7, SIGNAL_RIGHT = 8 }; }
namespace sound { enum : uint8_t { BEEP = 1, HAPPY = 2, SAD = 3, ALARM = 4, LEVELUP = 5 }; }
namespace emotion { enum : uint8_t { NEUTRAL = 0, HAPPY = 1, SAD = 2, EXCITED = 3, SCARED = 4, ANGRY = 5, SLEEPY = 6, LOVE = 7 }; }
namespace zone { enum : uint8_t { ANY = 0, LEFT = 1, CENTER = 2, RIGHT = 3 }; }
namespace corner { enum : uint8_t { ANY = 0, FRONT_LEFT = 1, FRONT_RIGHT = 2, BACK_LEFT = 3, BACK_RIGHT = 4 }; }
namespace tilt { enum : uint8_t { FORWARD = 1, BACKWARD = 2, LEFT = 3, RIGHT = 4 }; }
namespace axis { enum : uint8_t { X = 1, Y = 2, PITCH = 1, ROLL = 2 }; }
namespace button { enum : uint8_t { A = 1, B = 2, STICK = 3 }; }
namespace pad { enum : uint8_t { ANY = 0, HEAD = 1 }; }
namespace until { enum : uint8_t { OBSTACLE = 1, LINE = 2, EDGE = 3 }; }
namespace stopmode { enum : uint8_t { BRAKE = 1, COAST = 2 }; }
namespace icon { enum : uint8_t { HEART = 1, STAR = 2, ARROW_UP = 3, ARROW_DOWN = 4, ARROW_LEFT = 5, ARROW_RIGHT = 6, CHECK = 7, CROSS = 8, MUSIC = 9, QUESTION = 10, SMILE = 11 }; }
namespace anim { enum : uint8_t { SLEEPING = 1, THINKING = 2, LAUGHING = 3 }; }
namespace melody { enum : uint8_t { BIRTHDAY = 1, TWINKLE = 2, VICTORY = 3, SIREN = 4 }; }
namespace lineall { enum : uint8_t { BLACK = 1, WHITE = 2 }; }
namespace compass { enum : uint8_t { NORTH = 1, EAST = 2, SOUTH = 3, WEST = 4 }; }
}  // namespace enums

#ifdef ELX_WITH_NAMES  // debug/PC builds only
static const char* const SYSCALL_NAMES[256] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "drive_timed", "turn_degrees", "drive", "set_motors", "stop_motors", "turn_to_heading", "spin", "drive_arc",
  "move_until", "follow_line", "set_stop_mode", nullptr, nullptr, nullptr, nullptr, nullptr,
  "face_show", "face_look", "face_blink", "show_text", "show_number", "face_clear", "show_icon", "face_wink",
  "face_anim", "draw_pixel", "draw_line", "draw_rect", "draw_circle", nullptr, nullptr, nullptr,
  "lights_all", "light_set", "light_effect", "lights_brightness", "lights_off", "rgb", "random_color", "light_bar",
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "play_note", "play_sound", "rest", "set_tempo", "play_tone", "stop_sound", "play_melody", nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "distance", "line_value", "line_position", "line_detected", "edge_detected", "floor_depth", "tilt", "is_shaken",
  "is_touched", "sound_level", "timer", "reset_timer", "heading", "magnet_near", "magnet_strength", "object_near",
  "joystick", "remote_button", "remote_buzz", "joystick_drive", nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "react", "set_mood", "mood", "dance", "say_hello", "act_sleepy", nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "broadcast", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "line_all", "picked_up", "upside_down", "turned_angle", "reset_angle", "facing", "touch_count", "battery_level",
  "calibrate_line", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "i2c_write", "i2c_read", "uart_send", "uart_available", "uart_read", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
static const char* const EVENT_NAMES[32] = {"start", "touched", "obstacle", "line", "edge", "tilted", "shaken", "remote_button", "loud", "magnet", "message", "object_near", "picked_up", "put_down", "long_press", "double_tap", "battery_low", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
#endif

enum class LoadError : uint8_t {
  OK = 0,
  TOO_SMALL = 1,
  BAD_MAGIC = 2,
  BAD_VERSION = 3,
  TOO_MANY_SCRIPTS = 4,
  TOO_MANY_VARS = 5,
  BAD_LENGTH = 6,
  BAD_CRC = 7,
  BAD_SCRIPT_OFFSET = 8,
  BAD_OPCODE = 9,
  BAD_JUMP = 10,
  BAD_VAR = 11,
  BAD_SYSCALL = 12,
  BAD_EVENT = 13,
  BAD_STRINGS = 14,
  TRUNCATED = 15,
  BAD_LIST = 16,
};
enum class VmError : uint8_t {
  NONE = 0,
  STACK_OVERFLOW = 1,
  STACK_UNDERFLOW = 2,
  BAD_OPCODE = 3,
  BAD_SYSCALL = 4,
  BAD_STRING = 5,
};

}  // namespace elx
