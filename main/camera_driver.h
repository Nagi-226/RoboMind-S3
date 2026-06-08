#pragma once
#ifndef ROBOMIND_CAMERA_DRIVER_H
#define ROBOMIND_CAMERA_DRIVER_H

#include <cstddef>
#include <cstdint>
#include <functional>

class CameraDriver {
public:
    static CameraDriver* GetInstance();

    bool Initialize();  // Configure SCCB I2C and verify OV5640 CHIP_ID.

    bool WriteReg(uint16_t reg, uint8_t val);
    bool ReadReg(uint16_t reg, uint8_t* val);
    bool IsInitialized() const { return initialized_; }

    using FrameCallback = std::function<void(const uint8_t* data, size_t len)>;
    void SetFrameCallback(FrameCallback cb);

private:
    CameraDriver() = default;
    ~CameraDriver();
    CameraDriver(const CameraDriver&) = delete;
    CameraDriver& operator=(const CameraDriver&) = delete;

    void ReleaseI2c();

    FrameCallback frame_callback_;
    bool initialized_ = false;
    bool i2c_installed_ = false;
};

#endif  // ROBOMIND_CAMERA_DRIVER_H