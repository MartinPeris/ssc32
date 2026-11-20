// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <string>

#include "ssc32/transport.hpp"

namespace ssc32 {

class SerialTransport final : public Transport {
 public:
  SerialTransport(const std::string& device, unsigned int baud_rate);
  ~SerialTransport() override;

  SerialTransport(const SerialTransport&) = delete;
  SerialTransport& operator=(const SerialTransport&) = delete;
  SerialTransport(SerialTransport&& other) noexcept;
  SerialTransport& operator=(SerialTransport&& other) noexcept;

  void write_all(std::string_view data) override;
  [[nodiscard]] std::vector<std::uint8_t> read_exact(
      std::size_t size, std::chrono::milliseconds timeout) override;
  [[nodiscard]] std::string read_until(
      char delimiter, std::size_t max_size,
      std::chrono::milliseconds timeout) override;

  [[nodiscard]] bool is_open() const noexcept;
  void close() noexcept;

 private:
  int file_descriptor_{-1};
};

}  // namespace ssc32
