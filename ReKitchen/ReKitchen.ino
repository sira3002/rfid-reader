#include "ReaderCore.h"
#include <LittleFS.h>
#include <SD.h>
#include <vector>

// --- GLOBÁLNÍ PROMĚNNÉ PRO KUCHYŇ ---
String aktualniRezim = "VYDEJ"; // "VYDEJ", "HODNOCENI", "NORMAL"
String k_jidloId = "0";
String k_jidloNazev = "Nenastaveno";
String k_jidloTyp = "1";
String k_jidloSubItems = ""; 
int vydanoPorciCelkem = 0;

std::vector<String> vydejList;
std::vector<String> ratedList; 

String lastKitchenUID = "";
unsigned long lastKitchenScanTime = 0;
unsigned long k_scanId = 0; 
String k_stavPorce = "";
String k_stavHodnoceni = "NEHODNOTIL";

UserData lastScannedUser;

unsigned long customScreenShowTime = 0;
bool customScreenActive = false;

String getTeamColor(int teamId) {
    switch(teamId) {
        case 1: return "Zelena";
        case 2: return "Modra";
        case 3: return "Ruzova";
        case 4: return "Oranzova";
        case 5: return "Zluta";
        default: return "Cerna";
    }
}

String getTeamName(int teamId) {
    switch(teamId) {
        case 1: return "Zelení";
        case 2: return "Modří";
        case 3: return "Růžoví";
        case 4: return "Oranžoví";
        case 5: return "Vedoucí";
        default: return "Bez oddílu";
    }
}

void setup() {
    ReaderCore.begin("KUCHYN", "Tabor-Kuchyne", "tajneheslo");
    AsyncWebServer* server = ReaderCore.getServer();

    // 1. Servírování HTML souborů z LittleFS
    server->on("/tablet", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/tablet.html")) request->send(LittleFS, "/tablet.html", "text/html"); 
        else request->send(200, "text/plain", "Chyba: tablet.html nenalezen!"); 
    });
    
    server->on("/ovladani", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/ovladani.html")) request->send(LittleFS, "/ovladani.html", "text/html"); 
        else request->send(200, "text/plain", "Chyba: ovladani.html nenalezen!"); 
    });

    // 2. API - Seznam jídel z SD karty
    server->on("/api/seznam_jidel", HTTP_GET, [](AsyncWebServerRequest *request){
        String options = "";
        File f = SD.open("/jidla.csv", FILE_READ);
        if (f) {
            bool first = true;
            while(f.available()) {
                String line = f.readStringUntil('\n'); line.trim(); line.replace("\r", "");
                if(first) { first = false; continue; }
                if(line.length() > 0) {
                    String p[6] = {"", "", "", "", "", ""}; 
                    int start = 0;
                    for(int i=0; i<6; i++){
                        int idx = line.indexOf(';', start);
                        if(idx == -1) { p[i] = line.substring(start); break; }
                        p[i] = line.substring(start, idx); start = idx + 1;
                    }
                    String typText = "Jidlo";
                    if (p[2] == "1") typText = "Snidane";
                    else if (p[2] == "2") typText = "Obed";
                    else if (p[2] == "3") typText = "Svacina";
                    else if (p[2] == "4") typText = "Vecere";
                    else if (p[2] == "5") typText = "Degustace"; 

                    String formatedLabel = p[4] + " (" + p[3] + ") | " + typText + ": " + p[1];
                    String val = p[0] + "|" + p[1] + "|" + p[2] + "|" + p[5];
                    options += "<option value='" + val + "'>" + formatedLabel + "</option>";
                }
            }
            f.close();
        }
        request->send(200, "text/html", options);
    });

    // 3. API - Nastavení z Ovládacího panelu
    server->on("/api/nastavit", HTTP_POST, [](AsyncWebServerRequest *request){
        if(request->hasParam("rezim", true)) aktualniRezim = request->getParam("rezim", true)->value();
        if(request->hasParam("jidlo_data", true)) {
            String rawData = request->getParam("jidlo_data", true)->value();
            int sep1 = rawData.indexOf('|');
            int sep2 = rawData.indexOf('|', sep1 + 1);
            int sep3 = rawData.indexOf('|', sep2 + 1);

            if(sep1 > 0) {
                k_jidloId = rawData.substring(0, sep1);
                if (sep2 > sep1) {
                    k_jidloNazev = rawData.substring(sep1 + 1, sep2);
                    if (sep3 > sep2) {
                        k_jidloTyp = rawData.substring(sep2 + 1, sep3);
                        k_jidloSubItems = rawData.substring(sep3 + 1);
                    } else {
                        k_jidloTyp = rawData.substring(sep2 + 1);
                        k_jidloSubItems = "";
                    }
                } else {
                    k_jidloNazev = rawData.substring(sep1 + 1);
                    k_jidloTyp = "1";
                    k_jidloSubItems = "";
                }
            }
        }
        vydanoPorciCelkem = 0; 
        vydejList.clear(); 
        ratedList.clear(); 
        lastKitchenUID = ""; 
        k_scanId++; 
        customScreenActive = false;
        request->redirect("/ovladani");
    });

    // 4. API - Stav pro Tablet
    server->on("/api/stav", HTTP_GET, [](AsyncWebServerRequest *request){
        String color = getTeamColor(lastScannedUser.teamId);
        String oddilNazev = getTeamName(lastScannedUser.teamId);
        String jmeno = lastScannedUser.isFound ? lastScannedUser.firstName : "Neznamy";
        String prijmeni = lastScannedUser.isFound ? lastScannedUser.lastName : "";
        String osloveni = lastScannedUser.isFound ? lastScannedUser.vocative : "Strávníku";
        
        String json = "{\"id\":" + String(k_scanId) + 
                      ",\"user_id\":" + String(lastScannedUser.id) + 
                      ",\"uid\":\"" + lastKitchenUID + 
                      "\",\"jmeno\":\"" + jmeno + 
                      "\",\"prijmeni\":\"" + prijmeni + 
                      "\",\"osloveni\":\"" + osloveni + 
                      "\",\"role\":\"" + lastScannedUser.role + 
                      "\",\"oddil\":\"" + oddilNazev + 
                      "\",\"barva\":\"" + color + 
                      "\",\"alergie\":\"" + lastScannedUser.diet + 
                      "\",\"porce_celkem\":" + String(vydanoPorciCelkem) + 
                      ",\"jidlo\":\"" + k_jidloNazev + 
                      "\",\"jidlo_typ\":\"" + k_jidloTyp + 
                      "\",\"jidlo_subitems\":\"" + k_jidloSubItems + 
                      "\",\"stav\":\"" + k_stavPorce + 
                      "\",\"stav_hodnoceni\":\"" + k_stavHodnoceni + 
                      "\",\"rezim\":\"" + aktualniRezim + 
                      "\",\"isfound\":" + (lastScannedUser.isFound ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });

    // 5. API - Zápis hodnocení
    server->on("/api/hodnotit", HTTP_POST, [](AsyncWebServerRequest *request){
        if(request->hasParam("uid", true)) {
            String uid = request->getParam("uid", true)->value();
            uid.trim();

            if (uid.length() > 0) {
                ratedList.push_back(uid); 
                String stars = request->hasParam("stars", true) ? request->getParam("stars", true)->value() : "0";
                
                File f = SD.open("/hodnoceni.csv", FILE_APPEND);
                if(f) {
                    String cas = ReaderCore.getCurrentTime();
                    String jmeno = lastScannedUser.isFound ? (lastScannedUser.firstNameAscii + " " + lastScannedUser.lastNameAscii) : "Neznamy";
                    String idOsoby = lastScannedUser.isFound ? String(lastScannedUser.id) : "0";
                    f.printf("%s;%s;%s;%s;%s;%s;%s\n", 
                             cas.c_str(), uid.c_str(), idOsoby.c_str(), jmeno.c_str(), 
                             k_jidloId.c_str(), k_jidloNazev.c_str(), stars.c_str());
                    f.close();
                }
            }
        }
        lastKitchenUID = "";
        k_scanId++;
        request->send(200, "text/plain", "OK");
    });

    server->on("/api/clear", HTTP_GET, [](AsyncWebServerRequest *request){
        lastKitchenUID = "";
        k_scanId++;
        request->send(200, "text/plain", "OK");
    });

    // 6. Servírování fotografií z SD karty (/fotky/1.jpg)
    server->onNotFound([](AsyncWebServerRequest *request){
        String path = request->url();
        if (path.startsWith("/fotky/") && SD.exists(path)) {
            request->send(SD, path, "image/jpeg");
            return;
        }
        request->send(404, "text/plain", "Soubor nenalezen");
    });
}

void loop() {
    ReaderCore.update();

    // 1. ZPRACOVÁNÍ PŘILOŽENÍ ČIPU
    if (ReaderCore.isCardPresent()) {
        String uid = ReaderCore.readCardUID();
        UserData user = ReaderCore.findUserByUid(uid);
        lastScannedUser = user;

        // A) REŽIM PORUCHA
        if (ReaderCore.getState() == STATE_PORUCHA) {
            if (user.isFound) {
                ReaderCore.beep(60);
                String hint = ReaderCore.getSudokuHint(user.id);
                ReaderCore.setCurrentMessage(user.firstNameAscii + ":\n" + hint);
                ReaderCore.logAction(uid, "SUDOKU_HINT", hint);
            } else {
                ReaderCore.playError();
                ReaderCore.setCurrentMessage("NEZNAMY CIP\n" + uid);
            }
            // ZAPNUTÍ ČASOVAČE NA 5 SEKUND
            customScreenShowTime = millis();
            customScreenActive = true;
            ReaderCore.haltCard();
            return;
        }

        // B) REŽIM NORMAL (BĚŽNÝ PROVOZ BEZ TABLETU)
        if (aktualniRezim == "NORMAL") {
            if (user.isFound) {
                ReaderCore.playSuccess();
                String screenText = ReaderCore.parseUserTemplate(user);
                ReaderCore.setCurrentMessage(screenText);
                ReaderCore.logAction(uid, "BEZNY_SCAN", user.role);
            } else {
                ReaderCore.playError();
                ReaderCore.setCurrentMessage("NEZNAMA KARTA\n\nNeni v databazi");
            }
            customScreenShowTime = millis();
            customScreenActive = true;
            lastKitchenUID = "";
        } 
        // C) REŽIM VÝDEJ JÍDLA
        else if (aktualniRezim == "VYDEJ") {
            if (!user.isFound) {
                ReaderCore.playError();
                ReaderCore.setCurrentMessage("NEZNAMA KARTA\n\nNeni v databazi");
                lastKitchenUID = uid;
                k_stavPorce = "ZAMITNUTO";
                k_scanId++;
            } else {
                ReaderCore.playSuccess();
                if (uid == lastKitchenUID && (millis() - lastKitchenScanTime < 60000)) {
                    // Ignorujeme vícenásobné přiložení do 60s
                } else {
                    lastKitchenUID = uid;
                    lastKitchenScanTime = millis();
                    
                    bool uzJedl = false;
                    for (String vydanoUid : vydejList) { 
                        if (vydanoUid == uid) { uzJedl = true; break; } 
                    }
                    
                    if (uzJedl) { 
                        k_stavPorce = "PRIDAVEK";
                    } else {
                        k_stavPorce = "PRVNI PORCE";
                        vydejList.push_back(uid);
                    }
                    
                    vydanoPorciCelkem++;
                    k_scanId++;

                    String extra = "Porce:" + String(vydanoPorciCelkem) + ";" + k_jidloId + ";" + k_jidloNazev + ";" + k_stavPorce;
                    ReaderCore.logAction(uid, "VYDEJ", extra);
                }
                
                String msg = user.firstNameAscii + " " + user.lastNameAscii + "\n" + user.role + "\nStav: Zamestnan";
                ReaderCore.setCurrentMessage(msg);
            }
        } 
        // D) REŽIM HODNOCENÍ JÍDLA
        else if (aktualniRezim == "HODNOCENI") {
            if (!user.isFound) {
                ReaderCore.playError();
                ReaderCore.setCurrentMessage("NEZNAMA KARTA\n\nNeni v databazi");
                lastKitchenUID = uid;
                k_stavHodnoceni = "NELZE HODNOTIT";
                k_scanId++;
            } else {
                ReaderCore.playSuccess();
                lastKitchenUID = uid;
                bool uzHodnotil = false;
                for (String rUid : ratedList) { 
                    if (rUid == uid) { uzHodnotil = true; break; } 
                }
                k_stavHodnoceni = uzHodnotil ? "HODNOTIL" : "NEHODNOTIL";
                k_scanId++;

                String msg = user.firstNameAscii + " " + user.lastNameAscii + "\n" + user.role + "\nStav: Zamestnan";
                ReaderCore.setCurrentMessage(msg);
            }
        }
        ReaderCore.haltCard();
    }

    // 2. AUTOMATICKÉ VYPRŠENÍ OBRAZOVKY PO 5 SEKUNDÁCH (PRO PORUCHU I NORMAL)
    if (customScreenActive && (millis() - customScreenShowTime > 5000)) {
        customScreenActive = false;
        if (ReaderCore.getState() == STATE_PORUCHA) {
            // Návrat do výchozího textu poruchy
            ReaderCore.setCurrentMessage("Prilozte kartu...");
        }
    }

    // 3. OBNOVA KLIDOVÉ OBRAZOVKY V REŽIMU NORMAL
    if (!customScreenActive && ReaderCore.getState() == STATE_NORMAL && aktualniRezim == "NORMAL") {
        static unsigned long lastIdleUpdate = 0;
        if (millis() - lastIdleUpdate > 1000) {
            lastIdleUpdate = millis();

            String timeStr = ReaderCore.getCurrentTime();
            String datum = timeStr.substring(8, 10) + "." + timeStr.substring(5, 7) + ".";
            String cas = timeStr.substring(11, 16);

            String idleMsg = cas + " | " + datum + "\n";
            idleMsg += "---------------------\n";
            idleMsg += "TABOROVY SYSTEM ON\n";
            idleMsg += "Prilozte cip...";

            ReaderCore.setCurrentMessage(idleMsg);
        }
    }
}