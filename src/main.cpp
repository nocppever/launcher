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
};

enum Screen {
    MAIN_SCREEN,
    SETTINGS_SCREEN,
    WIFI_SCREEN,
    FIRMWARE_OPTIONS_SCREEN
};

Screen currentScreen = MAIN_SCREEN;
std::vector<FirmwareEntry> firmwareList;
int currentPage = 0;
int totalPages = 0;

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

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".bin")) {
            FirmwareEntry entry;
            entry.name = file.name();
            entry.path = String(FIRMWARE_PATH) + "/" + file.name();
            firmwareList.push_back(entry);
        }
        file = root.openNextFile();
    }
    root.close();
}

void drawScreen() {
    M5.Lcd.fillScreen(BG_COLOR);
    drawHeader(false);
    
    totalPages = (firmwareList.size() + MAX_ITEMS_PER_PAGE - 1) / MAX_ITEMS_PER_PAGE;
    int startIdx = currentPage * MAX_ITEMS_PER_PAGE;
    int endIdx = min(startIdx + MAX_ITEMS_PER_PAGE, (int)firmwareList.size());
    
    for (int i = startIdx; i < endIdx; i++) {
        int yPos = 50 + (i - startIdx) * (BUTTON_HEIGHT + BUTTON_SPACING);
        int xPos = -SCREEN_WIDTH;
        
        while (xPos < 10) {
            M5.Lcd.fillRect(0, yPos, SCREEN_WIDTH, BUTTON_HEIGHT, BG_COLOR);
            drawButton(xPos, yPos, SCREEN_WIDTH - 20, BUTTON_HEIGHT, 
                      firmwareList[i].name.c_str(), BUTTON_COLOR, false);
            xPos += 20;
            delay(5);
        }
    }
    
    drawButton(90, 190, 140, 40, "WiFi Upload", BUTTON_COLOR, false);
    
    if (totalPages > 1) {
        if (currentPage > 0) {
            drawButton(10, 190, 70, 40, "<<", BUTTON_COLOR, false);
        }
        if (currentPage < totalPages - 1) {
            drawButton(SCREEN_WIDTH - 80, 190, 70, 40, ">>", BUTTON_COLOR, false);
        }
        
        char pageText[20];
        sprintf(pageText, "%d/%d", currentPage + 1, totalPages);
        M5.Lcd.setTextColor(TEXT_COLOR);
        M5.Lcd.setCursor((SCREEN_WIDTH - strlen(pageText) * 12) / 2, 200);
        M5.Lcd.print(pageText);
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
        String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>M5Stack Firmware Uploader</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #1a1a2e, #16213e);
            color: #fff;
            min-height: 100vh;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: rgba(255, 255, 255, 0.1);
            padding: 30px;
            border-radius: 15px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);
            backdrop-filter: blur(10px);
        }
        h1 {
            color: #fff;
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
        }
        .upload-form {
            display: flex;
            flex-direction: column;
            gap: 20px;
            align-items: center;
        }
        .file-input-container {
            width: 100%;
            position: relative;
        }
        .file-input {
            width: 100%;
            padding: 15px;
            border: 2px dashed #4a90e2;
            border-radius: 10px;
            background: rgba(255, 255, 255, 0.05);
            color: #fff;
            cursor: pointer;
            transition: all 0.3s ease;
        }
        .file-input:hover {
            border-color: #64b5f6;
            background: rgba(255, 255, 255, 0.1);
        }
        .submit-btn {
            padding: 15px 40px;
            background: #4a90e2;
            color: white;
            border: none;
            border-radius: 25px;
            font-size: 1.1em;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .submit-btn:hover {
            background: #357abd;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(74, 144, 226, 0.3);
        }
        .progress {
            width: 100%;
            height: 10px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 5px;
            margin-top: 20px;
            overflow: hidden;
            display: none;
        }
        .progress-bar {
            width: 0%;
            height: 100%;
            background: #4a90e2;
            transition: width 0.3s ease;
        }
        @media (max-width: 480px) {
            .container {
                padding: 20px;
            }
            h1 {
                font-size: 2em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>M5Stack Firmware Uploader</h1>
        <form class="upload-form" method="POST" action="/upload" enctype="multipart/form-data">
            <div class="file-input-container">
                <input type="file" name="firmware" accept=".bin" class="file-input" required>
            </div>
            <button type="submit" class="submit-btn">Upload Firmware</button>
            <div class="progress">
                <div class="progress-bar"></div>
            </div>
        </form>
    </div>
    <script>
        document.querySelector('.upload-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const formData = new FormData(this);
            const progress = document.querySelector('.progress');
            const progressBar = document.querySelector('.progress-bar');
            const submitBtn = document.querySelector('.submit-btn');
            
            progress.style.display = 'block';
            submitBtn.disabled = true;
            submitBtn.textContent = 'Uploading...';
            
            fetch('/upload', {
                method: 'POST',
                body: formData
            }).then(response => {
                if (response.ok) {
                    progressBar.style.width = '100%';
                    submitBtn.textContent = 'Upload Complete!';
                    setTimeout(() => {
                        progress.style.display = 'none';
                        submitBtn.disabled = false;
                        submitBtn.textContent = 'Upload Firmware';
                        progressBar.style.width = '0%';
                    }, 2000);
                }
            }).catch(error => {
                submitBtn.textContent = 'Upload Failed';
                submitBtn.disabled = false;
                setTimeout(() => {
                    submitBtn.textContent = 'Upload Firmware';
                }, 2000);
            });
        });
    </script>
</body>
</html>
)";
        request->send(200, "text/html", html);
    });

    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) { 
            request->send(200, "text/plain", "File uploaded successfully"); 
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            static File file;
            
            if (!index) {
                file = SD.open(String(FIRMWARE_PATH) + "/" + filename, FILE_WRITE);
            }
            
            if (file && len) {
                file.write(data, len);
            }
            
            if (final) {
                if (file) file.close();
                loadFirmwareList();
                showPopup("Firmware Uploaded!", SUCCESS_COLOR);
            }
    });

    server.begin();
    
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

        if (currentScreen == MAIN_SCREEN) {
            // Settings button (top-right corner)
            if (point.x >= SCREEN_WIDTH - 50 && point.y <= HEADER_HEIGHT) {
                currentScreen = SETTINGS_SCREEN;
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
        else if (currentScreen == SETTINGS_SCREEN) {
            if (point.y >= 60 && point.y <= 110) {  // Delete All button
                if (confirmDelete()) {
                    deleteAllFirmware();
                }
            }
            else if (point.y >= 120 && point.y <= 170) {  // Back button
                currentScreen = MAIN_SCREEN;
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

void loop() {
    M5.update();
    
    // Check for long button press to force factory boot
    if (checkButtonPress()) {
        Serial.println("Long button press detected, forcing factory boot...");
        forceFactoryBoot();
        ESP.restart();
    }
    
    // Handle touch events
    handleTouch();
    
    // Feed watchdog
    esp_task_wdt_reset();
    
    delay(10);
}