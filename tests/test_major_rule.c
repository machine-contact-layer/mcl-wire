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
 * Major 1 is DEFINED but NOT CUT. The decoder must still refuse it: accepting
 * frames under a major whose bodies are not frozen would be the cutting, and
 * that is gated on the Stable meanings closing.
 */
static void test_stable_major_is_not_yet_accepted_on_the_wire(void)
{
    uint8_t frame[MCL_WIRE_TIER0_MAX_SIZE];
    mcl_wire_tier0_t object;
    size_t consumed = 0u;
    size_t written = 0u;
    mcl_wire_tier0_t presence;

    printf("[TEST] the Stable major is defined but not yet accepted\n");

    /* A real, valid PRESENCE encoded at the experimental major. */
    presence.kind = MCL_WIRE_KIND_PRESENCE;
    presence.priority = 1u;
    presence.source_ref = 0x11223344u;
    presence.body.presence.machine_class = 1u;
    presence.body.presence.capability_tag = 0x000001u;
    presence.body.presence.ttl = 60u;

    CHECK(mcl_wire_tier0_encode(&presence, frame, sizeof(frame), &written) ==
              MCL_WIRE_OK,
          "PRESENCE encodes at the experimental major");
    CHECK(mcl_wire_tier0_decode(frame, written, &object, &consumed) ==
              MCL_WIRE_OK,
          "and decodes there");

    /* Rewrite the header's major nibble to the Stable major and try again.
     * PRESENCE is a Stable object, so this is refused for the version, not for
     * the object -- which is what "not yet cut" means. */
    frame[0] = (uint8_t)((MCL_WIRE_STABLE_MAJOR << 4) | (frame[0] & 0x0Fu));
    CHECK(mcl_wire_tier0_decode(frame, written, &object, &consumed) ==
              MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC,
          "major 1 is refused on the wire until it is cut");

    /* And the rule already says PRESENCE would be welcome there. */
    CHECK(mcl_wire_kind_allowed_at_major(MCL_WIRE_STABLE_MAJOR,
                                         MCL_WIRE_KIND_PRESENCE) == 1,
          "the rule and the decoder disagree only about timing");
}

int main(void)
{
    printf("=== MCL Wire: a Stable major carries only Stable semantics ===\n");

    test_experimental_major_carries_everything();
    test_stable_major_carries_only_the_three();
    test_exactly_three_are_stable();
    test_unassigned_majors_are_refused();
    test_unknown_kind_refused_at_every_major();
    test_stable_major_is_not_yet_accepted_on_the_wire();

    printf("\n%d checks, 0 failed.\n", g_checks);
    return 0;
}
