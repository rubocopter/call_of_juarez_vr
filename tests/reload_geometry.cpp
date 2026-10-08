#include "games/call_of_juarez/reload_geometry.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<1e-4F;}
int main(){
    ElementWorldBasisTarget drum{{100,200,300},{0,1,0},{0,0,1},true};
    std::array<ElementWorldBasisTarget,6> mouths{};
    Check(BuildCoJReloadMouthFrames(CoJReloadModel::peacemaker,drum,mouths),"known cylinder rejected");
    Check(Near(mouths[3].position.x,98.765282F)&&Near(mouths[3].position.y,199.287538F)&&Near(mouths[3].position.z,297.616722F),"measured Peacemaker rear mouth not transformed in centimetres");
    drum.up={-1,0,0}; drum.forward={0,0,1};
    Check(BuildCoJReloadMouthFrames(CoJReloadModel::frontier,drum,mouths),"rotated Frontier cylinder rejected");
    Check(Near(mouths[2].position.x,100.010422F)&&Near(mouths[2].position.y,198.814174F)&&Near(mouths[2].position.z,297.989515F),"native cylinder phase must rotate chamber geometry");
    Check(mouths[2].up.x==-1&&mouths[2].forward.z==1,"mouth must retain actual insertion basis");
    drum.forward={0,0,2};
    Check(!BuildCoJReloadMouthFrames(CoJReloadModel::frontier,drum,mouths),"scaled frame admitted");
    for(const auto& mouth:mouths)Check(!mouth.valid,"failed observation retained previous mouths");
    drum={{},{0,1,0},{0,0,1},true};
    Check(!BuildCoJReloadMouthFrames(CoJReloadModel::unknown,drum,mouths),"unknown model admitted");
    drum.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!BuildCoJReloadMouthFrames(CoJReloadModel::peacemaker,drum,mouths),"nonfinite frame admitted");
    std::cout<<"exact reload cylinder geometry passed\n";
}
