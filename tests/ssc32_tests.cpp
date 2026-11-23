// SPDX-License-Identifier: BSD-3-Clause
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ssc32/controller.hpp"
#include "ssc32/protocol.hpp"

namespace {

class FakeTransport final : public ssc32::Transport {
 public:
  void write_all(const std::string_view data) override { writes.emplace_back(data); }

  std::vector<std::uint8_t> read_exact(const std::size_t size,
                                       std::chrono::milliseconds) override {
    if (binary_response.size() != size) {
      throw std::runtime_error("unexpected fake read size");
    }
    return binary_response;
  }

  std::string read_until(char, std::size_t, std::chrono::milliseconds) override {
    return text_response;
  }

  std::vector<std::string> writes;
  std::vector<std::uint8_t> binary_response;
  std::string text_response;
};

int failures = 0;

void expect_equal(const std::string_view actual, const std::string_view expected,
                  const std::string_view test_name) {
  if (actual != expected) {
    std::cerr << "FAIL: " << test_name << "\n  expected: " << expected
              << "\n  actual:   " << actual << '\n';
    ++failures;
  }
}

template <typename Callable>
void expect_protocol_error(Callable&& callable, const std::string_view test_name) {
  try {
    callable();
    std::cerr << "FAIL: " << test_name << " did not throw\n";
    ++failures;
  } catch (const ssc32::ProtocolError&) {
  }
}

template <typename Callable>
void expect_runtime_error(Callable&& callable, const std::string_view test_name) {
  try {
    callable();
    std::cerr << "FAIL: " << test_name << " did not throw\n";
    ++failures;
  } catch (const std::runtime_error&) {
  }
}

void test_protocol_encoding() {
  using namespace std::chrono_literals;
  expect_equal(ssc32::Protocol::move({{0, 1500, 500}, {5, 2200, std::nullopt}}, 750ms),
               "#0 P1500 S500 #5 P2200 T750 \r", "group move encoding");
  expect_equal(ssc32::Protocol::pulse_offsets({{1, -20}, {2, 35}}),
               "#1 PO-20 #2 PO35 \r", "pulse offset encoding");
  expect_equal(ssc32::Protocol::discrete_outputs({{7, true}, {8, false}}),
               "#7 H #8 L \r", "discrete output encoding");
  expect_equal(ssc32::Protocol::byte_output(2, 173), "#2:173 \r",
               "byte output encoding");
  expect_equal(ssc32::Protocol::query_pulse_widths({0, 4, 31}), "QP0QP4QP31\r",
               "pulse query encoding");
  expect_equal(ssc32::Protocol::query_movement_status(), "Q\r",
               "movement query encoding");
  expect_equal(ssc32::Protocol::version(), "VER\r", "version encoding");
  expect_equal(ssc32::Protocol::cancel(), std::string{"\x1B\r", 2}, "cancel encoding");
}

void test_validation() {
  using namespace std::chrono_literals;
  expect_protocol_error([] { (void)ssc32::Protocol::move({}); }, "empty move group");
  expect_protocol_error([] { (void)ssc32::Protocol::move({{32, 1500, {}}}); },
                        "invalid channel");
  expect_protocol_error([] { (void)ssc32::Protocol::move({{0, 499, {}}}); },
                        "invalid pulse width");
  expect_protocol_error([] { (void)ssc32::Protocol::move({{0, 2501, {}}}); },
                        "pulse width above maximum");
  expect_protocol_error([] { (void)ssc32::Protocol::move({{0, 1500, 0}}); },
                        "zero servo speed");
  expect_protocol_error(
      [] { (void)ssc32::Protocol::move({{0, 1500, {}}, {0, 1600, {}}}); },
      "duplicate channel");
  expect_protocol_error([] { (void)ssc32::Protocol::move({{0, 1500, {}}}, 0ms); },
                        "invalid duration");
  expect_protocol_error([] { (void)ssc32::Protocol::pulse_offsets({{0, 101}}); },
                        "invalid pulse offset");
  expect_protocol_error([] { (void)ssc32::Protocol::pulse_offsets({{0, -101}}); },
                        "negative pulse offset below minimum");
  expect_protocol_error([] { (void)ssc32::Protocol::pulse_offsets({}); },
                        "empty pulse-offset group");
  expect_protocol_error([] { (void)ssc32::Protocol::discrete_outputs({}); },
                        "empty discrete-output group");
  expect_protocol_error([] { (void)ssc32::Protocol::byte_output(4, 0); },
                        "invalid byte-output bank");
  expect_protocol_error([] { (void)ssc32::Protocol::query_pulse_widths({}); },
                        "empty pulse query");
  expect_protocol_error(
      [] {
        std::vector<ssc32::ServoCommand> commands;
        for (std::uint8_t channel = 0; channel < 32; ++channel) {
          commands.push_back({channel, 1500, std::nullopt});
        }
        commands.push_back({0, 1500, std::nullopt});
        (void)ssc32::Protocol::move(commands);
      },
      "command group above maximum size");
}

void test_controller() {
  using namespace std::chrono_literals;
  FakeTransport transport;
  ssc32::Controller controller(transport);

  controller.move({{3, 1750, 250}}, 1000ms);
  expect_equal(transport.writes.back(), "#3 P1750 S250 T1000 \r", "controller move");

  controller.cancel();
  expect_equal(transport.writes.back(), std::string{"\x1B\r", 2}, "controller cancel");
  controller.set_pulse_offsets({{2, -10}});
  expect_equal(transport.writes.back(), "#2 PO-10 \r", "controller pulse offsets");
  controller.set_discrete_outputs({{7, true}});
  expect_equal(transport.writes.back(), "#7 H \r", "controller discrete output");
  controller.set_byte_output(1, 42);
  expect_equal(transport.writes.back(), "#1:42 \r", "controller byte output");

  transport.binary_response = {static_cast<std::uint8_t>('+')};
  if (!controller.is_moving(100ms)) {
    std::cerr << "FAIL: controller movement response\n";
    ++failures;
  }
  expect_equal(transport.writes.back(), "Q\r", "controller movement query");

  transport.binary_response = {static_cast<std::uint8_t>('.')};
  if (controller.is_moving(100ms)) {
    std::cerr << "FAIL: controller stopped response\n";
    ++failures;
  }
  transport.binary_response = {static_cast<std::uint8_t>('?')};
  expect_runtime_error([&controller] { (void)controller.is_moving(100ms); },
                       "invalid controller movement response");

  transport.binary_response = {150, 200};
  const auto widths = controller.query_pulse_widths({0, 1}, 100ms);
  if (widths != std::vector<std::uint16_t>{1500, 2000}) {
    std::cerr << "FAIL: controller pulse-width response\n";
    ++failures;
  }

  transport.text_response = "SSC32-V2.03XE";
  expect_equal(controller.version(100ms), "SSC32-V2.03XE",
               "controller version response");
}

}  // namespace

int main() {
  try {
    test_protocol_encoding();
    test_validation();
    test_controller();
  } catch (const std::exception& error) {
    std::cerr << "Unexpected exception: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All SSC-32 tests passed\n";
  return EXIT_SUCCESS;
}
