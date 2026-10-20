#include "ReaderCore.h"
#include <LittleFS.h>

ReaderCoreClass ReaderCore;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    EspNowMsg msg;
    memcpy(&msg, incomingData, sizeof(msg));
    
    if (strcmp(msg.cmd, "PORUCHA_START") == 0) {
        ReaderCore.setSystemState(STATE_PORUCHA, false);
    } else if (strcmp(msg.cmd, "STATE_NORMAL") == 0) {
        ReaderCore.setSystemState(STATE_NORMAL, false);
    }
}

ReaderCoreClass::ReaderCoreClass() : _spiSD(HSPI) {
    _currentState = STATE_NORMAL;
}

bool ReaderCoreClass::begin(String readerId, String apSsid, String apPassword) {
    _readerId = readerId;
    
    Serial.begin(115200);
    delay(300);
    Serial.println("\n--- STARTING READER: " + _readerId + " ---");

    pinMode(2, OUTPUT);
    beep(80);

    _leds = new Adafruit_NeoPixel(8, 4, NEO_GRB + NEO_KHZ800);
    _leds->begin();
    _leds->setBrightness(40);
    _leds->clear();
    _leds->show();

    if (!LittleFS.begin(true)) {
        Serial.println("[ERR] LittleFS selhalo!");
    } else {
        Serial.println("[OK] LittleFS nacteno.");
    }

    Wire.begin(21, 22);
    _display = new Adafruit_SH1106G(128, 64, &Wire, -1);
    if (_display->begin(0x3C, true)) {
        _display->clearDisplay();
        _display->setTextSize(1);
        _display->setTextColor(SH110X_WHITE);
        _display->setCursor(0, 0);
        _display->println("Startuji...");
        _display->display();
    }

    SPI.begin(14, 17, 13, 5);
    _rfid = new MFRC522(5, 33);
    _rfid->PCD_Init();
    delay(20);
    byte v = _rfid->PCD_ReadRegister(_rfid->VersionReg);
    if (v == 0x91 || v == 0x92 || v == 0x12 || v == 0xB2) _rfidActive = true;

    _spiSD.begin(18, 19, 23, -1);
    pinMode(32, OUTPUT);
    digitalWrite(32, HIGH);
    delay(250);
    if (SD.begin(32, _spiSD, 4000000)) _sdActive = true;

    _myWire = new ThreeWire(27, 16, 12); 
    _rtc = new RtcDS1302<ThreeWire>(*_myWire);
    _rtc->Begin();
    if (_rtc->GetIsWriteProtected()) _rtc->SetIsWriteProtected(false);
    if (!_rtc->GetIsRunning()) _rtc->SetIsRunning(true);
    if (!_rtc->IsDateTimeValid()) _rtc->SetDateTime(RtcDateTime(__DATE__, __TIME__));

    setupWiFiAP(apSsid, apPassword);
    setupEspNow();

    _server = new AsyncWebServer(80);
    setupApiEndpoints();
    _server->begin();

    return true;
}

void ReaderCoreClass::setupWiFiAP(String ssid, String password) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid.c_str(), password.c_str());
}

void ReaderCoreClass::setupEspNow() {
    if (esp_now_init() != ESP_OK) return;
    
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void ReaderCoreClass::update() {
    updateLeds();
    updateDisplay();
}

SystemState ReaderCoreClass::getState() { return _currentState; }

void ReaderCoreClass::setSystemState(SystemState newState, bool broadcast) {
    _currentState = newState;
    if (_currentState == STATE_PORUCHA) {
        _currentMessage = "Prilozte kartu...";
    } else {
        _currentMessage = "System ONLINE";
        if (_leds) {
            _leds->clear();
            _leds->show();
        }
    }

    if (broadcast) {
        EspNowMsg msg;
        if (newState == STATE_PORUCHA) strcpy(msg.cmd, "PORUCHA_START");
        else strcpy(msg.cmd, "PORUCHA_STOP");
        esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
    }
}

bool ReaderCoreClass::isCardPresent() {
    if (!_rfidActive || !_rfid) return false;
    return _rfid->PICC_IsNewCardPresent() && _rfid->PICC_ReadCardSerial();
}

String ReaderCoreClass::readCardUID() {
    if (!_rfid) return "";
    String uid = "";
    for (byte i = 0; i < _rfid->uid.size; i++) {
        if (_rfid->uid.uidByte[i] < 0x10) uid += "0";
        uid += String(_rfid->uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println(uid);
    _lastUID = uid;
    return uid;
}

void ReaderCoreClass::haltCard() {
    if (!_rfid) return;
    _rfid->PICC_HaltA();
    _rfid->PCD_StopCrypto1();
}

void ReaderCoreClass::beep(int ms) {
    if (_isMuted) return;
    digitalWrite(2, HIGH);
    delay(ms);
    digitalWrite(2, LOW);
}

void ReaderCoreClass::toggleMute() { _isMuted = !_isMuted; }
bool ReaderCoreClass::isMuted() { return _isMuted; }

void ReaderCoreClass::playSuccess() {
    beep(80);
    if (!_leds) return;
    for(int i = 0; i < 8; i++) _leds->setPixelColor(i, _leds->Color(0, 255, 0));
    _leds->show();
    delay(200);
    _leds->clear();
    _leds->show();
}

void ReaderCoreClass::playError() {
    beep(200);
    if (!_leds) return;
    for(int i = 0; i < 8; i++) _leds->setPixelColor(i, _leds->Color(255, 0, 0));
    _leds->show();
    delay(400);
    _leds->clear();
    _leds->show();
}

void ReaderCoreClass::updateLeds() {
    if (!_leds) return;
    if (_currentState == STATE_PORUCHA) {
        int brightness = (sin(millis() / 200.0) + 1) * 127;
        for(int i = 0; i < 8; i++) _leds->setPixelColor(i, _leds->Color(brightness, 0, 0));
        _leds->show();
    }
}

void ReaderCoreClass::updateDisplay() {
    if (!_display) return;
    
    static unsigned long lastRefresh = 0;
    if (millis() - lastRefresh < 150) return;
    lastRefresh = millis();

    _display->clearDisplay();
    _display->setTextColor(SH110X_WHITE);
    
    if (_currentState == STATE_PORUCHA) {
        _display->setTextSize(2);
        _display->setCursor(10, 0);
        _display->print("PORUCHA!");
        _display->drawLine(0, 17, 127, 17, SH110X_WHITE);
        _display->setTextSize(1);
        _display->setCursor(0, 22);
        _display->println(_currentMessage);
    } else {
        _display->setTextSize(1);
        _display->setCursor(0, 0);
        _display->println(_currentMessage);
    }

    _display->display();
}

String ReaderCoreClass::getCurrentTime() {
    if (!_rtc || !_rtc->IsDateTimeValid()) return "2000-01-01 00:00:00";
    RtcDateTime now = _rtc->GetDateTime();
    char timeString[25];
    snprintf(timeString, sizeof(timeString), "%04d-%02d-%02d %02d:%02d:%02d", 
             now.Year(), now.Month(), now.Day(), 
             now.Hour(), now.Minute(), now.Second());
    return String(timeString);
}

UserData ReaderCoreClass::findUserByUid(String uid) {
    UserData user;
    user.isFound = false;
    if (!_sdActive) return user;

    File file = SD.open("/users.csv");
    if (!file) return user;

    if (file.available()) file.readStringUntil('\n');
    uid.trim();

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        String parts[11];
        int colIdx = 0;
        int lastPos = 0;

        for (int i = 0; i <= line.length(); i++) {
            if (i == line.length() || line.charAt(i) == ';') {
                if (colIdx < 11) {
                    parts[colIdx] = line.substring(lastPos, i);
                    parts[colIdx].trim();
                }
                colIdx++;
                lastPos = i + 1;
            }
        }

        if (colIdx >= 2 && parts[1].equalsIgnoreCase(uid)) {
            user.id = parts[0].toInt();
            user.rfidUid = parts[1];
            user.firstName = parts[2];
            user.lastName = parts[3];
            user.firstNameAscii = parts[4];
            user.lastNameAscii = parts[5];
            user.vocative = parts[6];
            user.vocativeAscii = parts[7];
            user.teamId = parts[8].toInt();
            user.role = parts[9];
            user.diet = parts[10];
            user.isFound = true;
            file.close();
            return user;
        }
    }
    file.close();
    return user;
}

String ReaderCoreClass::getSudokuHint(int userId) {
    if (!_sdActive) return "Chyba SD karty";
    
    File file = SD.open("/sudoku.csv");
    if (!file) return "Chyba systému";

    if (file.available()) file.readStringUntil('\n');

    String hints[2];
    int count = 0;

    while (file.available() && count < 2) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int sep1 = line.indexOf(';');
        int sep2 = line.indexOf(';', sep1 + 1);

        if (sep1 > 0 && sep2 > sep1) {
            int csvId = line.substring(0, sep1).toInt();
            if (csvId == userId) {
                String pole = line.substring(sep1 + 1, sep2);
                String cislo = line.substring(sep2 + 1);
                pole.trim(); cislo.trim();
                hints[count] = "ERROR " + pole + " - " + cislo;
                count++;
            }
        }
    }
    file.close();

    if (count == 0) return "Zadna napoveda";
    if (count == 1) return hints[0];
    return hints[0] + "\n" + hints[1];
}

String ReaderCoreClass::getActiveDisplayTemplate() {
    if (!_sdActive || !SD.exists("/display_cfg.txt")) {
        return "Ahoj {OSLOVENI}!\n\nOddil: {ODDIL}\n\nRole: {ROLE}";
    }

    File f = SD.open("/display_cfg.txt", FILE_READ);
    if (!f) return "Ahoj {OSLOVENI}!";

    String activePreset = "Zakladni";
    String currentSection = "";
    String sectionTexts[10];
    String sectionNames[10];
    int sectionCount = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        line.replace("\r", "");
        if (line.length() == 0) continue;

        if (line.startsWith("AKTIVNI=")) {
            activePreset = line.substring(8);
            activePreset.trim();
        } else if (line.startsWith("[") && line.endsWith("]")) {
            currentSection = line.substring(1, line.length() - 1);
            currentSection.trim();
            if (sectionCount < 10) {
                sectionNames[sectionCount] = currentSection;
                sectionTexts[sectionCount] = "";
                sectionCount++;
            }
        } else if (sectionCount > 0) {
            if (sectionTexts[sectionCount - 1].length() > 0) {
                sectionTexts[sectionCount - 1] += "\n";
            }
            sectionTexts[sectionCount - 1] += line;
        }
    }
    f.close();

    for (int i = 0; i < sectionCount; i++) {
        if (sectionNames[i].equalsIgnoreCase(activePreset)) {
            return sectionTexts[i];
        }
    }
    return sectionCount > 0 ? sectionTexts[0] : "Ahoj {OSLOVENI}!\nOddil: {ODDIL}";
}

String ReaderCoreClass::parseUserTemplate(UserData user) {
    String tpl = getActiveDisplayTemplate();
    
    String oddil = "Bez oddilu";
    switch(user.teamId) {
        case 1: oddil = "Zeleni"; break;
        case 2: oddil = "Modri"; break;
        case 3: oddil = "Ruzovi"; break;
        case 4: oddil = "Oranzovi"; break;
        case 5: oddil = "Vedouci"; break;
    }

    tpl.replace("{JMENO}", user.firstNameAscii);
    tpl.replace("{PRIJMENI}", user.lastNameAscii);
    tpl.replace("{OSLOVENI}", user.vocativeAscii);
    tpl.replace("{ROLE}", user.role);
    tpl.replace("{ODDIL}", oddil);
    tpl.replace("{DIETA}", user.diet);
    tpl.replace("{ID}", String(user.id));
    return tpl;
}

bool ReaderCoreClass::logAction(String uid, String action, String extraData) {
    if (!_sdActive) return false;
    String fileName = "/log_" + _readerId + ".csv";
    File logFile = SD.open(fileName, FILE_APPEND);
    if (!logFile) return false;

    String timeStr = getCurrentTime();
    logFile.printf("%s;%s;%s;%s\n", timeStr.c_str(), uid.c_str(), action.c_str(), extraData.c_str());
    logFile.close();
    return true;
}

void ReaderCoreClass::setupApiEndpoints() {
    // 1. Hlavní stránka Dashboardu
    _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/index.html")) request->send(LittleFS, "/index.html", "text/html");
        else request->send(200, "text/plain", "Nahrana stranka index.html nebyla v LittleFS nalezena!");
    });

    // 2. Stav systému pro dashboard
    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"readerId\":\"" + _readerId + "\",";
        json += "\"time\":\"" + getCurrentTime() + "\",";
        json += "\"state\":\"" + String(_currentState == STATE_PORUCHA ? "PORUCHA" : "NORMAL") + "\",";
        json += "\"muted\":" + String(_isMuted ? "true" : "false") + ",";
        json += "\"sdActive\":" + String(_sdActive ? "true" : "false");
        json += "}";
        request->send(200, "application/json", json);
    });

    // 3. Spolehlivý výpis souborů na SD kartě
    _server->on("/api/fs/list", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!_sdActive) { request->send(500, "application/json", "[]"); return; }
        
        String json = "[";
        File root = SD.open("/");
        if (root) {
            File file = root.openNextFile();
            while(file) {
                if(!file.isDirectory()) {
                    if(json != "[") json += ",";
                    String fname = String(file.name());
                    if (fname.startsWith("/")) fname = fname.substring(1);
                    json += "{\"name\":\"" + fname + "\",\"size\":" + String(file.size()) + "}";
                }
                file = root.openNextFile();
            }
            root.close();
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // 4. Stažení souboru
    _server->on("/api/fs/download", HTTP_GET, [this](AsyncWebServerRequest *request){
        if(!_sdActive) { request->send(500, "text/plain", "SD nenalezena"); return; }
        
        if(request->hasParam("file")) {
            String fileName = request->getParam("file")->value();
            if (!fileName.startsWith("/")) fileName = "/" + fileName;
            if (SD.exists(fileName)) {
                request->send(SD, fileName, "application/octet-stream", true);
                return;
            }
        }
        request->send(400, "text/plain", "Soubor nenalezen");
    });

    // 5. Smazání souboru
    _server->on("/api/fs/delete", HTTP_DELETE, [this](AsyncWebServerRequest *request){
        if(!_sdActive) { request->send(500, "text/plain", "SD nenalezena"); return; }
        
        if(request->hasParam("file")) {
            String fileName = request->getParam("file")->value();
            if (!fileName.startsWith("/")) fileName = "/" + fileName;
            if (SD.exists(fileName)) {
                SD.remove(fileName);
                request->send(200, "text/plain", "OK");
                return;
            }
        }
        request->send(400, "text/plain", "Chyba mazani");
    });

    // 6. Upload souboru na SD kartu
    _server->on("/api/fs/upload", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "OK");
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            if(!_sdActive) return;
            if(!index){
                String path = filename.startsWith("/") ? filename : ("/" + filename);
                if(SD.exists(path)) SD.remove(path);
                request->_tempFile = SD.open(path, FILE_WRITE);
            }
            if(request->_tempFile){
                request->_tempFile.write(data, len);
            }
            if(final){
                if(request->_tempFile){
                    request->_tempFile.close();
                }
            }
        }
    );

    // 7. Šablony displeje (display_cfg.txt)
    _server->on("/api/display_cfg/get", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!_sdActive || !SD.exists("/display_cfg.txt")) {
            request->send(200, "text/plain", "AKTIVNI=Zakladni\n\n[Zakladni]\nAhoj {OSLOVENI}!\nOddil: {ODDIL}\nRole: {ROLE}");
            return;
        }
        File f = SD.open("/display_cfg.txt", FILE_READ);
        String content = f.readString();
        f.close();
        request->send(200, "text/plain", content);
    });

    _server->on("/api/display_cfg/save", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (request->hasParam("cfg_content", true)) {
            String newContent = request->getParam("cfg_content", true)->value();
            File f = SD.open("/display_cfg.txt", FILE_WRITE);
            if (f) {
                f.print(newContent);
                f.close();
                request->send(200, "text/plain", "OK");
                return;
            }
        }
        request->send(500, "text/plain", "Chyba zapisu na SD");
    });

    // 8. Přepínání poruchy
    _server->on("/api/porucha", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (_currentState == STATE_NORMAL) {
            setSystemState(STATE_PORUCHA, true);
            request->send(200, "text/plain", "PORUCHA AKTIVOVANA");
        } else {
            setSystemState(STATE_NORMAL, true);
            request->send(200, "text/plain", "PORUCHA UKONCENA");
        }
    });

    // 9. Ztlumení zvuku
    _server->on("/api/mute", HTTP_GET, [this](AsyncWebServerRequest *request){
        toggleMute();
        request->send(200, "text/plain", isMuted() ? "ZTLUMENO" : "ZAPNUTO");
    });

    // 10. Synchronizace času z telefonu
    _server->on("/api/settime", HTTP_POST, [this](AsyncWebServerRequest *request){
        if(request->hasParam("timestamp", true)) {
            String tsStr = request->getParam("timestamp", true)->value();
            unsigned long epochTime = tsStr.toInt();
            if (epochTime > 0 && _rtc) {
                // Přičtení letního posunu +2 hodiny (+7200 s)
                RtcDateTime newTime(epochTime + 7200);
                _rtc->SetDateTime(newTime);
                request->send(200, "text/plain", "Cas seizen");
                return;
            }
        }
        request->send(400, "text/plain", "Chyba casu");
    });

    // 11. Restart
    _server->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Restartuji...");
        delay(500);
        ESP.restart();
    });
}

AsyncWebServer* ReaderCoreClass::getServer() { return _server; }
String ReaderCoreClass::getCurrentMessage() { return _currentMessage; }
void ReaderCoreClass::setCurrentMessage(String msg) { _currentMessage = msg; }
String ReaderCoreClass::getReaderId() { return _readerId; }