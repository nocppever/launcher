#include <M5Core2.h>
#include <vector>
#include <string>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Update.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>

// Network credentials
const char* ssid = "projetut";
const char* password = "projetutL1214";

// BLE characteristics UUIDs
#define SERVICE_UUID        ""
#define CHARACTERISTIC_UUID ""

// UI Constants
const uint16_t BACKGROUND_COLOR = 0x1B1B;
const uint16_t TEXT_COLOR = 0xFFFF;
const uint16_t SELECTED_COLOR = 0x2C6B;
const uint16_t ITEM_COLOR = 0x2D2D;

// Forward declarations
void drawInterface();
void processBLEData(std::string data);

// Firmware structure
struct FirmwareItem {
    String name;
    void (*function)();
    String version;
};

// Example firmware functions
void firmware1() {
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Firmware 1 Running");
    M5.Lcd.println("\nTouch screen to return");
    
    while(true) {
        M5.update();
        if (M5.Touch.ispressed()) {
            return;
        }
        delay(10);
    }
}

void firmware2() {
    M5.Lcd.fillScreen(GREEN);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Firmware 2 Running");
    M5.Lcd.println("\nTouch screen to return");
    
    while(true) {
        M5.update();
        if (M5.Touch.ispressed()) {
            return;
        }
        delay(10);
    }
}

void firmware3() {
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Firmware 3 Running");
    M5.Lcd.println("\nTouch screen to return");
    
    while(true) {
        M5.update();
        if (M5.Touch.ispressed()) {
            return;
        }
        delay(10);
    }
}

// Global variables
std::vector<FirmwareItem> firmwareList;
int selectedIndex = 0;
WebServer server(80);
BLEServer* pServer = nullptr;
Preferences preferences;
bool networkMode = false;

// BLE Callback class
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            processBLEData(value);
        }
    }
};

void processBLEData(std::string data) {
    // Process incoming BLE data
    if (data.substr(0, 3) == "CMD") {
        // Handle commands
    } else {
        // Handle firmware data
    }
}

void initializeFirmwareList() {
    firmwareList.push_back({"Demo 1 (Red)", firmware1, "1.0"});
    firmwareList.push_back({"Demo 2 (Green)", firmware2, "1.0"});
    firmwareList.push_back({"Demo 3 (Blue)", firmware3, "1.0"});
}

// Web update page
const char* serverIndex = 
"<html><head><title>Firmware Upload</title></head><body>"
"<h1>M5Stack Core2 Firmware Upload</h1>"
"<form method='POST' action='/update' enctype='multipart/form-data'>"
"<input type='file' name='update'><input type='submit' value='Upload'>"
"</form></body></html>";

void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    M5.Lcd.fillScreen(BACKGROUND_COLOR);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.print("Connecting to WiFi...");
    
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
        delay(500);
        attempt++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.println("\nConnected!");
        M5.Lcd.print("IP: ");
        M5.Lcd.println(WiFi.localIP());
        
        if (MDNS.begin("m5stack")) {
            MDNS.addService("http", "tcp", 80);
        }
        
        server.on("/", HTTP_GET, []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/html", serverIndex);
        });
        
        server.on("/update", HTTP_POST, []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart();
        }, []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    Serial.printf("Update Success: %u\n", upload.totalSize);
                } else {
                    Update.printError(Serial);
                }
            }
        });
        
        server.begin();
    } else {
        M5.Lcd.println("\nWiFi connection failed!");
        delay(2000);
    }
}

void setupBLE() {
    BLEDevice::init("M5Stack Core2");
    pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );
    
    pCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

void toggleNetworkMode() {
    networkMode = !networkMode;
    if (networkMode) {
        setupWiFi();
        setupBLE();
        M5.Lcd.println("Network Mode Active");
        M5.Lcd.println("WiFi IP: " + WiFi.localIP().toString());
        M5.Lcd.println("BLE: M5Stack Core2");
    } else {
        WiFi.disconnect(true);
        BLEDevice::deinit(true);
        drawInterface();
    }
}

void drawInterface() {
    M5.Lcd.fillScreen(BACKGROUND_COLOR);
    
    // Draw header
    M5.Lcd.fillRect(0, 0, 320, 40, SELECTED_COLOR);
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(20, 10);
    M5.Lcd.print("Firmware Launcher");
    
    // Draw menu items
    for (size_t i = 0; i < firmwareList.size(); i++) {
        int yPos = 50 + (i * 45);
        
        if (i == selectedIndex) {
            M5.Lcd.fillRoundRect(10, yPos, 300, 40, 8, SELECTED_COLOR);
        } else {
            M5.Lcd.fillRoundRect(10, yPos, 300, 40, 8, ITEM_COLOR);
        }
        
        M5.Lcd.setTextColor(TEXT_COLOR);
        M5.Lcd.setCursor(25, yPos + 10);
        M5.Lcd.print(firmwareList[i].name);
        M5.Lcd.setCursor(230, yPos + 10);
        M5.Lcd.print("v");
        M5.Lcd.print(firmwareList[i].version);
    }
    
    // Draw network mode button
    M5.Lcd.fillRoundRect(10, 280, 300, 30, 8, networkMode ? SELECTED_COLOR : ITEM_COLOR);
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.setCursor(25, 285);
    M5.Lcd.print(networkMode ? "Network Mode: ON" : "Network Mode: OFF");
}

void setup() {
    M5.begin();
    M5.Lcd.setRotation(0);
    M5.Lcd.setBrightness(100);
    
    preferences.begin("firmware", false);
    initializeFirmwareList();
    drawInterface();
}

void loop() {
    M5.update();
    
    if (networkMode) {
        server.handleClient();
    }
    
    if (M5.Touch.ispressed()) {
        TouchPoint_t pos = M5.Touch.getPressPoint();
        
        if (pos.y >= 280) {
            toggleNetworkMode();
            delay(100);
            return;
        }
        
        if (pos.y >= 50 && pos.y < 280) {
            int itemIndex = (pos.y - 50) / 45;
            if (itemIndex >= 0 && itemIndex < static_cast<int>(firmwareList.size())) {
                selectedIndex = itemIndex;
                drawInterface();
                delay(100);
                
                firmwareList[selectedIndex].function();
                drawInterface();
            }
        }
    }
    
    delay(5);
}