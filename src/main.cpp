#include <M5Core2.h>
#include <SD.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <Update.h>
#include <nvs_flash.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <esp_partition.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// Enhanced color scheme
#define HEADER_COLOR 0x2945  // Darker blue
#define BUTTON_COLOR 0x4B0D  // Lighter blue
#define BUTTON_HIGHLIGHT 0x6B4D  // Even lighter blue for hover effect
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR 0x1082  // Very dark blue
#define DANGER_COLOR 0xF800  // Red
#define SUCCESS_COLOR 0x0640  // Green

// Animation and UI constants
#define ANIMATION_SPEED 150
#define MAX_ITEMS_PER_PAGE 4
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define HEADER_HEIGHT 40
#define BUTTON_HEIGHT 50
#define BUTTON_SPACING 10
#define BUTTON_RADIUS 8

// File system constants
#define FIRMWARE_PATH "/firmware"
#define APP_PARTITION_LABEL "app0"
#define LAUNCHER_PARTITION_LABEL "launcher"

#include <esp_task_wdt.h>
#define WDT_TIMEOUT 15  // Watchdog timeout in seconds
#define BUTTON_HOLD_TIME 3000  // Time to hold button in ms


#define CIRCLE_RADIUS 45
#define CIRCLE_SPACING 90
#define CIRCLE_Y 120
#define ANIMATION_STEPS 10
#define ANIMATION_DELAY 30

#define DRAW_BUFFER_SIZE (SCREEN_WIDTH * 150)  // Buffer for partial screen updates
static uint16_t* drawBuffer = nullptr;


#define SPRITE_WIDTH 320
#define SPRITE_HEIGHT 240
#define EASING_STEPS 40
#define TRANSITION_DURATION 100 // milliseconds

#define ANIMATION_DURATION 250  // Slightly faster animation
#define MIN_FRAME_TIME 16      // Cap at ~60fps

#define ANIMATION_BUFFER_HEIGHT 180  // Height of the animation area
#define ANIMATION_Y_START 40         // Start Y position of animation area

#define RGB565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define R565(color) ((color >> 11) & 0x1F)
#define G565(color) ((color >> 5) & 0x3F)
#define B565(color) (color & 0x1F)

#define ICON_SIZE 64
#define DEFAULT_ICONS_COUNT 5
#define ICON_PATH "/icons"

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define DARKEN_COLOR(color) (((color & 0xF7DE) >> 1) | (color & 0x0821))

// WiFi credentials
const char* ssid = "";
const char* password = "";

// Global variables
AsyncWebServer server(80);
Preferences preferences;

RTC_DATA_ATTR bool launch_firmware = false;
RTC_DATA_ATTR esp_partition_t saved_partition;
RTC_DATA_ATTR int bootCount = 0;


struct FirmwareEntry {
    String name;
    String path;
    String iconPath;
    bool hasCustomIcon;
    int defaultIconIndex;
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
int currentPage = 0;
int totalPages = 0;

static uint32_t animationStartTime = 0;
static uint32_t lastDrawTime = 0;
static float currentOffset = 0.0f;
static float targetOffset = 0.0f;

static uint16_t* animationBuffer = nullptr;


static uint16_t gearIconData[ICON_SIZE * ICON_SIZE];
static uint16_t defaultIconsData[DEFAULT_ICONS_COUNT][ICON_SIZE * ICON_SIZE];


void drawHeader(bool animated);
void transitionToScreen(void (*drawFunc)());
void drawScreen();
void loadFirmwareList();
bool startWiFiServer();
void handleTouch();
bool executeFirmware(const String& path);
void showFirmwareOptions(int index);
bool confirmDelete();
void deleteFirmware(int index);
void deleteAllFirmware();
void showPopup(const char* message, uint16_t color, int duration);
void drawButton(int x, int y, int w, int h, const char* text, uint16_t color, bool rounded);




void generateGearIcon() {
    const int teeth = 12;
    const float innerRadius = ICON_SIZE * 0.3;
    const float outerRadius = ICON_SIZE * 0.45;
    const float centerX = ICON_SIZE / 2;
    const float centerY = ICON_SIZE / 2;

    for (int y = 0; y < ICON_SIZE; y++) {
        for (int x = 0; x < ICON_SIZE; x++) {
            float dx = x - centerX;
            float dy = y - centerY;
            float angle = atan2(dy, dx);
            float radius = sqrt(dx * dx + dy * dy);
            
            // Create gear teeth pattern
            float teethRadius = outerRadius + cos(teeth * angle) * (outerRadius - innerRadius) * 0.4;
            
            if (radius < innerRadius * 0.7) {
                // Inner circle
                gearIconData[y * ICON_SIZE + x] = RGB565(180, 180, 180);
            } else if (radius < teethRadius) {
                // Teeth
                gearIconData[y * ICON_SIZE + x] = RGB565(120, 120, 120);
            } else {
                // Background
                gearIconData[y * ICON_SIZE + x] = RGB565(32, 32, 32);
            }
        }
    }
}

// Function to generate default icons
void generateDefaultIcons() {
    const uint16_t colors[DEFAULT_ICONS_COUNT][3] = {
        {0x4A, 0x90, 0xE2},  // Blue
        {0xF3, 0x9C, 0x12},  // Orange
        {0x2E, 0xCC, 0x71},  // Green
        {0x9B, 0x59, 0xB6},  // Purple
        {0xE7, 0x4C, 0x3C}   // Red
    };

    for (int icon = 0; icon < DEFAULT_ICONS_COUNT; icon++) {
        // Create unique patterns for each icon
        switch (icon) {
            case 0: // Circuit pattern
                for (int y = 0; y < ICON_SIZE; y++) {
                    for (int x = 0; x < ICON_SIZE; x++) {
                        bool line = (x + y) % 16 < 4 || (x - y + ICON_SIZE) % 16 < 4;
                        defaultIconsData[icon][y * ICON_SIZE + x] = line ? 
                            RGB565(colors[icon][0], colors[icon][1], colors[icon][2]) :
                            RGB565(32, 32, 32);
                    }
                }
                break;

            case 1: // Hexagon pattern
                for (int y = 0; y < ICON_SIZE; y++) {
                    for (int x = 0; x < ICON_SIZE; x++) {
                        float dx = x - ICON_SIZE/2;
                        float dy = (y - ICON_SIZE/2) * 1.732;
                        float hex = (fmod(dx + dy, 20) < 10) ^ (fmod(dx - dy, 20) < 10);
                        defaultIconsData[icon][y * ICON_SIZE + x] = hex ?
                            RGB565(colors[icon][0], colors[icon][1], colors[icon][2]) :
                            RGB565(32, 32, 32);
                    }
                }
                break;

            case 2: // Concentric circles
                for (int y = 0; y < ICON_SIZE; y++) {
                    for (int x = 0; x < ICON_SIZE; x++) {
                        float dx = x - ICON_SIZE/2;
                        float dy = y - ICON_SIZE/2;
                        float dist = sqrt(dx*dx + dy*dy);
                        bool ring = int(dist) % 10 < 5;
                        defaultIconsData[icon][y * ICON_SIZE + x] = ring ?
                            RGB565(colors[icon][0], colors[icon][1], colors[icon][2]) :
                            RGB565(32, 32, 32);
                    }
                }
                break;

            case 3: // Diamond pattern
                for (int y = 0; y < ICON_SIZE; y++) {
                    for (int x = 0; x < ICON_SIZE; x++) {
                        float dx = abs(x - ICON_SIZE/2);
                        float dy = abs(y - ICON_SIZE/2);
                        bool diamond = (dx + dy) < ICON_SIZE/2;
                        defaultIconsData[icon][y * ICON_SIZE + x] = diamond ?
                            RGB565(colors[icon][0], colors[icon][1], colors[icon][2]) :
                            RGB565(32, 32, 32);
                    }
                }
                break;

            case 4: // Starburst pattern
                for (int y = 0; y < ICON_SIZE; y++) {
                    for (int x = 0; x < ICON_SIZE; x++) {
                        float dx = x - ICON_SIZE/2;
                        float dy = y - ICON_SIZE/2;
                        float angle = atan2(dy, dx);
                        bool ray = fmod(angle * 8 / M_PI + 16, 2) < 1;
                        defaultIconsData[icon][y * ICON_SIZE + x] = ray ?
                            RGB565(colors[icon][0], colors[icon][1], colors[icon][2]) :
                            RGB565(32, 32, 32);
                    }
                }
                break;
        }
    }
}



void initAnimationBuffer() {
    if (animationBuffer != nullptr) {
        free(animationBuffer);
    }
    animationBuffer = (uint16_t*)ps_malloc(SCREEN_WIDTH * ANIMATION_BUFFER_HEIGHT * sizeof(uint16_t));
    if (!animationBuffer) {
        Serial.println("Failed to allocate animation buffer!");
    }
}



float easeOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

uint16_t swapBytes(uint16_t color) {
    return (color >> 8) | (color << 8);
}

// Update the dimColor function
uint16_t dimColor(uint16_t color, uint8_t factor) {
    // Extract RGB components
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    // Convert to 8-bit values
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    
    // Apply dimming
    r = (r * factor) / 255;
    g = (g * factor) / 255;
    b = (b * factor) / 255;
    
    // Convert back to RGB565
    return M5.Lcd.color565(r, g, b);
}



void drawButtonHints(const char* btnA, const char* btnB, const char* btnC) {
    uint16_t normalColor = BUTTON_COLOR;
    uint16_t pressedColor = BUTTON_HIGHLIGHT;

    // Button A (Left)
    uint16_t colorA = M5.BtnA.isPressed() ? pressedColor : normalColor;
    M5.Lcd.fillRoundRect(5, SCREEN_HEIGHT-30, 70, 20, 5, colorA);
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(15, SCREEN_HEIGHT-25);
    M5.Lcd.print(btnA);

    // Button B (Center)
    uint16_t colorB = M5.BtnB.isPressed() ? pressedColor : normalColor;
    M5.Lcd.fillRoundRect(SCREEN_WIDTH/2-35, SCREEN_HEIGHT-30, 70, 20, 5, colorB);
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setCursor(SCREEN_WIDTH/2-25, SCREEN_HEIGHT-25);
    M5.Lcd.print(btnB);

    // Button C (Right)
    uint16_t colorC = M5.BtnC.isPressed() ? pressedColor : normalColor;
    M5.Lcd.fillRoundRect(SCREEN_WIDTH-75, SCREEN_HEIGHT-30, 70, 20, 5, colorC);
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setCursor(SCREEN_WIDTH-65, SCREEN_HEIGHT-25);
    M5.Lcd.print(btnC);
}


void drawMenuOptions(const char* options[], int numOptions, int selectedOption) {
    for (int i = 0; i < numOptions; i++) {
        int yPos = 60 + i * 60;
        uint16_t color = (i == selectedOption) ? BUTTON_HIGHLIGHT : BUTTON_COLOR;
        drawButton(40, yPos, SCREEN_WIDTH - 80, BUTTON_HEIGHT, options[i], color, true);
    }
}


void drawCircularIcon(int x, int y, int radius, const char* text, bool selected) {
    uint16_t baseColor = selected ? BUTTON_HIGHLIGHT : BUTTON_COLOR;
    
    // Draw shadow for selected item
    if (selected) {
        for(int r = radius + 5; r > radius; r--) {
            M5.Lcd.drawCircle(x, y, r, dimColor(baseColor, 100));
        }
    }
    
    // Draw main circle with gradient
    for(int r = radius; r > radius-8; r--) {
        uint16_t gradColor = dimColor(baseColor, 150 + (radius-r)*12);
        M5.Lcd.drawCircle(x, y, r, gradColor);
    }
    M5.Lcd.fillCircle(x, y, radius-8, baseColor);
    
    // Draw text
    M5.Lcd.setTextSize(selected ? 2 : 1);
    int charWidth = selected ? 12 : 6;
    int textWidth = strlen(text) * charWidth;
    
    // Draw text shadow for selected item
    if (selected) {
        M5.Lcd.setTextColor(TFT_BLACK);
        M5.Lcd.setCursor(x - textWidth/2 + 1, y - 8 + 1);
        M5.Lcd.print(text);
    }
    
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setCursor(x - textWidth/2, y - 8);
    M5.Lcd.print(text);
}

void enforceFactoryBoot() {
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        // Clear OTA data to force factory boot
        const esp_partition_t* otadata = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
        if (otadata != NULL) {
            esp_partition_erase_range(otadata, 0, otadata->size);
        }
        
        // Set boot partition to factory
        if (esp_ota_set_boot_partition(factory) != ESP_OK) {
            Serial.println("Failed to set factory boot partition!");
        }
    }
}

void drawNavigationHints() {
    if (uiState.selectedIndex > 0) {
        M5.Lcd.fillTriangle(10, CIRCLE_Y, 30, CIRCLE_Y-20, 30, CIRCLE_Y+20, 
                           dimColor(BUTTON_COLOR, 150));
        M5.Lcd.setTextColor(TEXT_COLOR);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(15, SCREEN_HEIGHT-25);
        M5.Lcd.print("BTN A");
    }
    
    if (uiState.selectedIndex < firmwareList.size()) {
        M5.Lcd.fillTriangle(SCREEN_WIDTH-10, CIRCLE_Y, 
                           SCREEN_WIDTH-30, CIRCLE_Y-20, 
                           SCREEN_WIDTH-30, CIRCLE_Y+20, 
                           dimColor(BUTTON_COLOR, 150));
        M5.Lcd.setCursor(SCREEN_WIDTH-45, SCREEN_HEIGHT-25);
        M5.Lcd.print("BTN C");
    }
}

void printPartitionInfo() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
        
    Serial.println("Partition Information:");
    Serial.printf("Running partition: type %d, subtype %d, offset 0x%08x\n",
                  running->type, running->subtype, running->address);
    Serial.printf("Boot partition: type %d, subtype %d, offset 0x%08x\n",
                  boot->type, boot->subtype, boot->address);
    if (factory) {
        Serial.printf("Factory partition: type %d, subtype %d, offset 0x%08x\n",
                     factory->type, factory->subtype, factory->address);
    } else {
        Serial.println("No factory partition found!");
    }
}


void clearOtaData() {
    const esp_partition_t* otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (otadata != NULL) {
        Serial.println("Erasing OTA data...");
        esp_partition_erase_range(otadata, 0, otadata->size);
    }
}

void forceFactoryBoot() {
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        clearOtaData();
        esp_ota_set_boot_partition(factory);
    }
}

void setupWatchdog() {
    esp_task_wdt_init(WDT_TIMEOUT, true);  // Enable panic so ESP32 restarts
    esp_task_wdt_add(NULL);  // Add current thread to WDT watch
}

void disableWatchdog() {
    esp_task_wdt_delete(NULL);  // Remove current thread from WDT watch
}

bool checkButtonPress() {
    static uint32_t pressStartTime = 0;
    static bool wasPressed = false;
    
    if (M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnC.isPressed()) {
        if (!wasPressed) {
            pressStartTime = millis();
            wasPressed = true;
        }
        if (millis() - pressStartTime >= BUTTON_HOLD_TIME) {
            return true;
        }
    } else {
        wasPressed = false;
    }
    return false;
}


void drawFirmwareOptions() {
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);

    M5.Lcd.setCursor(10, 50);
    M5.Lcd.printf("Selected: %s", firmwareList[uiState.selectedIndex].name.c_str());

    const char* options[] = {
        "Launch",
        "Delete",
        "Back"
    };

    drawMenuOptions(options, 3, menuState.selectedOption);
    drawButtonHints("↑ Up", "Select", "↓ Down");
}


void drawDeleteConfirm() {
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);
    
    M5.Lcd.setCursor(20, 60);
    M5.Lcd.print("Are you sure you want to");
    M5.Lcd.setCursor(20, 85);
    M5.Lcd.print("delete this firmware?");

    const char* options[] = {
        "Yes",
        "No"
    };

    drawMenuOptions(options, 2, menuState.selectedOption);
    drawButtonHints("↑ Up", "Select", "↓ Down");
}



// Implementation of utility functions
void drawButton(int x, int y, int w, int h, const char* text, uint16_t color, bool rounded ) {
    if (rounded) {
        M5.Lcd.fillRoundRect(x, y, w, h, BUTTON_RADIUS, color);
        M5.Lcd.drawRoundRect(x, y, w, h, BUTTON_RADIUS, color + 0x1082);
    } else {
        M5.Lcd.fillRect(x, y, w, h, color);
    }
    
    int textWidth = strlen(text) * 12;
    int textX = x + (w - textWidth) / 2;
    int textY = y + (h - 16) / 2;
    
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setCursor(textX, textY);
    M5.Lcd.print(text);
}

void showPopup(const char* message, uint16_t color = TEXT_COLOR, int duration = 2000) {
    uint16_t* buffer = (uint16_t*)ps_malloc(240 * 100 * sizeof(uint16_t));
    if (buffer) {
        M5.Lcd.readRect(40, 70, 240, 100, buffer);
        
        for (int i = 0; i < 100; i += 5) {
            M5.Lcd.fillRoundRect(40, 70, 240, i, 10, BUTTON_COLOR);
            M5.Lcd.drawRoundRect(40, 70, 240, i, 10, HEADER_COLOR);
            delay(5);
        }
        
        M5.Lcd.setTextColor(color);
        M5.Lcd.setTextSize(2);
        int16_t x = 40 + (240 - strlen(message) * 12) / 2;
        M5.Lcd.setCursor(x, 110);
        M5.Lcd.print(message);
        delay(duration);
        
        for (int i = 255; i >= 0; i -= 5) {
            M5.Lcd.fillRoundRect(40, 70, 240, 100, 10, 
                               M5.Lcd.color565(i/8, i/8, i/8));
            delay(5);
        }
        
        M5.Lcd.pushImage(40, 70, 240, 100, buffer);
        free(buffer);
    }
}

void drawHeader(bool animated) {
    if (animated) {
        int headerPos = -HEADER_HEIGHT;
        while (headerPos < 0) {
            M5.Lcd.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, BG_COLOR);
            M5.Lcd.fillRect(0, headerPos, SCREEN_WIDTH, HEADER_HEIGHT, HEADER_COLOR);
            M5.Lcd.setTextColor(TEXT_COLOR);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setCursor(10, headerPos + 10);
            M5.Lcd.print("Firmware Launcher");
            headerPos += 5;
            delay(10);
        }
    } else {
        M5.Lcd.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, HEADER_COLOR);
        M5.Lcd.setTextColor(TEXT_COLOR);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.print("Firmware Launcher");
    }
    
    M5.Lcd.fillCircle(SCREEN_WIDTH - 25, 20, 5, BUTTON_COLOR);
    M5.Lcd.fillCircle(SCREEN_WIDTH - 25, 20, 3, HEADER_COLOR);
}

void transitionToScreen(void (*drawFunc)()) {
    for (int i = 0; i <= SCREEN_WIDTH; i += 20) {
        M5.Lcd.fillRect(0, 0, i, SCREEN_HEIGHT, BG_COLOR);
        delay(5);
    }
    drawFunc();
}

void loadFirmwareList() {
    firmwareList.clear();
    File root = SD.open(FIRMWARE_PATH);
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open firmware directory");
        return;
    }

    // Create icons directory if it doesn't exist
    if (!SD.exists(ICON_PATH)) {
        SD.mkdir(ICON_PATH);
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".bin")) {
            FirmwareEntry entry;
            entry.name = file.name();
            entry.name.replace(".bin", ""); // Remove .bin extension for display
            entry.path = String(FIRMWARE_PATH) + "/" + file.name();
            
            // Check for matching icon
            String iconName = String(file.name());
            iconName.replace(".bin", ".png");
            String iconPath = String(ICON_PATH) + "/" + iconName;
            
            if (SD.exists(iconPath)) {
                entry.iconPath = iconPath;
                entry.hasCustomIcon = true;
            } else {
                entry.hasCustomIcon = false;
                entry.defaultIconIndex = random(DEFAULT_ICONS_COUNT);
            }
            
            firmwareList.push_back(entry);
        }
        file = root.openNextFile();
    }
    root.close();

    // Add settings entry
    FirmwareEntry settingsEntry;
    settingsEntry.name = "Settings";
    settingsEntry.hasCustomIcon = false;
    settingsEntry.defaultIconIndex = -1; // Special case for gear icon
    firmwareList.push_back(settingsEntry);
}



void drawSettingsMenu() {
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);

    // Draw menu options centered and bigger
    int yPos = SCREEN_HEIGHT/2 - 80;  // Start higher for better spacing
    
    // Delete All (Button A)
    drawButton(40, yPos, SCREEN_WIDTH - 80, BUTTON_HEIGHT, "Delete All", DANGER_COLOR, true);
    
    // WiFi Mode (Button B)
    drawButton(40, yPos + 70, SCREEN_WIDTH - 80, BUTTON_HEIGHT, "WiFi Mode", BUTTON_COLOR, true);
    
    // Back (Button C)
    drawButton(40, yPos + 140, SCREEN_WIDTH - 80, BUTTON_HEIGHT, "Back", BUTTON_COLOR, true);

    // Draw button hints
    drawButtonHints("Delete", "WiFi", "Back");
}


void drawCircleInBuffer(int x0, int y0, int r, uint16_t color,
                       const std::function<void(int,int,uint16_t)>& setPixel) {
    int16_t x = 0;
    int16_t y = r;
    int16_t err = 3 - 2 * r;

    while (y >= x) {
        setPixel(x0 + x, y0 + y, color);
        setPixel(x0 - x, y0 + y, color);
        setPixel(x0 + x, y0 - y, color);
        setPixel(x0 - x, y0 - y, color);
        setPixel(x0 + y, y0 + x, color);
        setPixel(x0 - y, y0 + x, color);
        setPixel(x0 + y, y0 - x, color);
        setPixel(x0 - y, y0 - x, color);

        if (err <= 0) {
            x++;
            err += 4 * x + 6;
        } else {
            y--;
            err += 4 * (x - y) + 10;
        }
    }
}

// Helper function to draw text in buffer
    const uint8_t font8x6[96][8] = {
        // Define the font8x6 array here with appropriate values
        // Example for character 'A' (ASCII 65):
        {0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00, 0x00}, // 'A'
        // Add other characters as needed
    };
    
void drawTextInBuffer(int x, int y, const char* text, bool selected, 
                              const std::function<void(int,int,uint16_t)>& setPixel) {
            int charWidth = selected ? 12 : 6;
            int textWidth = strlen(text) * charWidth;
            int textX = x - textWidth / 2;
            int textY = y - (selected ? 8 : 4);
        
            for (int i = 0; text[i] != '\0'; i++) {
                char c = text[i];
                for (int py = 0; py < 8; py++) {
                    for (int px = 0; px < 6; px++) {
                        if (font8x6[c - 32][py] & (1 << px)) {
                            setPixel(textX + i * charWidth + px, textY + py, TEXT_COLOR);
                        }
                    }
                }
            }
        }

// Helper function to fill circle in buffer
void fillCircleInBuffer(int x0, int y0, int r, uint16_t color,
                       const std::function<void(int,int,uint16_t)>& setPixel) {
    int16_t x = 0;
    int16_t y = r;
    int16_t err = 3 - 2 * r;

    while (y >= x) {
        // Draw horizontal lines for each octant
        for (int16_t i = x0 - x; i <= x0 + x; ++i) {
            setPixel(i, y0 + y, color);
            setPixel(i, y0 - y, color);
        }
        for (int16_t i = x0 - y; i <= x0 + y; ++i) {
            setPixel(i, y0 + x, color);
            setPixel(i, y0 - x, color);
        }

        if (err <= 0) {
            x++;
            err += 4 * x + 6;
        } else {
            y--;
            err += 4 * (x - y) + 10;
        }
    }
}

void drawIconToBuffer(int x, int y, int radius, const FirmwareEntry& entry, bool selected) {
    if (!animationBuffer) return;

    // Calculate scaling and position
    float scale = (float)radius * 2 / ICON_SIZE;
    int startX = x - radius;
    int startY = y - radius;
    
    // Get the appropriate icon data
    const uint16_t* iconData;
    if (entry.name == "Settings") {
        iconData = gearIconData;
    } else if (entry.hasCustomIcon) {
        // Load custom icon from SD card
        File iconFile = SD.open(entry.iconPath);
        if (iconFile) {
            // For now, fallback to default if custom icon exists
            // In a future update, implement proper PNG loading
            iconData = defaultIconsData[entry.defaultIconIndex];
            iconFile.close();
        } else {
            iconData = defaultIconsData[entry.defaultIconIndex];
        }
    } else {
        iconData = defaultIconsData[entry.defaultIconIndex];
    }

    // Draw the icon with scaling
    for (int dy = -radius; dy < radius; dy++) {
        for (int dx = -radius; dx < radius; dx++) {
            // Calculate source pixel
            int sourceY = (dy + radius) * ICON_SIZE / (radius * 2);
            
            // Calculate source pixel
            int sourceX = (dx + radius) * ICON_SIZE / (radius * 2);

            // Check bounds
            if (sourceX >= 0 && sourceX < ICON_SIZE && sourceY >= 0 && sourceY < ICON_SIZE) {
                // Calculate destination position
                int destX = x + dx;
                int destY = y + dy;
                
                if (destX >= 0 && destX < SCREEN_WIDTH && 
                    destY >= 0 && destY < ANIMATION_BUFFER_HEIGHT) {
                    
                    // Get source pixel
                    uint16_t color = iconData[sourceY * ICON_SIZE + sourceX];
                    
                    // Add highlight effect if selected
                    if (selected) {
                        // Create a glow effect around the icon
                        float distFromCenter = sqrt(dx*dx + dy*dy) / radius;
                        if (distFromCenter > 0.8 && distFromCenter < 1.0) {
                            // Add glow at the edges
                            uint16_t r = ((color >> 11) & 0x1F);
                            uint16_t g = ((color >> 5) & 0x3F);
                            uint16_t b = (color & 0x1F);
                            
                            r = min(0x1F, r + 4);
                            g = min(0x3F, g + 8);
                            b = min(0x1F, b + 4);
                            
                            color = (r << 11) | (g << 5) | b;
                        }
                    }
                    
                    // Store in animation buffer with byte swapping for M5Stack
                    animationBuffer[destY * SCREEN_WIDTH + destX] = (color >> 8) | (color << 8);
                }
            }
        }
    }
    
    // Draw icon name below
    if (!entry.name.isEmpty()) {
        M5.Lcd.setTextColor(TEXT_COLOR);
        M5.Lcd.setTextSize(selected ? 2 : 1);
        int textWidth = entry.name.length() * (selected ? 12 : 6);
        M5.Lcd.setCursor(x - textWidth/2, y + radius + 5);
        M5.Lcd.print(entry.name);
    }
}
    

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    static String currentFilePath;
    
    if (!index) {
        String uploadPath;
        if (filename.endsWith(".bin")) {
            uploadPath = String(FIRMWARE_PATH) + "/" + filename;
        } else if (filename.endsWith(".png")) {
            if (!SD.exists(ICON_PATH)) {
                SD.mkdir(ICON_PATH);
            }
            uploadPath = String(ICON_PATH) + "/" + filename;
        } else {
            return; // Unsupported file type
        }
        
        Serial.printf("Upload Start: %s\n", uploadPath.c_str());
        uploadFile = SD.open(uploadPath, FILE_WRITE);
        currentFilePath = uploadPath;
    }
    
    if (uploadFile && len) {
        uploadFile.write(data, len);
    }
    
    if (final) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("Upload Complete: %s, %u bytes\n", currentFilePath.c_str(), index + len);
            
            if (currentFilePath.endsWith(".bin")) {
                loadFirmwareList(); // Reload firmware list when new firmware is uploaded
            }
        }
    }
}





void drawScreen() {
    if (!animationBuffer) return;
    
    uint32_t currentTime = millis();
    if (currentTime - lastDrawTime < MIN_FRAME_TIME) {
        return;
    }
    lastDrawTime = currentTime;

    if (currentScreen == MAIN_MENU) {
        static bool firstDraw = true;
        
        if (firstDraw || !uiState.isAnimating) {
            M5.Lcd.fillScreen(BG_COLOR);
            drawHeader(false);
            firstDraw = false;
        }

        // Clear animation buffer
        uint16_t bgColor = ((BG_COLOR & 0xFF) << 8) | ((BG_COLOR >> 8) & 0xFF);
        for (int i = 0; i < SCREEN_WIDTH * ANIMATION_BUFFER_HEIGHT; i++) {
            animationBuffer[i] = bgColor;
        }

        int centerX = SCREEN_WIDTH / 2;
        float animationOffset = 0;
        
        if (uiState.isAnimating) {
            uint32_t elapsed = currentTime - animationStartTime;
            float progress = min(1.0f, (float)elapsed / ANIMATION_DURATION);
            animationOffset = targetOffset * easeOutCubic(progress);
            
            if (progress >= 1.0f) {
                uiState.isAnimating = false;
                animationOffset = 0;
                targetOffset = 0;
            }
        }
        
        // Draw icons
        int totalItems = firmwareList.size();
        for (int i = -1; i <= 1; i++) {
            int index = uiState.selectedIndex + i;
            if (index >= 0 && index < totalItems) {
                float offset = i * CIRCLE_SPACING + animationOffset;
                int x = centerX + offset;
                
                if (x >= -CIRCLE_RADIUS && x <= SCREEN_WIDTH + CIRCLE_RADIUS) {
                    float distFromCenter = abs(offset) / CIRCLE_SPACING;
                    float scale = 1.0f - (distFromCenter * 0.3f);
                    scale = max(0.0f, min(1.0f, scale));
                    
                    int radius = CIRCLE_RADIUS * scale;
                    if (radius > 0) {
                        drawIconToBuffer(x, CIRCLE_Y - ANIMATION_Y_START, radius, 
                                    firmwareList[index],
                                    index == uiState.selectedIndex && !uiState.isAnimating);
                    }
                }
            }
        }
        
        // Push buffer to screen
        M5.Lcd.pushImage(0, ANIMATION_Y_START, SCREEN_WIDTH, ANIMATION_BUFFER_HEIGHT, animationBuffer);
        
        // Draw navigation elements
        if (!uiState.isAnimating) {
            if (uiState.selectedIndex > 0) {
                M5.Lcd.fillTriangle(10, CIRCLE_Y, 30, CIRCLE_Y-20, 30, CIRCLE_Y+20, BUTTON_COLOR);
            }
            
            if (uiState.selectedIndex < totalItems - 1) {
                M5.Lcd.fillTriangle(SCREEN_WIDTH-10, CIRCLE_Y, 
                                SCREEN_WIDTH-30, CIRCLE_Y-20, 
                                SCREEN_WIDTH-30, CIRCLE_Y+20, BUTTON_COLOR);
            }
            
            drawButtonHints("←", "Select", "→");
        }
    }
}


bool startWiFiServer() {
    WiFi.begin(ssid, password);
    
    transitionToScreen([]() {
        M5.Lcd.fillScreen(BG_COLOR);
        drawHeader(false);
        M5.Lcd.setCursor(10, 60);
        M5.Lcd.print("Connecting to WiFi...");
    });
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        M5.Lcd.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        showPopup("Connection failed!", TFT_RED);
        transitionToScreen(drawScreen);
        return false;
    }

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>M5Stack Firmware Uploader</title>
    <!-- Load dependencies in the correct order -->
    <script crossorigin src="https://unpkg.com/@babel/standalone/babel.min.js"></script>
    <script crossorigin src="https://unpkg.com/react@18/umd/react.development.js"></script>
    <script crossorigin src="https://unpkg.com/react-dom@18/umd/react-dom.development.js"></script>
    <script src="https://cdn.tailwindcss.com"></script>
    <script>
        // Configure Tailwind
        tailwind.config = {
            darkMode: 'class',
            theme: {
                extend: {}
            }
        }
    </script>
</head>
<body>
    <!-- Root element for React -->
    <div id="root"></div>

    <!-- React component -->
    <script type="text/babel" data-type="module">
        // Main App component
        const App = () => {
            // State declarations
            const [firmwareFile, setFirmwareFile] = React.useState(null);
            const [iconFile, setIconFile] = React.useState(null);
            const [iconPreview, setIconPreview] = React.useState(null);
            const [isUploading, setIsUploading] = React.useState(false);
            const [uploadStatus, setUploadStatus] = React.useState(null);
            const formRef = React.useRef(null);

            // File handlers
            const handleFirmwareChange = (event) => {
                const file = event.target.files[0];
                if (file?.name.endsWith('.bin')) {
                    setFirmwareFile(file);
                }
            };

            const handleIconChange = (event) => {
                const file = event.target.files[0];
                if (file?.type === 'image/png') {
                    setIconFile(file);
                    const reader = new FileReader();
                    reader.onload = (e) => setIconPreview(e.target.result);
                    reader.readAsDataURL(file);
                }
            };

            // Form submission
            const handleSubmit = async (event) => {
                event.preventDefault();
                if (!firmwareFile) return;

                setIsUploading(true);
                setUploadStatus('uploading');

                const formData = new FormData();
                formData.append('firmware', firmwareFile);
                if (iconFile) formData.append('icon', iconFile);

                try {
                    const response = await fetch('/upload', {
                        method: 'POST',
                        body: formData
                    });

                    if (response.ok) {
                        setUploadStatus('success');
                        setTimeout(() => {
                            formRef.current?.reset();
                            setFirmwareFile(null);
                            setIconFile(null);
                            setIconPreview(null);
                            setUploadStatus(null);
                        }, 2000);
                    } else {
                        throw new Error('Upload failed');
                    }
                } catch (error) {
                    console.error('Upload error:', error);
                    setUploadStatus('error');
                } finally {
                    setIsUploading(false);
                }
            };

            // Render component
            return (
                <div className="min-h-screen bg-slate-900 text-white p-6">
                    <div className="max-w-md mx-auto bg-slate-800 rounded-lg p-6 shadow-xl">
                        <h1 className="text-2xl font-bold text-center mb-6">
                            M5Stack Firmware Uploader
                        </h1>

                        <form ref={formRef} onSubmit={handleSubmit} className="space-y-6">
                            {/* Firmware Upload */}
                            <div>
                                <label className="block text-sm font-medium mb-2">
                                    Firmware File (.bin)
                                </label>
                                <div className="border-2 border-dashed border-slate-600 rounded-lg p-4 hover:border-blue-500 transition-colors">
                                    <input
                                        type="file"
                                        accept=".bin"
                                        onChange={handleFirmwareChange}
                                        className="w-full"
                                        required
                                    />
                                </div>
                            </div>

                            {/* Icon Upload */}
                            <div>
                                <label className="block text-sm font-medium mb-2">
                                    Icon (optional)
                                </label>
                                <div className="flex space-x-4">
                                    <div className="flex-1 border-2 border-dashed border-slate-600 rounded-lg p-4 hover:border-blue-500 transition-colors">
                                        <input
                                            type="file"
                                            accept="image/png"
                                            onChange={handleIconChange}
                                            className="w-full"
                                        />
                                    </div>
                                    {iconPreview && (
                                        <div className="w-24 h-24 border border-slate-600 rounded-lg p-2">
                                            <img
                                                src={iconPreview}
                                                alt="Icon preview"
                                                className="w-full h-full object-contain"
                                            />
                                        </div>
                                    )}
                                </div>
                            </div>

                            {/* Upload Status */}
                            {uploadStatus && (
                                <div className="mt-4">
                                    <div className="flex justify-between mb-1">
                                        <span className="text-sm font-medium">
                                            {uploadStatus === 'uploading' ? 'Uploading...' :
                                             uploadStatus === 'success' ? 'Upload Complete!' :
                                             'Upload Failed'}
                                        </span>
                                    </div>
                                    <div className="w-full bg-slate-700 rounded-full h-2">
                                        <div
                                            className={`h-2 rounded-full transition-all duration-300 ${
                                                uploadStatus === 'success' ? 'bg-green-500' :
                                                uploadStatus === 'error' ? 'bg-red-500' :
                                                'bg-blue-500'
                                            }`}
                                            style={{width: uploadStatus === 'success' ? '100%' : '0%'}}
                                        />
                                    </div>
                                </div>
                            )}

                            {/* Submit Button */}
                            <button
                                type="submit"
                                disabled={!firmwareFile || isUploading}
                                className={`w-full py-2 px-4 rounded-lg font-medium transition-colors ${
                                    !firmwareFile || isUploading
                                        ? 'bg-slate-700 text-slate-400 cursor-not-allowed'
                                        : 'bg-blue-500 hover:bg-blue-600'
                                }`}
                            >
                                {isUploading ? 'Uploading...' : 'Upload Firmware'}
                            </button>
                        </form>

                        {/* Error Message */}
                        {uploadStatus === 'error' && (
                            <div className="mt-4 p-4 bg-red-900/50 rounded-lg text-red-200">
                                Upload failed. Please try again.
                            </div>
                        )}
                    </div>
                </div>
            );
        };

        // Mount the React app
        const root = ReactDOM.createRoot(document.getElementById('root'));
        root.render(<App />);
    </script>
</body>
</html>
)");
    });

    // Update upload handler to send progress updates
    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) { 
            request->send(200, "text/plain", "Upload successful"); 
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            static File uploadFile;
            static String currentFilePath;
            
            if (!index) {
                String uploadPath;
                if (filename.endsWith(".bin")) {
                    uploadPath = String(FIRMWARE_PATH) + "/" + filename;
                } else if (filename.endsWith(".png")) {
                    if (!SD.exists(ICON_PATH)) {
                        SD.mkdir(ICON_PATH);
                    }
                    uploadPath = String(ICON_PATH) + "/" + filename;
                } else {
                    return; // Unsupported file type
                }
                
                Serial.printf("Upload Start: %s\n", uploadPath.c_str());
                uploadFile = SD.open(uploadPath, FILE_WRITE);
                currentFilePath = uploadPath;
            }
            
            if (uploadFile && len) {
                uploadFile.write(data, len);
            }
            
            if (final) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.printf("Upload Complete: %s, %u bytes\n", currentFilePath.c_str(), index + len);
                    
                    if (currentFilePath.endsWith(".bin")) {
                        loadFirmwareList(); // Reload firmware list when new firmware is uploaded
                    }
                }
            }
        }
    );

    server.begin();
    
    // Show the connection screen
    transitionToScreen([]() {
        M5.Lcd.fillScreen(BG_COLOR);
        drawHeader(false);
        M5.Lcd.setTextColor(TEXT_COLOR);
        M5.Lcd.setTextSize(2);
        
        M5.Lcd.setCursor(10, 60);
        M5.Lcd.print("IP: ");
        M5.Lcd.println(WiFi.localIP());
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.println("Open in browser to upload");
        
        drawButton(10, 200, 300, 35, "Back", BUTTON_COLOR, false);
    });
    
    return true;
}

void showFirmwareOptions(int index) {
    transitionToScreen([]() {
        M5.Lcd.fillScreen(BG_COLOR);
        drawHeader(false);
    });
    

    if (firmwareList[index].name == "Settings") {
        currentScreen = SETTINGS_MENU;
        drawSettingsMenu();
        return;
    }


    M5.Lcd.setCursor(10, 50);
    M5.Lcd.printf("Selected: %s", firmwareList[index].name.c_str());
    
    const char* options[] = {"Launch", "Delete", "Back"};
    uint16_t colors[] = {BUTTON_COLOR, DANGER_COLOR, BUTTON_COLOR};
    
    for (int i = 0; i < 3; i++) {
        int yPos = 90 + i * 60;
        int xPos = -SCREEN_WIDTH;
        
        while (xPos < 40) {
            M5.Lcd.fillRect(0, yPos, SCREEN_WIDTH, BUTTON_HEIGHT, BG_COLOR);
            drawButton(xPos, yPos, SCREEN_WIDTH - 80, BUTTON_HEIGHT, 
                      options[i], colors[i], false);
            xPos += 20;
            delay(5);
        }
    }
    
    bool waiting = true;
    while (waiting) {
        M5.update();
        if (M5.Touch.ispressed()) {
            TouchPoint_t p = M5.Touch.getPressPoint();
            
            if (p.y >= 90 && p.y < 140) {  // Launch
                executeFirmware(firmwareList[index].path);
                waiting = false;
            }
            else if (p.y >= 150 && p.y < 200) {  // Delete
                if (confirmDelete()) {
                    deleteFirmware(index);
                    waiting = false;
                }
            }
            else if (p.y >= 210 && p.y < 260) {  // Back
                waiting = false;
            }
            
            while (M5.Touch.ispressed()) {
                M5.update();
                delay(10);
            }
        }
        delay(10);
    }
    
    transitionToScreen(drawScreen);
}



void deleteAllFirmware() {
    File root = SD.open(FIRMWARE_PATH);
    if (!root) {
        showPopup("Failed to open directory", TFT_RED);
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String path = String(FIRMWARE_PATH) + "/" + file.name();
            SD.remove(path);
        }
        file = root.openNextFile();
    }
    root.close();
    
    loadFirmwareList();
    showPopup("All firmware deleted", SUCCESS_COLOR);
}

void deleteFirmware(int index) {
    String path = firmwareList[index].path;
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    if (SD.remove(path)) {
        showPopup("Firmware deleted!", SUCCESS_COLOR);
        loadFirmwareList();
    } else {
        showPopup("Delete failed!", TFT_RED);
    }
}

void handleButtons() {
    if (!uiState.isAnimating) {
        if (currentScreen == MAIN_MENU) {
            int totalItems = firmwareList.size(); // Total number of items including Settings
            
            if (M5.BtnA.wasPressed() && uiState.selectedIndex > 0) {
                uiState.selectedIndex--;
                uiState.isAnimating = true;
                animationStartTime = millis();
                targetOffset = CIRCLE_SPACING;
                lastDrawTime = 0;
            }
            else if (M5.BtnC.wasPressed() && uiState.selectedIndex < (totalItems - 1)) {
                uiState.selectedIndex++;
                uiState.isAnimating = true;
                animationStartTime = millis();
                targetOffset = -CIRCLE_SPACING;
                lastDrawTime = 0;
            }
            else if (M5.BtnB.wasPressed()) {
                // Check if the selected entry is the Settings entry
                if (firmwareList[uiState.selectedIndex].name == "Settings") {
                    // Switch to settings menu
                    currentScreen = SETTINGS_MENU;
                    drawSettingsMenu();
                } else {
                    // Handle firmware entry
                    showFirmwareOptions(uiState.selectedIndex);
                }
            }
        }
        else if (currentScreen == SETTINGS_MENU) {
            if (M5.BtnA.wasPressed()) {
                // Delete All
                if (confirmDelete()) {
                    deleteAllFirmware();
                    currentScreen = MAIN_MENU;
                    drawScreen();
                }
            }
            else if (M5.BtnB.wasPressed()) {
                // WiFi Mode
                startWiFiServer();
            }
            else if (M5.BtnC.wasPressed()) {
                // Back to main menu
                currentScreen = MAIN_MENU;
                drawScreen();
            }
        }
    }
}


void updateAnimation() {
    if (uiState.isAnimating && (millis() - uiState.lastAnimationUpdate > ANIMATION_DELAY)) {
        uiState.animationStep++;
        if (uiState.animationStep >= ANIMATION_STEPS) {
            uiState.isAnimating = false;
            uiState.animationStep = 0;
        }
        uiState.lastAnimationUpdate = millis();
        drawScreen();
        
    }
}

bool confirmDelete() {
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);
    
    M5.Lcd.setCursor(20, 60);
    M5.Lcd.print("Are you sure you want");
    M5.Lcd.setCursor(20, 85);
    M5.Lcd.print("to delete this firmware?");
    
    drawButton(40, 160, 100, 40, "Yes", DANGER_COLOR, false);
    drawButton(180, 160, 100, 40, "No", BUTTON_COLOR, false);
    
    bool confirmed = false;
    bool waiting = true;
    
    while (waiting) {
        M5.update();
        if (M5.Touch.ispressed()) {
            TouchPoint_t p = M5.Touch.getPressPoint();
            if (p.y >= 160 && p.y <= 200) {
                if (p.x >= 40 && p.x <= 140) {
                    confirmed = true;
                    waiting = false;
                }
                else if (p.x >= 180 && p.x <= 280) {
                    confirmed = false;
                    waiting = false;
                }
            }
            while (M5.Touch.ispressed()) {
                M5.update();
                delay(10);
            }
        }
        delay(10);
    }
    
    return confirmed;
}

void handleTouch() {
    static uint32_t lastTouchTime = 0;
    const uint32_t DEBOUNCE_TIME = 300;

    if (M5.Touch.ispressed()) {
        uint32_t currentTime = millis();
        if (currentTime - lastTouchTime < DEBOUNCE_TIME) {
            return;
        }
        lastTouchTime = currentTime;

        TouchPoint_t point = M5.Touch.getPressPoint();

        if (currentScreen == MAIN_MENU) {
            // Settings button (top-right corner)
            if (point.x >= SCREEN_WIDTH - 50 && point.y <= HEADER_HEIGHT) {
                currentScreen = SETTINGS_MENU;
                transitionToScreen([]() {
                    M5.Lcd.fillScreen(BG_COLOR);
                    drawHeader(false);
                    drawButton(10, 60, SCREEN_WIDTH - 20, BUTTON_HEIGHT, "Delete All", DANGER_COLOR, false);
                    drawButton(10, 120, SCREEN_WIDTH - 20, BUTTON_HEIGHT, "Back", BUTTON_COLOR, false);
                });
                return;
            }

            // WiFi upload button
            if (point.y >= 190 && point.y <= 230 && point.x >= 90 && point.x <= 230) {
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.disconnect();
                    drawScreen();
                } else {
                    startWiFiServer();
                }
                return;
            }

            // Firmware list
            if (point.y >= 50 && point.y < 190) {
                int index = ((point.y - 50) / (BUTTON_HEIGHT + BUTTON_SPACING)) + (currentPage * MAX_ITEMS_PER_PAGE);
                if (index >= 0 && index < firmwareList.size()) {
                    showFirmwareOptions(index);
                }
                return;
            }
            if (point.y >= 50 && point.y < 190) {
                int index = ((point.y - 50) / (BUTTON_HEIGHT + BUTTON_SPACING)) + (currentPage * MAX_ITEMS_PER_PAGE);
            if (index >= 0 && index < firmwareList.size()) {
                if (firmwareList[index].name == "Settings") {
                    currentScreen = SETTINGS_MENU;
                    drawSettingsMenu();
            }   else {
                    showFirmwareOptions(index);
            }
        }}
        return;

            // Navigation buttons
            if (point.y >= 190 && point.y <= 230) {
                if (currentPage > 0 && point.x >= 10 && point.x <= 80) {
                    currentPage--;
                    drawScreen();
                }
                else if (currentPage < totalPages - 1 && point.x >= SCREEN_WIDTH - 80 && point.x <= SCREEN_WIDTH - 10) {
                    currentPage++;
                    drawScreen();
                }
            }
        }
        else if (currentScreen == SETTINGS_MENU) {
            if (point.y >= 60 && point.y <= 110) {  // Delete All button
                if (confirmDelete()) {
                    deleteAllFirmware();
                }
            }
            else if (point.y >= 120 && point.y <= 170) {  // Back button
                currentScreen = MAIN_MENU;
                drawScreen();
            }
        }

        while (M5.Touch.ispressed()) {
            M5.update();
            delay(10);
        }
    }
}


void setup() {
    // Initialize Serial first for debugging
    Serial.begin(115200);
    Serial.println("Starting setup...");

    enforceFactoryBoot();
    generateGearIcon();
    generateDefaultIcons();
    
    // Create necessary directories
    if (!SD.exists(FIRMWARE_PATH)) {
        SD.mkdir(FIRMWARE_PATH);
    }
    if (!SD.exists(ICON_PATH)) {
        SD.mkdir(ICON_PATH);
    }

    // Initialize NVS to store boot state
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP.restart();
    }
    
    Preferences preferences;
    preferences.begin("launcher", false);
    bool firstBoot = preferences.getBool("first_boot", true);

    // Get current partitions
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    
    Serial.printf("Currently running from partition: %s at 0x%x\n", 
                 running ? running->label : "NULL",
                 running ? running->address : 0);
                 
    Serial.printf("Factory partition: %s at 0x%x\n", 
                 factory ? factory->label : "NULL",
                 factory ? factory->address : 0);

    // If this is first boot or we're not in factory partition
    if (firstBoot || (factory != NULL && running != factory)) {
        Serial.println("First boot or not in factory partition, enforcing factory boot...");
        
        // Mark first boot as complete
        preferences.putBool("first_boot", false);
        preferences.end();

        // Clear all OTA data
        const esp_partition_t* otadata = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
        if (otadata != NULL) {
            Serial.println("Erasing OTA data...");
            esp_partition_erase_range(otadata, 0, otadata->size);
        }

        // Set boot partition to factory
        if (factory != NULL) {
            Serial.println("Setting boot partition to factory...");
            err = esp_ota_set_boot_partition(factory);
            if (err != ESP_OK) {
                Serial.printf("Failed to set boot partition: %s\n", esp_err_to_name(err));
            }
        }

        if (factory && running != factory) {
        Serial.println("Not running from factory partition, restarting...");
        ESP.restart();
        return;
    }
    }

        M5.begin(true, true, true, true);
        initAnimationBuffer();
    
    // Initialize SD Card
    if (!SD.begin()) {
        Serial.println("SD Card Mount Failed!");
        M5.Lcd.fillScreen(BG_COLOR);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.println("SD Card Mount Failed!");
        while(1) delay(100);
    }

    // Create firmware directory if it doesn't exist
    if (!SD.exists(FIRMWARE_PATH)) {
        SD.mkdir(FIRMWARE_PATH);
    }

    // Initialize WiFi in disconnected state
    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true);
    delay(100);

    // Load firmware list and initialize UI
    loadFirmwareList();
    totalPages = (firmwareList.size() + MAX_ITEMS_PER_PAGE - 1) / MAX_ITEMS_PER_PAGE;
    currentPage = 0;
    drawScreen();

    Serial.println("Setup completed successfully");
}

bool executeFirmware(const String& path) {
    Serial.printf("Starting firmware update from: %s\n", path.c_str());

    // Get current partitions
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    
    if (!update_partition) {
        Serial.println("No update partition found");
        showPopup("Partition Error!", TFT_RED);
        return false;
    }

    Serial.printf("Current running partition: %s\n", running->label);
    Serial.printf("Update partition target: %s\n", update_partition->label);

    // Open and verify firmware file
    File firmware = SD.open(path);
    if (!firmware) {
        showPopup("Failed to open file!", TFT_RED);
        return false;
    }

    size_t firmware_size = firmware.size();
    if (firmware_size == 0 || firmware_size > update_partition->size) {
        showPopup("Invalid firmware size!", TFT_RED);
        firmware.close();
        return false;
    }

    // Start OTA update
    esp_ota_handle_t update_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        Serial.printf("esp_ota_begin failed: %s\n", esp_err_to_name(err));
        firmware.close();
        showPopup("Update failed!", TFT_RED);
        return false;
    }

    // Show progress screen
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);
    M5.Lcd.setCursor(10, 60);
    M5.Lcd.print("Installing firmware...");
    M5.Lcd.drawRect(10, 100, 300, 30, TEXT_COLOR);

    // Flash firmware
    size_t written = 0;
    uint8_t buf[4096];
    
    while (firmware.available()) {
        size_t toRead = min((size_t)firmware.available(), sizeof(buf));
        if (firmware.read(buf, toRead) != toRead) {
            Serial.println("Failed to read firmware file");
            esp_ota_abort(update_handle);
            firmware.close();
            showPopup("Read Error!", TFT_RED);
            return false;
        }

        err = esp_ota_write(update_handle, buf, toRead);
        if (err != ESP_OK) {
            Serial.printf("esp_ota_write failed: %s\n", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            firmware.close();
            showPopup("Write Error!", TFT_RED);
            return false;
        }

        written += toRead;
        int progress = (written * 100) / firmware_size;
        M5.Lcd.fillRect(12, 102, 296 * progress / 100, 26, SUCCESS_COLOR);
        M5.Lcd.setCursor(140, 140);
        M5.Lcd.printf("%d%%", progress);
    }

    firmware.close();

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        Serial.printf("esp_ota_end failed: %s\n", esp_err_to_name(err));
        showPopup("Update failed!", TFT_RED);
        return false;
    }

    // Launch menu
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);
    M5.Lcd.setCursor(10, 60);
    M5.Lcd.print("Update successful!");
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.print("What would you like to do?");
    
    drawButton(20, 140, 280, 40, "Launch Now", BUTTON_COLOR, false);
    drawButton(20, 190, 280, 40, "Stay in Launcher", BUTTON_COLOR, false);
    
    bool waiting = true;
    bool launch = false;
    
    while (waiting) {
        M5.update();
        if (M5.Touch.ispressed()) {
            TouchPoint_t p = M5.Touch.getPressPoint();
            if (p.x >= 20 && p.x <= 300) {
                if (p.y >= 140 && p.y <= 180) {  // Launch Now
                    launch = true;
                    waiting = false;
                }
                else if (p.y >= 190 && p.y <= 230) {  // Stay in Launcher
                    launch = false;
                    waiting = false;
                }
            }
            while (M5.Touch.ispressed()) {
                M5.update();
                delay(10);
            }
        }
        delay(10);
    }

    if (launch) {
        // Set boot partition to the update partition and launch
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            Serial.printf("esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
            showPopup("Boot set failed!", TFT_RED);
            return false;
        }
        Serial.println("Rebooting to launch firmware...");
        ESP.restart();
    } else {
        // Keep factory partition as boot partition
        if (factory != NULL) {
            err = esp_ota_set_boot_partition(factory);
            if (err != ESP_OK) {
                Serial.printf("Failed to restore factory boot partition: %s\n", esp_err_to_name(err));
            }
        }
        // Return to main menu
        drawScreen();
    }

    return true;
} 

void checkResetToLauncher() {
    if (M5.BtnA.isPressed() && M5.BtnB.isPressed() && M5.BtnC.isPressed()) {
        delay(1000);  // Debounce delay
        if (M5.BtnA.isPressed() && M5.BtnB.isPressed() && M5.BtnC.isPressed()) {
            Serial.println("Resetting to launcher...");
            ESP.restart();
        }
    }
}

void loop() {
    M5.update();
    
    // Check for reset to launcher
    checkResetToLauncher();
    
    // Handle button navigation
    handleButtons();
    
    // Update animation
    if (uiState.isAnimating) {
        updateAnimation();
    }
    
    // Feed watchdog
    esp_task_wdt_reset();
    delay(10);
}
