# atm90e26-merilo

Uređaj, zasnovan na mernom čipu **ATM90E26** (Microchip) i mikrokontroleru ESP32, koji ima mogućnost preciznog merenja:
• efektivne (RMS) vrednosti prostoperiodičnog napona,
• efektivne (RMS) vrednosti prostoperiodične struje,
• frekvencije prostoperiodičnog napona,
• trenutne aktivne snage,
• utrošene aktivne električne energije.

**Dizajn hardvera**
• Priključivanje ulaznog prostoperiodičnog napona na uređaj izvršeno pomoću konektora
PJ-002AH-SMT-TR.
• Merenje struje pomoću strujnog transformatora SCT013 (YHDC).
• Priključivanje strujnog transformatora na uređaj pomoću konektora SJ-3523-
SMT-TR.

Izmereni podaci se istovremeno prosleđuju na tri izlaza:
1. **Web Dashboard u realnom vremenu** putem WebSocket protokola (sa podrškom za praćenje više uređaja istovremeno).
2. **Lokalni 16×2 LCD ekran** (ST7066U kontroler) za brzi pregled IP adrese i trenutnih merenja.
3. **Micro SD karticu** za dugotrajno lokalno logovanje podataka.

4. ##  Hardverska arhitektura

Štampana ploča (PCB) i električna šema projektovane su u **KiCad 9.0** okruženju. Donji sloj ploče izveden je kao neprekidna masena ravan (GND plane), dok su analogni i digitalni signali fizički razdvojeni radi smanjenja šuma.

### Glavni podsistemi:
* **Napajanje (PSU):** Uređaj se napaja direktno sa mernog naponskog ulaza (`PJ-002AH-SMT-TR`), čime se eliminiše potreba za dodatnim adapterom. Ulazni napon prolazi kroz varistor (`V07E11P`), mostni ispravljač (`B40R`), banku kondenzatora za filtriranje *ripple*-a i visokoefikasni *buck* regulator **TPS62162DSGT** (izlaz `3.3V`). Jumperom `J8` (`PSU_SEL`) moguće je fizički odvojiti interno napajanje i napajati ploču isključivo sa eksternog programatora.
* **Ulazni merni stepen:**
  * *Naponski kanal:* Naponski delilac (niz otpornika $120\,\text{k}\Omega$ prema $1\,\text{k}\Omega$) sa RC antialiasing filterom prilagođava mrežni napon opsegu ulaza ATM90E26 ($120\,\mu V_{RMS} \sim 600\,mV_{RMS}$).
  * *Strujni kanal:* Audio džek konektor (`SJ-3523-SMT-TR`) za strujni transformator **SCT013 (100A:50mA)** sa *burden* otpornikom $R_{10}$ ($51\,\Omega$ / $33\,\Omega$) i diferencijalnim RC niskopropusnim filterom.
* **Mikrokontroler (MCU):** **ESP32-S2-SOLO-2-N4** (jednojezgarni Xtensa LX7, niska potrošnja). Poseduje *native* USB-C interfejs sa ESD zaštitom (`ESDA5V3SC5`), tranzistorsku auto-reset logiku (`Q1`, `Q2` - SS8050) kao i dodatni 6-pinski konektor (`J3`) za eksterni UART programator.
* **Merni čip:** **ATM90E26-YU** sa eksternim kristalom od $8.192\,\text{MHz}$, hardverskom konfiguracijom modova preko jumpera `J6`/`J7` (`MMD0`, `MMD1`) i SPI komunikacijom sa MCU-om.
* **Periferije:** 16×2 karakter LCD (`HY1602E`) povezan u 4-bitnom modu sa potenciometrom `RV1` za kontrast, kao i slot za micro SD karticu (`MSD-4-A`) na SPI magistrali.

### Pinout i komunikacija (ESP32-S2 ↔ Periferije)

| Periferija | Interfejs | Pinovi na ESP32-S2 | Napomena |
| :--- | :--- | :--- | :--- |
| **ATM90E26** | SPI2 (Mode 3, 10 kHz) | `MOSI`: GPIO15, `MISO`: GPIO16, `SCLK`: GPIO4, `CS`: GPIO18 | Redni otpornici $33\,\Omega$ na linijama |
| **Micro SD** | SPI | Deljeni SPI bus, `CS_SD`: GPIO21 | Infrastruktura spremna u firmveru |
| **LCD 16×2** | 4-bit paralelni | `RS`/`E`/`D4-D7`: GPIO8 – GPIO14 | Kontroler ST7066U |
| **USB-C** | Native USB | `D-`: GPIO19, `D+`: GPIO20 | Direktno programiranje i serijski monitor |

---

##  Firmver (ESP-IDF)

Firmver je pisan u programskom jeziku **C** koristeći **ESP-IDF v5.x** razvojni okvir i FreeRTOS.

### Struktura koda (`firmware/main/`)
* `main.c` — Inicijalizacija NVS memorije, GPIO pinova, LCD-a, Wi-Fi podsistema, SPI magistrale, ATM90E26 čipa i pokretanje FreeRTOS taskova i HTTP servera.
* `src/atm90e26.c` — Drajver za ATM90E26: SPI transakcije (24-bitni okviri), otključavanje i upis kalibracionih registara (`CALSTART`, `ADJSTART`), automatsko računanje kontrolnih suma (`CS1`, `CS2`) i konverzija sirovih registara u fizičke jedinice.
* `src/wifi.c` — Upravljanje Wi-Fi konekcijom: podizanje inicijalnog Access Point-a (`Osiguraci_wifi_esp32`), skeniranje mreža (`APSTA`) i prebacivanje u `STA` mod sa *retry* i *fallback* logikom.
* `src/server.c` — Ugrađeni HTTP server (port `80`) koji servira Web aplikaciju i REST rute (`/scan`, `/set_wifi`, `/info`, `/status`).
* `src/socket.c` — WebSocket server (`/ws`) sa podrškom za do 4 istovremena klijenta i periodičnim emitovanjem merenja u JSON formatu svake sekunde.
* `src/my_tasks.c` — FreeRTOS taskovi za periodično očitavanje senzora i logovanje podataka.
* `src/lcd.c` — Drajver za 16×2 LCD ekran u 4-bitnom režimu rada.
* `index.html` — *Single-page* Web Dashboard aplikacija ugrađena direktno u Flash memoriju mikrokontrolera.

---

##  Web Dashboard i API

Prilikom prvog pokretanja, uređaj kreira otvorenu Wi-Fi pristupnu tačku **`Osiguraci_wifi_esp32`** i na LCD ekranu ispisuje svoju IP adresu (`192.168.4.1`). Kroz web interfejs korisnik može povezati uređaj na lokalnu Wi-Fi mrežu, nakon čega se nova dodeljena IP adresa prikazuje na ekranu.

### Primer WebSocket JSON paketa (`/ws`):
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

### HTTP API rute:
| Ruta | Metod | Opis |
| :--- | :---: | :--- |
| `/` | `GET` | Vraća ugrađenu `index.html` Web aplikaciju |
| `/scan` | `GET` | Skenira okolne Wi-Fi mreže i vraća JSON listu sa RSSI vrednostima |
| `/set_wifi` | `POST` | Prima `ssid` i `password` i pokreće povezivanje na lokalnu mrežu |
| `/info` | `GET` | Vraća trenutni SSID, MAC adresu i AP konfiguraciju |
| `/status` | `GET` | Vraća status Wi-Fi konekcije (`Idle` / `Connecting` / `Success` / `Failed`) |
| `/ws` | `GET` | WebSocket endpoint za prijem telemetrije u realnom vremenu |

---

##  Pokretanje i kompajliranje firmvera

1. Instalirati **ESP-IDF (v5.x)** i podesiti okruženje:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
2. Klonirati repozitorijum i ući u direktorijum firmvera:
   ```bash
   git clone [https://github.com/vas-nalog/OhmSprint-ATM90E26.git](https://github.com/vas-nalog/OhmSprint-ATM90E26.git)
   cd OhmSprint-ATM90E26/firmware
   ```
3. Podesiti ciljni čip na `esp32s2`:
   ```bash
   idf.py set-target esp32s2
   ```
4. Kompajlirati i flešovati na ploču (putem USB-C ili eksternog programatora):
   ```bash
   idf.py build
   idf.py -p /dev/ttyACM0 flash monitor
   ```

---

##  Status projekta i uočene hardverske revizije (Rev 1.0)

### Uspešno realizovano:
- [x] Stabilan napon napajanja od `3.3V` dobijen direktno sa mernog ulaza.
- [x] Uspešno programiranje ESP32-S2 preko *native* USB-a i eksternog programatora.
- [x] Kompletna WebSocket telemetrija, Wi-Fi provisioning i multi-device Web Dashboard.
- [x] Ispis statusa i IP adrese na 16×2 LCD ekranu.
- [x] Testirano čitanje i kalibracija ATM90E26 na prototipskoj postavci.

### Uočeni hardverski bagovi na Rev 1.0 ploči (za Rev 1.1):
1. **Mostni ispravljač (`B40R`):** Pogrešno povezani pinovi na footprint-u na prvoj reviziji PCB-a (ispravljeno ručnim prevezivanjem na prototipu).
2. **Pozadinsko osvetljenje LCD-a:** Nedostaju veze ka anodi i katodi (`LED+` / `LED-`) za pozadinsko osvetljenje ekrana.
3. **Kristalni oscilator (`8.192 MHz`):** Potrebno je dodati opteretne kondenzatore od $\sim 22\,\text{pF}$ ka masi za stabilan rad oscilatora ATM90E26 čipa.
4. **SPI `MOSI` linija (u toku debagovanja):** Osciloskopskim merenjem utvrđeno je da su `SCLK`, `CS` i `MISO` signali uredni, dok `MOSI` signal ima DC ofset od $\sim 1\,\text{V}$ i nepredvidivo ponašanje na trenutnoj ploči, što onemogućava stabilnu inicijalizaciju registara na sastavljenom PCB-u.
