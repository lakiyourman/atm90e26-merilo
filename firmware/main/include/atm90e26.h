#pragma once

#include <stdint.h>
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_http_server.h"

/* -----------------------------------------------------------------------
 * ATM90E26 Register Map
 * ----------------------------------------------------------------------- */

/* Special registers */
#define ATM90E26_SOFTRESET   0x00
#define ATM90E26_SYSSTATUS   0x01
#define ATM90E26_FUNCEN      0x02
#define ATM90E26_SAGTH       0x03
#define ATM90E26_LASTDATA    0x06

/* Calibration config registers */
#define ATM90E26_CALSTART    0x20   /* Write 0x5678 to unlock */
#define ATM90E26_PLCONSTH    0x21
#define ATM90E26_PLCONSTL    0x22
#define ATM90E26_LGAIN       0x23
#define ATM90E26_LPHI        0x24
#define ATM90E26_NGAIN       0x25
#define ATM90E26_NPHI        0x26
#define ATM90E26_PSTARTTH    0x27
#define ATM90E26_PNOLTH      0x28
#define ATM90E26_QSTARTTH    0x29
#define ATM90E26_QNOLTH      0x2A
#define ATM90E26_MMODE       0x2B
#define ATM90E26_CS1         0x2C   /* Checksum 1 */

/* Adjustment registers */
#define ATM90E26_ADJSTART    0x30   /* Write 0x5678 to unlock */
#define ATM90E26_UGAIN       0x31
#define ATM90E26_IGAINL      0x32
#define ATM90E26_IGAINN      0x33
#define ATM90E26_UOFFSET     0x34
#define ATM90E26_IOFFSETL    0x35
#define ATM90E26_POFFSETL    0x37
#define ATM90E26_QOFFSETL    0x38
#define ATM90E26_CS2         0x3B   /* Checksum 2 */

/* Measurement output registers */
#define ATM90E26_APENERGY    0x40   /* Forward active energy */
#define ATM90E26_ANENERGY    0x41   /* Reverse active energy */
#define ATM90E26_ATENERGY    0x42   /* Absolute active energy */
#define ATM90E26_IRMS        0x48   /* L line current RMS */
#define ATM90E26_URMS        0x49   /* Voltage RMS */
#define ATM90E26_PMEAN       0x4A   /* L line mean active power */
#define ATM90E26_QMEAN       0x4B   /* L line mean reactive power */
#define ATM90E26_FREQ        0x4C   /* Voltage frequency */
#define ATM90E26_POWERF      0x4D   /* Power factor */
#define ATM90E26_SMEAN       0x4F   /* Mean apparent power */

/* -----------------------------------------------------------------------
 * Calibration values
 * Konfiguracija za:
 *   - Naponski delitelj: 230Vrms → 7Vrms sekundar transformatora → ATM90E26 ulaz
 *   - Strujni transformator: SCT013 (100A:50mA), burden otpornik ~33 ohm
 *
 * UGAIN i IGAINL treba eksperimentalno podesiti koristeći referentni merač!
 * Vrednosti ispod su polazna tačka – mogu se promeniti u sdkconfig.
 * ----------------------------------------------------------------------- */
#define ATM90E26_PLCONSTH_VAL  0x01C9   /* Gornja rec PL_Const */
#define ATM90E26_PLCONSTL_VAL  0x3C00   /* Donja rec PL_Const  */
#define ATM90E26_LGAIN_VAL     0x4000   /* L line gain         */
#define ATM90E26_LPHI_VAL      0x0000   /* L fazni ofset       */
#define ATM90E26_UGAIN_VAL     0xD9D8   /* Voltage gain     0x4000 -Lazar   */ 
#define ATM90E26_IGAINL_VAL    0xAF5A   /* Current gain        */
#define ATM90E26_MMODE_VAL     0x9422   /* L+N, 50Hz, CF pulse */
#define ATM90E26_PSTARTTH_VAL  0x08BD   /* ~0.1 W startup thr  */
#define ATM90E26_PNOLTH_VAL    0x0000
#define ATM90E26_QSTARTTH_VAL  0x0AEC
#define ATM90E26_QNOLTH_VAL    0x0000

/* -----------------------------------------------------------------------
 * Data structure
 * ----------------------------------------------------------------------- */
typedef struct {
    float    voltage_rms;    /* V   */
    float    current_rms;    /* A   */
    float    frequency;      /* Hz  */
    float    active_power;   /* W   */
    float    power_factor;   /* dimensionless, -1..1 */
    float    active_energy;  /* Wh, kumulativno      */
    uint16_t sys_status;     /* ATM90E26 SysStatus register */
} atm90e26_data_t;

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */
esp_err_t atm90e26_init(spi_host_device_t host);
esp_err_t atm90e26_read_all(atm90e26_data_t *data);
esp_err_t atm90e26_read_all_test(atm90e26_data_t *data);
uint16_t  atm90e26_get_sys_status(void);