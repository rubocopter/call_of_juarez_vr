#include "backends/openvr/dlss_protocol.hpp"
#include <iostream>
#include <limits>
using namespace cojvr::backends::openvr::dlss;
int main() {
    Request r;if(!ValidRequest(r))return 1;
    r.operation=Operation::build;r.width=r.height=r.out_width=r.out_height=64;
    if(ValidRequest(r))return 2;
    for(auto& h:r.textures)h=1;r.fence_in=2;r.fence_out=3;
    if(!ValidRequest(r))return 3;
    r.out_width=96;if(ValidRequest(r))return 4;
    r.quality=Quality::quality;if(!ValidRequest(r))return 5;
    r.width=std::numeric_limits<unsigned>::max();if(ValidRequest(r))return 6;r.width=64;
    r.operation=Operation::evaluate;if(ValidRequest(r))return 7;r.sequence=1;
    if(!ValidRequest(r))return 8;
    r.jitter_x=std::numeric_limits<float>::infinity();if(ValidRequest(r))return 9;r.jitter_x=0;
    r.eye=2;if(ValidRequest(r))return 10;r.eye=1;
    r.protocol=2;if(ValidRequest(r))return 11;r.protocol=version;
    r.operation=static_cast<Operation>(99);if(ValidRequest(r))return 12;
    std::cout<<"DLSS x86/x64 wire sizes, bounds, finite jitter and invalid requests passed\n";
}
