#include "ReaderCore.h"

// --- KONFIGURACE KLÁVESNICE A PINU ---
const String UNLOCK_PIN = "8582"; // PIN pro ukončení poruchy
String enteredPin = "";

byte rowPins[4] = {15, 33, 25, 26}; 
byte colPins[4] = {34, 35, 36, 39}; 
char keys[4][4] = {
  {'D','C','B','A'},
  {'#','9','6','3'},
  {'0','8','5','2'},
  {'*','7','4','1'}
};

unsigned long customScreenShowTime = 0; 
bool customScreenActive = false;

bool greenDone = false;
bool blueDone = false;
bool pinkDone = false;
bool orangeDone = false;
int done = 0;

UserData lastUser;

void setupKeypad() {
    for (int i = 0; i < 4; i++) {
        pinMode(rowPins[i], OUTPUT);
        digitalWrite(rowPins[i], HIGH);
        pinMode(colPins[i], INPUT); 
    }
}

char readKeypad() {
    static char lastKey = 0;
    static unsigned long lastDebounce = 0;
    char currentKey = 0;
    for (int r = 0; r < 4; r++) {
        digitalWrite(rowPins[r], LOW);
        for (int c = 0; c < 4; c++) {
            if (digitalRead(colPins[c]) == LOW) currentKey = keys[r][c];
        }
        digitalWrite(rowPins[r], HIGH);
    }
    if (currentKey != lastKey && millis() - lastDebounce > 50) {
        lastDebounce = millis();
        lastKey = currentKey;
        return currentKey;
    }
    if (currentKey == 0) lastKey = 0;
    return 0;
}

void setup() {
    ReaderCore.begin("NAMESTICKO", "Tabor-Namesticko", "tajneheslo");
    setupKeypad();
}

void loop() {
    ReaderCore.update();

    // 1. ZPRACOVÁNÍ KLÁVESNICE
    char k = readKeypad();
    if (k && ReaderCore.getState() == STATE_PORUCHA) {
        ReaderCore.beep(30);
        customScreenActive = false;

        if (k == '#') {
            enteredPin = "";
            ReaderCore.setCurrentMessage(ReaderCore.getState() == STATE_PORUCHA ? "Zadejte PIN:" : "PIN smazan");
        } else if (k == '*') {
            enteredPin = "";
            ReaderCore.setCurrentMessage("Zruseno");
        } else {
            enteredPin += k;
            ReaderCore.setCurrentMessage("Zadano: " + enteredPin);

            if (enteredPin.length() == 4) {
                if (enteredPin == UNLOCK_PIN) {
                    ReaderCore.playSuccess();
                    if (lastUser.teamId == 1) {
                        greenDone = true;
                        done ++;
                        ReaderCore.setCurrentMessage("KOD PRIJAT!");
                        enteredPin = "";
                    }
                    else if(lastUser.teamId == 2) {
                        blueDone = true;
                        done ++;
                        ReaderCore.setCurrentMessage("KOD PRIJAT!");
                        enteredPin = "";
                    }
                    else if(lastUser.teamId == 3) {
                        pinkDone = true;
                        done ++;
                        ReaderCore.setCurrentMessage("KOD PRIJAT!");
                        enteredPin = "";
                    }
                    else if(lastUser.teamId == 4) {
                        orangeDone = true;
                        done ++;
                        ReaderCore.setCurrentMessage("KOD PRIJAT!");
                        enteredPin = "";
                    }
                    else if(lastUser.teamId == 5) {
                        blueDone = true;
                        done ++;
                        ReaderCore.setCurrentMessage("KOD PRIJAT!");
                        enteredPin = "";
                    }

                    ReaderCore.logAction(lastUser.rfidUid, "RIGHT PIN", lastUser.role);

                    if (greenDone && blueDone && pinkDone && orangeDone) {
                        ReaderCore.setSystemState(STATE_NORMAL, true);
                        ReaderCore.setCurrentMessage("KOD PRIJAT!\n\n4 / 4 \n\nPorucha ukoncena");
                        greenDone = blueDone = pinkDone = orangeDone = false;
                        done = 0;
                    }
                    // Zruší poruchu a přes ESP-NOW rozešle STOP signál ostatním čtečkám
                   
                } else {
                    ReaderCore.playError();
                    ReaderCore.setCurrentMessage("CHYBNY KOD!");
                }
                enteredPin = "";
            }
        }
    }

    // 2. ZPRACOVÁNÍ PŘILOŽENÉ RFID KARTY
    if (ReaderCore.isCardPresent()) {
        String uid = ReaderCore.readCardUID();
        UserData user = ReaderCore.findUserByUid(uid);
        lastUser = user;

        if (ReaderCore.getState() == STATE_PORUCHA) {
            // REŽIM PORUCHA -> Zobrazení nápovědy k Sudoku
            if (user.isFound) {
                ReaderCore.beep(60);
                String hint = ReaderCore.getSudokuHint(user.id);
                ReaderCore.setCurrentMessage(user.firstNameAscii + ":\n" + hint);
                ReaderCore.logAction(uid, "SUDOKU_HINT", hint);
            } else {

                ReaderCore.playError();
                ReaderCore.setCurrentMessage("NEZNAMY CIP\n" + uid);
            }
            customScreenShowTime = millis();
            customScreenActive = true;
        } else {
            // REŽIM NORMAL -> Personalizovaná šablona
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
        }
        ReaderCore.haltCard();
    }

    // 3. AUTOMATICKÉ VYPRŠENÍ OBRAZOVKY PO 5 SEKUNDÁCH (PRO PORUCHU I NORMAL)
    if (customScreenActive && (millis() - customScreenShowTime > 3000)) {
        customScreenActive = false;
        if (ReaderCore.getState() == STATE_PORUCHA) {
            ReaderCore.setCurrentMessage("Prilozte kartu...");
        }
    }

    // 4. OBNOVA KLIDOVÉ OBRAZOVKY V REŽIMU NORMAL
    if (!customScreenActive && ReaderCore.getState() == STATE_NORMAL) {
        static unsigned long lastIdleUpdate = 0;
        if (millis() - lastIdleUpdate > 1000) {
            lastIdleUpdate = millis();

            String timeStr = ReaderCore.getCurrentTime();
            String datum = timeStr.substring(8, 10) + "." + timeStr.substring(5, 7) + ".";
            String cas = timeStr.substring(11, 16);

            String idleMsg = cas + " | " + datum + "\n";
            idleMsg += "---------------------\n\n";
            idleMsg += "VESMIRNA MISE SPACE K\n\n";
            idleMsg += "Prilozte cip...";

            ReaderCore.setCurrentMessage(idleMsg);
        }
    }
}