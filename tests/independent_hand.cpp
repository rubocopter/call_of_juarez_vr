#include "games/call_of_juarez/body_adapter.hpp"
#include "finger_fixture.hpp"
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
bool FrameClose(const ElementWorldBasisTarget& a,const ElementWorldBasisTarget& b){
    auto v=[](auto p,auto q){return std::fabs(p.x-q.x)<.001F&&std::fabs(p.y-q.y)<.001F&&std::fabs(p.z-q.z)<.001F;};
    return a.valid==b.valid&&v(a.position,b.position)&&v(a.up,b.up)&&v(a.forward,b.forward);
}
bool FingerChecks(){
    using namespace finger_fixture;
    cojvr::runtime::FingerTrackingState curls{};curls.available=true;curls.quality=cojvr::runtime::FingerTrackingQuality::partial;
    for(int hand=0;hand<2;++hand){
        const auto& rest=hand?L_rest:R_rest;const auto& fist=hand?L_fist:R_fist;
        CoJHandFrames out{};
        for(float curl:{0.F,.5F,1.F}){
            curls.curls.fill(curl);
            if(!BuildCoJFingerTargets(hand,rest,rest,curls,out)){std::cerr<<"authored finger plan unavailable\n";return false;}
            for(int i=0;i<20;++i){
                if(out[i].element!=rest[i].element)return false;
                if((curl==0||i<4||i==19)&&!FrameClose(out[i].frame,rest[i].frame))return false;
                if(curl==1&&i>=4&&i<19&&!FrameClose(out[i].frame,fist[i].frame)){
                    std::cerr<<"finger endpoint disagrees with isolated native world getter\n";return false;}
                if(i>=4&&i<19){
                    const int parent=(i-4)%3==0?3:i-1;
                    auto distance=[](auto a,auto b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));};
                    if(!Close(distance(out[i].frame.position,out[parent].frame.position),distance(rest[i].frame.position,rest[parent].frame.position)))return false;
                }
            }
        }
        for(int finger=0;finger<5;++finger){curls.curls.fill(0);curls.curls[finger]=1;
            if(!BuildCoJFingerTargets(hand,rest,rest,curls,out))return false;
            for(int i=4;i<19;++i)if(!FrameClose(out[i].frame,((i-4)/3==finger?fist:rest)[i].frame))return false;
        }
        const ElementWorldBasisTarget tracked{{3,5,7},{-1,0,0},{0,0,1},true};
        CoJHandFrames rigid{};for(int i=0;i<20;++i)rigid[i]={rest[i].element,TransformWeaponElementFrame(rest[3].frame,tracked,rest[i].frame)};
        curls.curls.fill(1);
        if(!BuildCoJFingerTargets(hand,rest,rigid,curls,out))return false;
        for(int i=4;i<19;++i)if(!FrameClose(out[i].frame,TransformWeaponElementFrame(rest[3].frame,tracked,fist[i].frame)))return false;
        auto invalid=[&](){if(BuildCoJFingerTargets(hand,rest,rigid,curls,out))return false;for(int i=0;i<20;++i)if(!FrameClose(out[i].frame,rigid[i].frame))return false;return true;};
        for(float bad:{-1.F,1.01F,std::numeric_limits<float>::quiet_NaN()}){curls.curls[4]=bad;if(!invalid())return false;}curls.curls.fill(1);
        curls.available=false;if(!invalid())return false;curls.available=true;
        curls.quality=cojvr::runtime::FingerTrackingQuality::unavailable;if(!invalid())return false;curls.quality=cojvr::runtime::FingerTrackingQuality::partial;
        auto bad=rest;bad[15].frame.forward.x=std::numeric_limits<float>::quiet_NaN();if(BuildCoJFingerTargets(hand,bad,rigid,curls,out))return false;
        // A carried .357 uses an offline-measured skin-pad contact reference,
        // not the midpoint of the distal joint origins. These two hand-local
        // measurements come from the exact native Ray mesh at the held pose.
        curls.curls={.7F,.7F,.88F,.88F,.82F};
        Fixture f;f.frames=rest;CoJHandOverlay overlay;
        if(!overlay.Apply(rest,rest[3].frame,tracked,{10,0,0},&f,Fixture::Read,Fixture::Write,&curls,hand)||
            !overlay.fingers_active()||!overlay.Verify(&f,Fixture::Read))return false;
        cojvr::runtime::Vec3 anchor{};
        if(!overlay.ReadPinchAnchor(anchor,&f,Fixture::Read))return false;
        const ElementWorldBasisTarget local_identity{{},{0,1,0},{0,0,1},true};
        const auto contact_local=TransformWeaponElementFrame(f.frames[3].frame,local_identity,
            {anchor,{0,1,0},{0,0,1},true});
        const cojvr::runtime::Vec3 measured=hand==1?
            cojvr::runtime::Vec3{8.9962F,-2.1477F,-4.6588F}:
            cojvr::runtime::Vec3{8.6320F,2.8312F,-3.6611F};
        const auto within=[](float a,float b){return std::fabs(a-b)<.08F;};
        if(!contact_local.valid || !within(contact_local.position.x,measured.x) ||
           !within(contact_local.position.y,measured.y) ||
           !within(contact_local.position.z,measured.z)){
            std::cerr<<"Held-round anchor differs from measured native finger skin\n";
            return false;
        }
        const int writes_before_observation=f.writes;
        CoJReloadFingerObservation contact{};
        if(!overlay.ReadReloadFingerObservation(contact,&f,Fixture::Read)||!contact.valid||
           f.writes!=writes_before_observation)return false;
        const ElementWorldBasisTarget identity{{},{0,1,0},{0,0,1},true};
        for(int digit=0;digit<2;++digit){
            const int distal=digit?9:6;
            const auto expected_native=TransformWeaponElementFrame(rest[3].frame,identity,rest[distal].frame);
            const auto expected_displayed=TransformWeaponElementFrame(f.frames[3].frame,identity,f.frames[distal].frame);
            if(!FrameClose(contact.native[static_cast<std::size_t>(digit)],expected_native)||
               !FrameClose(contact.displayed[static_cast<std::size_t>(digit)],expected_displayed)){
                std::cerr<<"Reload finger observation mixed native/displayed or world/hand-local frames\n";return false;}
        }
        const auto a=f.frames[6].frame.position,b=f.frames[9].frame.position;
        if(std::fabs(anchor.x-(a.x+b.x)*.5F)<.3F &&
           std::fabs(anchor.y-(a.y+b.y)*.5F)<.3F &&
           std::fabs(anchor.z-(a.z+b.z)*.5F)<.3F)return false;
        const auto prior=f.frames[6].frame;
        f.frames[6].frame.position.x+=1;
        if(overlay.ReadPinchAnchor(anchor,&f,Fixture::Read))return false;
        if(overlay.ReadReloadFingerObservation(contact,&f,Fixture::Read)||contact.valid||
           contact.native[0].valid||contact.displayed[0].valid)return false;
        f.frames[6].frame=prior;
        if(!overlay.Restore({15,0,0},&f,Fixture::Read,Fixture::Write)||overlay.fingers_active())return false;
        if(overlay.ReadPinchAnchor(anchor,&f,Fixture::Read))return false;
        if(overlay.ReadReloadFingerObservation(contact,&f,Fixture::Read)||contact.valid)return false;
        for(int i=0;i<20;++i){auto expected=rest[i].frame;expected.position.x+=5;if(!FrameClose(f.frames[i].frame,expected))return false;}
        for(int fail:{5,10,18,20}){f.frames=rest;f.writes=0;f.fail_at=fail;CoJHandOverlay partial;
            if(partial.Apply(rest,rest[3].frame,tracked,{},&f,Fixture::Read,Fixture::Write,&curls,hand)||partial.active())return false;
            for(int i=0;i<20;++i)if(!FrameClose(f.frames[i].frame,rest[i].frame))return false;
        }
        f.frames=rest;f.writes=0;f.fail_at=-1;CoJHandOverlay retry;
        if(!retry.Apply(rest,rest[3].frame,tracked,{10,0,0},&f,Fixture::Read,Fixture::Write,&curls,hand))return false;
        f.fail_restore=true;
        if(retry.Restore({15,0,0},&f,Fixture::Read,Fixture::Write)||!retry.active()||!retry.faulted()||
            retry.Apply(rest,rest[3].frame,tracked,{},&f,Fixture::Read,Fixture::Write,&curls,hand))return false;
        if(retry.ReadPinchAnchor(anchor,&f,Fixture::Read))return false;
        if(retry.ReadReloadFingerObservation(contact,&f,Fixture::Read)||contact.valid)return false;
        f.fail_restore=false;if(!retry.Restore({20,0,0},&f,Fixture::Read,Fixture::Write))return false;
        for(int i=0;i<20;++i){auto expected=rest[i].frame;expected.position.x+=10;if(!FrameClose(f.frames[i].frame,expected))return false;}
        curls.available=false;f.frames=rest;CoJHandOverlay unavailable;
        if(!unavailable.Apply(rest,rest[3].frame,tracked,{},&f,Fixture::Read,Fixture::Write,&curls,hand)||unavailable.fingers_active()||
            unavailable.ReadPinchAnchor(anchor,&f,Fixture::Read)||
            unavailable.ReadReloadFingerObservation(contact,&f,Fixture::Read)||contact.valid||
            !unavailable.Restore({},&f,Fixture::Read,Fixture::Write))return false;curls.available=true;
    }
    return true;
}
int main() {
    if(!FingerChecks()){std::cerr<<"finger endpoint, independence, length or restoration contract failed\n";return 1;}
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
