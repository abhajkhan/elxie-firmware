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
    // Every loop (not in Blocks mode). Robot idle: eyes, or the icon menu once the remote is
    // used (back to the eyes after HOME_MENU_TIMEOUT_MS without input). Free Ride: its screen.
    void update(bool batteryLow, bool remoteConnected, bool appConnected, bool pairing);
    void handleInput(const InputEvent &event);
    void reset();

    MenuScreen getScreen() const { return currentScreen; }

private:
    void drawFreeRideScreen(bool batteryLow);
    void drawCarousel();
    void setFaceOwner(bool on);
    bool inMenuScreens() const;
    void wake();
    void selectCurrentItem();
    void goBack();

    DisplayController *pDisplay = nullptr;
    RobotController *pRobotController = nullptr;

    MenuScreen currentScreen = MenuScreen::MAIN_MENU;
    uint8_t selectedIndex = 0;
    uint8_t selectedGameIdx = 0;
    uint8_t selectedLevel = 1;
    unsigned long lastBtn2ClickMs = 0;
    bool menuVisible = false;          // icon menu shown (else the eyes)
    unsigned long lastInputMs = 0;
    bool navLatched = false;           // joystick must return to the middle between steps
    bool dirty = true;                 // carousel needs redrawing
    bool faceOwned = false;            // the face engine draws the screen (robot idle)
    uint8_t shownEyes = 0xFF;          // expression shown last (0xFF = none)
    unsigned long lastFreeRideDraw = 0;
};

#endif // LOCAL_MENU_H
