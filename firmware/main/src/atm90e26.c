    #include "atm90e26.h"
    #include "driver/gpio.h"
    #include "esp_log.h"
    #include <string.h>

    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"

    static const char *TAG = "ATM90E26";

    #define PIN_CS_ATM 18 //final: 18

    static spi_device_handle_t spi;

    static int utest=0;
    
    
    static uint16_t atm90e26_read_reg(uint8_t reg)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    uint8_t tx_buf[3] = { (uint8_t)(reg | 0x80), 0x00, 0x00 };
    uint8_t rx_buf[3] = { 0, 0, 0 };

    t.length    = 24;  
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) return 0xFFFF;

    return (uint16_t)((rx_buf[1] << 8) | rx_buf[2]);
}

    static void atm90e26_write_reg(uint8_t reg, uint16_t val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    uint8_t tx_buf[3] = {
        (uint8_t)(reg & 0x7F),   // Write bit 0 + Address
        (uint8_t)(val >> 8),     // Data High
        (uint8_t)(val & 0xFF)    // Data Low
    };

    t.length    = 24; 
    t.tx_buffer = tx_buf;

    spi_device_transmit(spi, &t);
}

    static uint16_t calc_cs1(void)
    {
        uint16_t cs = 0;
        cs += ATM90E26_PLCONSTH_VAL;
        cs += ATM90E26_PLCONSTL_VAL;
        cs += ATM90E26_LGAIN_VAL;
        cs += ATM90E26_LPHI_VAL;
        cs += 0x0000; 
        cs += 0x0000;   
        cs += ATM90E26_PSTARTTH_VAL;
        cs += ATM90E26_PNOLTH_VAL;
        cs += ATM90E26_QSTARTTH_VAL;
        cs += ATM90E26_QNOLTH_VAL;
        cs += ATM90E26_MMODE_VAL;
        return (~cs) + 1;
    }

    static uint16_t calc_cs2(void)
    {
        uint16_t cs = 0;
        cs += ATM90E26_UGAIN_VAL;
        cs += ATM90E26_IGAINL_VAL;
        cs += 0x0000;   
        cs += 0x0000;   
        cs += 0x0000;   
        cs += 0x0000;   
        cs += 0x0000;  
        cs += 0x0000;   
        cs += 0x0000;  
        cs += 0x0000;   
        return (~cs) + 1;
    }

    uint16_t atm90e26_get_sys_status(void)
    {
        return atm90e26_read_reg(ATM90E26_SYSSTATUS);
    }

    esp_err_t atm90e26_init(spi_host_device_t host)
    {

        spi_device_interface_config_t devcfg = {
            .clock_speed_hz = 10000,
            .mode           = 3, 
            .spics_io_num   = PIN_CS_ATM,
            .queue_size     = 7,
            .address_bits   = 0,
        };
    
        ESP_ERROR_CHECK(spi_bus_add_device(host, &devcfg, &spi));

        atm90e26_write_reg(ATM90E26_SOFTRESET, 0x789A);
        vTaskDelay(pdMS_TO_TICKS(1000));

        atm90e26_write_reg(ATM90E26_CALSTART, 0x5678); 
        vTaskDelay(pdMS_TO_TICKS(10));
        uint16_t readback = atm90e26_read_reg(ATM90E26_CALSTART);

        if (readback != 0x5678) {
            ESP_LOGE(TAG, "ATM90E26 Hardware not found! SPI Read: 0x%04X", readback);
            return ESP_ERR_NOT_FOUND; 
          }

        atm90e26_write_reg(ATM90E26_PLCONSTH,  ATM90E26_PLCONSTH_VAL);
        atm90e26_write_reg(ATM90E26_PLCONSTL,  ATM90E26_PLCONSTL_VAL);
        atm90e26_write_reg(ATM90E26_LGAIN,     ATM90E26_LGAIN_VAL);
        atm90e26_write_reg(ATM90E26_LPHI,      ATM90E26_LPHI_VAL);
        atm90e26_write_reg(ATM90E26_NGAIN,     0x0000);
        atm90e26_write_reg(ATM90E26_NPHI,      0x0000);
        atm90e26_write_reg(ATM90E26_PSTARTTH,  ATM90E26_PSTARTTH_VAL);
        atm90e26_write_reg(ATM90E26_PNOLTH,    ATM90E26_PNOLTH_VAL);
        atm90e26_write_reg(ATM90E26_QSTARTTH,  ATM90E26_QSTARTTH_VAL);
        atm90e26_write_reg(ATM90E26_QNOLTH,    ATM90E26_QNOLTH_VAL);
        atm90e26_write_reg(ATM90E26_MMODE,     ATM90E26_MMODE_VAL);
        atm90e26_write_reg(ATM90E26_CS1,       calc_cs1());

        uint16_t status = atm90e26_get_sys_status();
        if (status & 0x0001) {
            ESP_LOGE(TAG, "CS1 checksum error! SysStatus=0x%04X", status);
            return ESP_ERR_INVALID_CRC;
        }

        atm90e26_write_reg(ATM90E26_ADJSTART,  0x5678);
        atm90e26_write_reg(ATM90E26_UGAIN,     ATM90E26_UGAIN_VAL);
        atm90e26_write_reg(ATM90E26_IGAINL,    ATM90E26_IGAINL_VAL);
        atm90e26_write_reg(ATM90E26_IGAINN,    0x0000);
        atm90e26_write_reg(ATM90E26_UOFFSET,   0x0000);
        atm90e26_write_reg(ATM90E26_IOFFSETL,  0x0000);
        atm90e26_write_reg(ATM90E26_POFFSETL,  0x0000);
        atm90e26_write_reg(ATM90E26_QOFFSETL,  0x0000);
        atm90e26_write_reg(ATM90E26_CS2,       calc_cs2());

        status = atm90e26_get_sys_status();

    
        

        if (status & 0x0002) {
            ESP_LOGE(TAG, "CS2 checksum error! SysStatus=0x%04X", status);
            return ESP_ERR_INVALID_CRC;
        }

        ESP_LOGI(TAG, "ATM90E26 init OK, SysStatus=0x%04X", status);
        return ESP_OK;
    }

    esp_err_t atm90e26_read_all(atm90e26_data_t *data)
    {
        if (!data) return ESP_ERR_INVALID_ARG;

        uint16_t urms   = atm90e26_read_reg(ATM90E26_URMS);
        uint16_t irms   = atm90e26_read_reg(ATM90E26_IRMS);
        uint16_t freq   = atm90e26_read_reg(ATM90E26_FREQ);
        uint16_t pmean  = atm90e26_read_reg(ATM90E26_PMEAN);
        uint16_t powerf = atm90e26_read_reg(ATM90E26_POWERF);
        uint16_t energy = atm90e26_read_reg(ATM90E26_APENERGY);

        data->voltage_rms   = urms   / 100.0f; //(float)urms / 2949.8f
        data->current_rms   = irms   / 1000.0f;
        data->frequency     = freq   / 100.0f;

        data->active_power  = (int16_t)pmean / 10.0f;

        data->power_factor  = (int16_t)powerf / 1000.0f;

        data->active_energy = energy * 1.0f;

        data->sys_status = atm90e26_get_sys_status();

        return ESP_OK;
    }

    esp_err_t atm90e26_read_all_test(atm90e26_data_t *data)
    {
        if (!data) return ESP_ERR_INVALID_ARG;

        utest++;

        uint16_t urms   = utest;
        uint16_t irms   = 20;
        uint16_t freq   = 50;
        uint16_t pmean  = atm90e26_read_reg(ATM90E26_PMEAN);
        uint16_t powerf = atm90e26_read_reg(ATM90E26_POWERF);
        uint16_t energy = 1;

        data->voltage_rms   = urms   / 100.0f;
        data->current_rms   = irms   / 1000.0f;
        data->frequency     = freq   / 100.0f;

        data->active_power  = (int16_t)pmean / 10.0f;

        data->power_factor  = (int16_t)powerf / 1000.0f;

        data->active_energy = energy * 1.0f;

        data->sys_status = atm90e26_get_sys_status();

        return ESP_OK;
    }
