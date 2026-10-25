#include "runtime/temporal_reconstruction.hpp"
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
int main() {
    TemporalHistory history;bool reset=false;
    TemporalFrameIdentity frame{1,1,1,1,4,640,480,960,720,1,true,true};
    if(!history.Admit(frame,reset)||!reset)return 1;history.Commit(frame);
    if(history.Admit(frame,reset))return 2;
    ++frame.frame;++frame.pose;if(!history.Admit(frame,reset)||reset)return 3;history.Commit(frame);
    frame.frame+=2;frame.pose+=2;if(!history.Admit(frame,reset)||!reset)return 4;history.Commit(frame);
    ++frame.frame;++frame.pose;++frame.owner;if(!history.Admit(frame,reset)||!reset)return 5;history.Commit(frame);
    ++frame.frame;++frame.pose;frame.quality=0;if(!history.Admit(frame,reset)||!reset)return 6;
    frame.tracking_valid=false;if(history.Admit(frame,reset))return 7;frame.tracking_valid=true;
    frame.native_stereo=false;if(history.Admit(frame,reset))return 8;frame.native_stereo=true;
    ++frame.generation;frame.frame=1;if(!history.Admit(frame,reset)||!reset)return 9;
    auto current=TemporalIdentity(),previous=TemporalIdentity();previous[12]=.25F;TemporalMatrix transform{};
    if(!TemporalReprojection(current,previous,transform)||transform!=previous)return 10;
    current[12]=.5F;
    if(!TemporalReprojection(current,previous,transform)||std::abs(transform[12]+.25F)>1e-6)return 11;
    previous=current;if(!TemporalReprojection(current,previous,transform))return 12;
    for(unsigned k=0;k<16;++k)if(std::abs(transform[k]-TemporalIdentity()[k])>1e-6)return 13;
    current.fill(0);if(TemporalReprojection(current,previous,transform))return 14;
    current=TemporalIdentity();current[2]=std::numeric_limits<float>::quiet_NaN();
    if(TemporalReprojection(current,previous,transform))return 15;
    std::cout<<"Temporal row-matrix reprojection, singular rejection, frame/pose/generation/owner resets and no repeat advancement passed\n";
}
