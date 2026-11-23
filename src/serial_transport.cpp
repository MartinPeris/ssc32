// SPDX-License-Identifier: BSD-3-Clause
#include "ssc32/serial_transport.hpp"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace ssc32 {
namespace {

speed_t to_termios_baud(const unsigned int baud_rate) {
  switch (baud_rate) {
    case 2400:
      return B2400;
    case 9600:
      return B9600;
    case 38400:
      return B38400;
    case 115200:
      return B115200;
    default:
      throw std::invalid_argument(
          "SSC-32 baud rate must be one of 2400, 9600, 38400, or 115200");
  }
}

[[noreturn]] void throw_system_error(const char* operation) {
  throw std::system_error(errno, std::generic_category(), operation);
}

int remaining_timeout_ms(const std::chrono::steady_clock::time_point deadline) {
  const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - std::chrono::steady_clock::now());
  if (remaining.count() <= 0) {
    return 0;
  }
  return static_cast<int>(remaining.count());
}

void wait_for_input(const int file_descriptor,
                    const std::chrono::steady_clock::time_point deadline) {
  pollfd descriptor{file_descriptor, POLLIN, 0};
  while (true) {
    const int result = ::poll(&descriptor, 1, remaining_timeout_ms(deadline));
    if (result > 0) {
      if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        throw std::runtime_error("SSC-32 serial connection closed while reading");
      }
      return;
    }
    if (result == 0) {
      throw std::runtime_error("Timed out waiting for SSC-32 response");
    }
    if (errno != EINTR) {
      throw_system_error("poll SSC-32 serial port");
    }
  }
}

std::uint8_t read_byte(const int file_descriptor,
                       const std::chrono::steady_clock::time_point deadline) {
  while (true) {
    wait_for_input(file_descriptor, deadline);
    std::uint8_t value{};
    const auto count = ::read(file_descriptor, &value, 1);
    if (count == 1) {
      return value;
    }
    if (count == 0) {
      throw std::runtime_error("SSC-32 serial connection closed while reading");
    }
    if (errno != EINTR && errno != EAGAIN) {
      throw_system_error("read SSC-32 serial port");
    }
  }
}

}  // namespace

SerialTransport::SerialTransport(const std::string& device,
                                 const unsigned int baud_rate) {
  const auto baud = to_termios_baud(baud_rate);
  file_descriptor_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (file_descriptor_ < 0) {
    throw_system_error("open SSC-32 serial port");
  }

  try {
    termios options{};
    if (::tcgetattr(file_descriptor_, &options) != 0) {
      throw_system_error("read SSC-32 serial settings");
    }

    ::cfmakeraw(&options);
    if (::cfsetispeed(&options, baud) != 0 || ::cfsetospeed(&options, baud) != 0) {
      throw_system_error("set SSC-32 baud rate");
    }
    options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    options.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    options.c_cflag &= static_cast<tcflag_t>(~PARENB);
    options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    options.c_cflag |= CS8;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (::tcsetattr(file_descriptor_, TCSANOW, &options) != 0) {
      throw_system_error("configure SSC-32 serial port");
    }
    if (::tcflush(file_descriptor_, TCIOFLUSH) != 0) {
      throw_system_error("flush SSC-32 serial port");
    }
  } catch (...) {
    close();
    throw;
  }
}

SerialTransport::~SerialTransport() { close(); }

SerialTransport::SerialTransport(SerialTransport&& other) noexcept
    : file_descriptor_(other.file_descriptor_) {
  other.file_descriptor_ = -1;
}

SerialTransport& SerialTransport::operator=(SerialTransport&& other) noexcept {
  if (this != &other) {
    close();
    file_descriptor_ = other.file_descriptor_;
    other.file_descriptor_ = -1;
  }
  return *this;
}

void SerialTransport::write_all(const std::string_view data) {
  if (!is_open()) {
    throw std::logic_error("SSC-32 serial port is not open");
  }

  std::size_t written = 0;
  while (written < data.size()) {
    const auto count =
        ::write(file_descriptor_, data.data() + written, data.size() - written);
    if (count > 0) {
      written += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    throw_system_error("write SSC-32 serial port");
  }
  if (::tcdrain(file_descriptor_) != 0) {
    throw_system_error("drain SSC-32 serial port");
  }
}

std::vector<std::uint8_t> SerialTransport::read_exact(
    const std::size_t size, const std::chrono::milliseconds timeout) {
  if (!is_open()) {
    throw std::logic_error("SSC-32 serial port is not open");
  }
  if (timeout.count() <= 0) {
    throw std::invalid_argument("SSC-32 read timeout must be positive");
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::vector<std::uint8_t> data;
  data.reserve(size);
  while (data.size() < size) {
    data.push_back(read_byte(file_descriptor_, deadline));
  }
  return data;
}

std::string SerialTransport::read_until(const char delimiter,
                                        const std::size_t max_size,
                                        const std::chrono::milliseconds timeout) {
  if (!is_open()) {
    throw std::logic_error("SSC-32 serial port is not open");
  }
  if (max_size == 0U) {
    throw std::invalid_argument("SSC-32 maximum response size must be positive");
  }
  if (timeout.count() <= 0) {
    throw std::invalid_argument("SSC-32 read timeout must be positive");
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::string data;
  data.reserve(max_size);
  while (data.size() < max_size) {
    const auto value = read_byte(file_descriptor_, deadline);
    if (value == static_cast<std::uint8_t>(delimiter)) {
      return data;
    }
    data.push_back(static_cast<char>(value));
  }
  throw std::runtime_error("SSC-32 response exceeded the configured size limit");
}

bool SerialTransport::is_open() const noexcept { return file_descriptor_ >= 0; }

void SerialTransport::close() noexcept {
  if (file_descriptor_ >= 0) {
    ::close(file_descriptor_);
    file_descriptor_ = -1;
  }
}

}  // namespace ssc32
