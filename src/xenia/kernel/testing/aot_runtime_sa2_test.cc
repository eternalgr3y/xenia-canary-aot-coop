#include "third_party/catch/include/catch.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "xenia/cpu/aot_runtime_core.h"
#include "xenia/kernel/aot_runtime_sa2.h"

namespace xe::kernel::aot_runtime {
namespace {

using namespace std::chrono_literals;

constexpr uint32_t kOwn = 0x0200007Fu;
constexpr uint32_t kPeer = 0x0300007Fu;

struct FakeState {
  std::atomic<uint32_t> factory_calls{0};
  std::atomic<uint32_t> open_calls{0};
  std::atomic<uint32_t> close_calls{0};
  std::atomic<uint32_t> send_calls{0};
  std::atomic<uint32_t> receive_calls{0};
  bool open_result = true;
  bool immediate_invalid = false;
  std::mutex mutex;
  std::condition_variable cv;
  bool closed = false;
  std::deque<Sa2Packet> packets;
};

class FakeTransport final : public Sa2Transport {
 public:
  explicit FakeTransport(std::shared_ptr<FakeState> state)
      : state_(std::move(state)) {}

  bool Open(uint32_t) override {
    ++state_->open_calls;
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->closed = false;
    return state_->open_result;
  }

  bool Send(uint32_t, uint16_t, const uint8_t*, size_t) override {
    ++state_->send_calls;
    return true;
  }

  bool Receive(Sa2Packet* packet, std::chrono::milliseconds timeout) override {
    ++state_->receive_calls;
    std::unique_lock<std::mutex> lock(state_->mutex);
    if (!state_->packets.empty()) {
      *packet = state_->packets.front();
      state_->packets.pop_front();
      return true;
    }
    if (state_->immediate_invalid) {
      *packet = {};
      return true;
    }
    state_->cv.wait_for(lock, timeout, [&] {
      return state_->closed || !state_->packets.empty();
    });
    if (state_->packets.empty()) {
      return false;
    }
    *packet = state_->packets.front();
    state_->packets.pop_front();
    return true;
  }

  void Close() override {
    ++state_->close_calls;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->closed = true;
    }
    state_->cv.notify_all();
  }

 private:
  std::shared_ptr<FakeState> state_;
};

Sa2Manager::TransportFactory Factory(const std::shared_ptr<FakeState>& state) {
  return [state]() {
    ++state->factory_calls;
    return std::make_unique<FakeTransport>(state);
  };
}

Sa2WorkerOptions LongOptions() {
  Sa2WorkerOptions options;
  options.maximum_attempts = 20;
  options.maximum_receive_calls = 400;
  options.retry_interval = 50ms;
  options.receive_slice = 2ms;
  return options;
}

template <typename Predicate>
bool WaitUntil(Predicate predicate, std::chrono::milliseconds timeout = 1s) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

Sa2Packet Packet(const std::array<uint8_t, kSa2FrameSize>& frame,
                 uint32_t source = kPeer, uint16_t source_port = kSa2GamePort) {
  Sa2Packet packet;
  std::copy(frame.begin(), frame.end(), packet.bytes.begin());
  packet.size = frame.size();
  packet.source_ipv4_network = source;
  packet.source_port = source_port;
  return packet;
}

TEST_CASE("AoT runtime peer parser rejects overlong octets",
          "[aot][sa2][aot-runtime-core]") {
  uint32_t parsed = 0xA5A5A5A5u;
  CHECK_FALSE(xe::cpu::aot_runtime::ParseSyntheticPeerIpv4(
      "127.222222222222222222222222222222222222222222222222.2.3",
      &parsed));
  CHECK(parsed == 0xA5A5A5A5u);
  CHECK(xe::cpu::aot_runtime::ParseSyntheticPeerIpv4("127.2.3.4", &parsed));
  CHECK(parsed == 0x7F020304u);
}

TEST_CASE("SA2 request requires a prior local connect",
          "[aot][sa2][aot-runtime-core]") {
  Sa2Manager manager;
  const auto request = Sa2Manager::BuildFrame(0, kPeer, kOwn);
  std::atomic<uint32_t> ack_calls{0};
  const auto sender = [&](const auto&) {
    ++ack_calls;
    return true;
  };

  CHECK_FALSE(
      manager.HandleRequest(request.data(), request.size(), kPeer, sender));
  CHECK(manager.state() == Sa2State::kIdle);
  CHECK(ack_calls == 0);

  auto state = std::make_shared<FakeState>();
  REQUIRE(manager.Start(kOwn, kPeer, Factory(state), LongOptions()));
  REQUIRE(manager.Matches(kOwn, kPeer));
  REQUIRE(manager.HandleRequest(request.data(), request.size(), kPeer, sender));
  CHECK(manager.state() == Sa2State::kEstablished);
  CHECK(ack_calls == 1);

  manager.Stop();
  CHECK(manager.state() == Sa2State::kIdle);
  CHECK_FALSE(
      manager.HandleRequest(request.data(), request.size(), kPeer, sender));
  CHECK(ack_calls == 1);
}

TEST_CASE("SA2 rejects malformed and spoofed requests",
          "[aot][sa2][aot-runtime-core]") {
  Sa2Manager manager;
  auto state = std::make_shared<FakeState>();
  REQUIRE(manager.Start(kOwn, kPeer, Factory(state), LongOptions()));

  const auto valid = Sa2Manager::BuildFrame(0, kPeer, kOwn);
  std::vector<std::array<uint8_t, kSa2FrameSize>> frames;
  auto bad_magic = valid;
  bad_magic[0] ^= 0xFF;
  frames.push_back(bad_magic);
  frames.push_back(Sa2Manager::BuildFrame(1, kPeer, kOwn));
  frames.push_back(Sa2Manager::BuildFrame(0, kOwn, kOwn));
  frames.push_back(Sa2Manager::BuildFrame(0, kPeer, kPeer));

  uint32_t ack_calls = 0;
  const auto sender = [&](const auto&) {
    ++ack_calls;
    return true;
  };
  CHECK_FALSE(manager.HandleRequest(nullptr, valid.size(), kPeer, sender));
  CHECK_FALSE(
      manager.HandleRequest(valid.data(), valid.size() - 1, kPeer, sender));
  CHECK_FALSE(
      manager.HandleRequest(valid.data(), valid.size() + 1, kPeer, sender));
  CHECK_FALSE(manager.HandleRequest(valid.data(), valid.size(), kOwn, sender));
  for (const auto& frame : frames) {
    const auto original = frame;
    CHECK_FALSE(
        manager.HandleRequest(frame.data(), frame.size(), kPeer, sender));
    CHECK(frame == original);
  }
  CHECK(ack_calls == 0);
  CHECK(manager.state() == Sa2State::kPending);

  CHECK_FALSE(manager.HandleRequest(valid.data(), valid.size(), kPeer,
                                    [](const auto&) { return false; }));
  CHECK(manager.state() == Sa2State::kPending);
  REQUIRE(manager.HandleRequest(valid.data(), valid.size(), kPeer, sender));
  CHECK(ack_calls == 1);
  CHECK(manager.state() == Sa2State::kEstablished);
}

TEST_CASE("SA2 probe failure and malformed flood are bounded",
          "[aot][sa2][aot-runtime-core]") {
  SECTION("timed failure sends exactly the configured attempts") {
    Sa2Manager manager;
    auto state = std::make_shared<FakeState>();
    Sa2WorkerOptions options;
    options.maximum_attempts = 3;
    options.maximum_receive_calls = 100;
    options.retry_interval = 3ms;
    options.receive_slice = 1ms;
    REQUIRE(manager.Start(kOwn, kPeer, Factory(state), options));
    REQUIRE(WaitUntil([&] { return !manager.running(); }));
    CHECK(state->send_calls == 3);
    CHECK(state->close_calls == 1);
    CHECK(manager.state() == Sa2State::kPending);
    manager.Stop();
    const auto sends_after_stop = state->send_calls.load();
    std::this_thread::sleep_for(5ms);
    CHECK(state->send_calls == sends_after_stop);
    CHECK(manager.state() == Sa2State::kIdle);
  }

  SECTION("failed open closes without sending") {
    Sa2Manager manager;
    auto state = std::make_shared<FakeState>();
    state->open_result = false;
    REQUIRE(manager.Start(kOwn, kPeer, Factory(state), LongOptions()));
    REQUIRE(WaitUntil([&] { return !manager.running(); }));
    CHECK(state->open_calls == 1);
    CHECK(state->close_calls == 1);
    CHECK(state->send_calls == 0);
  }

  SECTION("immediate malformed packets hit the receive cap") {
    Sa2Manager manager;
    auto state = std::make_shared<FakeState>();
    state->immediate_invalid = true;
    Sa2WorkerOptions options;
    options.maximum_attempts = 3;
    options.maximum_receive_calls = 7;
    options.retry_interval = 50ms;
    options.receive_slice = 1ms;
    REQUIRE(manager.Start(kOwn, kPeer, Factory(state), options));
    REQUIRE(WaitUntil([&] { return !manager.running(); }));
    CHECK(state->receive_calls == 7);
    CHECK(state->send_calls <= 3);
    CHECK(manager.state() == Sa2State::kPending);
  }
}

TEST_CASE("SA2 concurrent same-peer starts own one worker",
          "[aot][sa2][aot-runtime-core]") {
  Sa2Manager manager;
  auto state = std::make_shared<FakeState>();
  std::atomic<uint32_t> ready{0};
  std::atomic<bool> go{false};
  bool results[2] = {};
  std::thread callers[2];
  for (uint32_t i = 0; i < 2; ++i) {
    callers[i] = std::thread([&, i] {
      ++ready;
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      results[i] = manager.Start(kOwn, kPeer, Factory(state), LongOptions());
    });
  }
  REQUIRE(WaitUntil([&] { return ready.load() == 2; }));
  go.store(true, std::memory_order_release);
  callers[0].join();
  callers[1].join();

  CHECK(results[0]);
  CHECK(results[1]);
  REQUIRE(WaitUntil([&] { return state->open_calls.load() == 1; }));
  CHECK(state->factory_calls == 1);
  CHECK(state->open_calls == 1);
  manager.Stop();
  CHECK(state->close_calls == 1);
}

TEST_CASE("SA2 unregister during ACK cannot resurrect state",
          "[aot][sa2][aot-runtime-core]") {
  Sa2Manager manager;
  auto state = std::make_shared<FakeState>();
  REQUIRE(manager.Start(kOwn, kPeer, Factory(state), LongOptions()));
  const auto request = Sa2Manager::BuildFrame(0, kPeer, kOwn);

  std::mutex gate_mutex;
  std::condition_variable gate_cv;
  bool sender_entered = false;
  bool release_sender = false;
  bool handled = true;
  std::thread handler([&] {
    handled = manager.HandleRequest(
        request.data(), request.size(), kPeer, [&](const auto&) {
          std::unique_lock<std::mutex> lock(gate_mutex);
          sender_entered = true;
          gate_cv.notify_all();
          gate_cv.wait(lock, [&] { return release_sender; });
          return true;
        });
  });

  {
    std::unique_lock<std::mutex> lock(gate_mutex);
    REQUIRE(gate_cv.wait_for(lock, 1s, [&] { return sender_entered; }));
  }
  manager.Stop();
  {
    std::lock_guard<std::mutex> lock(gate_mutex);
    release_sender = true;
  }
  gate_cv.notify_all();
  handler.join();

  CHECK_FALSE(handled);
  CHECK(manager.state() == Sa2State::kIdle);
}

TEST_CASE("SA2 ACK requires exact peer source target and port",
          "[aot][sa2][aot-runtime-core]") {
  Sa2Manager manager;
  auto state = std::make_shared<FakeState>();
  const auto valid = Sa2Manager::BuildFrame(1, kPeer, kOwn);
  auto wrong_type = Sa2Manager::BuildFrame(0, kPeer, kOwn);
  auto wrong_sender = Sa2Manager::BuildFrame(1, kOwn, kOwn);
  auto wrong_target = Sa2Manager::BuildFrame(1, kPeer, kPeer);
  auto bad_magic = valid;
  bad_magic[0] ^= 0xFF;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->packets.push_back(Packet(bad_magic));
    state->packets.push_back(Packet(wrong_type));
    state->packets.push_back(Packet(wrong_sender));
    state->packets.push_back(Packet(wrong_target));
    state->packets.push_back(Packet(valid, kOwn));
    state->packets.push_back(Packet(valid, kPeer, 999));
    state->packets.push_back(Packet(valid));
  }
  REQUIRE(manager.Start(kOwn, kPeer, Factory(state), LongOptions()));
  REQUIRE(WaitUntil([&] { return manager.state() == Sa2State::kEstablished; }));
  CHECK(state->receive_calls >= 7);
  manager.Stop();
}

}  // namespace
}  // namespace xe::kernel::aot_runtime
