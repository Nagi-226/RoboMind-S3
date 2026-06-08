#pragma once
#ifndef ROBOMIND_SD_CARD_H
#define ROBOMIND_SD_CARD_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SdCard {
public:
    static SdCard* GetInstance();

    bool Mount();
    void Unmount();
    bool IsMounted() const;

    bool SaveFile(const std::string& path, const uint8_t* data, size_t len);
    bool ReadFile(const std::string& path, std::vector<uint8_t>* out);
    bool ListDir(const std::string& path, std::vector<std::string>* entries);
    uint64_t GetFreeSpaceMB();

private:
    SdCard() = default;
    ~SdCard();
    SdCard(const SdCard&) = delete;
    SdCard& operator=(const SdCard&) = delete;

    std::string ResolvePath(const std::string& path) const;

    bool mounted_ = false;
    std::string mount_point_ = "/sdcard";
};

#endif  // ROBOMIND_SD_CARD_H
