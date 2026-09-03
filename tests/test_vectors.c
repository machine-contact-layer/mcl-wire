#include "mcl/wire.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_STATUS(call, expected) do { \
    const mcl_wire_status_t mcl_st__ = (call); \
    if (mcl_st__ != (expected)) { \
        fprintf(stderr, "FAIL at %s:%d: %s returned %d, expected %d\n", \
                __FILE__, __LINE__, #call, (int)mcl_st__, (int)(expected)); \
        exit(1); \
    } \
} while (0)

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL at %s:%d: (%s) is false\n", \
                __FILE__, __LINE__, #expr); \
        exit(1); \
    } \
} while (0)

typedef struct {
    const char *name;
    mcl_wire_tier0_t object;
    const uint8_t *bytes;
    size_t size;
} vector_t;

static const uint8_t presence_bytes[] = {0x00u,0x02u,0xaau,0x00u,0x00u,0x01u,0x01u,0x00u,0x00u,0x1fu,0x79u};
static const uint8_t hazard_bytes[] = {0x03u,0x0eu,0xa1u,0x00u,0x00u,0x01u,0x01u,0x7fu,0x00u,0x68u,0x01u,0x00u,0x00u,0x28u,0x78u};
static const uint8_t request_bytes[] = {0x05u,0x0cu,0xa1u,0x00u,0x00u,0x01u,0x02u,0xb1u,0x00u,0x00u,0x02u,0x01u,0x00u,0x00u,0x03u,0x0eu,0x40u};
static const uint8_t authority_bytes[] = {0x01u,0x0cu,0xa3u,0x00u,0x00u,0x01u,0x04u,0x59u,0x00u,0x3cu,0x28u,0xc0u,0x6fu,0xc0u};
static const uint8_t degraded_bytes[] = {0x06u,0x0cu,0xacu,0x00u,0x00u,0x01u,0x02u,0x96u,0xe4u,0xc0u};
static const uint8_t transport_bytes[] = {0x08u,0x02u,0xaeu,0x00u,0x00u,0x01u,0x4du,0x19u,0x42u,0x01u,0x02u,0x01u,0xd0u,0x0du,0x00u,0x01u,0x9fu};
static const uint8_t transport_accept_bytes[] = {0x08u,0x0au,0xb0u,0x00u,0x00u,0x02u,0x4du,0x19u,0x42u,0x01u,0x02u,0x01u,0x5eu,0x55u,0x10u,0xc7u};

static const vector_t vectors[] = {
    {"PRESENCE", { .kind=MCL_WIRE_KIND_PRESENCE, .priority=1u, .source_ref=UINT32_C(2852126721), .body.presence={1u,UINT32_C(31),121u}}, presence_bytes, sizeof(presence_bytes)},
    {"HAZARD", { .kind=MCL_WIRE_KIND_HAZARD, .priority=3u, .source_ref=UINT32_C(2701131777), .body.hazard={1u,3u,124u,26,4,0,10u,30u}}, hazard_bytes, sizeof(hazard_bytes)},
    {"REQUEST", { .kind=MCL_WIRE_KIND_REQUEST, .priority=2u, .source_ref=UINT32_C(2701131777), .body.request={2u,UINT32_C(2969567234),16,0,12u,57u}}, request_bytes, sizeof(request_bytes)},
    {"AUTHORITY_CLAIM", { .kind=MCL_WIRE_KIND_AUTHORITY_CLAIM, .priority=2u, .source_ref=UINT32_C(2734686209), .body.authority_claim={1u,356u,UINT32_C(15770369),191u}}, authority_bytes, sizeof(authority_bytes)},
    {"DEGRADED_STATE", { .kind=MCL_WIRE_KIND_DEGRADED_STATE, .priority=2u, .source_ref=UINT32_C(2885681153), .body.degraded_state={2u,75u,3u,147u}}, degraded_bytes, sizeof(degraded_bytes)},
    {"TRANSPORT_OFFER", { .kind=MCL_WIRE_KIND_TRANSPORT_OFFER, .priority=1u, .source_ref=UINT32_C(2919235585), .body.transport_offer={UINT32_C(0x4D194201),2u,1u,UINT32_C(0xD00D0001),159u}}, transport_bytes, sizeof(transport_bytes)},
    {"TRANSPORT_ACCEPT", { .kind=MCL_WIRE_KIND_TRANSPORT_ACCEPT, .priority=1u, .source_ref=UINT32_C(0xB0000002), .body.transport_accept={UINT32_C(0x4D194201),2u,1u,UINT32_C(0x5E5510C7)}}, transport_accept_bytes, sizeof(transport_accept_bytes)}
};

int main(void)
{
    size_t i;
    for (i = 0u; i < sizeof(vectors)/sizeof(vectors[0]); ++i) {
        uint8_t encoded[MCL_WIRE_TIER0_MAX_SIZE];
        mcl_wire_tier0_t decoded;
        size_t written = 0u, consumed = 0u;
        CHECK_STATUS(mcl_wire_tier0_encode(&vectors[i].object, encoded, sizeof(encoded), &written), MCL_WIRE_OK);
        CHECK_TRUE(written == vectors[i].size);
        CHECK_TRUE(memcmp(encoded, vectors[i].bytes, written) == 0);
        CHECK_STATUS(mcl_wire_tier0_decode(vectors[i].bytes, vectors[i].size, &decoded, &consumed), MCL_WIRE_OK);
        CHECK_TRUE(consumed == vectors[i].size);
        CHECK_TRUE(memcmp(&decoded, &vectors[i].object, sizeof(decoded)) == 0);
        printf("%s %zu PASS\n", vectors[i].name, vectors[i].size);
    }
    return 0;
}
