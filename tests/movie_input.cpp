#include "games/call_of_juarez/movie_input.hpp"
#include <cstdlib>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
int main(){
    for(unsigned mask=0;mask<256;++mask){
        const bool result=CoJMovieSkipRequested(mask&1,mask&2,mask&4,17,17,mask&8,mask&16,mask&32,mask&64,mask&128);
        const bool expected=(mask&1)&&(mask&2)&&!(mask&4)&&(mask&248);
        if(result!=expected){std::cerr<<"cinematic UI intent crossed an owner/input barrier\n";return 1;}
    }
    for(const auto expected:{0ULL,16ULL,18ULL})
        if(CoJMovieSkipRequested(true,true,false,expected,17,true,false,false,false,false)){
            std::cerr<<"an earlier owner edge skipped a newly entered movie\n";return 1;
        }
    CoJPresentationInputEpoch owner;
    const auto gameplay=owner.Observe(1,0),movie_a=owner.Observe(17,7);
    const auto stable_a=owner.Observe(17,7),movie_b=owner.Observe(17,8);
    if(!gameplay||gameplay==movie_a||movie_a!=stable_a||movie_a==movie_b||
        CoJMovieSkipRequested(true,true,false,movie_b,movie_a,true,false,false,false,false)){
        std::cerr<<"same-flags movie replacement retained earlier input ownership\n";return 1;
    }
    CoJMovieSkipDispatchGate gate;
    if(gate.Consume(true,0,1)||gate.Consume(true,1,0)||gate.Consume(false,1,1)||
        !gate.Consume(true,1,1)||gate.Consume(true,1,1)||gate.Consume(true,1,2)||
        !gate.Consume(true,2,2)||gate.Consume(true,1,3)){
        std::cerr<<"repeated/native-rejected sample replayed cinematic skip\n";return 1;
    }
    CoJPresentationOwner scene{};scene.valid=scene.player_alive=scene.movie_playing=true;
    scene.movie_input_epoch=9;
    CoJMovieSkipResult rejected{};rejected.reason="native_permission_denied";
    if(!CoJContinueMovieAfterSkip(rejected,true,scene,9)){
        std::cerr<<"refused native skip dropped unchanged immersive stereo\n";return 1;
    }
    for(unsigned flags=0;flags<128;++flags){
        auto now=scene;auto result=rejected;
        result.attempted=(flags&1)!=0;result.delivered=(flags&2)!=0;
        now.timer_frozen=(flags&4)!=0;now.movie_target_missing=(flags&8)!=0;
        now.movie_playing=(flags&16)==0;now.movie_input_epoch=(flags&32)?10:9;
        const bool observed=(flags&64)==0;
        const bool expected=flags==0;
        if(CoJContinueMovieAfterSkip(result,observed,now,9)!=expected){
            std::cerr<<"skip outcome reused attempted/stale/blocked movie preimage\n";return 1;
        }
    }
    if(CoJContinueMovieAfterSkip(rejected,true,scene,0)){
        std::cerr<<"skip rejection without original scene identity reused camera\n";return 1;
    }
    std::cout<<"cinematic skip intent admission passed\n";
}
