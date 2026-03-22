// Copyright 2025 Harrison W.

#include "camera_control.hh"
#include <gtest/gtest.h>
#include <vector>
#include <array>

// Mocking the CameraHandle class for testing
class MockCameraHandle : public UvcUsbIo {
public:
    std::error_code info() const override {
        return {}; // Simulate successful info retrieval
    }

    std::error_code io(uint8_t unit, uint8_t selector, uint8_t query, std::vector<uint8_t>& data) override {
        // Simulate successful IO operation
        return {};
    }
};

// Test fixture for Camera
class CameraTest : public ::testing::Test {
protected:
    Camera camera;

    // CameraTest() : camera("/dev/video0") {}
    CameraTest() : camera("OBSBOT Tiny") {}

    void SetUp() override {
        // Any setup needed before each test
    }

    void TearDown() override {
        // Any cleanup needed after each test
    }
};

// Test for setting AI mode
TEST_F(CameraTest, SetAIMode) {
    camera.set_ai_mode(AIMode::NormalTracking);
    EXPECT_NO_THROW(camera.get_ai_mode());
}

// Test for setting exposure mode
TEST_F(CameraTest, SetExposureMode) {
    EXPECT_NO_THROW(camera.set_exposure_mode(ExposureMode::Manual));
    EXPECT_NO_THROW(camera.set_exposure_mode(ExposureMode::Global));
    EXPECT_NO_THROW(camera.set_exposure_mode(ExposureMode::Face));
}

// Test for getting camera status
TEST_F(CameraTest, GetCameraStatus) {
    CameraStatus status = camera.get_status();
    // EXPECT_TRUE(status.hdr_on); // Assuming HDR is on by default
    // std::cout << "AI mode: " << static_cast<int>(status.ai_mode) << std::endl;
    // EXPECT_EQ(status.ai_mode, AIMode::NoTracking); // Assuming default AI mode
}

// Test for setting HDR mode
TEST_F(CameraTest, SetHDRMode) {
    EXPECT_NO_THROW(camera.set_hdr_mode(true));
    EXPECT_NO_THROW(camera.set_hdr_mode(false));
}

// Test for invalid AI mode
TEST_F(CameraTest, InvalidAIMode) {
    EXPECT_THROW(camera.set_ai_mode(static_cast<AIMode>(999)), std::runtime_error);
}

// Test for insufficient data in CameraStatus decode
TEST(CameraStatusTest, DecodeInsufficientData) {
    std::vector<uint8_t> insufficient_data(10); // Less than 28 bytes
    EXPECT_THROW(CameraStatus::decode(insufficient_data), std::runtime_error);
}

// Test for valid CameraStatus decoding
TEST(CameraStatusTest, DecodeValidData) {
    std::vector<uint8_t> valid_data(28, 0); // Fill with zeros
    valid_data[0x18] = 0; // NoTracking
    valid_data[0x1c] = 0; // NormalTracking
    valid_data[0x6] = 1; // HDR on

    CameraStatus status = CameraStatus::decode(valid_data);
    EXPECT_EQ(status.ai_mode, AIMode::NoTracking);
    EXPECT_TRUE(status.hdr_on);
}