#include "games/call_of_juarez/body_adapter.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
static_assert(std::tuple_size_v<CoJHandFrames> == 20, "Native weapon socket must share the rigid hand map");
struct Fixture {
    CoJHandFrames frames{};
    int writes = 0, fail_at = -1;
    bool fail_restore = false;
    bool attached_child = false;
    ElementWorldBasisTarget child{};
    static bool Read(void* c, int id, ElementWorldBasisTarget& f) noexcept {
        f=static_cast<Fixture*>(c)->frames[static_cast<std::size_t>(id)].frame;return true;
    }
    static bool Write(void* c, int id, const ElementWorldBasisTarget& f) noexcept {
        auto& self=*static_cast<Fixture*>(c);
        if (++self.writes==self.fail_at || self.fail_restore) return false;
        if (self.attached_child && id==19) {
            self.child=TransformWeaponElementFrame(self.frames.back().frame,f,self.child);
        }
        self.frames[static_cast<std::size_t>(id)].frame=f;return true;
    }
};
bool Close(float a,float b) { return std::fabs(a-b)<0.0001F; }
int main() {
    const auto neutral_socket=BuildTrackedHandSocketFrame({0,0,0,1},{1,0,0},{0,1,0},{0,0,1},{1,2,3});
    const auto rolled_socket=BuildTrackedHandSocketFrame({0,0,0.70710678F,0.70710678F},{1,0,0},{0,1,0},{0,0,1},{1,2,3});
    if (!neutral_socket.valid || !Close(neutral_socket.forward.z,-1) ||
        !rolled_socket.valid || !Close(rolled_socket.up.x,-1) ||
        BuildTrackedHandSocketFrame({0,0,0,0},{1,0,0},{0,1,0},{0,0,1},{}).valid) {
        std::cerr<<"Hand socket did not use absolute grip orientation or rejected pose validity\n";return 1;
    }
    Fixture f;
    for (int i=0;i<20;++i) f.frames[static_cast<std::size_t>(i)]={i,{{100.0F+i,200,300},{0,1,0},{0,0,1},true}};
    const auto native=f.frames;
    const ElementWorldBasisTarget source{{100,200,300},{0,1,0},{0,0,1},true};
    // Full 180-degree rotation close to torso, with no reach fit or residual gate.
    const ElementWorldBasisTarget target{{1,2,3},{0,-1,0},{0,0,1},true};
    CoJHandOverlay overlay;
    if (!overlay.Apply(native,source,target,{10,0,0},&f,Fixture::Read,Fixture::Write) ||
        !overlay.active() || !Close(f.frames[18].frame.position.x,-17) ||
        !Close(f.frames[18].frame.up.y,-1)) {
        std::cerr<<"Independent hand/fingers failed rigid rotation or changed proportions\n";return 1;
    }
    if (!overlay.Verify(&f,Fixture::Read)) return 1;
    const auto tracked=f.frames[18].frame;
    f.frames[18].frame.position.x+=1;
    if (overlay.Verify(&f,Fixture::Read)) return 1;
    f.frames[18].frame=tracked;
    f.frames[18].frame.up.y=std::numeric_limits<float>::quiet_NaN();
    if (overlay.Verify(&f,Fixture::Read)) return 1;
    f.frames[18].frame=tracked;
    if (!overlay.Restore({15,0,0},&f,Fixture::Read,Fixture::Write) || overlay.active() ||
        !Close(f.frames[18].frame.position.x,123) || !Close(f.frames[18].frame.up.y,1)) {
        std::cerr<<"Hand did not restore native pose plus actor movement\n";return 1;
    }
    f.frames=native;f.writes=0;f.fail_at=7;
    if (overlay.Apply(native,source,target,{},&f,Fixture::Read,Fixture::Write) || overlay.active() ||
        !Close(f.frames[0].frame.position.x,100) || !Close(f.frames[18].frame.position.x,118)) {
        std::cerr<<"Partial hand write did not restore all native elements\n";return 1;
    }
    CoJHandOverlay failed;
    f.frames=native;f.writes=0;f.fail_at=-1;
    if (!failed.Apply(native,source,target,{},&f,Fixture::Read,Fixture::Write)) return 1;
    f.fail_restore=true;
    if (failed.Restore({},&f,Fixture::Read,Fixture::Write) || !failed.active() || !failed.faulted() ||
        failed.Apply(native,source,target,{},&f,Fixture::Read,Fixture::Write)) {
        std::cerr<<"Failed hand restoration discarded ownership or allowed new writes\n";return 1;
    }
    f.fail_restore=false;
    if (!failed.Restore({5,0,0},&f,Fixture::Read,Fixture::Write) ||
        !Close(f.frames[0].frame.position.x,105)) {
        std::cerr<<"Hand restore retry lost source or duplicated actor offset\n";return 1;
    }
    CoJHandOverlay nested;
    auto offset_native=native;
    for (auto& sample : offset_native) sample.frame.position.x+=20;
    auto offset_source=source;offset_source.position.x+=20;
    f.frames=offset_native;f.fail_restore=false;
    if (!nested.Apply(offset_native,offset_source,target,{},&f,Fixture::Read,Fixture::Write)) return 1;
    f.fail_restore=true;
    if (nested.Restore({},&f,Fixture::Read,Fixture::Write)) return 1;
    // Outer pelvis restored while this failed hand transaction remains captured.
    nested.RemoveCapturedBodyOffset({20,0,0});
    f.fail_restore=false;
    if (!nested.Restore({5,0,0},&f,Fixture::Read,Fixture::Write) ||
        !Close(f.frames[0].frame.position.x,105)) {
        std::cerr<<"Hand retry reapplied the already-restored outer pelvis offset\n";return 1;
    }
    // The authored holding socket differs from the wrist. Both socket and
    // wrist/fingers must receive the same map, with the socket at the grip.
    auto socket_native=native;
    const ElementWorldBasisTarget socket{{108,198,300},{0,1,0},{0,0,1},true};
    socket_native.back().frame=socket;
    f.frames=socket_native;
    const auto socket_hand=TransformWeaponElementFrame(socket,target,source);
    CoJHandOverlay socket_overlay;
    if (!socket_overlay.Apply(socket_native,source,socket_hand,{},&f,Fixture::Read,Fixture::Write) ||
        !Close(f.frames.back().frame.position.x,target.position.x) ||
        !Close(f.frames.back().frame.position.y,target.position.y) ||
        !Close(f.frames.back().frame.up.y,target.up.y)) {
        std::cerr<<"Authored socket was left behind or wrist substituted for grip\n";return 1;
    }
    // Native AttachChild changes the weapon when its holding socket moves.
    // A pre-parent world weapon write is invalidated; committing after the
    // parent preserves the original hand/weapon relationship through restore.
    f.frames=socket_native;
    f.attached_child=true;
    const ElementWorldBasisTarget native_child{{108,198,330},{0,1,0},{0,0,1},true};
    const auto tracked_child=TransformWeaponElementFrame(socket,target,native_child);
    f.child=tracked_child;
    CoJHandOverlay wrong_order;
    if (!wrong_order.Apply(socket_native,source,socket_hand,{},&f,Fixture::Read,Fixture::Write) ||
        (Close(f.child.position.x,tracked_child.position.x) && Close(f.child.position.y,tracked_child.position.y))) {
        std::cerr<<"Fixture failed to reproduce child movement after pre-parent weapon write\n";return 1;
    }
    f.child=tracked_child;
    if (!wrong_order.Restore({},&f,Fixture::Read,Fixture::Write) ||
        !Close(f.child.position.x,native_child.position.x) || !Close(f.child.position.y,native_child.position.y)) {
        std::cerr<<"Parent-before-child restoration did not recover authored attachment\n";return 1;
    }
    return 0;
}
