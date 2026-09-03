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

typedef int32_t mcl_wire_status_t;
enum {
    MCL_WIRE_OK = 0,
    MCL_WIRE_ERR_INVALID_ARGUMENT = 1,
    MCL_WIRE_ERR_BUFFER_TOO_SMALL = 2,
    MCL_WIRE_ERR_RANGE = 3,
    MCL_WIRE_ERR_TRUNCATED = 4,
    MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC = 5,
    MCL_WIRE_ERR_NONCANONICAL = 6
};

typedef uint8_t mcl_wire_kind_t;
enum {
    MCL_WIRE_KIND_PRESENCE = 0u,
    MCL_WIRE_KIND_HAZARD = 1u,
    MCL_WIRE_KIND_REQUEST = 2u,
    MCL_WIRE_KIND_AUTHORITY_CLAIM = 3u,
    MCL_WIRE_KIND_DEGRADED_STATE = 4u,
    MCL_WIRE_KIND_TRANSPORT_OFFER = 5u,
    MCL_WIRE_KIND_TRANSPORT_ACCEPT = 6u
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

/*
 * migration_ref correlates one transport-change transaction, and nothing else.
 *
 * Without it a delayed acceptance from an abandoned offer is indistinguishable
 * from the acceptance of the current one, because transport_id and profile_id
 * are usually identical across a retry. Mature request/response protocols carry
 * a dedicated correlator for exactly this reason: CoAP's Token, which the
 * responder echoes, and MCTP's message tag, which is kept separate from
 * endpoint addressing rather than overloaded onto it.
 *
 * It is NOT identity, NOT authorization, NOT a session, and NOT a secret. Zero
 * is reserved so an uninitialised field never names a live transaction.
 */
typedef struct {
    uint32_t migration_ref;
    uint8_t transport_id;
    uint8_t profile_id;
    uint32_t endpoint_token;
    uint8_t validity;
} mcl_wire_transport_offer_t;

/*
 * TRANSPORT_ACCEPT is the other half of a transport change. Without it the
 * offering peer never learns which transport was selected, so two independent
 * implementations cannot complete a migration -- which is exactly the test for
 * whether something belongs in the specification.
 *
 * session_ref carries the accepting peer's chosen correlation reference for the
 * continued contact. It is a CORRELATION REFERENCE AND NOT A SECRET: it travels
 * in the clear over an observable medium, so anyone in range can read it and
 * anyone can quote it back. It lets an honest peer recognise a continuing
 * contact; it establishes nothing whatever about who the peer is.
 */
typedef struct {
    uint32_t migration_ref;
    uint8_t transport_id;
    uint8_t profile_id;
    uint32_t session_ref;
} mcl_wire_transport_accept_t;

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
        mcl_wire_transport_accept_t transport_accept;
    } body;
} mcl_wire_tier0_t;

/* ============================================================
 * DURATION CODES
 *
 * `ttl` and `validity` are both 8-bit duration fields, and both had no unit and
 * no scale: eight bits meaning "some duration". A receiver was forbidden to
 * infer seconds, which made them undecidable rather than merely imprecise.
 *
 * ONE ENCODING, USED BY BOTH. Two duration fields with different scales in one
 * protocol is a defect waiting to be written.
 *
 * Layout: a 4-bit exponent in the high nibble, a 4-bit mantissa in the low.
 *
 *     e == 0   seconds = m                      0..15, in 1 s steps
 *     e >= 1   seconds = (16 + m) << (e - 1)    16..507904
 *
 * The implied leading one above e == 0 is what makes the mapping INJECTIVE:
 * every representable duration has exactly one code, so a decoder never has two
 * answers and an encoder never has two choices. A plain `m << e` would encode
 * 64 seconds five different ways, and canonical form is not optional here.
 *
 * The range runs to 507904 s, a little under six days, and quantization costs
 * at most 5.88% of a requested duration above one minute -- measured across the
 * whole range, not estimated. Both ends matter: a field that cannot express
 * "for the rest of the shift" gets worked around, and a work-around is a second
 * scale.
 *
 * The bands do not touch. Band `e` covers 16<<(e-1) .. 31<<(e-1), so between
 * one band's top and the next band's bottom sits a gap of one step; 253952 and
 * 262144 are both representable and nothing between them is. A duration landing
 * in a gap rounds down to the band below.
 *
 * THERE IS NO CODE FOR "FOREVER", deliberately. An unbounded lifetime is the
 * opposite of what a TTL is for, and a sentinel meaning it would be reached for
 * by every sender that did not want to think about expiry.
 *
 * Zero is a real value meaning zero seconds: decode the object, then stop
 * treating it as current. It does not mean "unset".
 * ============================================================ */

#define MCL_WIRE_DURATION_MAX_SECONDS 507904u

/* Seconds named by a duration code. All 256 codes are valid, so this is total
 * and needs no status. */
uint32_t mcl_wire_duration_seconds(uint8_t code);

/*
 * The code for a duration, ROUNDED DOWN to the nearest representable value.
 *
 * Down, never up and never nearest. These fields bound how long a receiver may
 * keep treating something as current, so rounding up would extend the life of
 * stale information by up to one quantum every time it was re-encoded. Rounding
 * down can only shorten a bound, which is the safe direction.
 *
 * A duration above MCL_WIRE_DURATION_MAX_SECONDS is REFUSED with
 * MCL_WIRE_ERR_RANGE rather than clamped. Silently turning thirty days into six
 * is the kind of rounding that gets discovered in the field.
 */
mcl_wire_status_t mcl_wire_duration_encode(uint32_t seconds, uint8_t *code);

mcl_wire_status_t mcl_wire_header_encode(const mcl_wire_header_t *header, uint8_t out[MCL_WIRE_COMMON_HEADER_SIZE]);
mcl_wire_status_t mcl_wire_header_decode(const uint8_t in[MCL_WIRE_COMMON_HEADER_SIZE], mcl_wire_header_t *header);

size_t mcl_wire_tier0_encoded_size(mcl_wire_kind_t kind);

mcl_wire_status_t mcl_wire_tier0_encode(
    const mcl_wire_tier0_t *object,
    uint8_t *out,
    size_t out_capacity,
    size_t *written);

/*
 * Decode a Tier-0 object.
 *
 * Refuses an object whose header sets extension_present, because this decoder
 * cannot read extensions and an extension may be critical -- the sender saying
 * the object must not be acted on without it. Returning the body and
 * discarding the rest would turn "you must understand this" into "you may
 * ignore this". To read extensions, use mcl_wire_tier0_decode_ext in
 * mcl/extension.h.
 */
mcl_wire_status_t mcl_wire_tier0_decode(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    size_t *consumed);

/*
 * Decode the fixed Tier-0 body ONLY, ignoring extension_present entirely.
 *
 * Shared between mcl_wire_tier0_decode and the extension-aware decoder, which
 * is the reason it is public rather than static: extension handling lives in
 * a separate translation unit so that a build with no use for extensions does
 * not link them.
 *
 * `consumed` reports the fixed body length. Any extension block begins there.
 * A caller using this directly is responsible for the extension_present bit,
 * and gets no protection from a critical extension. Prefer the two functions
 * above.
 */
mcl_wire_status_t mcl_wire_tier0_decode_body(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif
