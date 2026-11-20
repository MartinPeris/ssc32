// SPDX-License-Identifier: BSD-3-Clause
#include "ssc32/protocol.hpp"

#include <array>
#include <limits>
#include <sstream>

namespace ssc32 {
namespace {

void validate_channel(const std::uint8_t channel) {
  if (channel > kMaxChannel) {
    throw ProtocolError("SSC-32 channel must be in the range 0..31");
  }
}

template <typename Item, typename ChannelAccessor>
void validate_group(const std::vector<Item>& items,
                    ChannelAccessor channel_accessor) {
  if (items.empty()) {
    throw ProtocolError("SSC-32 command group must not be empty");
  }
  if (items.size() > 32U) {
    throw ProtocolError("SSC-32 command group cannot exceed 32 channels");
  }

  std::array<bool, 32> used{};
  for (const auto& item : items) {
    const auto channel = channel_accessor(item);
    validate_channel(channel);
    if (used[channel]) {
      throw ProtocolError("SSC-32 command group contains a duplicate channel");
    }
    used[channel] = true;
  }
}

}  // namespace

std::string Protocol::move(
    const std::vector<ServoCommand>& commands,
    const std::optional<std::chrono::milliseconds> duration) {
  validate_group(commands,
                 [](const ServoCommand& command) { return command.channel; });

  if (duration.has_value() && duration->count() <= 0) {
    throw ProtocolError("SSC-32 move duration must be positive");
  }

  std::ostringstream message;
  for (const auto& command : commands) {
    if (command.pulse_width_us < kMinPulseWidthUs ||
        command.pulse_width_us > kMaxPulseWidthUs) {
      throw ProtocolError("SSC-32 pulse width must be in the range 500..2500 us");
    }
    if (command.speed_us_per_second.has_value() &&
        command.speed_us_per_second.value() == 0U) {
      throw ProtocolError("SSC-32 servo speed must be positive");
    }

    message << '#' << static_cast<unsigned int>(command.channel) << " P"
            << command.pulse_width_us;
    if (command.speed_us_per_second.has_value()) {
      message << " S" << command.speed_us_per_second.value();
    }
    message << ' ';
  }

  if (duration.has_value()) {
    message << 'T' << duration->count() << ' ';
  }
  message << '\r';
  return message.str();
}

std::string Protocol::cancel() { return std::string{"\x1B\r", 2}; }

std::string Protocol::pulse_offsets(const std::vector<PulseOffset>& offsets) {
  validate_group(offsets,
                 [](const PulseOffset& offset) { return offset.channel; });

  std::ostringstream message;
  for (const auto& offset : offsets) {
    if (offset.offset_us < -100 || offset.offset_us > 100) {
      throw ProtocolError("SSC-32 pulse offset must be in the range -100..100 us");
    }
    message << '#' << static_cast<unsigned int>(offset.channel) << " PO"
            << offset.offset_us << ' ';
  }
  message << '\r';
  return message.str();
}

std::string Protocol::discrete_outputs(
    const std::vector<DiscreteOutput>& outputs) {
  validate_group(outputs,
                 [](const DiscreteOutput& output) { return output.channel; });

  std::ostringstream message;
  for (const auto& output : outputs) {
    message << '#' << static_cast<unsigned int>(output.channel) << ' '
            << (output.high ? 'H' : 'L') << ' ';
  }
  message << '\r';
  return message.str();
}

std::string Protocol::byte_output(const std::uint8_t bank,
                                  const std::uint8_t value) {
  if (bank > 3U) {
    throw ProtocolError("SSC-32 output bank must be in the range 0..3");
  }
  std::ostringstream message;
  message << '#' << static_cast<unsigned int>(bank) << ':'
          << static_cast<unsigned int>(value) << " \r";
  return message.str();
}

std::string Protocol::query_movement_status() { return "Q\r"; }

std::string Protocol::query_pulse_widths(
    const std::vector<std::uint8_t>& channels) {
  validate_group(channels,
                 [](const std::uint8_t channel) { return channel; });

  std::ostringstream message;
  for (const auto channel : channels) {
    message << "QP" << static_cast<unsigned int>(channel);
  }
  message << '\r';
  return message.str();
}

std::string Protocol::version() { return "VER\r"; }

}  // namespace ssc32
