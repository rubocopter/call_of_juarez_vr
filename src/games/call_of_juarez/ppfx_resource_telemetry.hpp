#pragma once
#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include <cstdint>
#include <string>

namespace cojvr::games::call_of_juarez {
struct CoJPostprocessObservationContext {
    std::uint64_t scope=0, frame=0, owner=0, device=0, device_generation=0, probe_epoch=0;
    std::uint32_t thread=0, eye=0, route=0;
    bool begin=false, scope_complete=false;
};
// Optional diagnostic only. Copied identities and structural reads never admit
// resource mutation, reference retention, atomic snapshots or lifetime claims.
[[nodiscard]] std::string FormatCoJPostprocessObservation(
    const CoJPostprocessResourceSnapshot&, const CoJPostprocessObservationContext&);
}
