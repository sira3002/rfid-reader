# Autonomní Systém RFID Čteček

Tento projekt byl vyvinut jako funkční **prototyp (demo)** a zážitkový prvek pro dětský tábor s tematikou vesmírné společnosti. Hlavním cílem bylo navodit interaktivní „high-tech“ atmosféru, doprovázet celotáborovou hru a nabídnout dětem zážitek z personalizované interakce – čtečky reagují na jejich jméno na OLED displeji a při výdeji jídel zobrazují jejich „profil táborníka“ na tabletu.

Projekt slouží jako ověřený koncept (PoC), který řeší náročné terénní podmínky a poskytuje modularitu pro rozšiřování v dalších letech.

---

## Přehled Projektu a Architektonická Rozhodnutí

Zařízení bylo navrženo pro bezproblémový provoz v terénu bez dostupné infrastrukturní sítě a s ohledem na snadnou obsluhu přes mobilní telefon.

### 1. Řešení Absence Síťové Infrastruktury (Offline-First)
* **Problém:** V místě konání tábora není k dispozici internet, LAN síť ani standardní Wi-Fi routery.
* **Řešení:** Čtečky fungují v režimu **Offline-first**. Všechna data (databáze účastníků, harmonogram jídel, herní nápovědy a provozní logy) jsou uložena lokálně na SD kartách přímo v zařízeních.
* **Peer-to-Peer Komunikace:** Pro kritickou synchronizaci mezi stanovišti (např. globální stav herní poruchy) využívají čtečky bezdrátový protokol **ESP-NOW**, který funguje přímým spojením mezi čipy bez nutnosti přístupového bodu. To však bylo použito spíše pro testování dosahu a na použité vzdálenosti to nebylo úplně spolehlivé.

### 2. Energetická Autonomie
* **Problém:** Absence elektrických zásuvek na stanovištích v lese a na louce.
* **Řešení:** Komponenty a mikrokontrolery ESP32 byly optimalizovány na nízký odběr proudu, což umožňuje celodenní provoz zařízení napájených pouze z běžných powerbank.

### 3. Vzdálená Správa z Mobilního Telefonu
* **Problém:** Nutnost upravovat konfigurace, měnit šablony nebo stahovat logy bez nutnosti rozebírat krabičky a vyndávat SD karty.
* **Řešení:** Každá čtečka vytváří vlastní Wi-Fi Access Point (AP) a běží na ní asynchronní webový server (`ESPAsyncWebServer`). Organizátor se připojí smartphonem k webovému rozhraní, kde může:
  * Nahrávat a spravovat soubory na SD kartě (CSV databáze, fotky).
  * Synchronizować přesný čas systémových hodin (RTC) podle mobilu.
  * Měnit provozní režimy a upravovat textové šablony pro OLED displej.

### 4. Interaktivní Kioskové Rozhraní na Tabletu
* **Problém:** Děti lépe vnímají vizuální odezvu na větší obrazovce než na malém integrovaném OLED displeji čtečky.
* **Řešení:** Kuchyňská čtečka poskytuje přes lokální Wi-Fi webové rozhraní pro tablet. Při přiložení RFID čipu se na tabletu zobrazí:
  * Osobní „profil táborníka“ s fotografií, jménem a příslušností k oddílu.
  * Zvýraznění alergií a dietních omezení.
  * Počítadlo vydaných porcí a automatická detekce nároku na přídavek.
  * Hvězdičkové hodnocení jídel s okamžitým ukládáním zpětné vazby.

---

## Použité Technologie a Hardware

### Hardware
* **Řídící jednotka:** ESP32 (Wi-Fi, ESP-NOW, více jáder)
* **RFID Modul:** MFRC522 (13.56 MHz)
* **Integrovaný displej:** OLED SH1106G (128x64 px, I2C)
* **Hodiny reálného času (RTC):** DS1302 (zálohované baterií pro uchování času)
* **Úložiště:** SD modul (SPI bus) + interní flash paměť (LittleFS pro statický web)
* **Periferie:** RGB LED pásek (NeoPixel WS2812B), akustický bzučák, maticová klávesnice 4x4

### Softwarový Stack
* **Jazyk:** C++ / Arduino Framework pro ESP32
* **Web Server & Backend:** `ESPAsyncWebServer`, `AsyncTCP`
* **Frontend:** HTML5, CSS3, JavaScript (ES6+, Fetch API, polling)
* **Datový Formát:** CSV pro lokální databáze na SD kartě, JSON pro API komunikaci

---

## Architektura Kódu a Struktura Repozitáře

* **`libraries/ReaderCore/`** – Sdílené jádro systému. Zapouzdřuje hardware (RFID, OLED, RTC, SD karty, NeoPixel), spravuje REST API, parsuje CSV a zajišťuje komunikaci přes ESP-NOW.
* **`ReKitchen/`** – Aplikace pro kuchyňské stanoviště. Zpracovává režimy Výdej a Hodnocení a komunikuje s tabletem přes webové rozhraní.
* **`ReMain/`** – Hlavní řídící čtečka s klávesnicí. Slouží k zadávání PIN kódů, řešení herních poruchových stavů a řízení mechaniky celotáborové hry.
* **`ReKon/`** – Standardní čtečka určená pro běžná táborová stanoviště.

---

## Datová Rozhraní a Soubory

1. `users.csv` – Databáze účastníků (`id;rfidUid;firstName;lastName;...;teamId;role;diet`)
2. `jidla.csv` – Seznam a harmonogram jídel pro výdej a hodnocení.
3. `display_cfg.txt` – Konfigurace textových šablon pro OLED displej s dynamickými značkami (např. `{OSLOVENI}`, `{ODDIL}`).
4. `log_<READER_ID>.csv` – Auditní záznamy o přiložení karet a herních akcích.
5. `hodnoceni.csv` – Záznamy hodnocení strava z tabletu.

---

## Potenciál pro Další Rozvoj

Jelikož je systém postaven na modularitě knihovny `ReaderCore`, v dalších letech je možné projekt rozšířit např. o:
* Zavedení herního bodovacího systému a virtuální táborové měny.
* Rozšíření interaktivních stanovišť s dalšími senzory a akčními členy.
