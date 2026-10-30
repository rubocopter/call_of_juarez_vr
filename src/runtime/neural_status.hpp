#pragma once
#include <atomic>
#include <cstdint>

namespace cojvr::runtime {
enum class NeuralState : std::uint8_t {
    off, unavailable, starting, warming, processing, active, fallback, failed
};
struct NeuralStatusSnapshot {
    NeuralState state = NeuralState::off;
    std::uint64_t applied_pairs = 0;
};
namespace detail {
// One coherent value even on x86. No settings, file I/O or renderer types.
inline std::atomic<NeuralStatusSnapshot> neural_status{NeuralStatusSnapshot{}};
}
// The consumer publishes actual state and cumulative successfully applied pairs.
// Saved intent alone must never publish active. The publisher owns count lifetime.
inline void PublishNeuralStatus(NeuralState state, std::uint64_t applied_pairs) noexcept {
    detail::neural_status.store({state, applied_pairs}, std::memory_order_release);
}
[[nodiscard]] inline NeuralStatusSnapshot CurrentNeuralStatus() noexcept {
    return detail::neural_status.load(std::memory_order_acquire);
}
}
