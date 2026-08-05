#pragma once

#include <cstdint>
#include <unistd.h>
#include <string>

class LaserDistance {
public:
    LaserDistance(const std::string& device = "/dev/ttyAMA3", int baudrate = 115200);
    ~LaserDistance();

    bool openPort();
    void closePort();

    // 거리 측정 명령 전송 및 응답 파싱. 성공시 true, distance(m)와 status 반환
    bool measure(double& distance, uint8_t& status);

    bool isOpen() const { return fd_ >= 0; }

private:
    std::string device_;
    int baudrate_;
    int fd_;
};
