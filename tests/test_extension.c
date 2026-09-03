#include "mcl/extension.h"

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

static uint32_t state = 0x31415926u;

static uint32_t rnd(void)
{
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

static void test_uvarint(void)
{
    unsigned i;
    for (i = 0u; i < 10000u; ++i) {
        const uint32_t value = rnd() & 0x0fffffffu;
        uint8_t encoded[5];
        uint32_t decoded = 0u;
        size_t written = 0u;
        size_t consumed = 0u;
        CHECK_STATUS(mcl_wire_uvarint_encode(value, encoded, sizeof(encoded), &written), MCL_WIRE_OK);
        CHECK_STATUS(mcl_wire_uvarint_decode(encoded, written, &decoded, &consumed), MCL_WIRE_OK);
        CHECK_TRUE(decoded == value);
        CHECK_TRUE(consumed == written);
    }

    {
        const uint8_t noncanonical[2] = {0x80u, 0x00u};
        uint32_t value;
        size_t consumed;
        CHECK_STATUS(mcl_wire_uvarint_decode(noncanonical, sizeof(noncanonical), &value, &consumed), MCL_WIRE_ERR_NONCANONICAL);
    }
}

static void test_extension_blocks(void)
{
    unsigned trial;
    for (trial = 0u; trial < 2000u; ++trial) {
        mcl_wire_extension_t input[12];
        uint8_t values[12][40];
        uint8_t encoded[1024];
        size_t count = (size_t)(rnd() % 12u);
        size_t written = 0u;
        size_t i;
        uint32_t next_id = 1u;
        mcl_wire_extension_reader_t reader;

        for (i = 0u; i < count; ++i) {
            size_t j;
            const size_t value_size = (size_t)(rnd() % 40u);
            next_id += 1u + (rnd() % 400u);
            input[i].id = next_id;
            input[i].critical = (uint8_t)(rnd() & 1u);
            input[i].value = values[i];
            input[i].value_size = value_size;
            for (j = 0u; j < value_size; ++j) {
                values[i][j] = (uint8_t)rnd();
            }
        }

        CHECK_STATUS(mcl_wire_extensions_encode(input, count, encoded, sizeof(encoded), &written), MCL_WIRE_OK);
        mcl_wire_extension_reader_init(&reader, encoded, written);

        for (i = 0u; i < count; ++i) {
            mcl_wire_extension_t output;
            uint8_t has_extension = 0u;
            CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &output, &has_extension), MCL_WIRE_OK);
            CHECK_TRUE(has_extension == 1u);
            CHECK_TRUE(output.id == input[i].id);
            CHECK_TRUE(output.critical == input[i].critical);
            CHECK_TRUE(output.value_size == input[i].value_size);
            CHECK_TRUE(memcmp(output.value, input[i].value, output.value_size) == 0);
        }

        {
            mcl_wire_extension_t output;
            uint8_t has_extension = 1u;
            CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &output, &has_extension), MCL_WIRE_OK);
            CHECK_TRUE(has_extension == 0u);
        }
    }
}

static void test_negative_cases(void)
{
    uint8_t encoded[64];
    size_t written = 0u;
    const uint8_t a[] = {'a'};
    const uint8_t b[] = {'b'};
    mcl_wire_extension_t duplicate[2] = {
        {3u, 0u, a, sizeof(a)},
        {3u, 0u, b, sizeof(b)}
    };
    CHECK_STATUS(mcl_wire_extensions_encode(duplicate, 2u, encoded, sizeof(encoded), &written), MCL_WIRE_ERR_NONCANONICAL);

    {
        const uint8_t truncated[] = {0x02u, 0x05u, 0x01u};
        mcl_wire_extension_reader_t reader;
        mcl_wire_extension_t extension;
        uint8_t has_extension;
        mcl_wire_extension_reader_init(&reader, truncated, sizeof(truncated));
        CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &extension, &has_extension), MCL_WIRE_ERR_TRUNCATED);
    }

    {
        const uint8_t optional_value[] = {'f','u','t','u','r','e'};
        const mcl_wire_extension_t optional = {7u, 0u, optional_value, sizeof(optional_value)};
        mcl_wire_extension_reader_t reader;
        mcl_wire_extension_t decoded;
        uint8_t has_extension;
        CHECK_STATUS(mcl_wire_extensions_encode(&optional, 1u, encoded, sizeof(encoded), &written), MCL_WIRE_OK);
        mcl_wire_extension_reader_init(&reader, encoded, written);
        CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &decoded, &has_extension), MCL_WIRE_OK);
        CHECK_TRUE(has_extension == 1u && decoded.id == 7u && decoded.critical == 0u);
    }

    {
        const uint8_t critical_value[] = {'m','u','s','t'};
        const mcl_wire_extension_t critical = {7u, 1u, critical_value, sizeof(critical_value)};
        mcl_wire_extension_reader_t reader;
        mcl_wire_extension_t decoded;
        uint8_t has_extension;
        CHECK_STATUS(mcl_wire_extensions_encode(&critical, 1u, encoded, sizeof(encoded), &written), MCL_WIRE_OK);
        mcl_wire_extension_reader_init(&reader, encoded, written);
        CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &decoded, &has_extension), MCL_WIRE_OK);
        CHECK_TRUE(has_extension == 1u && decoded.id == 7u && decoded.critical == 1u);
    }
}

/* ============================================================
 * Extensions reachable from the Tier-0 API.
 *
 * The framework above was implemented and untestable through the normal
 * encode/decode path for most of this project's life: the Tier-0 encoder
 * hard-set extension_present to 0 and the decoder refused any object that set
 * it. These tests exist because "the mechanism exists" and "a caller can use
 * it" turned out to be different claims.
 * ============================================================ */

static void make_presence(mcl_wire_tier0_t *object)
{
    memset(object, 0, sizeof(*object));
    object->kind = MCL_WIRE_KIND_PRESENCE;
    object->priority = 1u;
    object->source_ref = 0x0000A17Cu;
    object->body.presence.machine_class = 3u;
    object->body.presence.capability_digest = 0x00ABCDEFu;
    object->body.presence.ttl = 60u;
}

static void test_tier0_without_extensions_is_unchanged(void)
{
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t plain;
    mcl_wire_tier0_t decoded;
    mcl_wire_extension_reader_t reader;
    uint8_t with_api[MCL_WIRE_TIER0_EXT_MAX_SIZE];
    uint8_t without[MCL_WIRE_TIER0_MAX_SIZE];
    size_t a = 0u;
    size_t b = 0u;
    size_t consumed = 0u;
    size_t i;
    uint8_t has_extension = 1u;
    mcl_wire_extension_t extension;

    make_presence(&object);

    /* Passing no extensions must produce byte-identical output to the plain
     * encoder. Otherwise adding the capability silently changes every object
     * already in the vectors. */
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, NULL, 0u, with_api,
                                           sizeof(with_api), &a), MCL_WIRE_OK);
    CHECK_STATUS(mcl_wire_tier0_encode(&object, without, sizeof(without), &b),
                 MCL_WIRE_OK);
    CHECK_TRUE(a == b);
    for (i = 0u; i < a; ++i) {
        CHECK_TRUE(with_api[i] == without[i]);
    }
    CHECK_TRUE((with_api[1] & 0x01u) == 0u);

    /* And the plain decoder still reads it. */
    CHECK_STATUS(mcl_wire_tier0_decode(with_api, a, &plain, &consumed),
                 MCL_WIRE_OK);
    CHECK_TRUE(consumed == a);

    /* The extension-aware decoder reads it too, with an empty reader. */
    CHECK_STATUS(mcl_wire_tier0_decode_ext(with_api, a, &decoded, &reader,
                                           &consumed), MCL_WIRE_OK);
    CHECK_TRUE(consumed == a);
    CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &extension,
                                                &has_extension), MCL_WIRE_OK);
    CHECK_TRUE(has_extension == 0u);
}

static void test_tier0_round_trip_with_extensions(void)
{
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t decoded;
    mcl_wire_extension_t extensions[2];
    mcl_wire_extension_reader_t reader;
    mcl_wire_extension_t read_back;
    static const uint8_t value_a[] = {0xDEu, 0xADu};
    static const uint8_t value_b[] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u};
    uint8_t buffer[MCL_WIRE_TIER0_EXT_MAX_SIZE];
    size_t written = 0u;
    size_t consumed = 0u;
    size_t body_only = 0u;
    uint8_t has_extension = 0u;

    make_presence(&object);

    extensions[0].id = 7u;
    extensions[0].critical = 0u;
    extensions[0].value = value_a;
    extensions[0].value_size = sizeof(value_a);
    extensions[1].id = 4096u;          /* forces a two-byte key uvarint */
    extensions[1].critical = 0u;
    extensions[1].value = value_b;
    extensions[1].value_size = sizeof(value_b);

    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    CHECK_TRUE((buffer[1] & 0x01u) != 0u);

    /*
     * The object is SELF-DELIMITING: its length is knowable from its own
     * bytes. Proven by decoding it out of a larger buffer with unrelated bytes
     * after it and checking `consumed` still reports the object alone. This is
     * what the raw-Wire path over AP needs, because nothing there supplies an
     * outer boundary the way a Link frame's payload_len does.
     */
    body_only = written;
    memset(buffer + written, 0xFFu, sizeof(buffer) - written);
    CHECK_STATUS(mcl_wire_tier0_decode_ext(buffer, sizeof(buffer), &decoded,
                                           &reader, &consumed), MCL_WIRE_OK);
    CHECK_TRUE(consumed == body_only);

    CHECK_STATUS(mcl_wire_tier0_decode_ext(buffer, written, &decoded, &reader,
                                           &consumed), MCL_WIRE_OK);
    CHECK_TRUE(consumed == written);
    CHECK_TRUE(decoded.kind == MCL_WIRE_KIND_PRESENCE);
    CHECK_TRUE(decoded.source_ref == object.source_ref);
    CHECK_TRUE(decoded.body.presence.ttl == 60u);

    CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &read_back,
                                                &has_extension), MCL_WIRE_OK);
    CHECK_TRUE(has_extension == 1u);
    CHECK_TRUE(read_back.id == 7u);
    CHECK_TRUE(read_back.critical == 0u);
    CHECK_TRUE(read_back.value_size == sizeof(value_a));
    CHECK_TRUE(read_back.value[0] == 0xDEu && read_back.value[1] == 0xADu);

    CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &read_back,
                                                &has_extension), MCL_WIRE_OK);
    CHECK_TRUE(has_extension == 1u);
    CHECK_TRUE(read_back.id == 4096u);
    CHECK_TRUE(read_back.value_size == sizeof(value_b));

    CHECK_STATUS(mcl_wire_extension_reader_next(&reader, &read_back,
                                                &has_extension), MCL_WIRE_OK);
    CHECK_TRUE(has_extension == 0u);

    /*
     * A decoder that cannot read extensions must REFUSE this object, not
     * return the body and discard the rest. An extension may be critical, and
     * quietly dropping it would turn "you must understand this" into "you may
     * ignore this".
     */
    CHECK_STATUS(mcl_wire_tier0_decode(buffer, written, &decoded, &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);
}

static void test_critical_extension_is_refused(void)
{
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t decoded;
    mcl_wire_extension_t extensions[2];
    mcl_wire_extension_reader_t reader;
    static const uint8_t value[] = {0x99u};
    uint8_t buffer[MCL_WIRE_TIER0_EXT_MAX_SIZE];
    size_t written = 0u;
    size_t consumed = 0u;

    make_presence(&object);

    /* A non-critical extension first, then a critical one. The critical one
     * must poison the whole object rather than only the tail of the block:
     * validating lazily would let a caller act on the object and the first
     * extension before discovering the object was never decodable. */
    extensions[0].id = 2u;
    extensions[0].critical = 0u;
    extensions[0].value = value;
    extensions[0].value_size = sizeof(value);
    extensions[1].id = 9u;
    extensions[1].critical = 1u;
    extensions[1].value = value;
    extensions[1].value_size = sizeof(value);

    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    CHECK_STATUS(mcl_wire_tier0_decode_ext(buffer, written, &decoded, &reader,
                                           &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);

    /* No extension id is registered, so this holds for every critical id. */
    extensions[0].critical = 1u;
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    CHECK_STATUS(mcl_wire_tier0_decode_ext(buffer, written, &decoded, &reader,
                                           &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);
}

static void test_tier0_extension_negatives(void)
{
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t decoded;
    mcl_wire_extension_t extensions[2];
    mcl_wire_extension_reader_t reader;
    static const uint8_t value[] = {0x11u, 0x22u};
    uint8_t buffer[MCL_WIRE_TIER0_EXT_MAX_SIZE];
    size_t written = 0u;
    size_t consumed = 0u;
    size_t body = 0u;
    size_t i;

    make_presence(&object);
    extensions[0].id = 5u;
    extensions[0].critical = 0u;
    extensions[0].value = value;
    extensions[0].value_size = sizeof(value);

    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    (void)body;

    /* Every truncation of a valid object reports truncation, never a partial
     * decode. Anything that decoded a prefix would let a peer act on half an
     * object. */
    for (i = 1u; i < written; ++i) {
        const mcl_wire_status_t st =
            mcl_wire_tier0_decode_ext(buffer, i, &decoded, &reader, &consumed);
        CHECK_TRUE(st == MCL_WIRE_ERR_TRUNCATED);
    }

    /* extension_present set with an empty block: not canonical, because the
     * empty case already has an encoding with the bit clear. */
    {
        uint8_t empty_block[MCL_WIRE_TIER0_MAX_SIZE + 1u];
        size_t plain = 0u;
        CHECK_STATUS(mcl_wire_tier0_encode(&object, empty_block,
                                           sizeof(empty_block), &plain),
                     MCL_WIRE_OK);
        empty_block[1] = (uint8_t)(empty_block[1] | 0x01u);
        empty_block[plain] = 0x00u;   /* uvarint 0 */
        CHECK_STATUS(mcl_wire_tier0_decode_ext(empty_block, plain + 1u,
                                               &decoded, &reader, &consumed),
                     MCL_WIRE_ERR_NONCANONICAL);
    }

    /* A block length larger than this implementation accepts is refused before
     * anything is read from it. */
    {
        uint8_t oversized[MCL_WIRE_TIER0_MAX_SIZE + 4u];
        size_t plain = 0u;
        CHECK_STATUS(mcl_wire_tier0_encode(&object, oversized,
                                           sizeof(oversized), &plain),
                     MCL_WIRE_OK);
        oversized[1] = (uint8_t)(oversized[1] | 0x01u);
        /* uvarint for 1000, which exceeds MCL_WIRE_EXTENSION_BLOCK_MAX */
        oversized[plain] = 0x87u;
        oversized[plain + 1u] = 0x68u;
        CHECK_STATUS(mcl_wire_tier0_decode_ext(oversized, plain + 2u, &decoded,
                                               &reader, &consumed),
                     MCL_WIRE_ERR_RANGE);
    }

    /* Ids must strictly increase. Refused rather than sorted, because sorting
     * would let two callers disagree about what they sent while both believing
     * they had succeeded. */
    extensions[1].id = 5u;
    extensions[1].critical = 0u;
    extensions[1].value = value;
    extensions[1].value_size = sizeof(value);
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_ERR_NONCANONICAL);
    extensions[1].id = 4u;
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_ERR_NONCANONICAL);

    /* Id 0 is reserved so a zeroed extension is never valid. */
    extensions[1].id = 0u;
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_ERR_RANGE);

    /* A buffer that fits the body but not the block fails as a whole, leaving
     * no half-written object behind a success code. */
    extensions[1].id = 9u;
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           MCL_WIRE_TIER0_MAX_SIZE + 1u,
                                           &written),
                 MCL_WIRE_ERR_BUFFER_TOO_SMALL);

    CHECK_STATUS(mcl_wire_tier0_encode_ext(NULL, extensions, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_ERR_INVALID_ARGUMENT);
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, NULL, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_ERR_INVALID_ARGUMENT);
    CHECK_STATUS(mcl_wire_tier0_decode_ext(buffer, written, &decoded, NULL,
                                           &consumed),
                 MCL_WIRE_ERR_INVALID_ARGUMENT);
}

/* The block cap must be enforced at encode time too, not only on decode. */
static void test_extension_block_cap(void)
{
    mcl_wire_tier0_t object;
    mcl_wire_extension_t extension;
    uint8_t big[MCL_WIRE_EXTENSION_BLOCK_MAX + 8u];
    uint8_t buffer[MCL_WIRE_TIER0_EXT_MAX_SIZE + 16u];
    size_t written = 0u;
    size_t size = 0u;

    make_presence(&object);
    memset(big, 0x5Au, sizeof(big));

    extension.id = 1u;
    extension.critical = 0u;
    extension.value = big;
    extension.value_size = MCL_WIRE_EXTENSION_BLOCK_MAX;

    CHECK_STATUS(mcl_wire_extensions_encoded_size(&extension, 1u, &size),
                 MCL_WIRE_ERR_RANGE);
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, &extension, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_ERR_RANGE);

    /* Just inside the cap, allowing for the two uvarints, does encode. */
    extension.value_size = MCL_WIRE_EXTENSION_BLOCK_MAX - 4u;
    CHECK_STATUS(mcl_wire_extensions_encoded_size(&extension, 1u, &size),
                 MCL_WIRE_OK);
    CHECK_TRUE(size <= MCL_WIRE_EXTENSION_BLOCK_MAX);
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, &extension, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    CHECK_TRUE(written <= MCL_WIRE_TIER0_EXT_MAX_SIZE);
}

/*
 * A caller states which critical extension ids it implements.
 *
 * Before this existed, every critical extension was refused unconditionally and
 * there was no way to ever accept one. An extension registry could have been
 * written and the decoder still could not have honoured it, because the decode
 * contract had nowhere to say "I understand id 17".
 */
static uint32_t g_known_id;
static unsigned g_known_calls;
static uint8_t g_required_value;
static size_t g_seen_value_size;

static uint8_t knows_one_id(void *user, uint32_t extension_id,
                            const uint8_t *value, size_t value_size)
{
    (void)user;
    ++g_known_calls;
    g_seen_value_size = value_size;
    if (extension_id != g_known_id) {
        return 0u;
    }
    /*
     * The callback sees the VALUE, not just the id. CRITICAL means the object
     * must not be acted on unless this extension is understood, and
     * understanding one means understanding its contents -- an id-only
     * predicate would return OK for a critical extension whose value the caller
     * cannot use.
     */
    if (value == NULL || value_size != 1u) {
        return 0u;
    }
    return (value[0] == g_required_value) ? 1u : 0u;
}

static void test_critical_extension_recognition(void)
{
    mcl_wire_tier0_t object;
    mcl_wire_tier0_t decoded;
    mcl_wire_extension_t extensions[2];
    mcl_wire_extension_reader_t reader;
    static const uint8_t value[] = {0x77u};
    uint8_t buffer[MCL_WIRE_TIER0_EXT_MAX_SIZE];
    size_t written = 0u;
    size_t consumed = 0u;

    make_presence(&object);
    extensions[0].id = 17u;
    extensions[0].critical = 1u;
    extensions[0].value = value;
    extensions[0].value_size = sizeof(value);

    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);

    /* Understood, and the value accepted: the object decodes. */
    g_known_id = 17u;
    g_required_value = 0x77u;
    g_known_calls = 0u;
    g_seen_value_size = 99u;
    CHECK_STATUS(mcl_wire_tier0_decode_ext_accept(buffer, written, &decoded,
                                                  &reader, knows_one_id, NULL,
                                                  &consumed), MCL_WIRE_OK);
    CHECK_TRUE(consumed == written);
    CHECK_TRUE(g_known_calls == 1u);
    /* The callback saw the value, not just the id. */
    CHECK_TRUE(g_seen_value_size == sizeof(value));

    /*
     * The id is implemented and the VALUE is refused. This is the case an
     * id-only predicate could not express: it would have returned
     * MCL_WIRE_OK for an object the caller must not act on, and nothing in the
     * status would have said so. Now the refusal reaches the caller as the
     * object being undecodable, which is what CRITICAL means.
     */
    g_required_value = 0x78u;
    CHECK_STATUS(mcl_wire_tier0_decode_ext_accept(buffer, written, &decoded,
                                                  &reader, knows_one_id, NULL,
                                                  &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);
    g_required_value = 0x77u;

    /* Not understood: the whole object is refused. */
    g_known_id = 18u;
    CHECK_STATUS(mcl_wire_tier0_decode_ext_accept(buffer, written, &decoded,
                                                  &reader, knows_one_id, NULL,
                                                  &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);

    /* No predicate means nothing is understood, which is what the plain
     * extension decoder does. */
    CHECK_STATUS(mcl_wire_tier0_decode_ext_accept(buffer, written, &decoded,
                                                  &reader, NULL, NULL,
                                                  &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);
    CHECK_STATUS(mcl_wire_tier0_decode_ext(buffer, written, &decoded, &reader,
                                           &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);

    /* The predicate is consulted for critical extensions only. A non-critical
     * unknown one is skipped by definition. */
    extensions[0].critical = 0u;
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 1u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    g_known_id = 99u;
    g_known_calls = 0u;
    CHECK_STATUS(mcl_wire_tier0_decode_ext_accept(buffer, written, &decoded,
                                                  &reader, knows_one_id, NULL,
                                                  &consumed), MCL_WIRE_OK);
    CHECK_TRUE(g_known_calls == 0u);

    /*
     * A known critical extension followed by an unknown one still refuses the
     * whole object. Validation completes before anything is returned, so a
     * caller cannot act on the object and the extensions it understood before
     * discovering the object was never decodable.
     */
    extensions[0].id = 17u;
    extensions[0].critical = 1u;
    extensions[0].value = value;
    extensions[0].value_size = sizeof(value);
    extensions[1].id = 33u;
    extensions[1].critical = 1u;
    extensions[1].value = value;
    extensions[1].value_size = sizeof(value);
    CHECK_STATUS(mcl_wire_tier0_encode_ext(&object, extensions, 2u, buffer,
                                           sizeof(buffer), &written),
                 MCL_WIRE_OK);
    g_known_id = 17u;
    CHECK_STATUS(mcl_wire_tier0_decode_ext_accept(buffer, written, &decoded,
                                                  &reader, knows_one_id, NULL,
                                                  &consumed),
                 MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC);
}

int main(void)
{
    test_uvarint();
    test_extension_blocks();
    test_negative_cases();
    test_tier0_without_extensions_is_unchanged();
    test_tier0_round_trip_with_extensions();
    test_critical_extension_is_refused();
    test_tier0_extension_negatives();
    test_extension_block_cap();
    test_critical_extension_recognition();
    puts("uvarint random round trips: 10000 PASS");
    puts("extension block random round trips: 2000 PASS");
    puts("canonicality/length/criticality exposure: PASS");
    return 0;
}
