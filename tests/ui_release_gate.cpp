#include "runtime/vr_settings.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
namespace {
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
UiReleaseInput Input(std::uint64_t time){
    UiReleaseInput in{};in.available=true;in.global_available=0xFF;
    in.context_token=1;in.monotonic_ms=time;in.gameplay.active=true;return in;
}
void Baseline(VrInputReleaseGate& gate,std::uint64_t time){
    (void)gate.Update(Input(time));Check(!gate.Update(Input(time+1)).input_captured,"neutral baseline locked input");
}
}
int main(){
    VrInputReleaseGate gate;auto in=Input(0);in.options=true;
    Check(!gate.Update(in).native_pause,"held startup Options paused");
    in.monotonic_ms=2000;Check(!gate.Update(in).native_pause,"held Options became quick settings/pause");
    Baseline(gate,2010);in=Input(2020);in.options=true;
    auto out=gate.Update(in);Check(out.native_pause&&!out.input_captured,"fresh Options must pause on press");
    in.monotonic_ms=10000;out=gate.Update(in);
    Check(!out.native_pause&&!out.input_captured,"held Options captured controls or repeated");
    Check(!gate.Update(Input(10001)).native_pause,"Options release dispatched another action");
    in=Input(10002);in.options=true;Check(gate.Update(in).native_pause,"fresh second pause missing");
    in.available=false;in.monotonic_ms++;Check(gate.Update(in).input_captured,"focus loss did not capture");
    in.available=true;in.monotonic_ms++;Check(!gate.Update(in).native_pause,"held focus recovery repeated pause");
    Baseline(gate,10010);
    in=Input(10020);in.context_token=2;in.options=true;
    Check(gate.Update(in).input_captured&&!gate.Update(in).native_pause,"new context inherited Options");
    gate.Invalidate();in=Input(10030);in.gameplay.fire_right=true;(void)gate.Update(in);
    in=Input(10031);in.gameplay.digital_available&=~2U;
    Check(gate.Update(in).input_captured,"unavailable held trigger fabricated release");
    Baseline(gate,10040);
    gate.Invalidate();in=Input(10050);in.gameplay.weapon_radial=true;(void)gate.Update(in);
    in=Input(10051);in.gameplay.radial_available=false;
    Check(gate.Update(in).input_captured,"unavailable wheel fabricated release");
    Baseline(gate,10060);
    gate.Invalidate();in=Input(10070);in.gameplay.turn.x=std::numeric_limits<float>::quiet_NaN();
    Check(gate.Update(in).input_captured,"NaN axis accepted as released");
    Baseline(gate,10080);
    in=Input(10079);in.options=true;Check(!gate.Update(in).native_pause,"time regression replayed Options");
    Baseline(gate,10090);
    in=Input(10100);in.global_available&=~UiReleaseInput::options_bit;(void)gate.Update(in);
    in=Input(10101);in.options=true;Check(!gate.Update(in).native_pause,"new held binding paused");
    Baseline(gate,10110);
    VrInputReleaseGate one_hand;in=Input(10200);in.global_available=0x97;
    in.gameplay.digital_available=0;in.gameplay.move_available=in.gameplay.turn_available=in.gameplay.radial_available=false;
    (void)one_hand.Update(in);in.monotonic_ms++;
    Check(!one_hand.Update(in).input_captured,"unbound left/optional channels froze native UI");
    in.options=true;in.monotonic_ms++;Check(one_hand.Update(in).native_pause,"one-hand pause unavailable");
    VrInputReleaseGate movie_entry;Baseline(movie_entry,10300);
    in=Input(10310);in.context_token=2;in.accept=true;in.select_right=true;in.gameplay.fire_right=true;
    Check(movie_entry.Update(in).input_captured,"gameplay press crossed directly into a movie");
    in.monotonic_ms++;Check(movie_entry.Update(in).input_captured,"held movie-entry triggers fabricated release");
    in=Input(10320);in.context_token=2;(void)movie_entry.Update(in);in.monotonic_ms++;
    Check(!movie_entry.Update(in).input_captured,"new movie did not recover after physical release");
    std::cout<<"native input release gate and Options press checks passed\n";
}
