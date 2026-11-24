# atm90e26-merilo

A device based on the **ATM90E26** energy metering IC (Microchip) and an **ESP32-S2** microcontroller, capable of accurately measuring:
- Root Mean Square (**RMS**) AC voltage,
- Root Mean Square (**RMS**) AC current,
- AC voltage frequency,
- Instantaneous active power,
- Cumulative active electrical energy consumption.

**Hardware Design**
- AC measurement voltage input connected via a `PJ-002AH-SMT-TR` barrel jack connector.
- Current measurement using an **SCT013** (YHDC) current transformer.
- Current transformer connected via an `SJ-3523-SMT-TR` 3.5 mm audio jack connector.

Measured data is simultaneously routed to three outputs:
1. **Real-Time Web Dashboard** via WebSockets (supporting simultaneous monitoring of multiple devices).
2. **Local 16×2 LCD Display** (ST7066U controller) for quick inspection of the IP address and live readings.
3. **Micro SD Card** for long-term local data logging.

<img width="564" height="794" alt="image" src="https://github.com/user-attachments/assets/16438e85-adaf-4fe2-92d5-7577e2fea84b" />

<img width="377" height="545" alt="image" src="https://github.com/user-attachments/assets/66a72a88-13ed-4c03-9686-2497036e9e17" />

---

## Hardware Architecture

<img width="1193" height="844" alt="image" src="https://github.com/user-attachments/assets/f62dd8b1-0379-4998-b6a2-6e9c75cc9408" />

The printed circuit board (PCB) and electrical schematic were designed in **KiCad 9.0**. The bottom layer is laid out as a continuous ground plane (GND plane), while analog and digital signals are physically separated to minimize noise.

### Main Subsystems:
- **Power Supply (PSU):** The device is powered directly from the measurement voltage input (`PJ-002AH-SMT-TR`), eliminating the need for an external power adapter. The input voltage passes through a varistor (`V07E11P`), a bridge rectifier (`B40R`), a capacitor bank for ripple filtering, and a high-efficiency **TPS62162DSGT** buck regulator (`3.3V` output). Using jumper `J8` (`PSU_SEL`), the internal power supply can be physically disconnected to power the board exclusively from an external programmer.
- **Measurement Front-End:**
  - *Voltage Channel:* A voltage divider (series chain of $120\,\text{k}\Omega$ resistors to $1\,\text{k}\Omega$) with an RC anti-aliasing filter scales the input voltage to match the ATM90E26 input range ($120\,\mu V_{RMS} \sim 600\,mV_{RMS}$).
  - *Current Channel:* Audio jack connector (`SJ-3523-SMT-TR`) for an **SCT013 (100A:50mA)** current transformer with a burden resistor $R_{10}$ ($51\,\Omega$ / $33\,\Omega$) and a differential RC low-pass filter.
- **Microcontroller (MCU):** **ESP32-S2-SOLO-2-N4** (single-core Xtensa LX7, low power consumption). Features a native USB-C interface with ESD protection (`ESDA5V3SC5`), transistor-based auto-reset circuitry (`Q1`, `Q2` - SS8050), and a 6-pin header (`J3`) for an external UART programmer.
- **Metering IC:** **ATM90E26-YU** with an external $8.192\,\text{MHz}$ crystal oscillator, hardware mode configuration via jumpers `J6`/`J7` (`MMD0`, `MMD1`), and SPI communication with the MCU.
- **Peripherals:** 16×2 character LCD (`HY1602E`) wired in 4-bit mode with a contrast potentiometer `RV1`, and a micro SD card slot (`MSD-4-A`) on the SPI bus.

### Pinout and Communication (ESP32-S2 ↔ Peripherals)

| Peripheral | Interface | ESP32-S2 Pins | Notes |
| :--- | :--- | :--- | :--- |
| **ATM90E26** | SPI2 (Mode 3, 10 kHz) | `MOSI`: GPIO15, `MISO`: GPIO16, `SCLK`: GPIO4, `CS`: GPIO18 | $33\,\Omega$ series damping resistors on lines |
| **Micro SD** | SPI | Shared SPI bus, `CS_SD`: GPIO21 | Infrastructure ready in firmware |
| **LCD 16×2** | 4-bit parallel | `RS`/`E`/`D4-D7`: GPIO8 – GPIO14 | ST7066U controller |
| **USB-C** | Native USB | `D-`: GPIO19, `D+`: GPIO20 | Direct flashing and serial monitor |

---

## Firmware (ESP-IDF)

The firmware is written in **C** using the **ESP-IDF v5.x** framework and FreeRTOS.

### Code Structure (`firmware/main/`)
- `main.c` — Initialization of NVS flash, GPIO pins, LCD, Wi-Fi subsystem, SPI bus, ATM90E26 IC, and startup of FreeRTOS tasks and the HTTP server.
- `src/atm90e26.c` — ATM90E26 driver: SPI transactions (24-bit frames), unlocking and writing calibration registers (`CALSTART`, `ADJSTART`), automatic checksum calculation (`CS1`, `CS2`), and conversion of raw register values into physical units.
- `src/wifi.c` — Wi-Fi connection management: starting the initial Access Point (`Osiguraci_wifi_esp32`), network scanning (`APSTA`), and switching to `STA` mode with retry and fallback logic.
- `src/server.c` — Embedded HTTP server (port `80`) serving the web application and REST endpoints (`/scan`, `/set_wifi`, `/info`, `/status`).
- `src/socket.c` — WebSocket server (`/ws`) supporting up to 4 concurrent clients and broadcasting JSON measurement frames every second.
- `src/my_tasks.c` — FreeRTOS tasks for periodic sensor polling and data logging.
- `src/lcd.c` — 4-bit driver for the 16×2 character LCD.
- `index.html` — Single-page Web Dashboard application embedded directly into the microcontroller flash memory.

---

## Web Dashboard and API

On initial boot, the device creates an open Wi-Fi Access Point named **`Osiguraci_wifi_esp32`** and displays its IP address (`192.168.4.1`) on the LCD screen. Through the web interface, the user can connect the device to a local Wi-Fi network, after which the newly assigned IP address is shown on the display.

<img width="1254" height="674" alt="image" src="https://github.com/user-attachments/assets/8a4b6034-e82d-433e-a132-7bc2d8dc29e7" />

### Sample WebSocket JSON Payload (`/ws`):
```json
{
  "voltage": 230.15,
  "current": 1.234,
  "frequency": 50.01,
  "power": 284.2,
  "power_factor": 0.998,
  "energy": 12.0,
  "status": 0
}
```

### HTTP API Endpoints:
| Route | Method | Description |
| :--- | :---: | :--- |
| `/` | `GET` | Serves the embedded `index.html` Web Dashboard |
| `/scan` | `GET` | Scans nearby Wi-Fi networks and returns a JSON array with RSSI values |
| `/set_wifi` | `POST` | Accepts `ssid` and `password` and initiates connection to the local network |
| `/info` | `GET` | Returns current SSID, MAC address, and AP configuration |
| `/status` | `GET` | Returns Wi-Fi connection status (`Idle` / `Connecting` / `Success` / `Failed`) |
| `/ws` | `GET` | WebSocket endpoint for real-time telemetry streaming |

---

## Building and Flashing the Firmware

1. Install **ESP-IDF (v5.x)** and set up the environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
2. Clone the repository and navigate to the firmware directory:
   ```bash
   git clone [https://github.com/your-username/atm90e26-merilo.git](https://github.com/your-username/atm90e26-merilo.git)
   cd atm90e26-merilo/firmware
   ```
3. Set the target chip to `esp32s2`:
   ```bash
   idf.py set-target esp32s2
   ```
4. Build and flash the firmware to the board (via USB-C or an external programmer):
   ```bash
   idf.py build
   idf.py -p /dev/ttyACM0 flash monitor
   ```

---

## Project Status and Hardware Revisions (Rev 1.0)

### Completed Milestones:
- [x] Stable `3.3V` power rail derived directly from the measurement voltage input.
- [x] Successful flashing of the ESP32-S2 via native USB and external programmer.
- [x] Full WebSocket telemetry, Wi-Fi provisioning, and multi-device Web Dashboard.
- [x] Status and IP address readout on the 16×2 LCD screen.
- [x] Verified register reading and calibration of the ATM90E26 on a prototype setup.

### Identified Hardware Issues on Rev 1.0 PCB (for Rev 1.1):
1. **Bridge Rectifier (`B40R`):** Incorrect pin mapping on the PCB footprint in the first revision (patched manually on the prototype).
2. **LCD Backlight:** Missing PCB traces to the anode and cathode (`LED+` / `LED-`) for the display backlight.
3. **Crystal Oscillator (`8.192 MHz`):** Load capacitors ($\sim 22\,\text{pF}$ to GND) need to be added for reliable oscillation on the ATM90E26 clock pins.
4. **SPI `MOSI` Line (Under Investigation):** Oscilloscope measurements confirmed clean `SCLK`, `CS`, and `MISO` waveforms, whereas the `MOSI` line exhibits a $\sim 1\,\text{V}$ DC offset and erratic behavior on the assembled board, preventing reliable register initialization on the PCB.
