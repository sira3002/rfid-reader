#ifndef READER_CORE_H
#define READER_CORE_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

enum SystemState {
    STATE_NORMAL,
    STATE_PORUCHA
};

struct EspNowMsg {
    char cmd[16];
};

struct UserData {
    int id;
    String rfidUid;
    String firstName;
    String lastName;
    String vocative;
    String firstNameAscii;
    String lastNameAscii;
    String vocativeAscii;
    int teamId;
    String role;
    String diet;
    bool isFound;
};

class ReaderCoreClass {
private:
    MFRC522* _rfid = nullptr;
    Adafruit_NeoPixel* _leds = nullptr;
    Adafruit_SH1106G* _display = nullptr;
    AsyncWebServer* _server = nullptr;
    ThreeWire* _myWire = nullptr;
    RtcDS1302<ThreeWire>* _rtc = nullptr;
    SPIClass _spiSD;

    String _readerId;
    SystemState _currentState = STATE_NORMAL;
    String _currentMessage = "Prilozte kartu";
    bool _sdActive = false;
    bool _rfidActive = false;
    String _lastUID = "";
    bool _isMuted = false;

    void setupWiFiAP(String ssid, String password);
    void setupEspNow();
    void setupApiEndpoints();

public:
    ReaderCoreClass();
    
    bool begin(String readerId, String apSsid, String apPassword); 
    void update();
    
    SystemState getState();
    void setSystemState(SystemState newState, bool broadcast = true);
    
    bool isCardPresent();
    String readCardUID();
    void haltCard();
    
    UserData findUserByUid(String uid);
    String getSudokuHint(int userId);
    bool logAction(String uid, String action, String extraData = "");
    
    // Správa zobrazení a šablon
    String getActiveDisplayTemplate();
    String parseUserTemplate(UserData user);
    
    void beep(int ms = 40);
    void playSuccess();
    void playError();
    void updateLeds();
    void updateDisplay();
    String getCurrentTime();
    
    void toggleMute();
    bool isMuted();
    
    AsyncWebServer* getServer(); 
    String getCurrentMessage();
    void setCurrentMessage(String msg);
    String getReaderId();
};

extern ReaderCoreClass ReaderCore;

#endif