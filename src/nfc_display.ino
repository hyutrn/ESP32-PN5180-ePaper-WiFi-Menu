/*
 * NFC Reader with ePaper Display and WiFi Menu System
 * Hardware: ESP32 + PN5180 NFC Reader + 3-color ePaper Display
 * Features: NFC card reading, WiFi configuration menu system, dual-button interface
 */

// ========== PIN CONFIGURATION ==========
constexpr uint8_t PN5180_NSS_PIN = 16;    // SPI Chip Select for PN5180
constexpr uint8_t PN5180_BUSY_PIN = 5;    // Busy pin for PN5180
constexpr uint8_t PN5180_RST_PIN = 17;    // Reset pin for PN5180

// ========== E-PAPER DISPLAY PINOUT ==========
constexpr uint8_t EPD_CS_PIN = 15;        // Chip Select for ePaper display
constexpr uint8_t EPD_DC_PIN = 2;         // Data/Command pin for ePaper
constexpr uint8_t EPD_RST_PIN = 4;        // Reset pin for ePaper
constexpr uint8_t EPD_BUSY_PIN = 13;      // Busy pin for ePaper

// ========== BUTTON CONFIGURATION ==========
constexpr uint8_t MODE_BUTTON_PIN = 14;   // Button 1: Mode switch (NFC/WiFi)
constexpr uint8_t SELECT_BUTTON_PIN = 27; // Button 2: Menu selection

// ========== WIFI CONFIGURATION ==========
const char* WIFI_SSID = "Me Select IT";   // Default WiFi SSID

// ========== SYSTEM INCLUDES ==========
#include <PN5180.h>
#include <PN5180ISO14443.h>
#include <PN5180ISO15693.h>
#include <SPI.h>

// ========== E-PAPER DISPLAY LIBRARIES ==========
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

// ========== HARDWARE OBJECT INSTANTIATION ==========
// ePaper Display object (3-color, 2.66-inch)
GxEPD2_3C<GxEPD2_266c, GxEPD2_266c::HEIGHT> display(
    GxEPD2_266c(/*CS=*/ EPD_CS_PIN, 
                /*DC=*/ EPD_DC_PIN, 
                /*RST=*/ EPD_RST_PIN, 
                /*BUSY=*/ EPD_BUSY_PIN));

// NFC Reader object (ISO14443 protocol)
PN5180ISO14443 nfc(PN5180_NSS_PIN, PN5180_BUSY_PIN, PN5180_RST_PIN);

// ========== SYSTEM STATE ENUMERATIONS ==========
/**
 * @brief Main system operation modes
 * 
 * MODE_NFC: NFC card reading mode
 * MODE_WIFI: WiFi configuration mode
 */
enum SystemMode {
    MODE_NFC = 0,
    MODE_WIFI = 1
};

/**
 * @brief WiFi mode sub-states
 * 
 * WIFI_SUB_NONE: Main WiFi menu
 * WIFI_SUB_SCAN: WiFi scanning submenu
 * WIFI_SUB_MANUAL: Manual WiFi connection submenu
 */
enum WifiSubMode {
    WIFI_SUB_NONE = 0,
    WIFI_SUB_SCAN = 1,
    WIFI_SUB_MANUAL = 2
};

// ========== SYSTEM STATE VARIABLES ==========
// Mode Management
volatile SystemMode currentMode = MODE_NFC;
SystemMode lastDisplayedMode = MODE_NFC;
WifiSubMode currentWifiSubMode = WIFI_SUB_NONE;
WifiSubMode lastWifiSubMode = WIFI_SUB_NONE;

// Display State
bool displayInitialized = false;
bool displayUpdateRequired = true;
bool systemReady = false;

// ========== BUTTON HANDLING VARIABLES ==========
// Mode Button (Button 1) State
volatile bool modeButtonPressed = false;
volatile bool modeButtonLongPressed = false;
volatile unsigned long modeButtonPressTime = 0;
const unsigned long LONG_PRESS_DURATION = 1000; // 1 second for long press

// Select Button (Button 2) State
volatile bool selectButtonPressed = false;
volatile unsigned long selectButtonPressTime = 0;
volatile unsigned long lastSelectDebounceTime = 0;
const unsigned long SELECT_DEBOUNCE_TIME = 200; // 200ms debounce

// ========== MENU NAVIGATION VARIABLES ==========
int wifiMenuSelection = 0;    // 0: Scan WiFi, 1: Manual Connect
int scanWifiSelection = 0;    // Selected WiFi network index

// ========== NFC READING STATE ==========
String lastUID = "";
bool cardDetected = false;
uint8_t lastValidUID[10] = {0};
int lastValidLength = 0;

// ========== WIFI DEMO DATA ==========
String wifiSSID = "Me Select IT";
String wifiIP = "192.168.1.100";
String manualSSID = "";
String manualPassword = "";
int manualInputField = 0; // 0: SSID field, 1: Password field

// ========== DEMO WIFI NETWORKS ==========
const int MAX_WIFI_NETWORKS = 5;
String wifiNetworks[MAX_WIFI_NETWORKS] = {
    "Me Select S",
    "Me Select IT",
    "Qinef",
    "Free_WiFi",
    ""
};

// ========== INTERRUPT SERVICE ROUTINES ==========
/**
 * @brief Interrupt handler for Mode button (GPIO14)
 * 
 * Handles both short press and long press detection
 * Triggered on both rising and falling edges
 */
void IRAM_ATTR handleModeButtonInterrupt() {
    unsigned long currentTime = millis();
    
    if (digitalRead(MODE_BUTTON_PIN) == LOW) {
        // Button pressed down - record start time
        modeButtonPressTime = currentTime;
    } else {
        // Button released - calculate press duration
        unsigned long pressDuration = currentTime - modeButtonPressTime;
        
        // Debounce check (minimum 50ms press)
        if (pressDuration > 50) {
            if (pressDuration >= LONG_PRESS_DURATION) {
                modeButtonLongPressed = true;
            } else {
                modeButtonPressed = true;
            }
        }
    }
}

/**
 * @brief Interrupt handler for Select button (GPIO27)
 * 
 * Handles button press with debouncing
 * Triggered on falling edge only
 */
void IRAM_ATTR handleSelectButtonInterrupt() {
    unsigned long currentTime = millis();
    
    // Debounce logic
    if (currentTime - lastSelectDebounceTime > SELECT_DEBOUNCE_TIME) {
        if (digitalRead(SELECT_BUTTON_PIN) == LOW) {
            selectButtonPressed = true;
            selectButtonPressTime = currentTime;
        }
        lastSelectDebounceTime = currentTime;
    }
}

// ========== DISPLAY UTILITY FUNCTIONS ==========
/**
 * @brief Draws a header section with specified text
 * 
 * @param text Header text to display
 * 
 * Creates a red header bar (50px height) with centered white text
 */
void drawHeader(String text) {
    // Draw red header background
    display.fillRect(0, 0, display.width(), 50, GxEPD_RED);
    
    // Configure text properties
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeMonoBold18pt7b);
    
    // Calculate text bounds for centering
    int16_t textBoundsX, textBoundsY;
    uint16_t textWidth, textHeight;
    display.getTextBounds(text, 0, 0, &textBoundsX, &textBoundsY, &textWidth, &textHeight);
    
    // Center text horizontally
    uint16_t xPosition = (display.width() - textWidth) / 2 - textBoundsX;
    
    // Center text vertically within 50px header
    uint16_t yPosition = (50 - textHeight) / 2 - textBoundsY;
    yPosition += 2; // Minor vertical adjustment
    
    // Draw centered text
    display.setCursor(xPosition, yPosition);
    display.println(text);
}

/**
 * @brief Draws centered text at specified vertical position
 * 
 * @param text Text to display
 * @param yPos Vertical position for text
 * @param font Pointer to font to use
 * @param color Text color
 * 
 * Centers text horizontally on the display
 */
void drawCenteredText(String text, uint16_t yPos, const GFXfont* font, uint16_t color) {
    display.setFont(font);
    display.setTextColor(color);
    
    // Calculate text bounds for centering
    int16_t textBoundsX, textBoundsY;
    uint16_t textWidth, textHeight;
    display.getTextBounds(text, 0, 0, &textBoundsX, &textBoundsY, &textWidth, &textHeight);
    
    // Calculate centered X position
    uint16_t xPosition = (display.width() - textWidth) / 2 - textBoundsX;
    
    // Draw centered text
    display.setCursor(xPosition, yPos);
    display.println(text);
}

// ========== E-PAPER DISPLAY SCREENS ==========
/**
 * @brief Initializes the ePaper display if not already initialized
 * 
 * Sets up SPI communication and display rotation
 */
void initDisplay() {
    if (!displayInitialized) {
        Serial.println("Initializing ePaper display...");
        display.init(115200, true, 2, false);
        display.setRotation(1); // 90-degree landscape rotation
        display.clearScreen();
        displayInitialized = true;
        Serial.println("ePaper display initialized!");
    }
}

/**
 * @brief Displays the NFC ready screen (no card detected)
 * 
 * Shows NFC mode header with instructions
 */
void displayNoCardScreen() {
    initDisplay();
    
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    
    // Draw NFC mode header
    drawHeader("NFC MODE");
    
    // Define layout positions
    const uint16_t LEFT_MARGIN = 20;
    const uint16_t READY_Y_POSITION = 80;
    const uint16_t STANDARD_Y_POSITION = 105;
    const uint16_t INSTRUCTION_Y_POSITION = display.height() - 15;
    
    // Display centered status messages
    drawCenteredText("Tap card to read", READY_Y_POSITION, &FreeMonoBold12pt7b, GxEPD_BLACK);
    drawCenteredText("ISO14443A", STANDARD_Y_POSITION, &FreeMonoBold9pt7b, GxEPD_BLACK);
    
    // Display left-aligned instructions
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(LEFT_MARGIN, INSTRUCTION_Y_POSITION);
    display.println("Press B1 to switch mode");
    
    display.display(true);
}

/**
 * @brief Displays detected card UID
 * 
 * @param uid Card UID string to display
 * 
 * Shows UID in large font with instructions
 */
void showUIDSmall(String uid) {
    initDisplay();
    
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    
    // Draw UID header
    drawHeader("Card UID");
    
    // Define layout positions
    const uint16_t LEFT_MARGIN = 20;
    const uint16_t INSTRUCTION_Y_POSITION = display.height() - 15;
    
    // Display centered UID
    drawCenteredText(uid, display.height() / 2 + 10, &FreeMonoBold18pt7b, GxEPD_BLACK);
    
    // Display instructions
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(LEFT_MARGIN, INSTRUCTION_Y_POSITION);
    display.println("Wait 10s to the next read");
    
    display.display(true);
}

/**
 * @brief Displays main WiFi mode menu
 * 
 * Shows two options: Scan WiFi and Manual Connect
 * Uses red highlight for selected option
 */
void displayWifiMainMenu() {
    initDisplay();
    
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    
    // Draw WiFi mode header
    drawHeader("WiFi MODE");
    
    // Define layout parameters
    const uint16_t LEFT_MARGIN = 20;
    const uint16_t START_Y_POSITION = 70;
    const uint16_t LINE_HEIGHT = 30;
    const uint16_t INSTRUCTION1_Y_POSITION = display.height() - 30;
    const uint16_t INSTRUCTION2_Y_POSITION = display.height() - 15;
    
    // Display menu options
    for (int optionIndex = 0; optionIndex < 2; optionIndex++) {
        uint16_t yPosition = START_Y_POSITION + (optionIndex * LINE_HEIGHT);
        
        if (optionIndex == wifiMenuSelection) {
            // Selected option: red with arrow
            display.setCursor(LEFT_MARGIN - 15, yPosition);
            display.setFont(&FreeMonoBold12pt7b);
            display.setTextColor(GxEPD_RED);
            display.print(">");
            
            display.setCursor(LEFT_MARGIN, yPosition);
            if (optionIndex == 0) {
                display.println("1. Scan WiFi");
            } else {
                display.println("2. Manual Connect");
            }
        } else {
            // Unselected option: black without arrow
            display.setCursor(LEFT_MARGIN, yPosition);
            display.setFont(&FreeMonoBold12pt7b);
            display.setTextColor(GxEPD_BLACK);
            if (optionIndex == 0) {
                display.println("1. Scan WiFi");
            } else {
                display.println("2. Manual Connect");
            }
        }
    }
    
    // Display centered instructions
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    
    // Instruction line 1: "Press B2 to select"
    String instruction1 = "Press B2 to select";
    int16_t textBoundsX1, textBoundsY1;
    uint16_t textWidth1, textHeight1;
    display.getTextBounds(instruction1, 0, 0, &textBoundsX1, &textBoundsY1, &textWidth1, &textHeight1);
    uint16_t xPosition1 = (display.width() - textWidth1) / 2 - textBoundsX1;
    display.setCursor(xPosition1, INSTRUCTION1_Y_POSITION);
    display.println(instruction1);
    
    // Instruction line 2: "Hold B1 to confirm"
    String instruction2 = "Hold B1 to confirm";
    int16_t textBoundsX2, textBoundsY2;
    uint16_t textWidth2, textHeight2;
    display.getTextBounds(instruction2, 0, 0, &textBoundsX2, &textBoundsY2, &textWidth2, &textHeight2);
    uint16_t xPosition2 = (display.width() - textWidth2) / 2 - textBoundsX2;
    display.setCursor(xPosition2, INSTRUCTION2_Y_POSITION);
    display.println(instruction2);
    
    display.display(true);
}

/**
 * @brief Displays WiFi scanning screen
 * 
 * Shows list of available WiFi networks with selection indicator
 */
void displayScanWifiScreen() {
    initDisplay();
    
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    
    // Draw WiFi scan header
    drawHeader("Scan WiFi");
    
    // Define layout parameters
    const uint16_t LEFT_MARGIN = 20;
    const uint16_t START_Y_POSITION = 70;
    const uint16_t LINE_HEIGHT = 25;
    
    // Display WiFi network list
    for (int networkIndex = 0; networkIndex < MAX_WIFI_NETWORKS; networkIndex++) {
        uint16_t yPosition = START_Y_POSITION + (networkIndex * LINE_HEIGHT);
        
        if (networkIndex == scanWifiSelection) {
            // Selected network: red with arrow
            display.setCursor(LEFT_MARGIN - 15, yPosition);
            display.setFont(&FreeMonoBold9pt7b);
            display.setTextColor(GxEPD_RED);
            display.print(">");
            
            display.setCursor(LEFT_MARGIN, yPosition);
            display.print(networkIndex + 1);
            display.print(". ");
            display.println(wifiNetworks[networkIndex]);
        } else {
            // Unselected network: black without arrow
            display.setCursor(LEFT_MARGIN, yPosition);
            display.setFont(&FreeMonoBold9pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.print(networkIndex + 1);
            display.print(". ");
            display.println(wifiNetworks[networkIndex]);
        }
    }
    
    display.display(true);
}

/**
 * @brief Displays manual WiFi connection screen
 * 
 * Shows SSID and password fields with input indicators
 */
void displayManualConnectScreen() {
    initDisplay();
    
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    
    // Draw manual connect header
    drawHeader("Manual Connect");
    
    // Define layout parameters
    const uint16_t LEFT_MARGIN = 20;
    const uint16_t START_Y_POSITION = 70;
    const uint16_t LINE_HEIGHT = 25;
    const uint16_t INSTRUCTION1_Y_POSITION = display.height() - 30;
    const uint16_t INSTRUCTION2_Y_POSITION = display.height() - 15;
    
    // Set base font and color
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Display SSID field
    display.setCursor(LEFT_MARGIN, START_Y_POSITION);
    display.print("SSID: ");
    
    if (manualInputField == 0) {
        display.setTextColor(GxEPD_BLUE); // Active field: blue
    }
    
    if (manualSSID.length() > 0) {
        display.println(manualSSID);
    } else {
        display.println("__________"); // Placeholder
    }
    
    // Display Password field
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(LEFT_MARGIN, START_Y_POSITION + LINE_HEIGHT);
    display.print("Password: ");
    
    if (manualInputField == 1) {
        display.setTextColor(GxEPD_BLUE); // Active field: blue
    }
    
    // Show password as asterisks
    String passwordDisplay = "";
    if (manualPassword.length() > 0) {
        for (int charIndex = 0; charIndex < manualPassword.length(); charIndex++) {
            passwordDisplay += "*";
        }
        display.println(passwordDisplay);
    } else {
        display.println("__________"); // Placeholder
    }
    
    // Draw selection arrow for active field
    display.setTextColor(GxEPD_RED);
    display.setCursor(LEFT_MARGIN - 15, START_Y_POSITION + (manualInputField * LINE_HEIGHT));
    display.print(">");
    
    // Display instructions
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    
    // Left-aligned instructions
    display.setCursor(LEFT_MARGIN, INSTRUCTION1_Y_POSITION);
    display.println("Press B2 to switch field");
    
    display.setCursor(LEFT_MARGIN, INSTRUCTION2_Y_POSITION);
    display.println("Press B1 to exit");
    
    display.display(true);
}

/**
 * @brief Displays system startup screen
 * 
 * Shows "Starting..." message during initialization
 */
void displayStartingScreen() {
    initDisplay();
    
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    
    // Display centered startup message
    drawCenteredText("Starting...", display.height() / 2, &FreeMonoBold18pt7b, GxEPD_BLACK);
    
    display.display(true);
}

// ========== BUTTON EVENT HANDLERS ==========
/**
 * @brief Handles Mode button short press events
 * 
 * Manages mode switching between NFC and WiFi
 * Handles submenu navigation in WiFi mode
 */
void handleModeButtonPress() {
    if (!systemReady) return;
    
    if (currentMode == MODE_NFC) {
        // Switch from NFC to WiFi mode
        currentMode = MODE_WIFI;
        currentWifiSubMode = WIFI_SUB_NONE;
        displayUpdateRequired = true;
        Serial.println("Mode switched to: WiFi");
    } 
    else if (currentMode == MODE_WIFI) {
        if (currentWifiSubMode != WIFI_SUB_NONE) {
            // Exit submenu, return to main WiFi menu
            currentWifiSubMode = WIFI_SUB_NONE;
            displayUpdateRequired = true;
            Serial.println("Exited WiFi sub menu");
        } else {
            // Exit WiFi mode, return to NFC mode
            currentMode = MODE_NFC;
            displayUpdateRequired = true;
            Serial.println("Mode switched to: NFC");
        }
    }
}

/**
 * @brief Handles Mode button long press events
 * 
 * Used for menu confirmation in WiFi mode
 */
void handleModeButtonLongPress() {
    if (!systemReady) return;
    
    if (currentMode == MODE_WIFI && currentWifiSubMode == WIFI_SUB_NONE) {
        // Confirm menu selection in WiFi mode
        if (wifiMenuSelection == 0) {
            currentWifiSubMode = WIFI_SUB_SCAN;
            Serial.println("Entered Scan WiFi mode");
        } else {
            currentWifiSubMode = WIFI_SUB_MANUAL;
            Serial.println("Entered Manual Connect mode");
        }
        displayUpdateRequired = true;
    }
}

/**
 * @brief Handles Select button press events
 * 
 * Manages menu navigation and field selection
 */
void handleSelectButtonPress() {
    if (!systemReady) return;
    
    if (currentMode == MODE_WIFI) {
        if (currentWifiSubMode == WIFI_SUB_NONE) {
            // Cycle through main WiFi menu options
            wifiMenuSelection = (wifiMenuSelection + 1) % 2;
            displayUpdateRequired = true;
            Serial.print("WiFi menu selection: ");
            Serial.println(wifiMenuSelection == 0 ? "Scan WiFi" : "Manual Connect");
        }
        else if (currentWifiSubMode == WIFI_SUB_SCAN) {
            // Cycle through available WiFi networks
            scanWifiSelection = (scanWifiSelection + 1) % MAX_WIFI_NETWORKS;
            displayUpdateRequired = true;
            Serial.print("Selected WiFi: ");
            Serial.println(wifiNetworks[scanWifiSelection]);
        }
        else if (currentWifiSubMode == WIFI_SUB_MANUAL) {
            // Switch between SSID and Password fields
            manualInputField = (manualInputField + 1) % 2;
            displayUpdateRequired = true;
            Serial.print("Manual input field: ");
            Serial.println(manualInputField == 0 ? "SSID" : "Password");
        }
    }
}

// ========== NFC CARD VALIDATION FUNCTIONS ==========
/**
 * @brief Validates 4-byte UID format
 * 
 * @param uid Pointer to UID byte array
 * @return true if UID is valid, false otherwise
 * 
 * Checks for invalid patterns (all zeros, all FFs, reserved values)
 */
bool validate4ByteUID(uint8_t *uid) {
    // Check for reserved values
    if (uid[0] == 0x00 || uid[0] == 0x88) return false;
    
    // Check for all zeros
    bool allZero = true;
    for (int byteIndex = 0; byteIndex < 4; byteIndex++) {
        if (uid[byteIndex] != 0x00) {
            allZero = false;
            break;
        }
    }
    if (allZero) return false;
    
    // Check for all FFs
    bool allFF = true;
    for (int byteIndex = 0; byteIndex < 4; byteIndex++) {
        if (uid[byteIndex] != 0xFF) {
            allFF = false;
            break;
        }
    }
    if (allFF) return false;
    
    return true;
}

/**
 * @brief Validates 7-byte UID format
 * 
 * @param uid Pointer to UID byte array
 * @return true if UID is valid, false otherwise
 * 
 * Validates double-size UID format with specific constraints
 */
bool validate7ByteUID(uint8_t *uid) {
    // Check for CT (Cascade Tag) byte
    if (uid[3] != 0x88) return false;
    
    // Validate last 3 bytes (not all zeros)
    bool last3AllZero = true;
    for (int byteIndex = 4; byteIndex < 7; byteIndex++) {
        if (uid[byteIndex] != 0x00) {
            last3AllZero = false;
            break;
        }
    }
    if (last3AllZero) return false;
    
    // Validate last 3 bytes (not all FFs)
    bool last3AllFF = true;
    for (int byteIndex = 4; byteIndex < 7; byteIndex++) {
        if (uid[byteIndex] != 0xFF) {
            last3AllFF = false;
            break;
        }
    }
    if (last3AllFF) return false;
    
    // Check for invalid first byte
    if (uid[0] == 0x00) return false;
    
    return true;
}

/**
 * @brief Validates 10-byte UID format
 * 
 * @param uid Pointer to UID byte array
 * @return true if UID is valid, false otherwise
 * 
 * Validates triple-size UID format
 */
bool validate10ByteUID(uint8_t *uid) {
    // Check for all zeros
    bool allZero = true;
    for (int byteIndex = 0; byteIndex < 10; byteIndex++) {
        if (uid[byteIndex] != 0x00) {
            allZero = false;
            break;
        }
    }
    if (allZero) return false;
    
    // Check for all FFs
    bool allFF = true;
    for (int byteIndex = 0; byteIndex < 10; byteIndex++) {
        if (uid[byteIndex] != 0xFF) {
            allFF = false;
            break;
        }
    }
    if (allFF) return false;
    
    return true;
}

// ========== NFC CARD READING FUNCTION ==========
/**
 * @brief Reads NFC card and extracts UID
 * 
 * @param uidBuffer Buffer to store extracted UID
 * @return Length of UID (4, 7, 10) or negative error code
 * 
 * Performs complete NFC card activation and validation
 */
int8_t readCard(uint8_t *uidBuffer) {
    uint8_t responseBuffer[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    // Activate Type A card and get raw response
    int8_t rawResponseLength = nfc.activateTypeA(responseBuffer, 0);
    
    // Check for activation errors
    if (rawResponseLength <= 0) {
        return rawResponseLength;
    }
    
    // Validate response length
    if (rawResponseLength != 4 && rawResponseLength != 7 && rawResponseLength != 10) {
        return -3; // Invalid length error
    }
    
    // Extract and validate ATQA (Answer To Request)
    uint16_t atqaValue = (responseBuffer[1] << 8) | responseBuffer[0];
    if (atqaValue == 0x0000 || atqaValue == 0xFFFF) {
        return -3; // Invalid ATQA error
    }
    
    // Validate SAK (Select Acknowledge)
    uint8_t sakValue = responseBuffer[2];
    bool isValidSAK = false;
    
    switch(sakValue) {
        case 0x00: case 0x08: case 0x09: case 0x10: case 0x11:
        case 0x18: case 0x20: case 0x28: case 0x88: case 0x89:
            isValidSAK = true;
            break;
    }
    
    if (!isValidSAK) {
        return -3; // Invalid SAK error
    }
    
    // Process based on UID length
    if (rawResponseLength == 4) {
        // Validate 4-byte UID
        if (!validate4ByteUID(&responseBuffer[3])) {
            return -3;
        }
        
        // Copy UID to output buffer
        for (int byteIndex = 0; byteIndex < 4; byteIndex++) {
            uidBuffer[byteIndex] = responseBuffer[3 + byteIndex];
        }
        
        return 4;
    }
    else if (rawResponseLength == 7) {
        // Validate 7-byte UID
        if (!validate7ByteUID(&responseBuffer[3])) {
            return -3;
        }
        
        // Copy UID to output buffer
        for (int byteIndex = 0; byteIndex < 7; byteIndex++) {
            uidBuffer[byteIndex] = responseBuffer[3 + byteIndex];
        }
        
        return 7;
    }
    else if (rawResponseLength == 10) {
        // Validate 10-byte UID
        if (!validate10ByteUID(&responseBuffer[3])) {
            return -3;
        }
        
        // Copy UID to output buffer
        for (int byteIndex = 0; byteIndex < 10; byteIndex++) {
            uidBuffer[byteIndex] = responseBuffer[3 + byteIndex];
        }
        
        return 10;
    }
    
    return -3; // General validation error
}

// ========== DISPLAY MODE MANAGEMENT ==========
/**
 * @brief Updates display based on current system state
 * 
 * Controls which screen is shown based on mode and sub-mode
 * Prevents unnecessary screen refreshes
 */
void updateDisplay() {
    // Check if display update is needed
    if (!displayUpdateRequired && 
        currentMode == lastDisplayedMode && 
        currentWifiSubMode == lastWifiSubMode) {
        return;
    }
    
    // Select screen based on current mode
    switch (currentMode) {
        case MODE_NFC:
            if (cardDetected) {
                showUIDSmall(lastUID);
            } else {
                displayNoCardScreen();
            }
            break;
            
        case MODE_WIFI:
            switch (currentWifiSubMode) {
                case WIFI_SUB_NONE:
                    displayWifiMainMenu();
                    break;
                case WIFI_SUB_SCAN:
                    displayScanWifiScreen();
                    break;
                case WIFI_SUB_MANUAL:
                    displayManualConnectScreen();
                    break;
            }
            break;
    }
    
    // Reset update flags
    displayUpdateRequired = false;
    lastDisplayedMode = currentMode;
    lastWifiSubMode = currentWifiSubMode;
}

// ========== ARDUINO SETUP FUNCTION ==========
/**
 * @brief Arduino setup function - runs once at startup
 * 
 * Initializes all hardware components and system state
 */
void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    
    // Print startup banner
    Serial.println(F("=================================="));
    Serial.println(F("PN5180 NFC Reader with ePaper"));
    Serial.println(F("Enhanced WiFi Menu System"));
    Serial.println(F("=================================="));
    
    // ========== BUTTON CONFIGURATION ==========
    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(SELECT_BUTTON_PIN, INPUT_PULLUP);
    
    // Attach interrupt handlers
    attachInterrupt(digitalPinToInterrupt(MODE_BUTTON_PIN), 
                    handleModeButtonInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(SELECT_BUTTON_PIN), 
                    handleSelectButtonInterrupt, FALLING);
    
    Serial.println("Buttons configured with interrupts");
    Serial.println("B1: GPIO14 (Mode/Confirm)");
    Serial.println("B2: GPIO27 (Select)");
    
    // ========== DISPLAY INITIALIZATION ==========
    display.init(115200, true, 2, false);
    display.setRotation(1);
    display.clearScreen();
    displayInitialized = true;
    
    // Show startup screen
    displayStartingScreen();
    
    // ========== SPI BUS INITIALIZATION ==========
    SPI.begin(18, 19, 23); // SCK, MISO, MOSI pins
    
    // ========== NFC READER INITIALIZATION ==========
    Serial.println(F("----------------------------------"));
    Serial.println(F("Initializing PN5180..."));
    
    nfc.begin();
    Serial.println(F("PN5180 Hard-Reset..."));
    nfc.reset();
    
    // Read and display PN5180 version
    Serial.println(F("----------------------------------"));
    uint8_t productVersion[2];
    nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
    Serial.print(F("Product version="));
    Serial.print(productVersion[1]);
    Serial.print(".");
    Serial.println(productVersion[0]);
    
    // Check for initialization failure
    if (0xff == productVersion[1]) {
        Serial.println(F("Initialization failed!?"));
        
        // Display error screen
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        drawCenteredText("INIT ERROR", display.height() / 2 - 20, 
                        &FreeMonoBold18pt7b, GxEPD_RED);
        drawCenteredText("Restart device", display.height() / 2 + 20, 
                        &FreeMonoBold12pt7b, GxEPD_BLACK);
        display.display(true);
        
        Serial.println(F("Press reset to restart..."));
        Serial.flush();
        exit(-1);
    }
    
    // Configure NFC RF field
    Serial.println(F("Enable RF field..."));
    nfc.setupRF();
    
    // ========== SYSTEM READY ==========
    Serial.println(F("----------------------------------"));
    Serial.println(F("System initialization complete"));
    
    systemReady = true;
    
    // Set default mode and update display
    currentMode = MODE_NFC;
    displayUpdateRequired = true;
    updateDisplay();
    
    Serial.println(F("System ready!"));
    Serial.println(F("B1: Switch mode / Confirm / Exit"));
    Serial.println(F("B2: Select menu item"));
}

// ========== ARDUINO MAIN LOOP ==========
/**
 * @brief Arduino main loop - runs continuously
 * 
 * Handles button events, mode-specific processing, and display updates
 */
void loop() {
    // ========== BUTTON EVENT PROCESSING ==========
    // Handle Mode button short press
    if (modeButtonPressed) {
        modeButtonPressed = false;
        handleModeButtonPress();
    }
    
    // Handle Mode button long press
    if (modeButtonLongPressed) {
        modeButtonLongPressed = false;
        handleModeButtonLongPress();
    }
    
    // Handle Select button press
    if (selectButtonPressed) {
        selectButtonPressed = false;
        handleSelectButtonPress();
    }
    
    // ========== MODE-SPECIFIC PROCESSING ==========
    switch (currentMode) {
        case MODE_NFC: {
            // NFC Card Reading Mode
            uint8_t uidBuffer[10] = {0};
            int8_t uidLength = readCard(uidBuffer);
            
            if (uidLength > 0) {
                // Convert UID bytes to hex string
                String newUID = "";
                for (int byteIndex = 0; byteIndex < uidLength; byteIndex++) {
                    if (uidBuffer[byteIndex] < 0x10) newUID += "0";
                    newUID += String(uidBuffer[byteIndex], HEX);
                    if (byteIndex < uidLength - 1) newUID += " ";
                }
                newUID.toUpperCase();
                
                // Update display if new card detected
                if (!cardDetected || newUID != lastUID) {
                    cardDetected = true;
                    lastUID = newUID;
                    displayUpdateRequired = true;
                    Serial.print("Card detected: ");
                    Serial.println(newUID);
                }
                
            } else if (uidLength < 0) {
                // Handle NFC reading errors
                if (uidLength == -2 || uidLength == -3) {
                    // Reset NFC reader on communication errors
                    nfc.reset();
                    delay(50);
                    nfc.setupRF();
                    delay(100);
                }
                
                // Clear card detected state
                if (cardDetected) {
                    cardDetected = false;
                    displayUpdateRequired = true;
                    Serial.println("Card removed");
                }
            } else {
                // No card present
                if (cardDetected) {
                    cardDetected = false;
                    lastUID = "";
                    displayUpdateRequired = true;
                    Serial.println("Card removed");
                }
            }
            
            delay(200); // NFC polling interval
            break;
        }
        
        case MODE_WIFI: {
            // WiFi Mode - minimal processing needed
            delay(100); // Reduce CPU usage
            break;
        }
    }
    
    // ========== DISPLAY UPDATE ==========
    updateDisplay();
}
