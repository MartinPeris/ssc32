// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ssc32/protocol.hpp"
#include "ssc32/transport.hpp"

namespace ssc32 {

class Controller final {
 public:
  explicit Controller(Transport& transport) noexcept;

  void move(const std::vector<ServoCommand>& commands,
            std::optional<std::chrono::milliseconds> duration = std::nullopt);
  void cancel();
  void set_pulse_offsets(const std::vector<PulseOffset>& offsets);
  void set_discrete_outputs(const std::vector<DiscreteOutput>& outputs);
  void set_byte_output(std::uint8_t bank, std::uint8_t value);

  [[nodiscard]] bool is_moving(std::chrono::milliseconds timeout);
  [[nodiscard]] std::vector<std::uint16_t> query_pulse_widths(
      const std::vector<std::uint8_t>& channels,
      std::chrono::milliseconds timeout);
  [[nodiscard]] std::string version(std::chrono::milliseconds timeout,
                                    std::size_t max_size = 128);

 private:
  Transport& transport_;
};

}  // namespace ssc32
