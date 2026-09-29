#ifndef LOCAL_MENU_H
#define LOCAL_MENU_H

#include "../core/RobotTypes.h"
#include "../core/InputEvent.h"
#include "../hardware/DisplayController.h"

class RobotController; // Forward declaration

enum class MenuScreen : uint8_t
{
    MAIN_MENU = 0,
    GAME_SELECT,
    LEVEL_SELECT,
    FREE_RIDE_ACTIVE,
    GAME_ACTIVE,
    GAME_RESULT
};

class LocalMenu
{
public:
    void begin(DisplayController *display, RobotController *controller);
    void update(bool batteryLow);
    void handleInput(const InputEvent &event);
    void reset();

    MenuScreen getScreen() const { return currentScreen; }

private:
    void drawMenuScreen(bool batteryLow);
    void selectCurrentItem();
    void goBack();

    DisplayController *pDisplay = nullptr;
    RobotController *pRobotController = nullptr;

    MenuScreen currentScreen = MenuScreen::MAIN_MENU;
    uint8_t selectedIndex = 0;
    uint8_t selectedGameIdx = 0;
    uint8_t selectedLevel = 1;
    unsigned long lastBtn2ClickMs = 0;
};

#endif // LOCAL_MENU_H
