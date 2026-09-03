/*
 * Duration code tests.
 *
 * `ttl` and `validity` share one 8-bit encoding. The properties below are what
 * make that encoding safe to freeze, and each one is checked exhaustively
 * rather than sampled: the space is 256 codes, so there is no excuse for
 * testing it any other way.
 *
 *   INJECTIVE      every representable duration has exactly one code. Without
 *                  this a decoder has two answers and an encoder has two
 *                  choices, and canonical form is not optional in a protocol
 *                  whose whole premise is that bytes mean one thing.
 *   MONOTONIC      a larger code names a longer duration. A comparison on the
 *                  raw byte therefore orders durations correctly, which is what
 *                  a receiver doing policy on a TTL will do whether or not the
 *                  specification invites it.
 *   ROUND-TRIP     encoding a representable duration returns its own code.
 *   ROUNDS DOWN    never up, never nearest. These fields bound how long stale
 *                  information may be treated as current.
 *   REFUSES        a duration past the top is an error, not a clamp.
 */

#include "mcl/wire.h"

#include <stdio.h>

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do {                                      \
    ++tests_run;                                                   \
    if (!(cond)) {                                                 \
        ++tests_failed;                                            \
        printf("  FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
    }                                                              \
} while (0)

static void test_injective_and_monotonic(void)
{
    uint32_t previous = 0u;
    int i;

    printf("[TEST] every code names a distinct, increasing duration\n");

    /* Code 0 is a real value: zero seconds. It is not "unset". */
    CHECK(mcl_wire_duration_seconds(0u) == 0u, "code 0 is zero seconds");

    for (i = 1; i < 256; ++i) {
        const uint32_t seconds = mcl_wire_duration_seconds((uint8_t)i);

        if (seconds <= previous) {
            char message[96];
            (void)snprintf(message, sizeof(message),
                           "code %d gives %us, not more than code %d's %us",
                           i, (unsigned)seconds, i - 1, (unsigned)previous);
            CHECK(0, message);
        } else {
            CHECK(1, "strictly increasing");
        }
        previous = seconds;
    }

    CHECK(previous == MCL_WIRE_DURATION_MAX_SECONDS,
          "the last code is the documented maximum");
}

static void test_round_trip_is_exact(void)
{
    int i;

    printf("[TEST] a representable duration encodes back to its own code\n");

    for (i = 0; i < 256; ++i) {
        const uint32_t seconds = mcl_wire_duration_seconds((uint8_t)i);
        uint8_t code = 0xFFu;

        CHECK(mcl_wire_duration_encode(seconds, &code) == MCL_WIRE_OK,
              "a representable duration encodes");
        if (code != (uint8_t)i) {
            char message[96];
            (void)snprintf(message, sizeof(message),
                           "%us came back as code %u, not %d",
                           (unsigned)seconds, (unsigned)code, i);
            CHECK(0, message);
        } else {
            CHECK(1, "round trip exact");
        }
    }
}

/*
 * The direction of rounding is a safety property, not a nicety. These fields
 * bound how long a receiver may keep treating something as current, so an
 * encoder that rounded up would extend the life of stale information a little
 * further every time the value was re-encoded.
 */
static void test_rounds_down_never_up(void)
{
    uint32_t seconds;

    printf("[TEST] a duration between two codes rounds down\n");

    /* Every representable second, not a sample. This loop used to step by 997
     * above 4096 to stay quick, which left most of the range unvisited -- and
     * the range is exactly where the encoder's band-gap defect lived: the
     * value bands 16<<(e-1) .. 31<<(e-1) do not touch, and selecting a band by
     * "first top not below seconds" landed ABOVE the gap, turning four days
     * into six. A sampled loop can miss a gap. Half a million iterations of
     * two shifts costs nothing, so there is no reason to sample. */
    for (seconds = 0u; seconds <= MCL_WIRE_DURATION_MAX_SECONDS; ++seconds) {
        uint8_t code = 0u;
        uint32_t decoded;

        CHECK(mcl_wire_duration_encode(seconds, &code) == MCL_WIRE_OK,
              "an in-range duration encodes");
        decoded = mcl_wire_duration_seconds(code);

        if (decoded > seconds) {
            char message[96];
            (void)snprintf(message, sizeof(message),
                           "%us encoded to %us, which is longer",
                           (unsigned)seconds, (unsigned)decoded);
            CHECK(0, message);
            continue;
        }
        CHECK(1, "never rounds up");

        /* And it is the CLOSEST representable value at or below: the next code
         * up must be strictly greater than the requested duration, or the
         * encoder gave away resolution it had. */
        if (code < 255u) {
            const uint32_t next = mcl_wire_duration_seconds((uint8_t)(code + 1u));
            if (next <= seconds) {
                char message[96];
                (void)snprintf(message, sizeof(message),
                               "%us encoded to %us when %us was available",
                               (unsigned)seconds, (unsigned)decoded,
                               (unsigned)next);
                CHECK(0, message);
            } else {
                CHECK(1, "closest value at or below");
            }
        }
    }
}

static void test_refuses_rather_than_clamps(void)
{
    uint8_t code = 0u;

    printf("[TEST] a duration past the top is refused, not clamped\n");

    CHECK(mcl_wire_duration_encode(MCL_WIRE_DURATION_MAX_SECONDS, &code)
          == MCL_WIRE_OK, "the maximum itself encodes");
    CHECK(code == 0xFFu, "the maximum is the last code");

    CHECK(mcl_wire_duration_encode(MCL_WIRE_DURATION_MAX_SECONDS + 1u, &code)
          == MCL_WIRE_ERR_RANGE, "one second past the top is refused");
    CHECK(mcl_wire_duration_encode(2592000u, &code) == MCL_WIRE_ERR_RANGE,
          "thirty days is refused rather than becoming six");
    CHECK(mcl_wire_duration_encode(0xFFFFFFFFu, &code) == MCL_WIRE_ERR_RANGE,
          "an all-ones duration is refused");

    CHECK(mcl_wire_duration_encode(60u, NULL) == MCL_WIRE_ERR_INVALID_ARGUMENT,
          "a missing output pointer is an argument error");
}

/*
 * The durations a deployment will actually reach for. This is not a
 * completeness claim; it is a check that the chosen exponent split did not
 * quietly make an ordinary interval unrepresentable.
 */
static void test_useful_durations_are_close(void)
{
    static const uint32_t wanted[] = {
        1u, 5u, 10u, 15u, 30u, 60u, 120u, 300u, 600u, 900u,
        1800u, 3600u, 7200u, 28800u, 86400u
    };
    size_t i;

    printf("[TEST] ordinary intervals survive quantization\n");

    for (i = 0u; i < sizeof(wanted) / sizeof(wanted[0]); ++i) {
        uint8_t code = 0u;
        uint32_t got;
        uint32_t lost;

        CHECK(mcl_wire_duration_encode(wanted[i], &code) == MCL_WIRE_OK,
              "encodes");
        got = mcl_wire_duration_seconds(code);
        lost = wanted[i] - got;

        /*
         * Within one part in sixteen, above a minute. That bound is not
         * arbitrary: a band's step is 1<<(e-1) and its bottom is 16<<(e-1), so
         * the worst relative loss occurs just below a band bottom and
         * approaches 1/16. Measured across the whole range it peaks at 5.88%.
         * Below a minute the steps are one second and a relative bound says
         * nothing.
         */
        if (wanted[i] >= 60u && lost * 16u > wanted[i]) {
            char message[96];
            (void)snprintf(message, sizeof(message),
                           "%us quantized to %us, losing too much",
                           (unsigned)wanted[i], (unsigned)got);
            CHECK(0, message);
        } else {
            CHECK(1, "acceptable quantization");
        }
    }
}

int main(void)
{
    printf("MCL Wire duration code tests\n");
    printf("============================\n");

    test_injective_and_monotonic();
    test_round_trip_is_exact();
    test_rounds_down_never_up();
    test_refuses_rather_than_clamps();
    test_useful_durations_are_close();

    printf("\n%d checks, %d failed\n", tests_run, tests_failed);
    return (tests_failed == 0) ? 0 : 1;
}
