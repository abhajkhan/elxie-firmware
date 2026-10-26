#include "LocalMenu.h"
#include "RobotController.h"
#include <Arduino.h>

#include "../blocks/blocks.h"
#include "../blocks/face.h"

// Home screen: one icon at a time, joystick left / right, stick press = select, B = back.
static const char *MAIN_MENU_ITEMS[] = {
    "Games",
    "Free Ride",
    "My Programs"};
static const uint8_t MAIN_MENU_ICONS[] = {homeicon::GAMES, homeicon::FREE_RIDE, homeicon::MY_PROGRAMS};
static const uint8_t MAIN_MENU_COUNT = 3;

static const char *GAME_NAMES[] = {
    "Colour Quest",
    "Echo Memory",
    "Reflex Dash",
    "Driving Pro",
    "Inverted Drive"};
static const uint8_t GAME_ICONS[] = {homeicon::COLOUR_QUEST, homeicon::ECHO_MEMORY, homeicon::REFLEX_DASH,
                                     homeicon::DRIVING_PRO, homeicon::INVERTED_DRIVE};
static const GameId GAME_IDS[] = {
    GameId::COLOR_QUEST,
    GameId::ECHO_MEMORY,
    GameId::REFLEX_DASH,
    GameId::DRIVING_PRO,
    GameId::INVERTED_DRIVE};
static const uint8_t GAME_COUNT = 5;
static const uint8_t LEVEL_COUNT = 6;

static const unsigned long HOME_MENU_TIMEOUT_MS = 15000; // back to the eyes after this without input
static const uint8_t EYES_NEUTRAL = 0;                   // = elx::enums::face::NEUTRAL

void LocalMenu::begin(DisplayController *display, RobotController *controller)
{
    pDisplay = display;
    pRobotController = controller;
    reset();
}

void LocalMenu::reset()
{
    currentScreen = MenuScreen::MAIN_MENU;
    selectedIndex = 0;
    selectedGameIdx = 0;
    selectedLevel = 1;
}

bool LocalMenu::inMenuScreens() const
{
    return currentScreen == MenuScreen::MAIN_MENU || currentScreen == MenuScreen::GAME_SELECT ||
           currentScreen == MenuScreen::LEVEL_SELECT;
}

void LocalMenu::wake()
{
    lastInputMs = millis();
    if (!menuVisible)
    {
        menuVisible = true;
        dirty = true;
    }
}

void LocalMenu::setFaceOwner(bool on)
{
    // Blocks mode switches the face on and off by itself while this code is not running,
    // so check the real state (a stale "owned" left the screen frozen on the Blocks list).
    if (faceOwned && !face.isActive())
        faceOwned = false;
    if (on == faceOwned)
        return;
    face.setActive(on);
    faceOwned = on;
    shownEyes = 0xFF;
    dirty = true;
}

void LocalMenu::update(bool batteryLow, bool remoteConnected, bool appConnected, bool pairing, bool blush)
{
    const TopLevelState mode = pRobotController ? pRobotController->getState().mode : TopLevelState::IDLE;
    if (pairing)
    {
        setFaceOwner(false); // the pairing screen draws directly
        return;
    }
    if (mode == TopLevelState::BLOCKS)
    {
        faceOwned = false; // the block engine owns the face (and gives it back when it leaves)
        return;
    }
    if (mode == TopLevelState::RC || mode == TopLevelState::GAME)
    {
        setFaceOwner(false); // the game code draws its own screens
        if (mode == TopLevelState::RC && currentScreen == MenuScreen::FREE_RIDE_ACTIVE && remoteConnected)
            drawFreeRideScreen(batteryLow);
        return;
    }

    // ---- robot idle: home screen
    const unsigned long now = millis();
    if (!inMenuScreens())
    { // a game / Free Ride just ended: come back to its menu
        const bool fromGame = currentScreen != MenuScreen::FREE_RIDE_ACTIVE;
        currentScreen = fromGame ? MenuScreen::GAME_SELECT : MenuScreen::MAIN_MENU;
        selectedIndex = fromGame ? selectedGameIdx : 0;
        menuVisible = remoteConnected;
        lastInputMs = now;
        dirty = true;
    }
    if (menuVisible && (blush || !remoteConnected || appConnected || now - lastInputMs > HOME_MENU_TIMEOUT_MS))
    {
        menuVisible = false;
        reset();
    }
    setFaceOwner(true);
    if (menuVisible)
    {
        if (dirty)
        {
            dirty = false;
            shownEyes = 0xFF;
            drawCarousel();
        }
        return;
    }
    (void)batteryLow; // the eyes stay normal: the low-battery beep and the app show the battery
    const uint8_t eyes = blush ? (uint8_t)FACE_BLUSH : EYES_NEUTRAL; // blushing while petted
    if (eyes != shownEyes)
    {
        shownEyes = eyes;
        dirty = true; // the carousel must be redrawn when it comes back
        face.idleEyes(eyes);
    }
}

void LocalMenu::drawCarousel()
{
    switch (currentScreen)
    {
    case MenuScreen::MAIN_MENU:
        face.carousel("Elxie", MAIN_MENU_ICONS[selectedIndex], MAIN_MENU_ITEMS[selectedIndex], selectedIndex,
                      MAIN_MENU_COUNT);
        break;
    case MenuScreen::GAME_SELECT:
        face.carousel("Games", GAME_ICONS[selectedIndex], GAME_NAMES[selectedIndex], selectedIndex, GAME_COUNT);
        break;
    case MenuScreen::LEVEL_SELECT:
    {
        char num[4];
        snprintf(num, sizeof num, "%u", (unsigned)(selectedIndex + 1));
        face.carousel(GAME_NAMES[selectedGameIdx], homeicon::LEVEL, num, selectedIndex, LEVEL_COUNT);
        break;
    }
    default:
        break;
    }
}

void LocalMenu::handleInput(const InputEvent &event)
{
    if (event.source != InputSource::ESP_NOW)
        return;

    // Home screen: the first remote input only wakes the menu from the eyes.
    if (inMenuScreens())
    {
        const bool joyMoved = event.type == InputEventType::JOYSTICK &&
                              (abs(event.data.joystick.x) > 80 || abs(event.data.joystick.y) > 80);
        const bool pressed = event.type == InputEventType::ACTION_SELECT || event.type == InputEventType::BACK ||
                             (event.type == InputEventType::BUTTON && event.data.button.pressed);
        if (!menuVisible)
        {
            if (joyMoved || pressed)
            {
                wake();
                navLatched = joyMoved; // this push must not also move the menu
            }
            return;
        }
        if (joyMoved || pressed)
            lastInputMs = millis();
    }

    // Handle double-press on btn2 (BACK) to exit Free Ride or Game Mode
    if (event.type == InputEventType::BACK)
    {
        unsigned long now = millis();
        if (now - lastBtn2ClickMs < 400)
        {
            // Double press detected! Exit active Free Ride or Game Mode to Main Menu
            if (currentScreen == MenuScreen::FREE_RIDE_ACTIVE || currentScreen == MenuScreen::GAME_ACTIVE)
            {
                currentScreen = MenuScreen::MAIN_MENU;
                selectedIndex = 0;
                if (pRobotController)
                {
                    RobotCommand cmd;
                    cmd.type = CommandType::EXIT_RC;
                    cmd.source = InputSource::ESP_NOW;
                    pRobotController->handleCommand(cmd);

                    RobotCommand abortCmd;
                    abortCmd.type = CommandType::ABORT_GAME;
                    abortCmd.source = InputSource::ESP_NOW;
                    pRobotController->handleCommand(abortCmd);
                }
                lastBtn2ClickMs = 0;
                return;
            }
        }
        lastBtn2ClickMs = now;

        // Single press BACK handling
        if (currentScreen != MenuScreen::FREE_RIDE_ACTIVE && currentScreen != MenuScreen::GAME_ACTIVE)
        {
            goBack();
        }
        return;
    }

    // Handle sw (ACTION_SELECT / Joystick Centre Button) for selection
    if (event.type == InputEventType::ACTION_SELECT)
    {
        selectCurrentItem();
        return;
    }

    // Handle btn1 (BUTTON buttonId=1) in Free Ride Mode to illuminate random LED colors
    if (event.type == InputEventType::BUTTON && event.data.button.buttonId == 1 && event.data.button.pressed)
    {
        if (currentScreen == MenuScreen::FREE_RIDE_ACTIVE && pRobotController)
        {
            static const uint8_t COLORS[][3] = {
                {255, 0, 0},    // Red
                {0, 255, 0},    // Green
                {0, 0, 255},    // Blue
                {255, 255, 0},  // Yellow
                {255, 0, 255},  // Purple
                {0, 255, 255},  // Cyan
                {255, 128, 0},  // Orange
                {255, 255, 255} // White
            };
            static uint8_t colorIdx = 0;
            colorIdx = (colorIdx + 1) % 8;

            RobotCommand cmd;
            cmd.type = CommandType::SET_LED_COLOR;
            cmd.source = InputSource::ESP_NOW;
            cmd.led.r = COLORS[colorIdx][0];
            cmd.led.g = COLORS[colorIdx][1];
            cmd.led.b = COLORS[colorIdx][2];
            pRobotController->handleCommand(cmd);
        }
        return;
    }

    // Joystick LEFT / RIGHT steps through the icons (wraps around); centre it between steps.
    if (event.type == InputEventType::JOYSTICK && inMenuScreens())
    {
        const int16_t x = event.data.joystick.x;
        const uint8_t count = currentScreen == MenuScreen::MAIN_MENU     ? MAIN_MENU_COUNT
                              : currentScreen == MenuScreen::GAME_SELECT ? GAME_COUNT
                                                                         : LEVEL_COUNT;
        if (!navLatched && (x > 80 || x < -80))
        {
            navLatched = true;
            selectedIndex = (uint8_t)((selectedIndex + count + (x > 0 ? 1 : -1)) % count);
            dirty = true;
        }
        else if (abs(x) <= 40)
        {
            navLatched = false;
        }
    }
}

void LocalMenu::selectCurrentItem()
{
    dirty = true;
    switch (currentScreen)
    {
    case MenuScreen::MAIN_MENU:
        if (selectedIndex == 0)
        { // Game Mode
            currentScreen = MenuScreen::GAME_SELECT;
            selectedIndex = 0;
        }
        else if (selectedIndex == 2)
        { // My Programs: the block engine's list of saved programs (remote B comes back)
            blocks::openPrograms();
        }
        else if (selectedIndex == 1)
        { // Free Ride Mode
            currentScreen = MenuScreen::FREE_RIDE_ACTIVE;
            if (pRobotController)
            {
                RobotCommand cmd;
                cmd.type = CommandType::START_RC;
                cmd.source = InputSource::ESP_NOW;
                pRobotController->handleCommand(cmd);
            }
        }
        break;

    case MenuScreen::GAME_SELECT:
        selectedGameIdx = selectedIndex;
        currentScreen = MenuScreen::LEVEL_SELECT;
        selectedIndex = 0;
        selectedLevel = 1;
        break;

    case MenuScreen::LEVEL_SELECT:
        selectedLevel = selectedIndex + 1;
        currentScreen = MenuScreen::GAME_ACTIVE;
        if (pRobotController)
        {
            RobotCommand cmd;
            cmd.type = CommandType::START_GAME;
            cmd.source = InputSource::ESP_NOW;
            cmd.game.gameId = GAME_IDS[selectedGameIdx];
            cmd.game.level = selectedLevel;
            pRobotController->handleCommand(cmd);
        }
        break;

    default:
        break;
    }
}

void LocalMenu::goBack()
{
    dirty = true;
    switch (currentScreen)
    {
    case MenuScreen::MAIN_MENU: // B on the main menu: back to the eyes
        menuVisible = false;
        break;

    case MenuScreen::GAME_SELECT:
    case MenuScreen::FREE_RIDE_ACTIVE:
        currentScreen = MenuScreen::MAIN_MENU;
        selectedIndex = 0;
        if (pRobotController)
        {
            RobotCommand cmd;
            cmd.type = CommandType::EXIT_RC;
            cmd.source = InputSource::ESP_NOW;
            pRobotController->handleCommand(cmd);
        }
        break;

    case MenuScreen::LEVEL_SELECT:
        currentScreen = MenuScreen::GAME_SELECT;
        selectedIndex = selectedGameIdx;
        break;

    case MenuScreen::GAME_ACTIVE:
        currentScreen = MenuScreen::GAME_SELECT;
        if (pRobotController)
        {
            RobotCommand cmd;
            cmd.type = CommandType::ABORT_GAME;
            cmd.source = InputSource::ESP_NOW;
            pRobotController->handleCommand(cmd);
        }
        break;

    default:
        break;
    }
}

void LocalMenu::drawFreeRideScreen(bool batteryLow)
{
    if (!pDisplay || !pDisplay->isPresent())
        return;
    const unsigned long now = millis();
    if (now - lastFreeRideDraw < 250) // keep the I2C bus free for the sensors
        return;
    lastFreeRideDraw = now;

    Adafruit_SSD1306 &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(8, 20);
    display.setTextSize(2);
    display.println("FREE RIDE");
    display.setTextSize(1);
    display.setCursor(18, 45);
    display.println("Mode Active");
    pDisplay->drawBatteryOverlay(batteryLow);
    display.display();
}
