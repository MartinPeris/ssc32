// SPDX-License-Identifier: BSD-3-Clause
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "ssc32/serial_transport.hpp"

namespace {

using namespace std::chrono_literals;

int failures = 0;

class PtyPair final {
 public:
  PtyPair() {
    master_ = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_ < 0 || grantpt(master_) != 0 || unlockpt(master_) != 0) {
      throw std::runtime_error("failed to create pseudoterminal");
    }
    const char* path = ptsname(master_);
    if (path == nullptr) {
      throw std::runtime_error("failed to resolve pseudoterminal path");
    }
    device_ = path;
  }

  ~PtyPair() {
    if (master_ >= 0) {
      close(master_);
    }
  }

  PtyPair(const PtyPair&) = delete;
  PtyPair& operator=(const PtyPair&) = delete;

  [[nodiscard]] const std::string& device() const noexcept { return device_; }

  void write_master(const std::string_view value) const {
    std::size_t offset = 0;
    while (offset < value.size()) {
      const auto count = write(master_, value.data() + offset, value.size() - offset);
      if (count <= 0) {
        throw std::runtime_error("failed to write pseudoterminal master");
      }
      offset += static_cast<std::size_t>(count);
    }
  }

  [[nodiscard]] std::string read_master(const std::size_t size) const {
    std::string result(size, '\0');
    std::size_t offset = 0;
    while (offset < size) {
      const auto count = read(master_, result.data() + offset, size - offset);
      if (count <= 0) {
        throw std::runtime_error("failed to read pseudoterminal master");
      }
      offset += static_cast<std::size_t>(count);
    }
    return result;
  }

  void disconnect() noexcept {
    if (master_ >= 0) {
      close(master_);
      master_ = -1;
    }
  }

 private:
  int master_{-1};
  std::string device_;
};

void expect(const bool condition, const std::string_view name) {
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    ++failures;
  }
}

template <typename Exception, typename Callable>
void expect_throws(Callable&& callable, const std::string_view name) {
  try {
    callable();
    std::cerr << "FAIL: " << name << " did not throw\n";
    ++failures;
  } catch (const Exception&) {
  }
}

void test_open_read_write_and_close() {
  PtyPair pty;
  ssc32::SerialTransport transport(pty.device(), 115200U);
  expect(transport.is_open(), "transport opens pseudoterminal");

  transport.write_all("VER\r");
  expect(pty.read_master(4U) == "VER\r", "write_all reaches master");

  pty.write_master(std::string{"\x96\xC8", 2U});
  expect(transport.read_exact(2U, 100ms) == std::vector<std::uint8_t>({150U, 200U}),
         "read_exact returns binary response");
  expect(transport.read_exact(0U, 100ms).empty(), "zero-byte read is empty");

  pty.write_master("SSC32-V2.03XE\rignored");
  expect(transport.read_until('\r', 64U, 100ms) == "SSC32-V2.03XE",
         "read_until strips delimiter");

  transport.write_all("");
  transport.close();
  expect(!transport.is_open(), "close marks transport closed");
  transport.close();
  expect_throws<std::logic_error>([&transport] { transport.write_all("Q\r"); },
                                  "closed write");
  expect_throws<std::logic_error>(
      [&transport] { (void)transport.read_exact(1U, 10ms); }, "closed exact read");
  expect_throws<std::logic_error>(
      [&transport] { (void)transport.read_until('\r', 8U, 10ms); },
      "closed delimited read");
}

void test_validation_timeouts_and_limits() {
  PtyPair pty;
  ssc32::SerialTransport transport(pty.device(), 38400U);
  expect_throws<std::invalid_argument>(
      [&transport] { (void)transport.read_exact(1U, 0ms); },
      "exact read rejects zero timeout");
  expect_throws<std::invalid_argument>(
      [&transport] { (void)transport.read_until('\r', 0U, 10ms); },
      "delimited read rejects zero maximum");
  expect_throws<std::invalid_argument>(
      [&transport] { (void)transport.read_until('\r', 8U, 0ms); },
      "delimited read rejects zero timeout");
  expect_throws<std::runtime_error>(
      [&transport] { (void)transport.read_exact(1U, 10ms); }, "exact read times out");
  expect_throws<std::runtime_error>(
      [&transport] { (void)transport.read_until('\r', 8U, 10ms); },
      "delimited read times out");

  pty.write_master("abcd");
  expect_throws<std::runtime_error>(
      [&transport] { (void)transport.read_until('\r', 3U, 100ms); },
      "delimited read enforces size limit");
  pty.disconnect();
  expect_throws<std::runtime_error>(
      [&transport] { (void)transport.read_exact(1U, 100ms); },
      "disconnect is reported while reading");
}

void test_construction_and_move_semantics() {
  expect_throws<std::invalid_argument>(
      [] { ssc32::SerialTransport transport("/dev/null", 12345U); },
      "unsupported baud rate");
  expect_throws<std::system_error>(
      [] { ssc32::SerialTransport transport("/definitely/missing/ssc32", 9600U); },
      "missing serial device");
  expect_throws<std::system_error>(
      [] { ssc32::SerialTransport transport("/dev/null", 2400U); },
      "non-terminal serial device");

  PtyPair first_pty;
  PtyPair second_pty;
  ssc32::SerialTransport first(first_pty.device(), 9600U);
  ssc32::SerialTransport moved(std::move(first));
  expect(!first.is_open() && moved.is_open(), "move construction transfers ownership");
  moved.write_all("Q\r");
  expect(first_pty.read_master(2U) == "Q\r", "moved transport remains usable");

  ssc32::SerialTransport assigned(second_pty.device(), 115200U);
  assigned = std::move(moved);
  expect(!moved.is_open() && assigned.is_open(), "move assignment transfers ownership");
  auto* same_transport = &assigned;
  assigned = std::move(*same_transport);
  expect(assigned.is_open(), "self move assignment preserves transport");
}

}  // namespace

int main() {
  try {
    test_open_read_write_and_close();
    test_validation_timeouts_and_limits();
    test_construction_and_move_semantics();
  } catch (const std::exception& error) {
    std::cerr << "Unexpected exception: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All SSC-32 serial transport tests passed\n";
  return EXIT_SUCCESS;
}
