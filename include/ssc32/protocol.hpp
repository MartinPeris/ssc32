// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ssc32 {

inline constexpr std::uint8_t kMaxChannel = 31;
inline constexpr std::uint16_t kMinPulseWidthUs = 500;
inline constexpr std::uint16_t kCenterPulseWidthUs = 1500;
inline constexpr std::uint16_t kMaxPulseWidthUs = 2500;

class ProtocolError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

struct ServoCommand {
  std::uint8_t channel{};
  std::uint16_t pulse_width_us{kCenterPulseWidthUs};
  std::optional<std::uint16_t> speed_us_per_second;
};

struct PulseOffset {
  std::uint8_t channel{};
  std::int16_t offset_us{};
};

struct DiscreteOutput {
  std::uint8_t channel{};
  bool high{};
};

class Protocol final {
 public:
  [[nodiscard]] static std::string move(
      const std::vector<ServoCommand>& commands,
      std::optional<std::chrono::milliseconds> duration = std::nullopt);

  [[nodiscard]] static std::string cancel();
  [[nodiscard]] static std::string pulse_offsets(
      const std::vector<PulseOffset>& offsets);
  [[nodiscard]] static std::string discrete_outputs(
      const std::vector<DiscreteOutput>& outputs);
  [[nodiscard]] static std::string byte_output(std::uint8_t bank, std::uint8_t value);
  [[nodiscard]] static std::string query_movement_status();
  [[nodiscard]] static std::string query_pulse_widths(
      const std::vector<std::uint8_t>& channels);
  [[nodiscard]] static std::string version();
};

}  // namespace ssc32
