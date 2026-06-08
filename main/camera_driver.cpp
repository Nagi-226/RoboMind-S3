#include "camera_driver.h"

#include <utility>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#ifndef CONFIG_ROBOMIND_CAMERA_PIN_SCL
#define CONFIG_ROBOMIND_CAMERA_PIN_SCL 17
#endif

#ifndef CONFIG_ROBOMIND_CAMERA_PIN_SDA
#define CONFIG_ROBOMIND_CAMERA_PIN_SDA 18
#endif

static const char* TAG = "camera";

namespace {
constexpr i2c_port_t kSccbPort = I2C_NUM_0;
constexpr uint8_t kOv5640Address = 0x3C;  // OV5640 7-bit SCCB address.
constexpr uint16_t kChipIdHighReg = 0x300A;
constexpr uint16_t kChipIdLowReg = 0x300B;
constexpr uint16_t kExpectedChipId = 0x5640;
constexpr uint32_t kSccbClockHz = 100 * 1000;
constexpr TickType_t kI2cTimeout = pdMS_TO_TICKS(1000);
}

CameraDriver* CameraDriver::GetInstance() {
    static CameraDriver instance;
    return &instance;
}

CameraDriver::~CameraDriver() {
    ReleaseI2c();
}

bool CameraDriver::Initialize() {
    if (initialized_) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing OV5640 SCCB on SCL=%d SDA=%d",
             CONFIG_ROBOMIND_CAMERA_PIN_SCL, CONFIG_ROBOMIND_CAMERA_PIN_SDA);

    i2c_config_t i2c_config = {};
    i2c_config.mode = I2C_MODE_MASTER;
    i2c_config.sda_io_num = static_cast<gpio_num_t>(CONFIG_ROBOMIND_CAMERA_PIN_SDA);
    i2c_config.scl_io_num = static_cast<gpio_num_t>(CONFIG_ROBOMIND_CAMERA_PIN_SCL);
    i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_config.master.clk_speed = kSccbClockHz;

    esp_err_t ret = i2c_param_config(kSccbPort, &i2c_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = i2c_driver_install(kSccbPort, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return false;
    }
    i2c_installed_ = true;

    uint8_t chip_id_high = 0;
    uint8_t chip_id_low = 0;
    if (!ReadReg(kChipIdHighReg, &chip_id_high) || !ReadReg(kChipIdLowReg, &chip_id_low)) {
        ESP_LOGE(TAG, "Failed to read OV5640 CHIP_ID registers");
        ReleaseI2c();
        return false;
    }

    const uint16_t chip_id = (static_cast<uint16_t>(chip_id_high) << 8) | chip_id_low;
    if (chip_id != kExpectedChipId) {
        ESP_LOGE(TAG, "Unexpected OV5640 CHIP_ID: 0x%04X (expected 0x%04X)",
                 chip_id, kExpectedChipId);
        ReleaseI2c();
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "OV5640 CHIP_ID verified: 0x%04X", chip_id);
    return true;
}

bool CameraDriver::WriteReg(uint16_t reg, uint8_t val) {
    if (!i2c_installed_) {
        ESP_LOGE(TAG, "WriteReg called before I2C initialization");
        return false;
    }

    const uint8_t data[] = {
        static_cast<uint8_t>((reg >> 8) & 0xFF),
        static_cast<uint8_t>(reg & 0xFF),
        val,
    };
    esp_err_t ret = i2c_master_write_to_device(kSccbPort, kOv5640Address, data,
                                               sizeof(data), kI2cTimeout);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCCB write reg 0x%04X failed: %s", reg, esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool CameraDriver::ReadReg(uint16_t reg, uint8_t* val) {
    if (!val) {
        ESP_LOGE(TAG, "ReadReg called with null output");
        return false;
    }
    if (!i2c_installed_) {
        ESP_LOGE(TAG, "ReadReg called before I2C initialization");
        return false;
    }

    const uint8_t reg_addr[] = {
        static_cast<uint8_t>((reg >> 8) & 0xFF),
        static_cast<uint8_t>(reg & 0xFF),
    };
    esp_err_t ret = i2c_master_write_read_device(kSccbPort, kOv5640Address,
                                                 reg_addr, sizeof(reg_addr), val,
                                                 1, kI2cTimeout);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCCB read reg 0x%04X failed: %s", reg, esp_err_to_name(ret));
        return false;
    }
    return true;
}

void CameraDriver::SetFrameCallback(FrameCallback cb) {
    frame_callback_ = std::move(cb);
}

void CameraDriver::ReleaseI2c() {
    initialized_ = false;
    if (!i2c_installed_) {
        return;
    }

    esp_err_t ret = i2c_driver_delete(kSccbPort);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver delete failed: %s", esp_err_to_name(ret));
    }
    i2c_installed_ = false;
}
