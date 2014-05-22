// rng.cpp -- Mersenne Twister PRNG

#include "includes.h"
#include "lib/rng.h"

uint32_t mt_state[624];
uint32_t mt_index = 0;

void rng_seed(uint32_t seed) {
    mt_index = 0;
    mt_state[0] = seed;
    for(int i=1;i<624;i++) {
        mt_state[i] = (0x6c078965 * (mt_state[i-1] ^ (mt_state[i-1]>>30)) + i);
    }
}

void rng_generate() {
    for(int i=0;i<624;i++) {
        uint32_t y = (mt_state[i] & 0x80000000) + (mt_state[(i+1)%624] & 0x7FFFFFFF);
        mt_state[i] = mt_state[(i+397)%624] ^ (y>>1);
        if(y&1) {
            mt_state[i] = mt_state[i] ^ 0x9908b0df;
        }
    }
}

uint32_t rand() {
    if(mt_index == 0) {
        rng_generate();
    }
    
    uint32_t y = mt_state[mt_index];
    mt_index = (mt_index + 1) % 624;
    
    y ^= (y>>11);
    y ^= (y>>11) & 0x9d2c5680;
    y ^= (y>>15) & 0xefc60000;
    y ^= (y>>18);

    return y;
}