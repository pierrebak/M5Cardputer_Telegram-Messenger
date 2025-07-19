/*
 * Bidirectional Telegram Messenger App for M5Cardputer
 * FINAL VERSION
 *
 * Features:
 * - SEND Mode: Write and send messages.
 * - RECEIVE Mode: Read and scroll through received messages.
 * - Switch between modes with the 'Fn' key.
 * - Scroll long messages with the UP and DOWN ARROWS.
 * - Automatic fetching of new messages in receive mode.
 * - Correct display of accents by converting Unicode (uXXXX).
 * - User interface in English.
 * - Fixed scrolling for long messages (Word Wrap).
 * - Plays a sound and auto-switches to RECEIVE mode on new message.
 *
 * Prerequisites:
 * 1. M5Cardputer with an updated M5Cardputer library.
 * 2. UrlEncode library.
 * 3. A Telegram bot (Token) and your personal Chat ID.
 *
 * Instructions:
 * 1. Fill in your Wi-Fi information, Bot Token, and your own Chat ID.
 * 2. Upload this code.
 */

// --- Required Libraries ---
#include "M5Cardputer.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <vector> 

// --- Settings to Configure ---
const char* WIFI_SSID = "xxxxxxxxxxxx";      // << REPLACE with your Wi-Fi network name
const char* WIFI_PASS = "xxxxxxxxxxxx";     // << REPLACE with your Wi-Fi password
String BOT_TOKEN     = "xxxxxxxxxxxxx";    // << REPLACE with your bot token
String CHAT_ID       = "xxxxxxxxxxxxx";   // << REPLACE with the recipient's Chat ID

// --- Global Variables ---
enum AppMode {
    MODE_SEND,
    MODE_RECEIVE
};
AppMode currentMode = MODE_SEND;

// Variables for SEND mode
String messageBuffer = "";
String statusMessage = "Enter your message...";

// Variables for RECEIVE mode
String receivedMessage = "No message received.\nSwitch to receive mode...";
std::vector<String> displayLines; 
int scrollOffset = 0; 
long lastCheckTime = 0;
long lastUpdateId = 0;

// --- Functions ---

// Formats the received message into lines that fit the screen
void wrapReceivedMessage() {
    displayLines.clear();
    const int MAX_CHARS = 19;
    
    int start = 0;
    while (start < receivedMessage.length()) {
        int end = receivedMessage.indexOf('\n', start);
        if (end == -1) {
            end = receivedMessage.length();
        }
        String line = receivedMessage.substring(start, end);
        
        while (line.length() > MAX_CHARS) {
            int wrapAt = MAX_CHARS;
            int lastSpace = line.lastIndexOf(' ', wrapAt);
            if (lastSpace > 0) {
                wrapAt = lastSpace;
            }
            displayLines.push_back(line.substring(0, wrapAt));
            line = line.substring(wrapAt + (lastSpace > 0 ? 1 : 0));
        }
        displayLines.push_back(line);
        start = end + 1;
    }
}

// Draws the common header
void drawHeader() {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(5, 10);
    String modeStr = (currentMode == MODE_SEND) ? "[SEND]" : "[RECEIVE]";
    M5.Lcd.printf("Telegram %s", modeStr.c_str());
    M5.Lcd.drawFastHLine(0, 35, M5.Lcd.width(), DARKGREY);
}

// Displays the SEND mode UI
void drawSendUI() {
    M5.Lcd.fillScreen(BLACK);
    drawHeader();
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.setCursor(5, 45);
    M5.Lcd.print(statusMessage);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.setCursor(5, 70);
    M5.Lcd.print("> ");
    M5.Lcd.print(messageBuffer);
}

// Displays the RECEIVE mode UI
void drawReceiveUI() {
    M5.Lcd.fillScreen(BLACK);
    drawHeader();
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.setCursor(5, 45);
    M5.Lcd.print("Last message received");

    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.setCursor(5, 70);
    
    for (int i = 0; i < 4; ++i) {
        if (scrollOffset + i < displayLines.size()) {
            M5.Lcd.println(displayLines[scrollOffset + i]);
        }
    }
}

// Decodes Unicode escape sequences (\uXXXX)
String decodeUnicode(String str) {
    String result = "";
    int i = 0;
    while (i < str.length()) {
        if (str.charAt(i) == '\\' && i + 1 < str.length() && str.charAt(i + 1) == 'u') {
            if (i + 5 < str.length()) {
                String hexCode = str.substring(i + 2, i + 6);
                long unicodeValue = strtol(hexCode.c_str(), NULL, 16);
                
                char decodedChar = '?';
                switch(unicodeValue) {
                    case 0x00E9: decodedChar = 'e'; break; // é
                    case 0x00E8: decodedChar = 'e'; break; // è
                    case 0x00EA: decodedChar = 'e'; break; // ê
                    case 0x00EB: decodedChar = 'e'; break; // ë
                    case 0x00E0: decodedChar = 'a'; break; // à
                    case 0x00E2: decodedChar = 'a'; break; // â
                    case 0x00E7: decodedChar = 'c'; break; // ç
                    case 0x00F9: decodedChar = 'u'; break; // ù
                    case 0x00FB: decodedChar = 'u'; break; // û
                    case 0x00EE: decodedChar = 'i'; break; // î
                    case 0x00EF: decodedChar = 'i'; break; // ï
                    case 0x00F4: decodedChar = 'o'; break; // ô
                    case 0x00C9: decodedChar = 'E'; break; // É
                    case 0x00C8: decodedChar = 'E'; break; // È
                    case 0x00C0: decodedChar = 'A'; break; // À
                    case 0x00C7: decodedChar = 'C'; break; // Ç
                    case 0x2019: decodedChar = '\''; break; // apostrophe
                }
                result += decodedChar;
                i += 6;
            } else {
                result += str.charAt(i++);
            }
        } else {
            result += str.charAt(i++);
        }
    }
    return result;
}

void sendMessage(String message) {
    if (WiFi.status() != WL_CONNECTED) {
        statusMessage = "Error: Wi-Fi not connected.";
        drawSendUI();
        return;
    }
    statusMessage = "Sending...";
    drawSendUI();

    String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + urlEncode(message);
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    statusMessage = (httpCode == HTTP_CODE_OK) ? "Message sent!" : "Error: " + String(httpCode);
    http.end();
    
    messageBuffer = "";
    drawSendUI();
    delay(2000);
    statusMessage = "Enter your message...";
    drawSendUI();
}

void getUpdates() {
    if (WiFi.status() != WL_CONNECTED) return;

    String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&limit=1";
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        int updateIdIndex = payload.indexOf("\"update_id\":");
        int fromFirstNameIndex = payload.indexOf("\"first_name\":\"");
        int textIndex = payload.indexOf("\"text\":\"");

        if (updateIdIndex != -1 && fromFirstNameIndex != -1 && textIndex != -1) {
            // A new message was found
            M5.Speaker.tone(4400, 150); // Play a notification sound
            currentMode = MODE_RECEIVE; // Automatically switch to receive mode

            int idStartIndex = updateIdIndex + 12;
            int idEndIndex = payload.indexOf(",", idStartIndex);
            String idStr = payload.substring(idStartIndex, idEndIndex);
            lastUpdateId = idStr.toInt();

            int nameStartIndex = fromFirstNameIndex + 14;
            int nameEndIndex = payload.indexOf("\"", nameStartIndex);
            String from = payload.substring(nameStartIndex, nameEndIndex);

            int textStartIndex = textIndex + 8;
            int textEndIndex = payload.indexOf("\"", textStartIndex);
            String text = payload.substring(textStartIndex, textEndIndex);

            text.replace("\\\"", "\"");
            text.replace("\\n", "\n");
            String decodedText = decodeUnicode(text);

            receivedMessage = from + ":\n\n" + decodedText;
            scrollOffset = 0;
            wrapReceivedMessage();
            drawReceiveUI();
        }
    }
    http.end();
}


// --- Main Program ---

void setup() {
    M5Cardputer.begin();
    M5.Lcd.setRotation(1);
    Serial.begin(115200);
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(20, 50);
    M5.Lcd.print("Connecting to Wi-Fi...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    
    wrapReceivedMessage();
    drawSendUI();
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState keyState = M5Cardputer.Keyboard.keysState();

        if (keyState.fn) {
            if (currentMode == MODE_SEND) {
                currentMode = MODE_RECEIVE;
                getUpdates();
                drawReceiveUI();
            } else {
                currentMode = MODE_SEND;
                drawSendUI();
            }
            delay(200);
            return;
        }

        if (currentMode == MODE_SEND) {
            for (auto i : keyState.word) { 
                if ((uint8_t)i >= 32) {
                    messageBuffer += i;
                }
            }
            if (keyState.del) {
                if (messageBuffer.length() > 0) {
                    messageBuffer.remove(messageBuffer.length() - 1);
                }
            }
            if (keyState.enter) {
                if (messageBuffer.length() > 0) {
                    sendMessage(messageBuffer);
                }
            }
            drawSendUI();
        } 
        else { // MODE_RECEIVE
            bool scrolled = false;
            for (auto key_char : keyState.word) {
                if (key_char == 0) continue;

                if ((uint8_t)key_char == 0x3B) { // Code for UP ARROW
                    if (scrollOffset > 0) {
                        scrollOffset--;
                        scrolled = true;
                    }
                }
                if ((uint8_t)key_char == 0x2E) { // Code for DOWN ARROW
                    if (scrollOffset + 4 < displayLines.size()) {
                        scrollOffset++;
                        scrolled = true;
                    }
                }
            }
            if (scrolled) {
                drawReceiveUI();
                delay(150); // Debounce for scrolling
            }
        }
    }

    // Periodically check for new messages, regardless of the current mode
    if (millis() - lastCheckTime > 10000) {
        getUpdates();
        lastCheckTime = millis();
    }
    
    delay(20);
}
