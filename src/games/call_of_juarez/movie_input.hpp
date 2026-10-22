#pragma once
#include <cstdint>
namespace cojvr::games::call_of_juarez {
// Equality token for native presentation shape plus JNI-owned movie identity.
class CoJPresentationInputEpoch final {
public:
    std::uint64_t Observe(std::uint64_t shape,std::uint64_t movie) noexcept {
        if(!epoch_||shape!=shape_||movie!=movie_){
            if(++epoch_==0)++epoch_;
            shape_=shape;movie_=movie;
        }
        return epoch_;
    }
private:
    std::uint64_t shape_=0,movie_=0,epoch_=0;
};
// Separate one-sample UI intent from the cinematic's empty gameplay snapshot.
// Inputs are fresh presenter edges, after its focus/owner/release barrier.
inline bool CoJMovieSkipRequested(bool immersive,bool input_allowed,bool captured,
    std::uint64_t expected_context,std::uint64_t polled_context,
    bool options,bool back,bool accept,bool left_select,bool right_select) noexcept {
    return immersive&&input_allowed&&!captured&&expected_context&&expected_context==polled_context&&
        (options||back||accept||left_select||right_select);
}
// Consume before dispatch, including native rejection/failure. Repeated views
// of one tracking sample never retry its request into a changing native movie.
class CoJMovieSkipDispatchGate final {
public:
    bool Consume(bool requested,std::uint64_t sequence,std::uint64_t context) noexcept {
        if(!requested||!sequence||!context||sequence<=sequence_)return false;
        sequence_=sequence;return true;
    }
private:
    std::uint64_t sequence_=0;
};
}
