#include "ReaderCore.h"

unsigned long customScreenShowTime = 0;
bool customScreenActive = false;

void setup() {
    ReaderCore.begin("KONIRNA", "Tabor-Konirna", "tajneheslo");
}

void loop() {
    ReaderCore.update();

    // 1. ZPRACOVÁNÍ RFID KARTY
    if (ReaderCore.isCardPresent()) {
        String uid = ReaderCore.readCardUID();
        UserData user = ReaderCore.findUserByUid(uid);

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

    // 2. AUTOMATICKÉ VYPRŠENÍ OBRAZOVKY PO 5 SEKUNDÁCH (PRO PORUCHU I NORMAL)
    if (customScreenActive && (millis() - customScreenShowTime > 5000)) {
        customScreenActive = false;
        if (ReaderCore.getState() == STATE_PORUCHA) {
            ReaderCore.setCurrentMessage("Prilozte kartu...");
        }
    }

    // 3. OBNOVA KLIDOVÉ OBRAZOVKY V REŽIMU NORMAL
    if (!customScreenActive && ReaderCore.getState() == STATE_NORMAL) {
        static unsigned long lastIdleUpdate = 0;
        if (millis() - lastIdleUpdate > 1000) {
            lastIdleUpdate = millis();

            String timeStr = ReaderCore.getCurrentTime();
            String datum = timeStr.substring(8, 10) + "." + timeStr.substring(5, 7) + ".";
            String cas = timeStr.substring(11, 16);

            String idleMsg = cas + " | " + datum + "\n";
            idleMsg += "---------------------\n";
            idleMsg += "SPOLECNOST SPACE K\n";
            idleMsg += "Stredisko pro ocistu\n";
            idleMsg += "telesne schranky\n\n";
            idleMsg += "Prilozte cip...";

            ReaderCore.setCurrentMessage(idleMsg);
        }
    }
}