// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ssc32 {

class Transport {
 public:
  virtual ~Transport() = default;

  virtual void write_all(std::string_view data) = 0;
  [[nodiscard]] virtual std::vector<std::uint8_t> read_exact(
      std::size_t size, std::chrono::milliseconds timeout) = 0;
  [[nodiscard]] virtual std::string read_until(char delimiter, std::size_t max_size,
                                               std::chrono::milliseconds timeout) = 0;
};

}  // namespace ssc32
