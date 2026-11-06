#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace cojvr::games::call_of_juarez {

enum class CoJRenderWorkRoute : unsigned { gameplay, settings, movie, invalid };
struct CoJRenderWorkStamp {
    std::uint64_t frame = 0;
    std::uintptr_t owner = 0;
    std::uint64_t generation = 0, epoch = 0;
    std::uint32_t thread = 0;
    unsigned eye = 0;
    CoJRenderWorkRoute route = CoJRenderWorkRoute::invalid;
    bool operator==(const CoJRenderWorkStamp&) const = default;
};
struct CoJRenderWorkPair {
    CoJRenderWorkStamp left{}, right{};
    std::uint64_t left_cpu_wall_ns = 0, right_cpu_wall_ns = 0;
};
struct CoJRenderWorkStatistics {
    std::size_t count = 0;
    double mean_ns = 0, min_ns = 0, max_ns = 0, p50_ns = 0, p95_ns = 0, p99_ns = 0;
};
struct CoJRenderWorkCounters {
    std::uint64_t begins = 0, completes = 0, rejected = 0, nested = 0, unbalanced = 0;
    std::uint64_t attempts = 0, discarded_left = 0, invalidations = 0;
};

class CoJRenderWorkObservation {
public:
    static constexpr std::size_t capacity = 64, attempt_limit = 128;
    static constexpr std::uint64_t frame_stride = 90;
    static constexpr std::size_t non_gameplay_capacity = 16, non_gameplay_attempt_limit = 32;

    // Check under the same caller lock as Observe: no clock reads for ordinary
    // unsampled frames, invalid scopes or exhausted routes.
    bool NeedsTimestamp(const CoJRenderWorkStamp& stamp, bool begin) const noexcept {
        if (finished()) return false;
        if (!begin) return depth_ == 1 && selected_ && !poisoned_ && stamp == active_;
        if (depth_ || !Valid(stamp)) return false;
        return stamp.eye == 0 ? CanSampleLeft(stamp) : have_left_ && SamePair(pending_.left, stamp);
    }

    // Caller serializes access. Timestamps are monotonic CPU-wall nanoseconds,
    // not CPU execution time, GPU timestamps, draw counts, or frame/FPS timing.
    // Observe only the existing full-wrapper scope, never capture or fallback.
    void Observe(const CoJRenderWorkStamp& stamp, bool begin, bool complete,
        std::uint64_t now_ns) noexcept {
        if (finished()) return;
        if (begin) {
            ++counters_.begins;
            if (depth_++) {
                ++counters_.nested;
                poisoned_ = true;
                DiscardLeft();
                return;
            }
            active_ = stamp;
            selected_ = false;
            poisoned_ = !Valid(stamp);
            if (poisoned_) { DiscardLeft(); return; }
            if (stamp.eye == 0) {
                DiscardLeft();
                // Process-lifetime budgets/high-water marks survive invalidation.
                if (!CanSampleLeft(stamp)) return;
                const auto route = static_cast<unsigned>(stamp.route);
                last_attempt_[route] = stamp.frame;
                ++counters_.attempts;
                ++route_attempts_[route];
                selected_ = true;
            } else {
                selected_ = have_left_ && SamePair(pending_.left, stamp);
                if (!selected_) DiscardLeft();
            }
            started_ns_ = now_ns;
            return;
        }
        if (!depth_) {
            ++counters_.unbalanced;
            DiscardLeft();
            return;
        }
        if (--depth_) return; // A nested scope cannot complete its outer scope.
        if (poisoned_ || stamp != active_ || !complete ||
            (selected_ && (!started_ns_ || now_ns < started_ns_))) {
            ++counters_.rejected;
            DiscardLeft();
            selected_ = false;
            return;
        }
        ++counters_.completes;
        if (!selected_) return;
        selected_ = false;
        if (stamp.eye == 0) {
            pending_ = {stamp, {}, now_ns - started_ns_, 0};
            have_left_ = true;
        } else {
            pending_.right = stamp;
            pending_.right_cpu_wall_ns = now_ns - started_ns_;
            samples_[size_++] = pending_;
            ++route_samples_[static_cast<unsigned>(stamp.route)];
            have_left_ = false;
        }
    }

    // Reset/retirement/observation gaps poison in-flight scopes. Keep their
    // depth until balanced ends arrive, so a late old end cannot close a new one.
    void Invalidate() noexcept {
        ++counters_.invalidations;
        DiscardLeft();
        if (depth_) poisoned_ = true;
    }
    std::span<const CoJRenderWorkPair> samples() const noexcept { return {samples_.data(), size_}; }
    CoJRenderWorkCounters counters() const noexcept { return counters_; }
    bool finished() const noexcept {
        return size_ == capacity || (counters_.attempts == attempt_limit && !depth_ && !have_left_);
    }

    // Shutdown-only calculation; nearest-rank percentiles over retained pairs.
    // eye=2 sums the two wrapper durations, excluding inter-eye work/capture.
    CoJRenderWorkStatistics Statistics(CoJRenderWorkRoute route, unsigned eye) const noexcept {
        CoJRenderWorkStatistics result;
        if (route >= CoJRenderWorkRoute::invalid || eye > 2) return result;
        std::array<double, capacity> values{};
        double total = 0;
        for (const auto& pair : samples()) {
            if (pair.left.route != route) continue;
            double value = eye == 1 ? static_cast<double>(pair.right_cpu_wall_ns) :
                static_cast<double>(pair.left_cpu_wall_ns);
            if (eye == 2) value += static_cast<double>(pair.right_cpu_wall_ns);
            values[result.count++] = value;
            total += value;
        }
        if (!result.count) return result;
        std::sort(values.begin(), values.begin() + result.count);
        result.mean_ns = total / result.count;
        result.min_ns = values[0];
        result.max_ns = values[result.count - 1];
        result.p50_ns = values[(result.count * 50 + 99) / 100 - 1];
        result.p95_ns = values[(result.count * 95 + 99) / 100 - 1];
        result.p99_ns = values[(result.count * 99 + 99) / 100 - 1];
        return result;
    }

private:
    bool CanSampleLeft(const CoJRenderWorkStamp& stamp) const noexcept {
        const auto route = static_cast<unsigned>(stamp.route);
        const auto last = last_attempt_[route];
        // Menus/movies cannot exhaust the gameplay sample or attempt budget.
        const bool route_full = stamp.route != CoJRenderWorkRoute::gameplay &&
            (route_samples_[route] >= non_gameplay_capacity ||
             route_attempts_[route] >= non_gameplay_attempt_limit);
        return !route_full && size_ < capacity && counters_.attempts < attempt_limit && stamp.frame > 3 &&
            (!last || (stamp.frame > last && stamp.frame - last >= frame_stride));
    }
    static bool Valid(const CoJRenderWorkStamp& s) noexcept {
        return s.frame && s.owner && s.owner != ~std::uintptr_t{0} && s.generation &&
            s.epoch && s.thread && s.eye < 2 && s.route < CoJRenderWorkRoute::invalid;
    }
    static bool SamePair(CoJRenderWorkStamp left, const CoJRenderWorkStamp& right) noexcept {
        left.eye = 1;
        return left == right;
    }
    void DiscardLeft() noexcept {
        if (have_left_) ++counters_.discarded_left;
        have_left_ = false;
    }
    std::array<CoJRenderWorkPair, capacity> samples_{};
    std::array<std::uint64_t, 3> last_attempt_{};
    std::array<std::size_t, 3> route_samples_{}, route_attempts_{};
    CoJRenderWorkCounters counters_{};
    CoJRenderWorkStamp active_{};
    CoJRenderWorkPair pending_{};
    std::size_t size_ = 0;
    std::uint64_t started_ns_ = 0, depth_ = 0;
    bool have_left_ = false, selected_ = false, poisoned_ = false;
};

} // namespace cojvr::games::call_of_juarez
