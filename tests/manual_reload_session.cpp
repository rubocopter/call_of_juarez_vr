#include "games/call_of_juarez/manual_reload_session.hpp"
#include <cstdlib>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
using namespace cojvr::runtime;
void Check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
Pose At(Vec3 p){Pose pose{};pose.position=p;pose.position_valid=pose.orientation_valid=true;return pose;}
struct Fixture{
    CoJManualReloadSession session;
    CoJManualReloadInput s{}; GameplayInputState mapped{};
    Fixture(){s.enabled=s.allowed=true;s.player=7;s.context=5;
        s.raw.active=true;s.raw.input_context_generation=8;
        s.admission={0,42,true,true};s.recovery=s.admission;
        s.head=At({0,1.6F,0});s.left=At({-.2F,1.05F,-.1F});s.right=At({.2F,1.35F,-.35F});
        s.native.valid=s.native.probe_valid=true;s.native.weapon_id=42;s.native.armed_hand=0;s.native.states[0]={1,1,1,1};
    }
    CoJManualReloadOutput Poll(){++s.sequence;s.now+=50;mapped=s.raw;return session.Update(mapped,s);}
    void Open(){(void)Poll();s.raw.reload=true;auto out=Poll();Check(out.start,"Square must prepare");
        session.NoteStart(true);s.raw.reload=false;s.native.probe_status=1;s.native.states[0]={20,21,21,21};
        Check(Poll().tracked,"owned opening must keep VR presentation");
        s.admission={};s.native.probe_status=2;s.native.states[0]={21,21,21,21};
        Check(Poll().tracked,"owned waiting must keep VR presentation");
        Check(session.phase()==CoJManualReloadPhase::manual_load,"opening -> manual");}
    void Claim(){s.raw.fire_left=false;(void)Poll();s.raw.fire_left=true;Check(Poll().gesture.cartridge_held,"waist trigger pickup");}
    CoJManualReloadOutput Insert(){s.left=s.right;(void)Poll();s.raw.fire_left=false;return Poll();}
};
int main(){
    Fixture disabled;disabled.Open();disabled.s.raw.reload=true;
    Check(disabled.Poll().cancel,"Square closes the owned session before disable");
    disabled.s.enabled=false;disabled.s.native.probe_status=4;
    (void)disabled.Poll();disabled.s.raw.reload=false;(void)disabled.Poll();
    disabled.s.raw.reload=true;
    Check(!disabled.Poll().start&&disabled.mapped.reload,
        "disabling manual reload restores native Square after the physical release");
    // Square must not borrow a release from another input owner or from before
    // availability was lost. Otherwise a held button opens on menu/tracking return.
    for(int boundary=0;boundary<10;++boundary){
        Fixture x;(void)x.Poll();
        if(boundary==0)x.s.raw.input_context_generation++;
        if(boundary==1)x.s.player++;
        if(boundary==2)x.s.context+=2;
        if(boundary==8)x.s.native.weapon_id=x.s.admission.weapon_id=x.s.recovery.weapon_id=43;
        if(boundary==9){x.s.now+=300;}
        if(boundary>=3&&boundary<8){
            if(boundary==3)x.s.raw.digital_available&=~8U;
            if(boundary==4)x.s.allowed=false;
            if(boundary==5)x.s.left.position_valid=false;
            if(boundary==6)x.s.enabled=false;
            if(boundary==7)x.s.raw.weapon_radial=true;
            (void)x.Poll();
            x.s.raw.digital_available|=8U;x.s.allowed=true;x.s.left.position_valid=true;
            x.s.enabled=true;x.s.raw.weapon_radial=false;
        }
        x.s.raw.reload=true;
        Check(!x.Poll().start,"held Square cannot open across owner/availability boundary");
        Check(!x.Poll().start,"held Square stays disarmed on subsequent fresh samples");
        x.s.raw.reload=false;(void)x.Poll();x.s.raw.reload=true;
        Check(x.Poll().start,"a fresh available release rearms Square in the recovered owner");
    }
    Fixture f;f.Open();f.Claim();auto inserted=f.Insert();Check(inserted.insert,"valid release inserts");
    Check(!f.mapped.reload,"insertion must not dispatch native action31");f.session.NoteInsertion(true);
    Check(f.Poll().gesture.cartridge_held,"accepted insertion replenishes visual token");
    f.s.raw.fire_left=true;(void)f.Poll();f.s.raw.fire_left=false;Check(f.Poll().insert,"second fresh release inserts once");
    auto duplicate=f.session.Update(f.mapped,f.s);Check(!duplicate.insert,"duplicate cannot reinsert");
    f.session.NoteInsertion(true);(void)f.Poll();f.s.raw.reload=true;
    Check(f.Poll().cancel&&!f.mapped.reload,"second Square closes without conventional reload");
    f.s.native.probe_status=3;f.s.native.states[0]={22,1,1,1};(void)f.Poll();
    f.s.native.probe_status=4;f.s.native.states[0]={1,1,1,1};f.s.admission={0,42,true,true};
    Check(!f.Poll().start&&f.session.phase()==CoJManualReloadPhase::ready,"held Square cannot reopen after close");
    f.s.raw.reload=false;(void)f.Poll();f.s.raw.reload=true;Check(f.Poll().start,"fresh Square rearms");
    for(int reason=0;reason<13;++reason){Fixture x;x.Open();x.Claim();
        if(reason==0)x.s.allowed=false;
        if(reason==1)x.s.left.position_valid=false;
        if(reason==2)x.s.player++;
        if(reason==3)x.s.context++;
        if(reason==4)x.s.recentered=true;
        if(reason==5)x.s.raw.quick_load=true;
        if(reason==6)x.s.raw.fire_right=true;
        if(reason==7)x.s.enabled=false;
        if(reason==8)x.s.native.weapon_id++;
        if(reason==9)x.s.raw.input_context_generation++;
        if(reason==10)x.s.raw.quick_save=true;
        if(reason==11)x.s.raw.weapon_next=true;
        if(reason==12)x.s.raw.weapon_radial=true;
        const auto out=x.Poll();Check(out.cancel&&!out.insert&&!out.tracked,"interruption must cancel and yield");
        Check(!x.mapped.fire_left,"held support claim stays suppressed on cancellation");
        if(reason==6)Check(!x.mapped.fire_right,"close by fire cannot shoot until release");
    }
    Fixture mirrored;mirrored.s.right=At({.2F,1.05F,-.1F});mirrored.s.left=At({-.2F,1.35F,-.35F});
    mirrored.s.admission={1,42,true,true};mirrored.s.recovery=mirrored.s.admission;mirrored.s.native.armed_hand=1;
    mirrored.s.native.states[0]={};mirrored.s.native.states[1]={1,1,1,1};(void)mirrored.Poll();
    mirrored.s.raw.reload=true;Check(mirrored.Poll().start,"left armed Square opens");mirrored.session.NoteStart(true);
    mirrored.s.raw.reload=false;mirrored.s.admission={};mirrored.s.native.probe_status=2;
    mirrored.s.native.states[1]={21,21,21,21};(void)mirrored.Poll();
    mirrored.s.raw.fire_right=true;Check(mirrored.Poll().gesture.cartridge_held,"mirrored free trigger picks up");
    mirrored.s.right=mirrored.s.left;(void)mirrored.Poll();mirrored.s.raw.fire_right=false;
    Check(mirrored.Poll().insert&&!mirrored.mapped.reload,"mirrored release inserts without action31");
    Fixture miss;miss.Open();miss.Claim();miss.s.left=At({-1,1.2F,0});(void)miss.Poll();miss.s.raw.fire_left=false;
    Check(!miss.Poll().insert,"dropped cartridge is not inserted");
    Fixture full;full.Open();full.s.native.probe_status=3;full.s.native.states[0]={22,1,1,1};
    Check(full.Poll().tracked&&full.session.phase()==CoJManualReloadPhase::closing,"coherent native close keeps VR presentation");
    Fixture failed;failed.Open();failed.Claim();Check(failed.Insert().insert,"request before reject");
    failed.session.NoteInsertion(false);Check(failed.Poll().cancel,"native insertion failure closes, never retries");
    Check(!failed.Poll().supervise,"failed closing must not renew a still-waiting native watchdog");
    Fixture retry;retry.Open();retry.session.NoteInsertion(false);retry.session.Cancel();
    retry.s.native.probe_status=3;(void)retry.Poll();retry.s.native.probe_status=4;(void)retry.Poll();
    retry.s.admission={0,42,true,true};retry.s.raw.reload=false;(void)retry.Poll();retry.s.raw.reload=true;
    Check(retry.Poll().start,"failed insertion permits fresh reopening");retry.session.NoteStart(true);
    retry.s.raw.reload=false;retry.s.native.probe_status=1;
    Check(!retry.Poll().cancel,"old insertion rejection must not cancel a fresh session");
    Fixture unavailable;unavailable.Open();unavailable.Claim();unavailable.s.raw.digital_available&=~1U;
    Check(unavailable.Poll().cancel,"unavailable support trigger must cancel supervision");
    unavailable.s.native.probe_status=4;(void)unavailable.Poll();
    Check(!unavailable.mapped.fire_left,"unavailable trigger cannot satisfy release barrier");
    Fixture unsupported;(void)unsupported.Poll();unsupported.s.admission.single_round_supported=false;unsupported.s.native_fallback_allowed=true;unsupported.s.raw.reload=true;
    Check(!unsupported.Poll().start&&unsupported.mapped.reload,"unsupported keeps native Square");
    Fixture stale;stale.Open();stale.s.now+=300;Check(stale.Poll().cancel,"tracking publication gap closes");
    for(int fault=0;fault<7;++fault){Fixture x;(void)x.Poll();x.s.raw.reload=true;
        if(fault==0)x.s.native.valid=false;
        if(fault==1)x.s.native.probe_valid=false;
        if(fault==2)x.s.left.orientation_valid=false;
        if(fault==3)x.s.now+=300;
        if(fault==4)x.s.sequence=0;
        if(fault==5)x.s.admission={};
        if(fault==6)x.s.allowed=false;
        Check(!x.Poll().start&&!x.mapped.reload,"failed manual admission must never fall through to native Square");
        x.s.native.valid=x.s.native.probe_valid=true;x.s.left.orientation_valid=true;x.s.allowed=true;
        x.s.admission={0,42,true,true};
        Check(!x.Poll().start&&!x.mapped.reload,"held rejected Square cannot open or fall through after recovery");
        x.s.raw.reload=false;(void)x.Poll();x.s.raw.reload=true;
        Check(x.Poll().start,"fresh Square recovers after admission failure");
    }
    std::cout<<"manual reload session passed\n";
}
