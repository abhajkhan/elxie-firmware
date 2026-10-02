#include "LocalMenu.h"
#include "RobotController.h"
#include <Arduino.h>

static const char *MAIN_MENU_ITEMS[] = {
    "Game Mode",
    "Free Ride Mode"};
static const uint8_t MAIN_MENU_COUNT = 2;

static const char *GAME_NAMES[] = {
    "Colour Quest",
    "Echo Memory",
    "Reflex Arc",
    "Driving Pro",
    "Inverted Drive"};
static const GameId GAME_IDS[] = {
    GameId::COLOR_QUEST,
    GameId::ECHO_MEMORY,
    GameId::REFLEX_ARC,
    GameId::DRIVING_PRO,
    GameId::INVERTED_DRIVE};
static const uint8_t GAME_COUNT = 5;

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

void LocalMenu::update(bool batteryLow)
{
    drawMenuScreen(batteryLow);
}

void LocalMenu::handleInput(const InputEvent &event)
{
    if (event.source != InputSource::ESP_NOW)
        return;

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

    // Handle Joystick navigation in menu (y > 100 is UP, y < -100 is DOWN)
    if (event.type == InputEventType::JOYSTICK && currentScreen != MenuScreen::FREE_RIDE_ACTIVE && currentScreen != MenuScreen::GAME_ACTIVE)
    {
        int16_t y = event.data.joystick.y;
        static bool debounced = false;

        if (y > 80 && !debounced)
        { // Joystick UP -> decrease index
            debounced = true;
            if (selectedIndex > 0)
                selectedIndex--;
        }
        else if (y < -80 && !debounced)
        { // Joystick DOWN -> increase index
            debounced = true;
            uint8_t maxIdx = (currentScreen == MenuScreen::MAIN_MENU) ? (MAIN_MENU_COUNT - 1) : (currentScreen == MenuScreen::GAME_SELECT ? GAME_COUNT - 1 : 5);
            if (selectedIndex < maxIdx)
                selectedIndex++;
        }
        else if (abs(y) <= 40)
        {
            debounced = false;
        }
    }
}

void LocalMenu::selectCurrentItem()
{
    switch (currentScreen)
    {
    case MenuScreen::MAIN_MENU:
        if (selectedIndex == 0)
        { // Game Mode
            currentScreen = MenuScreen::GAME_SELECT;
            selectedIndex = 0;
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
    switch (currentScreen)
    {
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

void LocalMenu::drawMenuScreen(bool batteryLow)
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    Adafruit_SSD1306 &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if (currentScreen == MenuScreen::MAIN_MENU)
    {
        display.setCursor(0, 0);
        display.println("--- LOCAL MENU ---");

        for (uint8_t i = 0; i < MAIN_MENU_COUNT; i++)
        {
            display.setCursor(10, 18 + i * 16);
            if (i == selectedIndex)
            {
                display.print("> ");
            }
            else
            {
                display.print("  ");
            }
            display.println(MAIN_MENU_ITEMS[i]);
        }
    }
    else if (currentScreen == MenuScreen::GAME_SELECT)
    {
        display.setCursor(0, 0);
        display.println("- SELECT GAME -");

        for (uint8_t i = 0; i < GAME_COUNT; i++)
        {
            if (i < 3)
            {
                display.setCursor(10, 16 + i * 14);
                if (i == selectedIndex)
                    display.print("> ");
                else
                    display.print("  ");
                display.println(GAME_NAMES[i]);
            }
        }
    }
    else if (currentScreen == MenuScreen::LEVEL_SELECT)
    {
        display.setCursor(0, 0);
        display.print("GAME: ");
        display.println(GAME_NAMES[selectedGameIdx]);

        for (uint8_t lvl = 1; lvl <= 3; lvl++)
        {
            display.setCursor(10, 18 + (lvl - 1) * 14);
            if ((lvl - 1) == selectedIndex)
                display.print("> ");
            else
                display.print("  ");
            display.print("Level ");
            display.println(lvl);
        }
    }
    else if (currentScreen == MenuScreen::FREE_RIDE_ACTIVE)
    {
        display.setCursor(8, 20);
        display.setTextSize(2);
        display.println("FREE RIDE");
        display.setTextSize(1);
        display.setCursor(18, 45);
        display.println("Mode Active");
    }
    else if (currentScreen == MenuScreen::GAME_ACTIVE)
    {
        // Active game engine (e.g. ColorQuest) renders its own OLED screens
        return;
    }

    pDisplay->drawBatteryOverlay(batteryLow);
    display.display();
}
