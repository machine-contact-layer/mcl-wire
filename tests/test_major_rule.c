/*
 * A Stable major carries only Stable semantics.
 *
 * V1_SCOPE.md section 4.7. This rule exists in code and in test BEFORE major 1
 * is cut and before any major-1 vector is generated, because a vector frozen
 * under an ambiguous rule fixes the ambiguity into the artifacts that define
 * the release. The gap it closes was silence, not a wrong decision: the scope
 * made Stable layouts major 1 and left four objects Candidate without ever
 * saying which major the Candidate bytes travel under.
 */

#include "mcl/wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0;

#define CHECK(expr, what) do { \
    ++g_checks; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, (what)); \
        exit(1); \
    } \
} while (0)

static void test_experimental_major_carries_everything(void)
{
    unsigned k;

    printf("[TEST] the experimental major carries every implemented object\n");

    for (k = 0u; k <= (unsigned)MCL_WIRE_KIND_TRANSPORT_ACCEPT; ++k) {
        CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_EXPERIMENTAL_MAJOR,
                                             (mcl_wire_kind_t)k) == 1,
              "every implemented kind is legal at major 0");
    }
}

static void test_stable_major_carries_only_the_three(void)
{
    printf("[TEST] the Stable major carries the three Stable objects only\n");

    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_PRESENCE) == 1,
          "PRESENCE is Stable");
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_TRANSPORT_OFFER) == 1,
          "TRANSPORT_OFFER is Stable");
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_TRANSPORT_ACCEPT) == 1,
          "TRANSPORT_ACCEPT is Stable");

    /* The four Candidate objects. Their layouts exist and their vectors pass;
     * that is precisely why the refusal has to be explicit. */
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_HAZARD) == 0,
          "HAZARD is Candidate and refused at the Stable major");
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_REQUEST) == 0,
          "REQUEST is Candidate and refused at the Stable major");
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_AUTHORITY_CLAIM) == 0,
          "AUTHORITY_CLAIM is Candidate and refused at the Stable major");
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_DEGRADED_STATE) == 0,
          "DEGRADED_STATE is Candidate and refused at the Stable major");
}

static void test_exactly_three_are_stable(void)
{
    unsigned k;
    int stable = 0;

    printf("[TEST] exactly three objects are Stable, counted not assumed\n");

    for (k = 0u; k <= (unsigned)MCL_WIRE_KIND_TRANSPORT_ACCEPT; ++k) {
        stable += mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                                 (mcl_wire_kind_t)k);
    }
    /* If a fourth object is ever promoted, this fails and forces the promotion
     * to be a deliberate act recorded in V1_SCOPE rather than a side effect. */
    CHECK(stable == 3, "three of seven objects are Stable at major 1");
}

static void test_unassigned_majors_are_refused(void)
{
    unsigned major;

    printf("[TEST] every unassigned major refuses every object\n");

    for (major = 2u; major <= 15u; ++major) {
        unsigned k;
        for (k = 0u; k <= (unsigned)MCL_WIRE_KIND_TRANSPORT_ACCEPT; ++k) {
            CHECK(mcl_wire_kind_allowed_at_major((uint8_t)major,
                                                 (mcl_wire_kind_t)k) == 0,
                  "an unassigned major carries nothing");
        }
    }
}

static void test_unknown_kind_refused_at_every_major(void)
{
    unsigned major;

    printf("[TEST] an unknown object kind is refused at every major\n");

    for (major = 0u; major <= 15u; ++major) {
        CHECK(mcl_wire_kind_allowed_at_major(
                  (uint8_t)major,
                  (mcl_wire_kind_t)(MCL_WIRE_KIND_TRANSPORT_ACCEPT + 1u)) == 0,
              "unknown is refused, never guessed");
        CHECK(mcl_wire_kind_allowed_at_major((uint8_t)major,
                                             (mcl_wire_kind_t)255u) == 0,
              "unknown is refused, never guessed");
    }
}

/*
 * Major 1 is CUT. This test previously asserted the opposite -- that a
 * major-1 frame was refused -- because accepting frames under a major whose
 * bodies were not frozen would have BEEN the cutting. The bodies are frozen
 * now, so the assertion is inverted rather than deleted: the record of what
 * the rule was before the cut lives in the commit history, and what it is
 * after lives here.
 */
static void test_stable_major_round_trips(void)
{
    uint8_t frame[MCL_WIRE_TIER0_MAX_SIZE];
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t presence;
    size_t consumed = 0u;
    size_t written = 0u;

    printf("[TEST] the Stable major round trips, and is 10 bytes\n");

    presence.kind = MCL_WIRE_KIND_PRESENCE;
    presence.priority = 1u;
    presence.source_ref = 0x11223344u;
    presence.body.presence.machine_class = 7u;   /* must NOT reach the wire */
    presence.body.presence.capability_tag = 0x00ABCDu;
    presence.body.presence.ttl = 60u;

    CHECK(mcl_wire_tier0_encode_at_major(MCL_WIRE_STABLE_MAJOR, &presence,
                                         frame, sizeof(frame), &written) ==
              MCL_WIRE_OK,
          "PRESENCE encodes at the Stable major");
    CHECK(written == 10u, "and is 10 bytes");
    CHECK((frame[0] >> 4) == MCL_WIRE_STABLE_MAJOR,
          "and the header names major 1");

    CHECK(mcl_wire_tier0_decode(frame, written, &object, &consumed) ==
              MCL_WIRE_OK,
          "and decodes");
    CHECK(consumed == written, "consuming every byte");
    CHECK(object.source_ref == presence.source_ref, "source_ref survives");
    CHECK(object.body.presence.capability_tag == 0x00ABCDu,
          "capability_tag survives");
    CHECK(object.body.presence.ttl == 60u, "ttl survives");
    /* machine_class was never written, so the decoder leaves it zero. Absent
     * is not "class 0" -- there is no class 0 -- and a caller must not read it
     * at this major. */
    CHECK(object.body.presence.machine_class == 0u,
          "machine_class did not reach the wire");

    /* The default entry point still emits major 0, unchanged. That is the
     * source-compatibility promise: an existing caller's bytes do not move. */
    CHECK(mcl_wire_tier0_encode(&presence, frame, sizeof(frame), &written) ==
              MCL_WIRE_OK,
          "the default entry point still works");
    CHECK(written == 11u, "and still emits an 11-byte major-0 PRESENCE");
    CHECK((frame[0] >> 4) == MCL_WIRE_EXPERIMENTAL_MAJOR,
          "naming major 0");
}

/*
 * A Candidate object offered at the Stable major is refused at BOTH ends.
 */
static void test_candidate_refused_at_stable_major(void)
{
    uint8_t frame[MCL_WIRE_TIER0_MAX_SIZE];
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t hazard;
    size_t written = 0u;
    size_t consumed = 0u;

    printf("[TEST] a Candidate object cannot travel at the Stable major\n");

    memset(&hazard, 0, sizeof(hazard));
    hazard.kind = MCL_WIRE_KIND_HAZARD;
    hazard.priority = 2u;
    hazard.source_ref = 0x55667788u;

    CHECK(mcl_wire_tier0_encode_at_major(MCL_WIRE_STABLE_MAJOR, &hazard,
                                         frame, sizeof(frame), &written) ==
              MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC,
          "the encoder refuses to produce one");

    /* And a peer that produced one anyway is refused on receipt: encode a
     * legal major-0 HAZARD, then rewrite the major nibble. */
    CHECK(mcl_wire_tier0_encode(&hazard, frame, sizeof(frame), &written) ==
              MCL_WIRE_OK,
          "HAZARD encodes at the experimental major");
    frame[0] = (uint8_t)((MCL_WIRE_STABLE_MAJOR << 4) | (frame[0] & 0x0Fu));
    CHECK(mcl_wire_tier0_decode(frame, written, &object, &consumed) ==
              MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC,
          "and the decoder refuses it, never guessing a layout");
}

static void test_major_1_presence_drops_machine_class(void)
{
    unsigned k;

    printf("[TEST] major-1 PRESENCE is 10 bytes; major-0 stays 11\n");

    CHECK(mcl_wire_tier0_encoded_size(MCL_WIRE_KIND_PRESENCE) == 11u,
          "major 0 keeps its 11-byte PRESENCE, permanently");
    CHECK(mcl_wire_tier0_encoded_size_at_major(
              MCL_WIRE_EXPERIMENTAL_MAJOR, MCL_WIRE_KIND_PRESENCE) == 11u,
          "and the major-aware size agrees for major 0");
    CHECK(mcl_wire_tier0_encoded_size_at_major(
              MCL_WIRE_STABLE_MAJOR, MCL_WIRE_KIND_PRESENCE) == 10u,
          "major 1 drops machine_class: exactly one byte smaller");

    /* The other two Stable objects are unchanged, so the difference is
     * specific to the field that was removed rather than a general shift. */
    CHECK(mcl_wire_tier0_encoded_size_at_major(
              MCL_WIRE_STABLE_MAJOR, MCL_WIRE_KIND_TRANSPORT_OFFER) ==
          mcl_wire_tier0_encoded_size(MCL_WIRE_KIND_TRANSPORT_OFFER),
          "TRANSPORT_OFFER is the same size at both majors");
    CHECK(mcl_wire_tier0_encoded_size_at_major(
              MCL_WIRE_STABLE_MAJOR, MCL_WIRE_KIND_TRANSPORT_ACCEPT) ==
          mcl_wire_tier0_encoded_size(MCL_WIRE_KIND_TRANSPORT_ACCEPT),
          "TRANSPORT_ACCEPT is the same size at both majors");

    /* A Candidate object has no size at the Stable major, because it is not
     * carried there at all. */
    CHECK(mcl_wire_tier0_encoded_size_at_major(
              MCL_WIRE_STABLE_MAJOR, MCL_WIRE_KIND_HAZARD) == 0u,
          "a Candidate object has no major-1 size");

    /* Every unassigned major carries nothing, so every size there is zero. */
    for (k = 2u; k <= 15u; ++k) {
        CHECK(mcl_wire_tier0_encoded_size_at_major(
                  (uint8_t)k, MCL_WIRE_KIND_PRESENCE) == 0u,
              "an unassigned major has no sizes");
    }
}

int main(void)
{
    printf("=== MCL Wire: a Stable major carries only Stable semantics ===\n");

    test_experimental_major_carries_everything();
    test_stable_major_carries_only_the_three();
    test_exactly_three_are_stable();
    test_unassigned_majors_are_refused();
    test_unknown_kind_refused_at_every_major();
    test_stable_major_round_trips();
    test_candidate_refused_at_stable_major();
    test_major_1_presence_drops_machine_class();

    printf("\n%d checks, 0 failed.\n", g_checks);
    return 0;
}
