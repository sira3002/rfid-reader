# Autonomous RFID Reader System

This project was developed as a functional **prototype (demo)** and an interactive feature for a children's summer camp themed around a space corporation. The main goal was to create an interactive "high-tech" atmosphere, complement the camp-wide game, and provide campers with a personalized interactive experience—the readers address them by name on an integrated OLED display and display their "camper profile" on a tablet during meal distribution.

The project serves as a proof of concept (PoC) designed to operate under demanding field conditions while offering modularity for future expansion.

---

## Project Overview & Architectural Decisions

The hardware and software were designed for seamless field operation without reliance on infrastructure networks, with an emphasis on easy administration via mobile devices.

### 1. Offline-First Approach (No Network Infrastructure)
* **Challenge:** The camp site lacks internet access, local area networks (LAN), and standard Wi-Fi routers.
* **Solution:** Readers operate on an **offline-first** architecture. All data (camper database, meal schedules, game hints, and operational logs) are stored locally on SD cards directly inside each device.
* **Peer-to-Peer Communication:** For critical synchronization between stations (e.g., global game status/outages), the devices utilize the **ESP-NOW** wireless protocol, enabling direct chip-to-chip communication without an access point. (Note: This feature was primarily used to test range limitations and proved somewhat unreliable across longer distances in terrain).

### 2. Power Autonomy
* **Challenge:** Absence of electrical outlets at remote forest and meadow locations.
* **Solution:** Hardware components and the ESP32 microcontrollers were optimized for low power consumption, allowing full-day operation powered solely by standard consumer power banks.

### 3. Remote Mobile Management
* **Challenge:** The need to update configurations, change templates, or retrieve logs without disassembling enclosures or removing SD cards.
* **Solution:** Each reader creates its own Wi-Fi Access Point (AP) and hosts an asynchronous web server (`ESPAsyncWebServer`). Camp staff can connect via smartphone to an intuitive web interface to:
  * Upload and manage files on the SD card (CSV databases, images).
  * Synchronize real-time clock (RTC) system time directly with the phone.
  * Switch operational modes and modify text display templates for the OLED screen.

### 4. Interactive Tablet Kiosk Interface
* **Challenge:** Children engage much better with visual feedback on a larger screen than on a small embedded OLED display.
* **Solution:** The kitchen reader hosts a local web interface accessible via Wi-Fi from a tablet. Swiping an RFID tag triggers the tablet display to show:
  * A personal "camper profile" featuring a photo, name, and assigned squad/team.
  * Visual highlights for allergies and dietary restrictions.
  * Served portion counters with automatic detection for second-helping eligibility.
  * An interactive star-rating system for meals with real-time feedback logging.

---

## Hardware & Technology Stack

### Hardware
* **Microcontroller:** ESP32 (Wi-Fi, ESP-NOW, Dual-Core)
* **RFID Module:** MFRC522 (13.56 MHz)
* **Integrated Display:** OLED SH1106G (128x64 px, I2C)
* **Real-Time Clock (RTC):** DS1302 (Battery-backed for accurate timekeeping)
* **Storage:** MicroSD Card Module (SPI bus) + Internal Flash (LittleFS for static web assets)
* **Peripherals:** RGB LED Strip (NeoPixel WS2812B), Piezo Buzzer, 4x4 Matrix Keypad

### Software Stack
* **Programming Language:** C++ / Arduino Framework for ESP32
* **Web Server & Backend:** `ESPAsyncWebServer`, `AsyncTCP`
* **Frontend:** HTML5, CSS3, JavaScript (ES6+, Fetch API, Polling)
* **Data Formats:** CSV for local databases on SD cards, JSON for REST API communication

---

## Code Architecture & Repository Structure

* **`libraries/ReaderCore/`** – Shared system core library. Encapsulates hardware drivers (RFID, OLED, RTC, SD, NeoPixel), manages REST APIs, parses CSV files, and handles ESP-NOW messaging.
* **`ReKitchen/`** – Application for the kitchen station. Handles Distribution (*Výdej*) and Rating (*Hodnocení*) modes while communicating with the tablet via the web interface.
* **`ReMain/`** – Master control reader with keypad interface. Used for PIN entry, resolving game failure states, and controlling overarching game mechanics.
* **`ReKon/`** – Standard station reader designed for general camp activity locations.

---

## Data Interfaces & File Structures

1. `users.csv` – Camper database (`id;rfidUid;firstName;lastName;...;teamId;role;diet`).
2. `jidla.csv` – Schedule and details of meals for distribution and rating.
3. `display_cfg.txt` – Text template configurations for the OLED display with dynamic placeholders (e.g., `{OSLOVENI}`, `{ODDIL}`).
4. `log_<READER_ID>.csv` – Audit logs recording tag scans and game actions.
5. `hodnoceni.csv` – Meal feedback and rating logs collected from the tablet interface.

---

## Future Roadmap & Enhancements

Thanks to the modular design of the `ReaderCore` library, the system can be expanded in future iterations with features such as:
* An integrated game scoring system and virtual camp currency.
* Interactive activity stations expanded with additional sensors and physical actuators.