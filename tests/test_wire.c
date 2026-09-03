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

static uint32_t mcl_test_rng_state = 0x12345678u;

static uint32_t mcl_test_random(void)
{
    uint32_t value = mcl_test_rng_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    mcl_test_rng_state = value;
    return value;
}

static int16_t mcl_test_signed(unsigned bits)
{
    const uint32_t mask = (1u << bits) - 1u;
    const uint32_t raw = mcl_test_random() & mask;
    if ((raw & (1u << (bits - 1u))) != 0u) {
        return (int16_t)(raw - (1u << bits));
    }
    return (int16_t)raw;
}

static void mcl_test_headers(void)
{
    unsigned version;
    unsigned category;
    unsigned opcode;
    unsigned priority;
    unsigned extension;
    unsigned long count = 0ul;

    for (version = 0u; version < 16u; ++version) {
        for (category = 0u; category < 16u; ++category) {
            for (opcode = 0u; opcode < 32u; ++opcode) {
                for (priority = 0u; priority < 4u; ++priority) {
                    for (extension = 0u; extension < 2u; ++extension) {
                        mcl_wire_header_t input;
                        mcl_wire_header_t output;
                        uint8_t encoded[MCL_WIRE_COMMON_HEADER_SIZE];

                        input.major_version = (uint8_t)version;
                        input.category = (uint8_t)category;
                        input.opcode = (uint8_t)opcode;
                        input.priority = (uint8_t)priority;
                        input.extension_present = (uint8_t)extension;

                        CHECK_STATUS(mcl_wire_header_encode(&input, encoded), MCL_WIRE_OK);
                        CHECK_STATUS(mcl_wire_header_decode(encoded, &output), MCL_WIRE_OK);
                        CHECK_TRUE(memcmp(&input, &output, sizeof(input)) == 0);
                        ++count;
                    }
                }
            }
        }
    }

    CHECK_TRUE(count == 65536ul);
}

static void mcl_test_fill(mcl_wire_tier0_t *object, mcl_wire_kind_t kind)
{
    memset(object, 0, sizeof(*object));
    object->kind = kind;
    object->priority = (uint8_t)(mcl_test_random() & 3u);
    object->source_ref = mcl_test_random();

    switch (kind) {
    case MCL_WIRE_KIND_PRESENCE:
        object->body.presence.machine_class = (uint8_t)mcl_test_random();
        object->body.presence.capability_digest = mcl_test_random() & 0x00ffffffu;
        object->body.presence.ttl = (uint8_t)mcl_test_random();
        break;
    case MCL_WIRE_KIND_HAZARD:
        object->body.hazard.hazard_class = (uint8_t)mcl_test_random();
        object->body.hazard.severity = (uint8_t)(mcl_test_random() & 7u);
        object->body.hazard.confidence = (uint8_t)(mcl_test_random() & 127u);
        object->body.hazard.x = mcl_test_signed(12u);
        object->body.hazard.y = mcl_test_signed(12u);
        object->body.hazard.z = mcl_test_signed(10u);
        object->body.hazard.radius = (uint16_t)(mcl_test_random() & 1023u);
        object->body.hazard.ttl = (uint8_t)mcl_test_random();
        break;
    case MCL_WIRE_KIND_REQUEST:
        object->body.request.request_class = (uint8_t)mcl_test_random();
        object->body.request.target_ref = mcl_test_random();
        object->body.request.x = mcl_test_signed(12u);
        object->body.request.y = mcl_test_signed(12u);
        object->body.request.radius = (uint16_t)(mcl_test_random() & 1023u);
        object->body.request.ttl = (uint8_t)mcl_test_random();
        break;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        object->body.authority_claim.authority_class = (uint8_t)(mcl_test_random() & 63u);
        object->body.authority_claim.jurisdiction = (uint16_t)(mcl_test_random() & 4095u);
        object->body.authority_claim.credential_ref = mcl_test_random();
        object->body.authority_claim.validity = (uint8_t)mcl_test_random();
        break;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        object->body.degraded_state.affected_capability = (uint8_t)mcl_test_random();
        object->body.degraded_state.health = (uint8_t)(mcl_test_random() & 127u);
        object->body.degraded_state.severity = (uint8_t)(mcl_test_random() & 7u);
        object->body.degraded_state.ttl = (uint8_t)mcl_test_random();
        break;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        object->body.transport_offer.transport_id = (uint8_t)mcl_test_random();
        object->body.transport_offer.profile_id = (uint8_t)mcl_test_random();
        object->body.transport_offer.endpoint_token = mcl_test_random();
        object->body.transport_offer.validity = (uint8_t)mcl_test_random();
        break;
    case MCL_WIRE_KIND_TRANSPORT_ACCEPT:
        object->body.transport_accept.transport_id = (uint8_t)mcl_test_random();
        object->body.transport_accept.profile_id = (uint8_t)mcl_test_random();
        object->body.transport_accept.session_ref = mcl_test_random();
        break;
    default:
        exit(1);
    }
}

static int mcl_kind_has_padding(mcl_wire_kind_t kind)
{
    return kind == MCL_WIRE_KIND_HAZARD ||
           kind == MCL_WIRE_KIND_REQUEST ||
           kind == MCL_WIRE_KIND_AUTHORITY_CLAIM ||
           kind == MCL_WIRE_KIND_DEGRADED_STATE;
}

static void mcl_test_tier0_roundtrips(void)
{
    static const size_t expected_sizes[7] = {11u, 15u, 17u, 14u, 10u, 13u, 12u};
    unsigned kind_index;
    unsigned trial;
    uint8_t encoded[MCL_WIRE_TIER0_MAX_SIZE];

    for (kind_index = 0u; kind_index < 7u; ++kind_index) {
        const mcl_wire_kind_t kind = (mcl_wire_kind_t)kind_index;
        CHECK_TRUE(mcl_wire_tier0_encoded_size(kind) == expected_sizes[kind_index]);

        for (trial = 0u; trial < 20000u; ++trial) {
            mcl_wire_tier0_t input;
            mcl_wire_tier0_t output;
            size_t written = 0u;
            size_t consumed = 0u;

            mcl_test_fill(&input, kind);
            CHECK_STATUS(mcl_wire_tier0_encode(
                       &input, encoded, sizeof(encoded), &written), MCL_WIRE_OK);
            CHECK_TRUE(written == expected_sizes[kind_index]);
            CHECK_STATUS(mcl_wire_tier0_decode(
                       encoded, written, &output, &consumed), MCL_WIRE_OK);
            CHECK_TRUE(consumed == written);
            CHECK_TRUE(memcmp(&input, &output, sizeof(input)) == 0);

            CHECK_STATUS(mcl_wire_tier0_decode(
                       encoded, written - 1u, &output, &consumed), MCL_WIRE_ERR_TRUNCATED);
            CHECK_STATUS(mcl_wire_tier0_encode(
                       &input, encoded, written - 1u, &consumed), MCL_WIRE_ERR_BUFFER_TOO_SMALL);

            if (mcl_kind_has_padding(kind) != 0) {
                const uint8_t saved = encoded[written - 1u];
                encoded[written - 1u] = (uint8_t)(saved | 0x01u);
                CHECK_STATUS(mcl_wire_tier0_decode(
                           encoded, written, &output, &consumed), MCL_WIRE_ERR_NONCANONICAL);
                encoded[written - 1u] = saved;
            }
        }
    }
}

int main(void)
{
    mcl_test_headers();
    mcl_test_tier0_roundtrips();

    puts("headers: 65536 PASS");
    puts("Tier-0 randomized round trips: 120000 PASS");
    puts("sizes: PRESENCE=11 HAZARD=15 REQUEST=17 AUTHORITY_CLAIM=14 DEGRADED_STATE=10 TRANSPORT_OFFER=13");
    return 0;
}
