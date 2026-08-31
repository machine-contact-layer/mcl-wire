#include "mcl/wire.h"

#include <stdint.h>
#include <stdio.h>

static uint32_t mcl_fuzz_state = 0x9e3779b9u;

static uint32_t mcl_fuzz_random(void)
{
    uint32_t value = mcl_fuzz_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    mcl_fuzz_state = value;
    return value;
}

int main(void)
{
    uint8_t data[32];
    mcl_wire_tier0_t object;
    size_t consumed = 0u;
    size_t trial;

    for (trial = 0u; trial < 1000000u; ++trial) {
        const size_t length = (size_t)(mcl_fuzz_random() % 33u);
        size_t i;
        for (i = 0u; i < length; ++i) {
            data[i] = (uint8_t)mcl_fuzz_random();
        }
        (void)mcl_wire_tier0_decode(data, length, &object, &consumed);
    }

    puts("random decode inputs: 1000000 PASS");
    return 0;
}
