#include "games/call_of_juarez/winchester_loading_geometry.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::games::call_of_juarez;
using cojvr::runtime::Vec3;

namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool Near(float a, float b, float epsilon = 2e-5F) {
    return std::abs(a - b) <= epsilon;
}
bool Near(Vec3 a, Vec3 b) {
    return Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z);
}
constexpr ElementWorldBasisTarget identity{{},{0,1,0},{0,0,1},true};
constexpr std::array<CoJWinchesterLoadingModel,2> models{
    CoJWinchesterLoadingModel::ordinary, CoJWinchesterLoadingModel::scoped};
// Independent fixture from rim-measurements.json. Ordinary/scoped vertex cycles
// differ, but all eight measured GunBody-local positions agree (native cm).
constexpr std::array<Vec3,8> measured{{
    {1.2345394813455641F,-14.82387183594983F,-5.256761945434846F},
    {1.2345394813455641F,-20.06876739661675F,-5.249093969468959F},
    {1.2115355534479022F,-20.06876739661675F,-3.4221986955963075F},
    {1.213452547439374F,-14.846875763847493F,-3.4471196174854413F},
    {1.2345394813455641F,-14.84495876985602F,-3.79217853595037F},
    {1.2345394813455641F,-14.488397887442261F,-3.79217853595037F},
    {1.2441244513029233F,-14.496065863408148F,-4.905952044995502F},
    {1.2441244513029233F,-14.821954841958359F,-4.913620020961389F}}};

CoJWinchesterLoadingGeometry Geometry(CoJWinchesterLoadingModel model) {
    CoJWinchesterLoadingGeometry g{};
    g.model=model; g.source=CoJReloadGeometrySource::post_overlay;
    g.root=g.body=g.loader=g.holder=g.barrel=identity;
    Check(BuildCoJWinchesterLoadingLip(model,g.body,g.lip),"fixture lip unavailable");
    g.valid=true;
    return g;
}
ElementWorldBasisTarget Tip(Vec3 position) {
    // Nose points along -X here: observation must retain the actual direction.
    return {position,{0,1,0},{-1,0,0},true};
}
Vec3 Rotate(Vec3 v) {
    // A proper rotation with no axis aligned to any world axis.
    return {(2*v.x-2*v.y+v.z)/3,(v.x+2*v.y+2*v.z)/3,(-2*v.x-v.y+2*v.z)/3};
}
Vec3 Move(Vec3 v) {
    v=Rotate(v); return {v.x+12,v.y-7,v.z+35};
}
ElementWorldBasisTarget Move(ElementWorldBasisTarget f) {
    f.position=Move(f.position); f.up=Rotate(f.up); f.forward=Rotate(f.forward);
    return f;
}
void Invalid(const CoJWinchesterLoadingGeometry& g, const ElementWorldBasisTarget& tip,
             bool owner=true) {
    const auto o=ObserveCoJWinchesterLoadingTip(g,tip,owner);
    Check(!o.valid && !o.projected_inside && o.plane_distance_cm==0 &&
          o.edge_distance_cm==0 && Near(o.body_local_tip,Vec3{}) &&
          Near(o.body_local_axis,Vec3{}),"rejected observation retained diagnostics");
}

void CornersAndRigidMotion() {
    for (auto model:models) {
        const auto g=Geometry(model);
        for (std::size_t i=0;i<measured.size();++i)
            Check(g.lip[i].x==measured[i].x && g.lip[i].y==measured[i].y &&
                  g.lip[i].z==measured[i].z,"wrong measured lip corner/order/units");
        auto moved=g;
        moved.root=Move(g.root); moved.body=Move(g.body); moved.loader=Move(g.loader);
        moved.holder=Move(g.holder); moved.barrel=Move(g.barrel);
        Check(BuildCoJWinchesterLoadingLip(model,moved.body,moved.lip),"tilted body rejected");
        Check(Near(moved.lip[0],Vec3{20.95335356F,-19.97557603F,35.61375633F}),
              "non-axis-aligned corner transform wrong");
        for (std::size_t i=0;i<measured.size();++i)
            Check(Near(moved.lip[i],Move(measured[i])),"rigid lip transform wrong");
        for (const auto p: {Vec3{2,-20,-4.5F},Vec3{2,-14.7F,-3.6F},measured[2]}) {
            const auto tip=Tip(p);
            const auto a=ObserveCoJWinchesterLoadingTip(g,tip,true);
            const auto b=ObserveCoJWinchesterLoadingTip(moved,Move(tip),true);
            Check(a.valid && b.valid && a.projected_inside==b.projected_inside &&
                  Near(a.body_local_tip,b.body_local_tip) &&
                  Near(a.body_local_axis,b.body_local_axis) &&
                  Near(a.plane_distance_cm,b.plane_distance_cm) &&
                  Near(a.edge_distance_cm,b.edge_distance_cm),
                  "observation changed under rigid rotation/translation");
        }
        // Independent child poses are valid: loader motion is not gate admission.
        moved.loader=Move(moved.loader); moved.holder.position={-90,12,80};
        Check(ObserveCoJWinchesterLoadingTip(moved,Move(Tip({2,-20,-4.5F})),true).valid,
              "independent rigid child frames rejected");
    }
}

void ProjectedPolygonAndPlane() {
    for (auto model:models) {
        const auto g=Geometry(model);
        const auto a=ObserveCoJWinchesterLoadingTip(g,Tip({2,-20,-4.5F}),true);
        Check(a.valid && a.projected_inside,"interior tip not observed");
        Check(Near(a.body_local_tip,Vec3{2,-20,-4.5F}) &&
              Near(a.body_local_axis,Vec3{-1,0,0}),"tip/nose direction not body-local");
        // Closest edge is the measured constant-Y segment 1->2, not centroid.
        Check(Near(a.edge_distance_cm,.0687673966F),"nearest edge distance wrong");
        Check(Near(a.plane_distance_cm,.7752351384F),"best-fit signed plane wrong");
        auto tip=Tip({0,-20,-4.5F});
        const auto behind=ObserveCoJWinchesterLoadingTip(g,tip,true);
        Check(behind.valid && behind.projected_inside &&
              Near(behind.plane_distance_cm,-1.2246363505F) &&
              Near(behind.edge_distance_cm,a.edge_distance_cm),
              "plane side affected projected geometry or sign");
        tip.forward={1,0,0};
        const auto reversed=ObserveCoJWinchesterLoadingTip(g,tip,true);
        Check(reversed.valid && Near(reversed.body_local_axis,Vec3{1,0,0}) &&
              Near(reversed.plane_distance_cm,behind.plane_distance_cm),
              "actual nose axis inferred/rejected as an insertion direction");
        tip=identity; tip.position={2,-20,-4.5F};
        Check(Near(ObserveCoJWinchesterLoadingTip(g,tip,true).body_local_axis,Vec3{0,0,1}),
              "tangential cartridge direction hidden");
        const auto outside=ObserveCoJWinchesterLoadingTip(g,Tip({2,-21,-4.5F}),true);
        Check(outside.valid && !outside.projected_inside &&
              Near(outside.edge_distance_cm,.9312326034F),"outside edge distance wrong");
        const auto notch=ObserveCoJWinchesterLoadingTip(g,Tip({2,-14.7F,-3.6F}),true);
        Check(notch.valid && !notch.projected_inside,"concave notch replaced by convex hull/AABB");
        Check(ObserveCoJWinchesterLoadingTip(g,Tip({2,-14.65F,-4.3F}),true).projected_inside,
              "projecting tab omitted from concave lip");
        // Boundary is included only as a numerical tie, with zero edge distance.
        for (std::size_t i=0;i<measured.size();++i) {
            const auto v=ObserveCoJWinchesterLoadingTip(g,Tip(measured[i]),true);
            Check(v.valid && v.projected_inside && Near(v.edge_distance_cm,0),
                  "measured vertex not classified on boundary");
            const auto next=measured[(i+1)%measured.size()];
            const Vec3 midpoint{0,(measured[i].y+next.y)*.5F,(measured[i].z+next.z)*.5F};
            const auto edge=ObserveCoJWinchesterLoadingTip(g,Tip(midpoint),true);
            Check(edge.valid && edge.projected_inside && Near(edge.edge_distance_cm,0),
                  "projected edge midpoint not classified on boundary");
        }
        // No interaction tolerance: 0.0002 cm outside remains outside.
        const auto close=ObserveCoJWinchesterLoadingTip(g,Tip({0,measured[1].y-.0002F,-4.5F}),true);
        Check(close.valid && !close.projected_inside,"numeric tie became acceptance padding");
        const auto residual=ObserveCoJWinchesterLoadingTip(g,Tip(measured[0]),true);
        Check(Near(residual.plane_distance_cm,-.0096405601F),
              "slightly nonplanar lip flattened to constant X");
    }
}

void Rejections() {
    const auto good=Geometry(CoJWinchesterLoadingModel::ordinary);
    const auto tip=Tip({2,-20,-4.5F});
    Invalid(good,tip,false);
    auto bad=good; bad.valid=false; Invalid(bad,tip);
    for (auto source:{CoJReloadGeometrySource::pre_native,CoJReloadGeometrySource::unknown,
                      static_cast<CoJReloadGeometrySource>(255)}) {
        bad=good; bad.source=source; Invalid(bad,tip);
    }
    for (auto model:{CoJWinchesterLoadingModel::unknown,static_cast<CoJWinchesterLoadingModel>(255)}) {
        bad=good; bad.model=model; Invalid(bad,tip);
        auto lip=good.lip;
        Check(!BuildCoJWinchesterLoadingLip(model,identity,lip),"unknown model built lip");
        for (const auto v:lip) Check(Near(v,Vec3{}),"rejected builder kept old corners");
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    for (auto invalid:{ElementWorldBasisTarget{},
                      ElementWorldBasisTarget{{nan,0,0},{0,1,0},{0,0,1},true},
                      ElementWorldBasisTarget{{0,inf,0},{0,1,0},{0,0,1},true},
                      ElementWorldBasisTarget{{},{nan,1,0},{0,0,1},true},
                      ElementWorldBasisTarget{{},{0,1,0},{0,0,inf},true},
                      ElementWorldBasisTarget{{},{0,2,0},{0,0,1},true},
                      ElementWorldBasisTarget{{},{0,1,0},{0,0,2},true},
                      ElementWorldBasisTarget{{},{0,1,0},{0,1,0},true},
                      ElementWorldBasisTarget{{},{0,1,0},{.6F,.8F,0},true}}) {
        auto lip=good.lip;
        Check(!BuildCoJWinchesterLoadingLip(good.model,invalid,lip),"non-rigid body built lip");
        for (const auto v:lip) Check(Near(v,Vec3{}),"invalid body retained lip");
        Invalid(good,invalid);
        for (auto member:{&CoJWinchesterLoadingGeometry::root,&CoJWinchesterLoadingGeometry::body,
                          &CoJWinchesterLoadingGeometry::loader,&CoJWinchesterLoadingGeometry::holder,
                          &CoJWinchesterLoadingGeometry::barrel}) {
            bad=good; bad.*member=invalid; Invalid(bad,tip);
        }
    }
    for (std::size_t i=0;i<good.lip.size();++i) {
        bad=good; bad.lip[i].x=std::nextafter(bad.lip[i].x,inf); Invalid(bad,tip);
        bad=good; bad.lip[i].y+=1; Invalid(bad,tip);
        bad=good; bad.lip[i].z=nan; Invalid(bad,tip);
    }
    bad=good; std::swap(bad.lip[0],bad.lip[1]); Invalid(bad,tip);
    bad=good; bad.body.position.x=1; Invalid(bad,tip);
    // Finite input alone is insufficient if the derived local cm exceed float.
    bad=good; bad.body.position.x=-std::numeric_limits<float>::max();
    Check(BuildCoJWinchesterLoadingLip(bad.model,bad.body,bad.lip),"finite far body rejected");
    auto far_tip=tip; far_tip.position.x=std::numeric_limits<float>::max();
    Invalid(bad,far_tip);
}
}

int main() {
    CornersAndRigidMotion();
    ProjectedPolygonAndPlane();
    Rejections();
    std::cout << "Winchester loading lip observation geometry passed\n";
}
