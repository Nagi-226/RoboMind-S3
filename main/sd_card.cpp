#include "sd_card.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/statvfs.h>

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"


static const char* TAG = "sd_card";

namespace {
constexpr int kSdModeSpi = 0;
constexpr size_t kMaxOpenFiles = 5;
constexpr size_t kAllocationUnitSize = 16 * 1024;

sdmmc_card_t* s_card = nullptr;
sdmmc_host_t s_host = {};
bool s_spi_bus_initialized = false;
}

SdCard* SdCard::GetInstance() {
    static SdCard instance;
    return &instance;
}

SdCard::~SdCard() {
    Unmount();
}

bool SdCard::Mount() {
    if (mounted_) {
        return true;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = kMaxOpenFiles;
    mount_config.allocation_unit_size = kAllocationUnitSize;

    esp_err_t ret = ESP_OK;
    if (CONFIG_ROBOMIND_SD_MODE == kSdModeSpi) {
        ESP_LOGI(TAG, "Mounting SD card in SPI mode at %s", mount_point_.c_str());
        s_host = SDSPI_HOST_DEFAULT();

        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = CONFIG_ROBOMIND_SD_PIN_MOSI;
        bus_cfg.miso_io_num = CONFIG_ROBOMIND_SD_PIN_MISO;
        bus_cfg.sclk_io_num = CONFIG_ROBOMIND_SD_PIN_CLK;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4000;

        ret = spi_bus_initialize(static_cast<spi_host_device_t>(s_host.slot), &bus_cfg,
                                 SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init for SD failed: %s", esp_err_to_name(ret));
            return false;
        }
        s_spi_bus_initialized = true;

        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = static_cast<gpio_num_t>(CONFIG_ROBOMIND_SD_PIN_CS);
        slot_config.host_id = static_cast<spi_host_device_t>(s_host.slot);

        ret = esp_vfs_fat_sdspi_mount(mount_point_.c_str(), &s_host, &slot_config,
                                      &mount_config, &s_card);
    } else {
        ESP_LOGI(TAG, "Mounting SD card in SDMMC mode at %s", mount_point_.c_str());
        s_host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        ret = esp_vfs_fat_sdmmc_mount(mount_point_.c_str(), &s_host, &slot_config,
                                      &mount_config, &s_card);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        if (s_spi_bus_initialized) {
            esp_err_t free_ret = spi_bus_free(static_cast<spi_host_device_t>(s_host.slot));
            if (free_ret != ESP_OK) {
                ESP_LOGE(TAG, "SPI bus free after mount failure failed: %s",
                         esp_err_to_name(free_ret));
            }
            s_spi_bus_initialized = false;
        }
        s_card = nullptr;
        return false;
    }

    mounted_ = true;
    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD mounted: %s", mount_point_.c_str());
    return true;
}

void SdCard::Unmount() {
    if (!mounted_) {
        return;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point_.c_str(), s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD unmount failed: %s", esp_err_to_name(ret));
    }
    s_card = nullptr;
    mounted_ = false;

    if (s_spi_bus_initialized) {
        ret = spi_bus_free(static_cast<spi_host_device_t>(s_host.slot));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SD SPI bus free failed: %s", esp_err_to_name(ret));
        }
        s_spi_bus_initialized = false;
    }
}

bool SdCard::IsMounted() const {
    return mounted_;
}

bool SdCard::SaveFile(const std::string& path, const uint8_t* data, size_t len) {
    if (!mounted_) {
        ESP_LOGE(TAG, "SaveFile failed: SD card is not mounted");
        return false;
    }
    if (path.empty() || (!data && len > 0)) {
        ESP_LOGE(TAG, "SaveFile failed: invalid arguments");
        return false;
    }

    const std::string full_path = ResolvePath(path);
    FILE* file = std::fopen(full_path.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Open for write failed: %s", full_path.c_str());
        return false;
    }

    const size_t written = (len > 0) ? std::fwrite(data, 1, len, file) : 0;
    if (written != len) {
        ESP_LOGE(TAG, "Write failed: %s (%u/%u bytes)", full_path.c_str(),
                 static_cast<unsigned>(written), static_cast<unsigned>(len));
        std::fclose(file);
        return false;
    }

    if (std::fclose(file) != 0) {
        ESP_LOGE(TAG, "Close after write failed: %s", full_path.c_str());
        return false;
    }
    return true;
}

bool SdCard::ReadFile(const std::string& path, std::vector<uint8_t>* out) {
    if (!mounted_) {
        ESP_LOGE(TAG, "ReadFile failed: SD card is not mounted");
        return false;
    }
    if (path.empty() || !out) {
        ESP_LOGE(TAG, "ReadFile failed: invalid arguments");
        return false;
    }

    const std::string full_path = ResolvePath(path);
    FILE* file = std::fopen(full_path.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Open for read failed: %s", full_path.c_str());
        return false;
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "Seek end failed: %s", full_path.c_str());
        std::fclose(file);
        return false;
    }
    const long size = std::ftell(file);
    if (size < 0) {
        ESP_LOGE(TAG, "Tell failed: %s", full_path.c_str());
        std::fclose(file);
        return false;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Seek start failed: %s", full_path.c_str());
        std::fclose(file);
        return false;
    }

    out->resize(static_cast<size_t>(size));
    if (size > 0) {
        const size_t read = std::fread(out->data(), 1, out->size(), file);
        if (read != out->size()) {
            ESP_LOGE(TAG, "Read failed: %s (%u/%u bytes)", full_path.c_str(),
                     static_cast<unsigned>(read), static_cast<unsigned>(out->size()));
            std::fclose(file);
            return false;
        }
    }

    std::fclose(file);
    return true;
}

bool SdCard::ListDir(const std::string& path, std::vector<std::string>* entries) {
    if (!mounted_) {
        ESP_LOGE(TAG, "ListDir failed: SD card is not mounted");
        return false;
    }
    if (path.empty() || !entries) {
        ESP_LOGE(TAG, "ListDir failed: invalid arguments");
        return false;
    }

    entries->clear();
    const std::string full_path = ResolvePath(path);
    DIR* dir = opendir(full_path.c_str());
    if (!dir) {
        ESP_LOGE(TAG, "Open dir failed: %s", full_path.c_str());
        return false;
    }

    while (dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        entries->emplace_back(entry->d_name);
    }

    if (closedir(dir) != 0) {
        ESP_LOGE(TAG, "Close dir failed: %s", full_path.c_str());
        return false;
    }
    return true;
}

uint64_t SdCard::GetFreeSpaceMB() {
    if (!mounted_) {
        ESP_LOGE(TAG, "GetFreeSpaceMB failed: SD card is not mounted");
        return 0;
    }

    statvfs stat = {};
    if (statvfs(mount_point_.c_str(), &stat) != 0) {
        ESP_LOGE(TAG, "statvfs failed: %s", mount_point_.c_str());
        return 0;
    }

    const uint64_t bytes = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
    return bytes / (1024ULL * 1024ULL);
}

std::string SdCard::ResolvePath(const std::string& path) const {
    if (path.rfind(mount_point_, 0) == 0) {
        return path;
    }
    if (!path.empty() && path[0] == '/') {
        return mount_point_ + path;
    }
    return mount_point_ + "/" + path;
}