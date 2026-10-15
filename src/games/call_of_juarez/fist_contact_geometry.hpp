#pragma once
#include "games/call_of_juarez/body_adapter.hpp"
#include <cmath>
#include <cstdint>
namespace cojvr::games::call_of_juarez {
enum class CoJFistSkinModel : std::uint8_t {ray,billy};
// Positions are native world centimetres, reconstructed from CURRENT displayed
// frames. The actual attachment socket and anatomical Hand are distinct bases.
struct CoJFistContactGeometry {
    std::array<runtime::Vec3,4> contacts_cm{}; // index, middle, ring, pinky skin
    ElementWorldBasisTarget socket{},hand_root{};
    bool valid=false;
};
struct CoJFistJointGeometry {
    // Conservative joint samples, NOT a surface, radius or fist-volume claim.
    std::array<runtime::Vec3,8> contacts_cm{}; // proximal/intermediate per digit
    ElementWorldBasisTarget socket{},hand_root{};
    bool valid=false;
};
namespace fist_contact_detail {
inline runtime::Vec3 Add(runtime::Vec3 a,runtime::Vec3 b)noexcept{return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline runtime::Vec3 Scale(runtime::Vec3 a,float s)noexcept{return {a.x*s,a.y*s,a.z*s};}
inline float Dot(runtime::Vec3 a,runtime::Vec3 b)noexcept{return a.x*b.x+a.y*b.y+a.z*b.z;}
inline runtime::Vec3 Cross(runtime::Vec3 a,runtime::Vec3 b)noexcept{return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Finite(runtime::Vec3 a)noexcept{return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline bool Valid(const CoJHandFrames& frames)noexcept{
    // Same rigidity tolerance as the displayed-finger policy. Semantic slots
    // are resolved by native bone names; numeric mesh IDs differ by model.
    for(std::size_t i=0;i<frames.size();++i){const auto& s=frames[i];const auto& f=s.frame;
        if(s.element<0||!f.valid||!Finite(f.position)||!Finite(f.up)||!Finite(f.forward)||
           std::fabs(Dot(f.up,f.up)-1)>=.002F||std::fabs(Dot(f.forward,f.forward)-1)>=.002F||
           std::fabs(Dot(f.up,f.forward))>=.002F)return false;
        for(std::size_t j=0;j<i;++j)if(s.element==frames[j].element)return false;
    }return true;
}
struct Influence {std::uint8_t frame;std::uint16_t weight;runtime::Vec3 bind_local_cm;};
struct SkinPoint {Influence influences[2];std::uint32_t source_vertex;};
// Original skin witnesses, NOT tuned reach offsets. Ray mesh SHA-256 starts
// 9b3f4579cd32476f; Billy ce1e70dfab7ddae4. Full pinned identities are owned by
// split_coj_player_mesh.py. Weights are original unsigned values / 32767.
// Each bind-local point follows its entire live bone blend, including curls.
inline constexpr SkinPoint kSkin[2][2][4]={
 { // Ray
  { // physical R
   {{{7,32767,{4.18818502F,-0.8269871538F,1.443250575F}},{0,0,{}}},4747},
   {{{10,32767,{4.602016253F,0.061503646F,1.664926456F}},{0,0,{}}},4761},
   {{{13,32767,{4.89581555F,-0.2258628405F,1.560867747F}},{0,0,{}}},4813},
   {{{16,32767,{3.475664967F,0.516716847F,1.506239687F}},{0,0,{}}},4803},
  },
  { // physical L
   {{{7,32767,{4.18821774F,0.8268323716F,1.443238674F}},{0,0,{}}},4796},
   {{{10,32767,{4.602011459F,-0.0616635797F,1.664908207F}},{0,0,{}}},4781},
   {{{13,32767,{4.730483683F,-0.6543391991F,1.612058122F}},{0,0,{}}},4774},
   {{{16,32767,{3.475596584F,-0.5168527021F,1.506202121F}},{0,0,{}}},4713},
  },
 },
 { // Billy
  { // physical R
   {{{7,19162,{3.421824562F,-0.506105309F,1.089329432F}},{8,13605,{-0.7888261035F,-0.5061049217F,0.9238690267F}}},3893},
   {{{11,16384,{-0.5867158474F,-0.3336042922F,0.9373308194F}},{10,16383,{4.673530516F,-0.3336037864F,1.049524712F}}},3721},
   {{{13,32767,{3.995998895F,0.3551216937F,0.9723586596F}},{0,0,{}}},3625},
   {{{16,32767,{2.508650654F,-0.3076848622F,0.9251212957F}},{0,0,{}}},3572},
  },
  { // physical L
   {{{7,19162,{3.421852849F,0.5059372948F,1.089336794F}},{8,13605,{-0.7887966216F,0.5059373336F,0.9238912158F}}},2869},
   {{{11,16384,{-0.5867210253F,0.3334231889F,0.9373284157F}},{10,16383,{4.673525318F,0.3334228551F,1.049516397F}}},2722},
   {{{13,16384,{4.907605796F,-0.3638407417F,1.062799689F}},{14,16383,{-0.0380291569F,-0.3638411148F,1.101088208F}}},3358},
   {{{16,32767,{2.52248674F,-0.3959729371F,0.8599419229F}},{0,0,{}}},3080},
  },
 },
};
inline runtime::Vec3 Skin(const SkinPoint& point,const CoJHandFrames& frames)noexcept{
    runtime::Vec3 result{};
    for(const auto& influence:point.influences){if(!influence.weight)continue;
        const auto& f=frames[influence.frame].frame;const auto p=influence.bind_local_cm;
        const auto world=Add(f.position,Add(Scale(Cross(f.up,f.forward),p.x),Add(Scale(f.up,p.y),Scale(f.forward,p.z))));
        result=Add(result,Scale(world,static_cast<float>(influence.weight)/32767.F));
    }return result;
}
}
// Caller must qualify exact model/assets, same current Being/generation and an
// active verified display transaction. This pure function grants no ownership,
// closure/attack admission or collision authority. hand 0=right, 1=left.
inline CoJFistContactGeometry BuildCoJFistContactGeometry(CoJFistSkinModel model,int hand,const CoJHandFrames& frames)noexcept{
    if((model!=CoJFistSkinModel::ray&&model!=CoJFistSkinModel::billy)||hand<0||hand>1||!fist_contact_detail::Valid(frames))return {};
    CoJFistContactGeometry out{};out.socket=frames[19].frame;out.hand_root=frames[3].frame;
    const auto& points=fist_contact_detail::kSkin[static_cast<unsigned>(model)][hand];
    for(std::size_t i=0;i<out.contacts_cm.size();++i){out.contacts_cm[i]=fist_contact_detail::Skin(points[i],frames);
        if(!fist_contact_detail::Finite(out.contacts_cm[i]))return {};}
    out.valid=true;return out;
}
inline CoJFistJointGeometry BuildCoJFistJointGeometry(const CoJHandFrames& frames)noexcept{
    if(!fist_contact_detail::Valid(frames))return {};
    CoJFistJointGeometry out{};out.socket=frames[19].frame;out.hand_root=frames[3].frame;
    constexpr std::size_t slots[]{7,8,10,11,13,14,16,17};
    for(std::size_t i=0;i<out.contacts_cm.size();++i)out.contacts_cm[i]=frames[slots[i]].frame.position;
    out.valid=true;return out;
}
}
