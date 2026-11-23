// SPDX-License-Identifier: BSD-3-Clause
#include "ssc32/controller.hpp"

#include <stdexcept>

namespace ssc32 {

Controller::Controller(Transport& transport) noexcept : transport_(transport) {}

void Controller::move(const std::vector<ServoCommand>& commands,
                      const std::optional<std::chrono::milliseconds> duration) {
  transport_.write_all(Protocol::move(commands, duration));
}

void Controller::cancel() { transport_.write_all(Protocol::cancel()); }

void Controller::set_pulse_offsets(const std::vector<PulseOffset>& offsets) {
  transport_.write_all(Protocol::pulse_offsets(offsets));
}

void Controller::set_discrete_outputs(const std::vector<DiscreteOutput>& outputs) {
  transport_.write_all(Protocol::discrete_outputs(outputs));
}

void Controller::set_byte_output(const std::uint8_t bank, const std::uint8_t value) {
  transport_.write_all(Protocol::byte_output(bank, value));
}

bool Controller::is_moving(const std::chrono::milliseconds timeout) {
  transport_.write_all(Protocol::query_movement_status());
  const auto response = transport_.read_exact(1, timeout);
  if (response.front() == static_cast<std::uint8_t>('+')) {
    return true;
  }
  if (response.front() == static_cast<std::uint8_t>('.')) {
    return false;
  }
  throw std::runtime_error("SSC-32 returned an invalid movement-status response");
}

std::vector<std::uint16_t> Controller::query_pulse_widths(
    const std::vector<std::uint8_t>& channels,
    const std::chrono::milliseconds timeout) {
  transport_.write_all(Protocol::query_pulse_widths(channels));
  const auto response = transport_.read_exact(channels.size(), timeout);

  std::vector<std::uint16_t> pulse_widths;
  pulse_widths.reserve(response.size());
  for (const auto value : response) {
    pulse_widths.push_back(static_cast<std::uint16_t>(value) * 10U);
  }
  return pulse_widths;
}

std::string Controller::version(const std::chrono::milliseconds timeout,
                                const std::size_t max_size) {
  transport_.write_all(Protocol::version());
  return transport_.read_until('\r', max_size, timeout);
}

}  // namespace ssc32
