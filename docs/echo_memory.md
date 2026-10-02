# Game: Echo Memory

The game echo memory is intended to test the users memory power. In the game the robotoy illuminates some colours one after the other in a sequence. The user have to remember the colours in correct sequence order. Each colour represents a assigned action like honk, pet, up, down, left, right. The user has to then perform those actions in correct sequence order.

Echo Memory is implemented directly in the ESP32-S3 firmware as a dedicated game engine.

The actions are:

| Action ID | Action | Fixed color |
| --------: | ------ | ----------- |
|         0 | UP     | Red         |
|         1 | RIGHT  | Yellow      |
|         2 | DOWN   | Green       |
|         3 | LEFT   | Blue        |
|         4 | PET    | Purple      |
|         5 | HONK   | White       |

Levels 1–3 use only the first four directional actions.
Levels 4–6 use all six actions.

Easy: In levels 1 to 3, colours are illuminated on its assigned region only, the whole strip is not illuminated.

Medium: In level 4 and 5, whole strip illuminates the colour sequences, user must remember the colours in sequence and then do the actions in that sequence.

Hard: In level 6, the color-action mapping is assigned dynamically. The colour assigned is from the above listed colours only. No external colours are used. The dynamically assigned colour mapping is sent to frontend. Frontend displays the colour-action mapping for some time. User have to study the mapping within a limited time period like 5 seconds.

The game uses maximum sequence length 7.

level 1 generate 4 sequences.
level 2 generate 5 sequences.
level 3 generate 6 sequences.

Levels 1 to 3 are easy.

level 4 generate 5 sequences.
level 5 generate 6 sequences.

Levels 4 and 5 are medium.

level 6 is hard, generate 5 sequences, otherwise it would become impossible to win.

Level table:

| Level | Sequence length | Flash interval | Actions | LED mode   | Mapping |
| ----: | --------------: | -------------: | ------: | ---------- | ------- |
|     1 |               4 |          3.0 s |       4 | Regional   | Fixed   |
|     2 |               5 |          1.5 s |       4 | Regional   | Fixed   |
|     3 |               6 |          1.5 s |       4 | Regional   | Fixed   |
|     4 |               5 |          3.0 s |       6 | Full strip | Fixed   |
|     5 |               6 |          1.5 s |       6 | Full strip | Fixed   |
|     6 |               5 |          1.5 s |       6 | Full strip | Dynamic |

---

Levels 1–5 use the fixed identity mapping:

```text
UP     → Red
RIGHT  → Yellow
DOWN   → Green
LEFT   → Blue
PET    → Purple
HONK   → White
```

Level 6 changes the color mapping.

The six colors remain:

```text
Red
Yellow
Green
Blue
Purple
White
```

but their assignment to the six actions is randomized.

The firmware first initializes the identity mapping and then performs a Fisher-Yates-style shuffle:

```cpp
for (uint8_t i = 5; i > 0; i--) {
  uint8_t j = random(0, i + 1);

  uint8_t tmp = echoMemoryActionColor[i];
  echoMemoryActionColor[i] = echoMemoryActionColor[j];
  echoMemoryActionColor[j] = tmp;
}
```

This creates a one-to-one mapping.

For example, one Level 6 game could produce:

```text
UP     → Blue
RIGHT  → White
DOWN   → Red
LEFT   → Purple
PET    → Yellow
HONK   → Green
```

The actual mapping is generated randomly each time Level 6 starts.

## Levels 1–3: Regional LEDs

Only the corresponding physical region is illuminated.

The directional mapping is:

```text
UP     → FRONT
RIGHT  → RIGHT
DOWN   → BACK
LEFT   → LEFT
```

The colors are:

```text
UP     → Red
RIGHT  → Yellow
DOWN   → Green
LEFT   → Blue
```

This allows the player to associate both direction and color with the signal.

---

## Levels 4–6: Full Strip

For Levels 4–6, the complete external NeoPixel strip is illuminated with the color assigned to the action.

The color values are:

```text
Red     → RGB(255, 0, 0)
Yellow  → RGB(255, 180, 0)
Green   → RGB(0, 255, 0)
Blue    → RGB(0, 0, 255)
Purple  → RGB(150, 0, 255)
White   → RGB(255, 255, 255)
```

There should be a flash interval between illuminating colours in sequence, otherwise if same colours are illuminated in adjacent sequence order, user will not be able to differentiate without the interval gap.

## JSON Contract

### Game start

Mobile → ESP32:

```json
{
  "v": 1,
  "type": "command",
  "command": "game_start",
  "id": "game-001",
  "payload": {
    "gameId": "echo_memory",
    "level": 1
  }
}
```

ESP32 response:

```json
{
  "v": 1,
  "type": "response",
  "response": "game_started",
  "id": "game-001",
  "payload": {
    "gameId": "echo_memory",
    "level": 1,
    "status": "started",
    "mapping": {
      "up": "red",
      "right": "yellow",
      "down": "green",
      "left": "blue"
    }
  }
}
```

The response should be contain real mapping values, especially for 6th level colour-action mapping happens dynamically at the start of a level.

The protocol must not prevent future levels from being added.

After recieving the mapping from the response the mapping is shown to the user in UI untill the user clicks the 'GO' button. The game shouldn't start the execution untill user sends the run_seq command. On clicking the GO button the run_seq command is sent and the mapping table is disappeared from UI and the control buttons are shown.

Mobile -> ESP32

```json
{
  "v": 1,
  "type": "command",
  "command": "run_seq",
  "id": "game-001",
  "payload": {
    "gameId": "echo_memory",
    "level": 1
  }
}
```

---

### Game events

Mobile -> Robot answering uses same joystick input to answer:

```json
{
  "v": 1,
  "type": "input",
  "id": "input-1001",
  "ts": 123456789,
  "payload": {
    "inputType": "joystick",
    "dir": "up"
  }
}
```

```json
{
  "v": 1,
  "type": "command",
  "command": "honk",
  "id": "command-1001",
  "ts": 123456789
}
```

Robot -> Mobile after each task is answered by the user:

```json
{
  "v": 1,
  "type": "response",
  "response": "game_feedback",
  "gameId": "echo_memory",
  "level": 1,
  "taskId": 1,
  "correct": true,
  "correctCount": 1
}
```

---

### Game result

Robot → Mobile after a mobile game completes, or later during result synchronization for a remote-only game:

```json
{
  "v": 1,
  "type": "response",
  "response": "game_result",
  "gameId": "echo_memory",
  "level": 1,
  "score": 5,
  "stars": 3,
  "tasksCompleted": 5,
  "tasksTotal": 5
}
```

---

### Game abort

```json
{
  "v": 1,
  "type": "command ",
  "command": "game_abort",
  "id": "abort-001",
  "payload": {}
}
```

ESP32 must safely stop active game movement and return to the
appropriate non-game state.

---

## ESP-NOW Compatibility

While playing the game using esp-now remote, map the normal directional inputs to appropriate regions and validate the answer. For the honk functionality use the btn1's input from remote.
Show the colour-action mapping on the OLED Display on start of level. When user again presses the same select button(i.e, "sw") the level start running. This behaviour is similar to that of Mobile based implementation.

## Common Rule

While playing the game via any controller(Mobile or ESP-NOW remote), the input shouldn't be taken while the game is flashing the sequence of lights. After flashing all sequence of lights in that level, pause for 3 sec and then user can start giving input one by one. On each input user is giving robotoy will respond if the input is correct in that sequence or not. Finally the result is published.

---

## Legacy Echo Memory Game Engine

This legacy implementation snippets are for logic reference only. Donot implement this directly. The final updated logic behaviour is stated above:

```cpp
// ========================================
// ECHO MEMORY GAME ENGINE — LEVELS 1-6
// ========================================
// Levels 1-3 use the fixed directional mapping (regions):
// UP = Red, RIGHT = Yellow, DOWN = Green, LEFT = Blue.
// Levels 4-5 use the fixed action mapping (full strip):
// UP = Red, RIGHT = Yellow, DOWN = Green, LEFT = Blue, PET = Purple, HONK = White.
// Level 6 uses dynamic 1-to-1 color-to-action mapping across all 6 actions (full strip).
// Level 1: 4 steps, 3.0s flash interval, 3.0s wait, 4 actions, regional LEDs
// Level 2: 5 steps, 1.5s flash interval, 3.0s wait, 4 actions, regional LEDs
// Level 3: 6 steps, 1.5s flash interval, 3.0s wait, 4 actions, regional LEDs
// Level 4: 5 steps, 3.0s flash interval, 3.0s wait, 6 actions, full strip
// Level 5: 6 steps, 1.5s flash interval, 3.0s wait, 6 actions, full strip
// Level 6: 7 steps, 1.5s flash interval, 3.0s wait, 6 actions, full strip, dynamic color mapping
// The ESP32 owns the sequence, playback timing, input validation and score.
enum EchoMemoryAction {
  EMA_UP = 0,     // Red in fixed mapping
  EMA_RIGHT = 1,  // Yellow in fixed mapping
  EMA_DOWN = 2,   // Green in fixed mapping
  EMA_LEFT = 3,   // Blue in fixed mapping
  EMA_PET = 4,    // Purple in fixed mapping
  EMA_HONK = 5    // White RGB(255, 255, 255) in fixed mapping
};

enum EchoMemoryState {
  EMS_IDLE,
  EMS_MAPPING,
  EMS_FLASHING,
  EMS_WAIT,
  EMS_INPUT,
  EMS_RESULT
};

EchoMemoryState echoMemoryState = EMS_IDLE;
int echoMemoryLevel = 0;
const uint8_t ECHO_MEMORY_MAX_SEQUENCE_LENGTH = 8;
uint8_t echoMemorySequence[ECHO_MEMORY_MAX_SEQUENCE_LENGTH];
uint8_t echoMemorySequenceLength = 4;
unsigned int echoMemoryFlashInterval = 3000;
uint8_t echoMemoryActionCount = 4;
bool echoMemoryFullStrip = false;
// Dynamic 1-to-1 mapping from action ID (0..5) to color ID (0..5)
// Color IDs: 0: Red, 1: Yellow, 2: Green, 3: Blue, 4: Purple, 5: White
uint8_t echoMemoryActionColor[6] = {0, 1, 2, 3, 4, 5};
const unsigned long ECHO_MEMORY_GAP_MS = 400;
bool echoMemoryFlashInGap = false;
uint8_t echoMemoryFlashIndex = 0;
unsigned long echoMemoryPhaseUntil = 0;
unsigned long echoMemoryFeedbackUntil = 0;
uint8_t echoMemoryInputIndex = 0;
uint8_t echoMemoryWrongCount = 0;

void echoMemoryHandlePet();

void echoMemoryStopOutputs() {
  setStripColor(0, 0, 0);
  noTone(PIN_BUZZER);
}

void echoMemoryShowOLED(const char *title, const char *detail = nullptr) {
  if (!oledPresent)
    return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 4);
  display.println(title);

  if (detail != nullptr) {
    display.setTextSize(1);
    display.setCursor(0, 34);
    display.println(detail);
  }

  drawBatteryOverlay();
  display.display();
}

void echoMemoryPlayHonkSound() {
  tone(PIN_BUZZER, 400, 100);
  delay(110);
  tone(PIN_BUZZER, 550, 100);
  delay(110);
  tone(PIN_BUZZER, 750, 180);
  delay(190);
  noTone(PIN_BUZZER);
}

void echoMemoryShowAction(uint8_t action) {
  if (echoMemoryFullStrip) {
    uint8_t colorIdx = echoMemoryActionColor[action];
    switch (colorIdx) {
    case 0:
      setStripColor(255, 0, 0);     // Red
      break;
    case 1:
      setStripColor(255, 180, 0);   // Yellow
      break;
    case 2:
      setStripColor(0, 255, 0);     // Green
      break;
    case 3:
      setStripColor(0, 0, 255);     // Blue
      break;
    case 4:
      setStripColor(150, 0, 255);   // Purple
      break;
    case 5:
      setStripColor(255, 255, 255); // White RGB(255, 255, 255)
      break;
    default:
      return;
    }
  } else {
    clearStripBuffer();
    // Direction/index mapping follows joystickToDirection():
    // 0 = UP/front, 1 = RIGHT, 2 = DOWN/back, 3 = LEFT.
    switch (action) {
    case EMA_UP:
      setRegionColor(REGION_FRONT, 255, 0, 0);       // Red
      break;
    case EMA_RIGHT:
      setRegionColor(REGION_RIGHT, 255, 180, 0);     // Yellow
      break;
    case EMA_DOWN:
      setRegionColor(REGION_BACK, 0, 255, 0);        // Green
      break;
    case EMA_LEFT:
      setRegionColor(REGION_LEFT, 0, 0, 255);        // Blue
      break;
    default:
      return;
    }
  }
  strip.show();
}

void echoMemoryShowDirection(uint8_t direction) {
  echoMemoryShowAction(direction);
}

void echoMemoryBeginInput() {
  echoMemoryStopOutputs();
  echoMemoryInputIndex = 0;
  echoMemoryWrongCount = 0;
  echoMemoryState = EMS_INPUT;
  if (echoMemoryLevel >= 4) {
    echoMemoryShowOLED("REPEAT!", "D-pad, Honk, Pet");
  } else {
    echoMemoryShowOLED("REPEAT!", "Use D-pad");
  }

  JsonDocument phase;
  phase["type"] = "phase";
  phase["game"] = "echo-memory";
  phase["level"] = echoMemoryLevel;
  phase["phase"] = "input";
  phase["length"] = echoMemorySequenceLength;
  sendJson(phase);
}

void echoMemoryFinishLevel() {
  echoMemoryStopOutputs();

  int correctCount = echoMemorySequenceLength - echoMemoryWrongCount;
  int scorePercent = (correctCount * 100) / echoMemorySequenceLength;
  int stars = 0;
  if (scorePercent >= 90)
    stars = 3;
  else if (scorePercent >= 70)
    stars = 2;
  else if (scorePercent >= 50)
    stars = 1;

  echoMemoryState = EMS_RESULT;

  JsonDocument result;
  result["type"] = "response";
  result["game"] = "echo-memory";
  result["level"] = echoMemoryLevel;
  result["score"] = scorePercent / 100.0f;
  result["scorePercent"] = scorePercent;
  result["stars"] = stars;
  result["correct"] = correctCount;
  result["total"] = echoMemorySequenceLength;
  sendJson(result);

  if (scorePercent >= 50) {
    playTone(2400, 180);
  } else {
    playTone(900, 220);
  }

  if (oledPresent) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ECHO MEMORY");
    display.setTextSize(2);
    display.setCursor(8, 16);
    display.printf("%d%%", scorePercent);
    display.setTextSize(1);
    display.setCursor(8, 43);
    display.printf("Stars: %d/3", stars);
    drawBatteryOverlay();
    display.display();
  }

  // Keep the result visible briefly; updateEchoMemory() returns to idle.
  echoMemoryPhaseUntil = millis() + 2000;
}

void echoMemoryStartLevel(int level) {
  if (level < 1 || level > 6) {
    sendError("Echo Memory levels 1-6 are supported");
    return;
  }

  currentGame = GAME_ECHO_MEMORY;
  echoMemoryLevel = level;
  echoMemoryStopOutputs();
  if (level == 1) {
    echoMemorySequenceLength = 4;
    echoMemoryFlashInterval = 3000;
    echoMemoryActionCount = 4;
    echoMemoryFullStrip = false;
  } else if (level == 2) {
    echoMemorySequenceLength = 5;
    echoMemoryFlashInterval = 1500;
    echoMemoryActionCount = 4;
    echoMemoryFullStrip = false;
  } else if (level == 3) {
    echoMemorySequenceLength = 6;
    echoMemoryFlashInterval = 1500;
    echoMemoryActionCount = 4;
    echoMemoryFullStrip = false;
  } else if (level == 4) {
    echoMemorySequenceLength = 5;
    echoMemoryFlashInterval = 3000;
    echoMemoryActionCount = 6;
    echoMemoryFullStrip = true;
  } else if (level == 5) {
    echoMemorySequenceLength = 6;
    echoMemoryFlashInterval = 1500;
    echoMemoryActionCount = 6;
    echoMemoryFullStrip = true;
  } else if (level == 6) {
    echoMemorySequenceLength = 7;
    echoMemoryFlashInterval = 1500;
    echoMemoryActionCount = 6;
    echoMemoryFullStrip = true;
  }

  // Setup color mapping:
  // L1-L5: fixed identity mapping (UP=Red, RIGHT=Yellow, DOWN=Green, LEFT=Blue, PET=Purple, HONK=White)
  // L6: dynamic 1-to-1 bijection (Fisher-Yates shuffle across the 6 colors)
  for (uint8_t i = 0; i < 6; i++) {
    echoMemoryActionColor[i] = i;
  }
  if (level == 6) {
    for (uint8_t i = 5; i > 0; i--) {
      uint8_t j = random(0, i + 1);
      uint8_t tmp = echoMemoryActionColor[i];
      echoMemoryActionColor[i] = echoMemoryActionColor[j];
      echoMemoryActionColor[j] = tmp;
    }
  }

  echoMemoryFlashIndex = 0;
  echoMemoryFlashInGap = false;
  echoMemoryInputIndex = 0;
  echoMemoryWrongCount = 0;

  // Generate the complete sequence once. It is never sent to the web app.
  for (uint8_t i = 0; i < echoMemorySequenceLength; i++) {
    echoMemorySequence[i] = random(0, echoMemoryActionCount);
  }

  if (level == 6) {
    echoMemoryState = EMS_MAPPING;
    echoMemoryPhaseUntil = millis() + 5000;

    char oledTitle[16];
    snprintf(oledTitle, sizeof(oledTitle), "ECHO L%d", echoMemoryLevel);
    echoMemoryShowOLED(oledTitle, "Study mapping");

    JsonDocument phase;
    phase["type"] = "phase";
    phase["game"] = "echo-memory";
    phase["level"] = echoMemoryLevel;
    phase["phase"] = "mapping";
    phase["durationMs"] = 5000;
    const char *colNames[6] = {"red", "yellow", "green", "blue", "purple", "white"};
    const char *actNames[6] = {"up", "right", "down", "left", "pet", "honk"};
    JsonObject mObj = phase["mapping"].to<JsonObject>();
    for (uint8_t a = 0; a < 6; a++) {
      mObj[actNames[a]] = colNames[echoMemoryActionColor[a]];
    }
    sendJson(phase);
  } else {
    echoMemoryState = EMS_FLASHING;

    char oledTitle[16];
    snprintf(oledTitle, sizeof(oledTitle), "ECHO L%d", echoMemoryLevel);
    echoMemoryShowOLED(oledTitle, "Watch carefully");

    echoMemoryShowAction(echoMemorySequence[0]);
    echoMemoryPhaseUntil = millis() + (echoMemoryFlashInterval - ECHO_MEMORY_GAP_MS);

    JsonDocument phase;
    phase["type"] = "phase";
    phase["game"] = "echo-memory";
    phase["level"] = echoMemoryLevel;
    phase["phase"] = "flash";
    phase["index"] = 0;
    phase["length"] = echoMemorySequenceLength;
    sendJson(phase);
  }
}

void echoMemoryAnswerAction(int action) {
  if (echoMemoryState != EMS_INPUT)
    return;
  if (action < 0 || action >= echoMemoryActionCount)
    return;
  if ((long)(millis() - echoMemoryFeedbackUntil) < 0)
    return;

  // When HONK action is received, play short distinct horn sound first
  if (action == EMA_HONK) {
    echoMemoryPlayHonkSound();
  }

  bool correct = (action == echoMemorySequence[echoMemoryInputIndex]);

  if (correct) {
    playTone(2400, 100);
    setStripColor(0, 80, 0);
  } else {
    echoMemoryWrongCount++;
    playTone(500, 250);
    setStripColor(80, 0, 0);
  }

  JsonDocument feedback;
  feedback["type"] = "input_result";
  feedback["game"] = "echo-memory";
  feedback["level"] = echoMemoryLevel;
  feedback["index"] = echoMemoryInputIndex;
  feedback["correct"] = correct;
  feedback["score"] = ((int)(echoMemorySequenceLength - echoMemoryWrongCount) * 100) /
                      echoMemorySequenceLength;
  sendJson(feedback);

  echoMemoryInputIndex++;
  echoMemoryFeedbackUntil = millis() + 300;

  if (echoMemoryInputIndex >= echoMemorySequenceLength) {
    // Keep the correct/incorrect feedback visible briefly
    delay(300);
    echoMemoryFinishLevel();
  }
}

void echoMemoryAnswerDirection(int direction) {
  echoMemoryAnswerAction(direction);
}

void echoMemoryHandleInput(JsonDocument &doc) {
  if (echoMemoryState != EMS_INPUT)
    return;

  int action = -1;

  if (doc.containsKey("action")) {
    const char *act = doc["action"] | "";
    if (strcasecmp(act, "up") == 0)
      action = EMA_UP;
    else if (strcasecmp(act, "right") == 0)
      action = EMA_RIGHT;
    else if (strcasecmp(act, "down") == 0)
      action = EMA_DOWN;
    else if (strcasecmp(act, "left") == 0)
      action = EMA_LEFT;
    else if (strcasecmp(act, "pet") == 0)
      action = EMA_PET;
    else if (strcasecmp(act, "honk") == 0)
      action = EMA_HONK;
  } else if (doc.containsKey("dir")) {
    const char *dir = doc["dir"] | "";
    if (strcasecmp(dir, "up") == 0)
      action = EMA_UP;
    else if (strcasecmp(dir, "right") == 0)
      action = EMA_RIGHT;
    else if (strcasecmp(dir, "down") == 0)
      action = EMA_DOWN;
    else if (strcasecmp(dir, "left") == 0)
      action = EMA_LEFT;
    else if (strcasecmp(dir, "honk") == 0)
      action = EMA_HONK;
    else if (strcasecmp(dir, "pet") == 0)
      action = EMA_PET;
  } else if (doc.containsKey("x") && doc.containsKey("y")) {
    int x = constrain((int)(doc["x"] | 512), 0, 1023);
    int y = constrain((int)(doc["y"] | 512), 0, 1023);
    action = joystickToDirection(x, y);
  }

  if (action >= 0) {
    echoMemoryAnswerAction(action);
  } else {
    sendError("Echo Memory input needs dir, action, or x/y");
  }
}

void echoMemoryHandlePet() {
  if (currentGame == GAME_ECHO_MEMORY && echoMemoryState == EMS_INPUT) {
    if (echoMemoryLevel >= 4) {
      echoMemoryAnswerAction(EMA_PET);
    }
  }
}

void echoMemoryAbort() {
  echoMemoryStopOutputs();
  echoMemoryState = EMS_IDLE;
  currentGame = GAME_NONE;
  echoMemoryLevel = 0;
  echoMemoryFlashInGap = false;
  challengeShowIdle();

  JsonDocument response;
  response["type"] = "aborted";
  response["game"] = "echo-memory";
  sendJson(response);
}

void updateEchoMemory() {
  unsigned long now = millis();

  if (echoMemoryState == EMS_MAPPING) {
    if ((long)(now - echoMemoryPhaseUntil) >= 0) {
      echoMemoryState = EMS_FLASHING;
      echoMemoryFlashIndex = 0;
      echoMemoryFlashInGap = false;

      char oledTitle[16];
      snprintf(oledTitle, sizeof(oledTitle), "ECHO L%d", echoMemoryLevel);
      echoMemoryShowOLED(oledTitle, "Watch carefully");

      echoMemoryShowAction(echoMemorySequence[0]);
      echoMemoryPhaseUntil = now + (echoMemoryFlashInterval - ECHO_MEMORY_GAP_MS);

      JsonDocument phase;
      phase["type"] = "phase";
      phase["game"] = "echo-memory";
      phase["level"] = echoMemoryLevel;
      phase["phase"] = "flash";
      phase["index"] = 0;
      phase["length"] = echoMemorySequenceLength;
      sendJson(phase);
    }
  } else if (echoMemoryState == EMS_FLASHING) {
    if ((long)(now - echoMemoryPhaseUntil) >= 0) {
      if (!echoMemoryFlashInGap) {
        // Step LED ON finished -> enter OFF gap so repeated directions are visually distinct
        echoMemoryStopOutputs();
        echoMemoryFlashInGap = true;
        echoMemoryPhaseUntil = now + ECHO_MEMORY_GAP_MS;
      } else {
        // OFF gap finished -> advance to next step or wait phase
        echoMemoryFlashInGap = false;
        echoMemoryFlashIndex++;

        if (echoMemoryFlashIndex >= echoMemorySequenceLength) {
          echoMemoryStopOutputs();
          echoMemoryState = EMS_WAIT;
          echoMemoryPhaseUntil = now + 3000;
          echoMemoryShowOLED("WAIT...", "Get ready");

          JsonDocument phase;
          phase["type"] = "phase";
          phase["game"] = "echo-memory";
          phase["level"] = echoMemoryLevel;
          phase["phase"] = "wait";
          phase["durationMs"] = 3000;
          sendJson(phase);
        } else {
          echoMemoryShowAction(echoMemorySequence[echoMemoryFlashIndex]);
          echoMemoryPhaseUntil = now + (echoMemoryFlashInterval - ECHO_MEMORY_GAP_MS);

          JsonDocument phase;
          phase["type"] = "phase";
          phase["game"] = "echo-memory";
          phase["level"] = echoMemoryLevel;
          phase["phase"] = "flash";
          phase["index"] = echoMemoryFlashIndex;
          phase["length"] = echoMemorySequenceLength;
          sendJson(phase);
        }
      }
    }
  } else if (echoMemoryState == EMS_WAIT) {
    if ((long)(now - echoMemoryPhaseUntil) >= 0) {
      echoMemoryBeginInput();
    }
  } else if (echoMemoryState == EMS_INPUT) {
    if (echoMemoryFeedbackUntil != 0 && (long)(now - echoMemoryFeedbackUntil) >= 0) {
      echoMemoryStopOutputs();
      echoMemoryFeedbackUntil = 0;
    }
  } else if (echoMemoryState == EMS_RESULT) {
    if ((long)(now - echoMemoryPhaseUntil) >= 0) {
      echoMemoryState = EMS_IDLE;
      currentGame = GAME_NONE;
      echoMemoryLevel = 0;
      challengeShowIdle();
    }
  }
}

```
