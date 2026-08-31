#ifndef MCL_WIRE_H
#define MCL_WIRE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCL_WIRE_EXPERIMENTAL_MAJOR 0u
#define MCL_WIRE_COMMON_HEADER_SIZE 2u
#define MCL_WIRE_TIER0_MAX_SIZE 17u

typedef int32_t mcl_status_t;
enum {
    MCL_OK = 0,
    MCL_ERR_INVALID_ARGUMENT = 1,
    MCL_ERR_BUFFER_TOO_SMALL = 2,
    MCL_ERR_RANGE = 3,
    MCL_ERR_TRUNCATED = 4,
    MCL_ERR_UNSUPPORTED_SEMANTIC = 5,
    MCL_ERR_NONCANONICAL = 6
};

typedef uint8_t mcl_wire_kind_t;
enum {
    MCL_WIRE_KIND_PRESENCE = 0u,
    MCL_WIRE_KIND_HAZARD = 1u,
    MCL_WIRE_KIND_REQUEST = 2u,
    MCL_WIRE_KIND_AUTHORITY_CLAIM = 3u,
    MCL_WIRE_KIND_DEGRADED_STATE = 4u,
    MCL_WIRE_KIND_TRANSPORT_OFFER = 5u
};

typedef struct {
    uint8_t major_version;
    uint8_t category;
    uint8_t opcode;
    uint8_t priority;
    uint8_t extension_present;
} mcl_wire_header_t;

typedef struct {
    uint8_t machine_class;
    uint32_t capability_digest;
    uint8_t ttl;
} mcl_wire_presence_t;

typedef struct {
    uint8_t hazard_class;
    uint8_t severity;
    uint8_t confidence;
    int16_t x;
    int16_t y;
    int16_t z;
    uint16_t radius;
    uint8_t ttl;
} mcl_wire_hazard_t;

typedef struct {
    uint8_t request_class;
    uint32_t target_ref;
    int16_t x;
    int16_t y;
    uint16_t radius;
    uint8_t ttl;
} mcl_wire_request_t;

typedef struct {
    uint8_t authority_class;
    uint16_t jurisdiction;
    uint32_t credential_ref;
    uint8_t validity;
} mcl_wire_authority_claim_t;

typedef struct {
    uint8_t affected_capability;
    uint8_t health;
    uint8_t severity;
    uint8_t ttl;
} mcl_wire_degraded_state_t;

typedef struct {
    uint8_t transport_id;
    uint8_t profile_id;
    uint32_t endpoint_token;
    uint8_t validity;
} mcl_wire_transport_offer_t;

typedef struct {
    mcl_wire_kind_t kind;
    uint8_t priority;
    uint32_t source_ref;
    union {
        mcl_wire_presence_t presence;
        mcl_wire_hazard_t hazard;
        mcl_wire_request_t request;
        mcl_wire_authority_claim_t authority_claim;
        mcl_wire_degraded_state_t degraded_state;
        mcl_wire_transport_offer_t transport_offer;
    } body;
} mcl_wire_tier0_t;

mcl_status_t mcl_wire_header_encode(const mcl_wire_header_t *header, uint8_t out[MCL_WIRE_COMMON_HEADER_SIZE]);
mcl_status_t mcl_wire_header_decode(const uint8_t in[MCL_WIRE_COMMON_HEADER_SIZE], mcl_wire_header_t *header);

size_t mcl_wire_tier0_encoded_size(mcl_wire_kind_t kind);

mcl_status_t mcl_wire_tier0_encode(
    const mcl_wire_tier0_t *object,
    uint8_t *out,
    size_t out_capacity,
    size_t *written);

mcl_status_t mcl_wire_tier0_decode(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif
