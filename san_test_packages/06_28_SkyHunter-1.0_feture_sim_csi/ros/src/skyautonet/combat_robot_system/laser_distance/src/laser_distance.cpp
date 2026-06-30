#include "laser_distance.hpp"
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

// 단일 측정 명령 패킷
static const uint8_t SINGLE_MEASURE_CMD[6] = {0xEE, 0x16, 0x02, 0x03, 0x02, 0x05};

LaserDistance::LaserDistance(const std::string& device, int baudrate)
    : device_(device), baudrate_(baudrate), fd_(-1)
{}

LaserDistance::~LaserDistance() {
    closePort();
}

bool LaserDistance::openPort() {
    if (fd_ >= 0) close(fd_);
    fd_ = open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    speed_t speed = B115200;
    switch (baudrate_) {
        case 9600: speed = B9600; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default: speed = B115200;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 0.1초 타임아웃 (단위 0.1s)
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));  // 1.5초 대기 (센서 초기화)
    return true;
}

void LaserDistance::closePort() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool LaserDistance::measure(double& distance, uint8_t& status) {
    if (fd_ < 0) return false;

    // 이전 버퍼 비우기
    tcflush(fd_, TCIFLUSH);

    int written = write(fd_, SINGLE_MEASURE_CMD, 6);
    if (written != 6) return false;

    uint8_t buf[32] = {0};
    int total_read = 0;
    int expected_len = 10; // 예상 응답 길이

    // 데이터가 다 들어올 때까지 반복해서 읽음 (타임아웃 VTIME 의존)
    while (total_read < expected_len) {
        int n = read(fd_, buf + total_read, expected_len - total_read);
        if (n > 0) {
            total_read += n;
        } else {
            // n == 0 (타임아웃) 또는 n < 0 (에러)
            break;
        }
    }

    if (total_read >= 10 && buf[0] == 0xEE && buf[1] == 0x16 && buf[4] == 0x02) {
        status = buf[5];
        int high = buf[6];
        int low = buf[7];
        int dec = buf[8];
        distance = high * 256 + low + dec * 0.01;
        return true;
    }
    return false;
}
