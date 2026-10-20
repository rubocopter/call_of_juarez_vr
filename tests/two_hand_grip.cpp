#include "runtime/two_hand_grip.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace cojvr::runtime;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.00002F; }
void Equal(Vec3 a, Vec3 b, const char* message) {
    Check(Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z), message);
}
Vec3 Point(const std::array<float,12>& m, Vec3 p) {
    return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],
            m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],
            m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};
}
Vec3 Direction(const std::array<float,12>& m, Vec3 p) {
    return {m[0]*p.x+m[1]*p.y+m[2]*p.z,
            m[4]*p.x+m[5]*p.y+m[6]*p.z,
            m[8]*p.x+m[9]*p.y+m[10]*p.z};
}
float Distance(Vec3 a, Vec3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
TwoHandGripGeometry Geometry() {
    return {{1,0,0,1, 0,1,0,2, 0,0,1,3},
            {0,-1,0,4, 1,0,0,5, 0,0,1,6},
            {1.3F,2,2.6F}, {4,5.3F,5.6F}};
}
struct PolicyFixture {
    TwoHandGripPolicy policy;
    TwoHandGripInput in{};
    PolicyFixture() {
        in.owner=0x80000001ULL; in.input_generation=7;
        in.enabled=in.eligible=in.tracking_valid=in.contact_qualified=true;
        in.grip_available=in.support_trigger_available=true;
        in.acquisition_radius_m=.05F; in.contact_distance_m=.01F;
        in.sequence=1; in.sample_time_ms=in.now_ms=1000;
    }
    TwoHandGripOutput Step() {
        ++in.sequence; in.sample_time_ms+=10; in.now_ms=in.sample_time_ms;
        return policy.Update(in);
    }
    TwoHandGripOutput Attach() {
        Check(!Step().acquired,"release baseline attached");
        in.grip_held=true;
        const auto out=Step();
        Check(out.acquired && out.stage==TwoHandGripStage::attached,"fresh near grip did not attach");
        Check(out.consume_grip && out.consume_support_trigger,"claim did not consume both channels");
        return out;
    }
};

void OffAxisRollAndFullMap() {
    const auto g=Geometry(); const auto s=SolveTwoHandGrip(g);
    Check(s.valid,"off-axis geometry rejected");
    Equal(Point(s.target_from_source,{1,2,3}),{4,5,6},"dominant anchor moved");
    Equal(Point(s.target_from_source,g.source_support_m),g.target_support_m,"equal-span support missed");
    Equal(Direction(s.target_from_source,{0,1,0}),{-1,0,0},"dominant roll lost");
    Equal(Point(s.target_from_source,{1,2,2}),{4,5,5},"full muzzle origin map wrong");
    Equal(Direction(s.target_from_source,{0,0,-1}),{0,0,-1},"off-axis support substituted for barrel");
    Check(Near(s.source_span_m,.5F)&&Near(s.target_span_m,.5F)&&Near(s.support_error_m,0),"span diagnostics wrong");
}
void UnequalSpanNeverScales() {
    auto g=Geometry(); g.target_support_m={4,5.6F,5.2F};
    const auto s=SolveTwoHandGrip(g); Check(s.valid,"unequal spacing rejected");
    Equal(Point(s.target_from_source,{1,2,3}),{4,5,6},"spacing moved dominant anchor");
    Equal(Point(s.target_from_source,g.source_support_m),{4,5.3F,5.6F},"support stretched toward controller");
    Check(Near(s.support_error_m,.5F),"spacing mismatch not exposed");
    const auto a=Point(s.target_from_source,{2,-1,4});
    const auto b=Point(s.target_from_source,{-3,1,2});
    Check(Near(Distance(a,b),std::sqrt(33.F)),"rigid map changed arbitrary distance");
    const auto& m=s.target_from_source;
    Check(Near(m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+
               m[2]*(m[4]*m[9]-m[5]*m[8]),1),"map reflected or scaled");
}
void SourceFrameCovariance() {
    auto g=Geometry();
    g.source_from_dominant={0,0,1,3, 0,1,0,2, -1,0,0,-1};
    g.source_support_m={2.6F,2,-1.3F};
    const auto s=SolveTwoHandGrip(g); Check(s.valid,"rotated source rejected");
    Equal(Point(s.target_from_source,{3,2,-1}),{4,5,6},"rotated source anchor wrong");
    Equal(Point(s.target_from_source,g.source_support_m),{4,5.3F,5.6F},"rotated source support wrong");
    Equal(Direction(s.target_from_source,{-1,0,0}),{0,0,-1},"source rotation applied twice");
}
void TargetFrameCovariance() {
    auto g=Geometry();
    g.target_from_dominant={0,-1,0,4, 0,0,-1,-6, 1,0,0,5};
    g.target_support_m={4,-5.6F,5.3F};
    const auto s=SolveTwoHandGrip(g); Check(s.valid,"rotated target rejected");
    Equal(Point(s.target_from_source,{1,2,2}),{4,-5,5},"target rotation origin wrong");
    Equal(Direction(s.target_from_source,{0,0,-1}),{0,1,0},"target rotation direction wrong");
}
void AuthoredDominantRollIsNotAssumedIdentity() {
    TwoHandGripGeometry g{{0,-1,0,1, 1,0,0,2, 0,0,1,3},
                          {1,0,0,4, 0,1,0,5, 0,0,1,6},
                          {1,2,2.5F}, {4.5F,5,6}};
    const auto s=SolveTwoHandGrip(g); Check(s.valid,"authored rolled socket rejected");
    // Hand-derived map: -Z span -> +X, -X authored up -> +Y, +Y -> +Z.
    Equal(Direction(s.target_from_source,{0,0,-1}),{1,0,0},"span direction wrong");
    Equal(Direction(s.target_from_source,{-1,0,0}),{0,1,0},"authored socket roll ignored");
    Equal(Direction(s.target_from_source,{0,1,0}),{0,0,1},"third map axis wrong");
    Equal(Point(s.target_from_source,{1,2,3}),{4,5,6},"rolled socket anchor moved");
}
void SupportAimChangesWithoutDominantRotation() {
    TwoHandGripGeometry g{{1,0,0,1, 0,1,0,2, 0,0,1,3},
                          {1,0,0,1, 0,1,0,2, 0,0,1,3},
                          {1.3F,2.4F,3}, {1,2.3F,3.4F}};
    const auto s=SolveTwoHandGrip(g); Check(s.valid,"independent support aim rejected");
    Equal(Point(s.target_from_source,{1,2,3}),{1,2,3},"support aiming moved dominant anchor");
    Equal(Point(s.target_from_source,g.source_support_m),g.target_support_m,"dominant rotation substituted for support aim");
    // Source span (.6,.8,0), projected up (-.8,.6,0); target span
    // (0,.6,.8), projected up (0,.8,-.6). These define R independently.
    Equal(Direction(s.target_from_source,{-.8F,.6F,0}),{0,.8F,-.6F},"projected roll not preserved");
    Equal(Direction(s.target_from_source,{1,0,0}),{0,-.28F,.96F},"independent aim first axis wrong");
    Equal(Direction(s.target_from_source,{0,1,0}),{0,.96F,.28F},"independent aim second axis wrong");
    Equal(Direction(s.target_from_source,{0,0,-1}),{1,0,0},"independent aim barrel wrong");
    Check(Near(Distance(Point(s.target_from_source,{2,0,1}),
                        Point(s.target_from_source,{-1,2,0})),std::sqrt(14.F)),"independent aim scaled geometry");
}
void InvalidGeometryRejects() {
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    for (int i=0;i<12;++i) {
        auto g=Geometry(); g.source_from_dominant[i]=nan;
        Check(!SolveTwoHandGrip(g).valid,"nonfinite source frame admitted");
        g=Geometry(); g.target_from_dominant[i]=inf;
        Check(!SolveTwoHandGrip(g).valid,"nonfinite target frame admitted");
    }
    for (int i=0;i<6;++i) {
        auto g=Geometry();
        if(i==0)g.source_support_m.x=nan; if(i==1)g.source_support_m.y=inf;
        if(i==2)g.source_support_m.z=nan; if(i==3)g.target_support_m.x=inf;
        if(i==4)g.target_support_m.y=nan; if(i==5)g.target_support_m.z=inf;
        Check(!SolveTwoHandGrip(g).valid,"nonfinite support admitted");
    }
    auto g=Geometry(); g.source_support_m={1,2,3};
    Check(!SolveTwoHandGrip(g).valid,"coincident authored sockets admitted");
    g=Geometry(); g.target_support_m={4,5,6};
    Check(!SolveTwoHandGrip(g).valid,"coincident tracked grips admitted");
    g=Geometry(); g.source_support_m={1,3,3};
    Check(!SolveTwoHandGrip(g).valid,"source span parallel to up admitted");
    g=Geometry(); g.target_support_m={3,5,6};
    Check(!SolveTwoHandGrip(g).valid,"target span parallel to dominant up admitted");
    g=Geometry(); g.target_support_m={3,5.000001F,6};
    Check(!SolveTwoHandGrip(g).valid,"near-collinear target up admitted");
    g=Geometry(); g.source_from_dominant[0]=-1;
    Check(!SolveTwoHandGrip(g).valid,"mirrored source frame admitted");
    g=Geometry(); g.target_from_dominant[10]=-1;
    Check(!SolveTwoHandGrip(g).valid,"mirrored tracked frame admitted");
    g=Geometry(); g.source_from_dominant[0]=2;
    Check(!SolveTwoHandGrip(g).valid,"scaled source frame admitted");
    g=Geometry(); g.target_from_dominant[1]=-.5F;
    Check(!SolveTwoHandGrip(g).valid,"sheared target frame admitted");
}
void FreshPressOnly() {
    PolicyFixture f; Check(!f.Step().acquired,"proximity alone attached");
    f.in.grip_held=true; const auto o=f.Step();
    Check(o.acquired && o.consume_grip,"near fresh edge not claimed");
    Check(!f.Step().acquired,"hold acquired twice");
    f.in.grip_held=false; const auto release=f.Step();
    Check(release.released && release.consume_grip && release.stage==TwoHandGripStage::free,"claimed release leaked");
    f.in.grip_held=true; Check(f.Step().acquired,"deliberate regrip failed");
}
void HeldEntryAndOutsideHoldPreserveInteraction() {
    PolicyFixture f; f.in.grip_held=true;
    auto o=f.Step(); Check(!o.acquired&&!o.consume_grip,"held entry claimed interaction");
    f.in.grip_held=false; (void)f.Step();
    f.in.contact_distance_m=.2F; f.in.grip_held=true;
    o=f.Step(); Check(!o.acquired&&!o.consume_grip,"outside press consumed interaction");
    f.in.contact_distance_m=0;
    for(int i=0;i<5;++i) { o=f.Step(); Check(!o.acquired&&!o.consume_grip,"held outside interaction stolen near contact"); }
    f.in.grip_held=false; o=f.Step(); Check(!o.consume_grip,"unclaimed release consumed");
    f.in.grip_held=true; Check(f.Step().acquired,"new near edge failed");
}
void UnqualifiedAndInvalidContactNeverClaim() {
    for(int i=0;i<7;++i) {
        PolicyFixture f; (void)f.Step(); f.in.grip_held=true;
        if(i==0)f.in.contact_qualified=false;
        if(i==1)f.in.contact_distance_m=-1;
        if(i==2)f.in.contact_distance_m=std::numeric_limits<float>::quiet_NaN();
        if(i==3)f.in.acquisition_radius_m=0;
        if(i==4)f.in.acquisition_radius_m=std::numeric_limits<float>::infinity();
        if(i==5)f.in.eligible=false; if(i==6)f.in.owner=0;
        const auto o=f.Step(); Check(!o.acquired&&!o.consume_grip,"unqualified contact stole interaction");
    }
}
void AttachmentDoesNotDependOnProximityAfterClaim() {
    PolicyFixture f; (void)f.Attach(); f.in.contact_distance_m=100;
    const auto o=f.Step(); Check(o.stage==TwoHandGripStage::attached&&o.consume_grip&&!o.acquired,"acquisition radius used as retention gate");
}
void InterruptedClaimConsumesUntilAvailableRelease() {
    for(int cause=0;cause<10;++cause) {
        PolicyFixture f; (void)f.Attach(); f.in.support_trigger_held=true;
        const auto saved=f.in;
        if(cause==0)f.in.enabled=false; if(cause==1)f.in.eligible=false;
        if(cause==2)f.in.tracking_valid=false; if(cause==3)f.in.recentered=true;
        if(cause==4)++f.in.owner; if(cause==5)++f.in.input_generation;
        if(cause==6)f.in.grip_available=false; if(cause==7)f.in.contact_qualified=false;
        if(cause==8)f.in.support_trigger_available=false; if(cause==9)f.in.owner=0;
        auto o=f.Step();
        Check(o.cancelled&&o.stage==TwoHandGripStage::release_required&&o.consume_grip&&o.consume_support_trigger,"interrupted claim escaped consumption");
        const auto sequence=f.in.sequence, time=f.in.sample_time_ms;
        f.in=saved; f.in.sequence=sequence; f.in.sample_time_ms=time;
        for(int i=0;i<3;++i) { o=f.Step(); Check(!o.acquired&&o.consume_grip&&o.consume_support_trigger,"recovery replayed held claim"); }
        f.in.grip_available=false; f.in.grip_held=false;
        o=f.Step(); Check(o.consume_grip,"unavailable source counted as release");
        f.in.grip_available=true; f.in.grip_held=true;
        Check(f.Step().consume_grip,"held reacquisition escaped barrier");
        f.in.grip_held=false; o=f.Step(); Check(o.consume_grip&&o.consume_support_trigger,"claim release or held trigger leaked");
        o=f.Step(); Check(!o.consume_grip&&o.consume_support_trigger,"independent trigger barrier not retained");
        f.in.support_trigger_available=false; f.in.support_trigger_held=false;
        Check(f.Step().consume_support_trigger,"unavailable trigger counted as release");
        f.in.support_trigger_available=true; Check(f.Step().consume_support_trigger,"trigger release leaked");
        Check(!f.Step().consume_support_trigger,"available trigger release did not clear barrier");
        f.in.grip_held=true; Check(f.Step().acquired,"fresh claim did not recover");
    }
}
void ReleaseWhileDisabledStillDischargesClaim() {
    PolicyFixture f; (void)f.Attach();
    f.in.support_trigger_held=true; f.in.enabled=false; (void)f.Step();
    f.in.grip_held=false; f.in.support_trigger_held=false;
    auto o=f.Step(); Check(o.consume_grip&&o.consume_support_trigger,"disabled claim release leaked");
    o=f.Step(); Check(!o.consume_grip&&!o.consume_support_trigger&&o.stage==TwoHandGripStage::free,"disable trapped released controls");
}
void NormalDetachRetainsHeldSupportTrigger() {
    PolicyFixture f; (void)f.Attach(); f.in.support_trigger_held=true; (void)f.Step();
    f.in.grip_held=false;
    auto o=f.Step();
    Check(o.released&&o.consume_grip&&o.consume_support_trigger,"normal detach leaked held trigger");
    o=f.Step(); Check(!o.consume_grip&&o.consume_support_trigger,"normal detach dropped independent trigger barrier");
    f.in.support_trigger_available=false; f.in.support_trigger_held=false;
    Check(f.Step().consume_support_trigger,"normal detach used unavailable trigger as release");
    f.in.support_trigger_available=true; f.in.support_trigger_held=true;
    Check(f.Step().consume_support_trigger,"normal detach fired on held reacquisition");
    f.in.support_trigger_held=false;
    Check(f.Step().consume_support_trigger,"normal detach leaked trigger release");
    Check(!f.Step().consume_support_trigger,"normal detach trigger did not recover");
}
void DuplicateNeverAcquiresOrConfirmsRelease() {
    PolicyFixture f; (void)f.Attach();
    auto o=f.policy.Update(f.in); Check(!o.acquired&&o.stage==TwoHandGripStage::attached,"duplicate replayed acquisition");
    f.in.grip_held=false; o=f.policy.Update(f.in);
    Check(!o.released&&o.consume_grip&&o.stage==TwoHandGripStage::attached,"duplicate fabricated release");
    Check(f.Step().released,"fresh release failed after duplicate");
}
void InvalidSamplesCancelAndCannotRearm() {
    for(int cause=0;cause<7;++cause) {
        PolicyFixture f; (void)f.Attach();
        if(cause==0)--f.in.sequence;
        if(cause==1)f.in.now_ms+=200;
        if(cause==2)--f.in.now_ms;
        if(cause==3) { ++f.in.sequence; --f.in.sample_time_ms; }
        if(cause==4) { ++f.in.sequence; f.in.sample_time_ms+=10; }
        if(cause==5)f.in.sequence=0;
        if(cause==6) { ++f.in.sequence; f.in.now_ms+=200; f.in.sample_time_ms=f.in.now_ms; }
        auto o=f.policy.Update(f.in);
        Check(o.cancelled&&o.consume_grip&&o.stage==TwoHandGripStage::release_required,"invalid sample kept attachment");
        f.in.now_ms+=300; f.in.sample_time_ms=f.in.now_ms; f.in.sequence+=100;
        o=f.policy.Update(f.in); Check(!o.acquired&&o.consume_grip,"recovery sample reattached held grip");
    }
}
void NewGenerationCannotInheritReleaseBaseline() {
    PolicyFixture f; (void)f.Step(); ++f.in.input_generation; f.in.grip_held=true;
    Check(!f.Step().acquired,"input generation inherited released baseline");
    f.in.grip_held=false; (void)f.Step(); f.in.grip_held=true;
    Check(f.Step().acquired,"new generation did not recover from release");
}
}
int main() {
    struct Test { const char* name; void (*run)(); };
    const Test tests[]={
        {"off-axis roll and full map",OffAxisRollAndFullMap},
        {"unequal span never scales",UnequalSpanNeverScales},
        {"source-frame covariance",SourceFrameCovariance},
        {"target-frame covariance",TargetFrameCovariance},
        {"authored dominant roll",AuthoredDominantRollIsNotAssumedIdentity},
        {"independent support aim",SupportAimChangesWithoutDominantRotation},
        {"invalid geometry",InvalidGeometryRejects},
        {"fresh press only",FreshPressOnly},
        {"held entry and outside interaction",HeldEntryAndOutsideHoldPreserveInteraction},
        {"unqualified contact",UnqualifiedAndInvalidContactNeverClaim},
        {"retained claim outside proximity",AttachmentDoesNotDependOnProximityAfterClaim},
        {"interrupted claim and independent release barriers",InterruptedClaimConsumesUntilAvailableRelease},
        {"disabled release recovery",ReleaseWhileDisabledStillDischargesClaim},
        {"normal detach trigger barrier",NormalDetachRetainsHeldSupportTrigger},
        {"duplicate input",DuplicateNeverAcquiresOrConfirmsRelease},
        {"invalid sample cancellation",InvalidSamplesCancelAndCannotRearm},
        {"generation release baseline",NewGenerationCannotInheritReleaseBaseline}
    };
    int failures=0;
    for(const auto& t:tests) {
        try { t.run(); std::cout<<"PASS "<<t.name<<'\n'; }
        catch(const std::exception& e) { ++failures; std::cerr<<"FAIL "<<t.name<<": "<<e.what()<<'\n'; }
    }
    std::cout<<"two_hand_grip: "<<(sizeof(tests)/sizeof(tests[0]))<<" groups, "<<failures<<" failures\n";
    return failures?1:0;
}
