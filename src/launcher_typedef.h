#ifndef LAUNCHER_TYPEDEF_H
#define LAUNCHER_TYPEDEF_H
#include <M5Core2.h>

struct FirmwareEntry {
    String name;
    String path;
    String iconPath;
    bool hasCustomIcon;
    int defaultIconIndex;
    uint16_t* iconData;
    
    FirmwareEntry() : iconData(nullptr), hasCustomIcon(false), defaultIconIndex(0) {}
    
    // Allow copying
    FirmwareEntry(const FirmwareEntry& other) {
        name = other.name;
        path = other.path;
        iconPath = other.iconPath;
        hasCustomIcon = other.hasCustomIcon;
        defaultIconIndex = other.defaultIconIndex;
        
        if (other.iconData) {
            iconData = (uint16_t*)ps_malloc(ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
            if (iconData) {
                memcpy(iconData, other.iconData, ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
            } else {
                hasCustomIcon = false;
            }
        } else {
            iconData = nullptr;
        }
    }
    
    FirmwareEntry& operator=(const FirmwareEntry& other) {
        if (this != &other) {
            if (iconData) {
                free(iconData);
                iconData = nullptr;
            }
            
            name = other.name;
            path = other.path;
            iconPath = other.iconPath;
            hasCustomIcon = other.hasCustomIcon;
            defaultIconIndex = other.defaultIconIndex;
            
            if (other.iconData) {
                iconData = (uint16_t*)ps_malloc(ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
                if (iconData) {
                    memcpy(iconData, other.iconData, ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
                } else {
                    hasCustomIcon = false;
                }
            }
        }
        return *this;
    }
    
    // Move constructor
    FirmwareEntry(FirmwareEntry&& other) noexcept {
        name = std::move(other.name);
        path = std::move(other.path);
        iconPath = std::move(other.iconPath);
        hasCustomIcon = other.hasCustomIcon;
        defaultIconIndex = other.defaultIconIndex;
        iconData = other.iconData;
        other.iconData = nullptr;
    }
    
    // Move assignment
    FirmwareEntry& operator=(FirmwareEntry&& other) noexcept {
        if (this != &other) {
            if (iconData) free(iconData);
            name = std::move(other.name);
            path = std::move(other.path);
            iconPath = std::move(other.iconPath);
            hasCustomIcon = other.hasCustomIcon;
            defaultIconIndex = other.defaultIconIndex;
            iconData = other.iconData;
            other.iconData = nullptr;
        }
        return *this;
    }
    
    ~FirmwareEntry() {
        if (iconData) {
            free(iconData);
            iconData = nullptr;
        }
    }
};

enum MenuOption {
    OPTION_NONE = -1,
    OPTION_1 = 0,
    OPTION_2 = 1,
    OPTION_3 = 2
};

struct MenuState {
    MenuOption selectedOption = OPTION_1;
    bool isAnimating = false;
    int animationStep = 0;
};

MenuState menuState;


struct IconData {
    const uint16_t* data;
    uint16_t width;
    uint16_t height;
};



struct {
    int selectedIndex = 0;
    int animationStep = 0;
    bool isAnimating = false;
    int animationDirection = 0;  // -1 for left, 1 for right
    int lastAnimationUpdate = 0;
} uiState;


enum Screen {
    MAIN_MENU,
    SETTINGS_MENU,
    WIFI_MENU,
    FIRMWARE_OPTIONS
};

Screen currentScreen = MAIN_MENU;
std::vector<FirmwareEntry> firmwareList;

#endif