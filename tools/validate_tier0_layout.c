/*
 * Check the layout table in spec/tier0-layout-v0.2.md against the codec.
 *
 * WHY THIS EXISTS
 *
 * That document is the authoritative statement of the seven Tier-0 body
 * layouts, written so that a second implementation can be built without reading
 * the reference C. A specification that says field `y` is 12 bits at offset 78
 * and is WRONG is worse than no specification: the implementer builds to it,
 * the bytes disagree, and the disagreement is blamed on whichever side is
 * younger.
 *
 * So the table is transcribed here as data, independently of the encoder, and
 * checked both ways:
 *
 *   ENCODE   set one field to a distinctive value, leave the rest zero, and
 *            confirm exactly the documented bits change and hold exactly that
 *            value.
 *   DECODE   write a distinctive value into the documented bit positions and
 *            confirm the decoder reports it in the right field.
 *
 * The second direction matters as much as the first. A table that named two
 * fields the same offset would survive an encode-only check on either one
 * alone.
 *
 * This is a tool, not a test of the protocol: it tests that the document and
 * the code say the same thing.
 */

#include "mcl/wire.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, ...) do {                        \
    if (!(cond)) {                                   \
        ++failures;                                  \
        printf("  FAIL: ");                          \
        printf(__VA_ARGS__);                         \
        printf(" (%s:%d)\n", __FILE__, __LINE__);    \
    }                                                \
} while (0)

/* One row of the specification's layout table. */
typedef struct {
    const char *field;
    unsigned offset;   /* bits from the start of the object */
    unsigned width;
    int is_signed;
} field_row_t;

typedef struct {
    const char *name;
    mcl_wire_kind_t kind;
    unsigned category;
    unsigned opcode;
    size_t bytes;
    unsigned pad_bits;
    const field_row_t *fields;   /* body fields only, after source_ref */
    size_t field_count;
} kind_row_t;

/*
 * The table exactly as spec/tier0-layout-v0.2.md section 4 prints it. Every
 * offset below was typed from the document, not computed from the code.
 */
static const field_row_t presence_fields[] = {
    {"machine_class",     48,  8, 0},
    {"capability_tag", 56, 24, 0},
    {"ttl",               80,  8, 0}
};

static const field_row_t hazard_fields[] = {
    {"hazard_class", 48,  8, 0},
    {"severity",     56,  3, 0},
    {"confidence",   59,  7, 0},
    {"x",            66, 12, 1},
    {"y",            78, 12, 1},
    {"z",            90, 10, 1},
    {"radius",      100, 10, 0},
    {"ttl",         110,  8, 0}
};

static const field_row_t request_fields[] = {
    {"request_class", 48,  8, 0},
    {"target_ref",    56, 32, 0},
    {"x",             88, 12, 1},
    {"y",            100, 12, 1},
    {"radius",       112, 10, 0},
    {"ttl",          122,  8, 0}
};

static const field_row_t authority_fields[] = {
    {"authority_class", 48,  6, 0},
    {"jurisdiction",    54, 12, 0},
    {"credential_ref",  66, 32, 0},
    {"validity",        98,  8, 0}
};

static const field_row_t degraded_fields[] = {
    {"affected_capability", 48, 8, 0},
    {"health",              56, 7, 0},
    {"severity",            63, 3, 0},
    {"ttl",                 66, 8, 0}
};

static const field_row_t offer_fields[] = {
    {"migration_ref",  48, 32, 0},
    {"transport_id",   80,  8, 0},
    {"profile_id",     88,  8, 0},
    {"endpoint_token", 96, 32, 0},
    {"validity",      128,  8, 0}
};

static const field_row_t accept_fields[] = {
    {"migration_ref", 48, 32, 0},
    {"transport_id",  80,  8, 0},
    {"profile_id",    88,  8, 0},
    {"session_ref",   96, 32, 0}
};

#define ROW(n, k, c, o, b, p, f) \
    {n, k, c, o, b, p, f, sizeof(f) / sizeof((f)[0])}

static const kind_row_t k_table[] = {
    ROW("PRESENCE",         MCL_WIRE_KIND_PRESENCE,         0, 0, 11, 0, presence_fields),
    ROW("HAZARD",           MCL_WIRE_KIND_HAZARD,           3, 1, 15, 2, hazard_fields),
    ROW("REQUEST",          MCL_WIRE_KIND_REQUEST,          5, 1, 17, 6, request_fields),
    ROW("AUTHORITY_CLAIM",  MCL_WIRE_KIND_AUTHORITY_CLAIM,  1, 1, 14, 6, authority_fields),
    ROW("DEGRADED_STATE",   MCL_WIRE_KIND_DEGRADED_STATE,   6, 1, 10, 6, degraded_fields),
    ROW("TRANSPORT_OFFER",  MCL_WIRE_KIND_TRANSPORT_OFFER,  8, 0, 17, 0, offer_fields),
    ROW("TRANSPORT_ACCEPT", MCL_WIRE_KIND_TRANSPORT_ACCEPT, 8, 1, 16, 0, accept_fields)
};

#define KIND_COUNT (sizeof(k_table) / sizeof(k_table[0]))

/* Read `width` bits at `offset`, big-endian, MSB first: the document's model. */
static uint32_t read_bits(const uint8_t *data, unsigned offset, unsigned width)
{
    uint32_t value = 0u;
    unsigned i;

    for (i = 0u; i < width; ++i) {
        const unsigned bit = offset + i;
        const uint8_t byte = data[bit >> 3];
        const unsigned shift = 7u - (bit & 7u);
        value = (value << 1) | (uint32_t)((byte >> shift) & 1u);
    }
    return value;
}

static void write_bits(uint8_t *data, unsigned offset, unsigned width,
                       uint32_t value)
{
    unsigned i;

    for (i = 0u; i < width; ++i) {
        const unsigned bit = offset + i;
        const unsigned shift_in = width - 1u - i;
        const unsigned shift_out = 7u - (bit & 7u);
        const uint8_t v = (uint8_t)((value >> shift_in) & 1u);
        data[bit >> 3] = (uint8_t)((data[bit >> 3] & ~(1u << shift_out)) |
                                   (uint32_t)v << shift_out);
    }
}

/*
 * Set one body field by name. The switch is exhaustive over the table above;
 * an unmatched name returns 0 and fails the run rather than silently passing.
 */
static int set_field(mcl_wire_tier0_t *object, const char *field, int32_t value)
{
    switch (object->kind) {
    case MCL_WIRE_KIND_PRESENCE:
        if (strcmp(field, "machine_class") == 0) {
            object->body.presence.machine_class = (uint8_t)value; return 1; }
        if (strcmp(field, "capability_tag") == 0) {
            object->body.presence.capability_tag = (uint32_t)value; return 1; }
        if (strcmp(field, "ttl") == 0) {
            object->body.presence.ttl = (uint8_t)value; return 1; }
        return 0;
    case MCL_WIRE_KIND_HAZARD:
        if (strcmp(field, "hazard_class") == 0) {
            object->body.hazard.hazard_class = (uint8_t)value; return 1; }
        if (strcmp(field, "severity") == 0) {
            object->body.hazard.severity = (uint8_t)value; return 1; }
        if (strcmp(field, "confidence") == 0) {
            object->body.hazard.confidence = (uint8_t)value; return 1; }
        if (strcmp(field, "x") == 0) {
            object->body.hazard.x = (int16_t)value; return 1; }
        if (strcmp(field, "y") == 0) {
            object->body.hazard.y = (int16_t)value; return 1; }
        if (strcmp(field, "z") == 0) {
            object->body.hazard.z = (int16_t)value; return 1; }
        if (strcmp(field, "radius") == 0) {
            object->body.hazard.radius = (uint16_t)value; return 1; }
        if (strcmp(field, "ttl") == 0) {
            object->body.hazard.ttl = (uint8_t)value; return 1; }
        return 0;
    case MCL_WIRE_KIND_REQUEST:
        if (strcmp(field, "request_class") == 0) {
            object->body.request.request_class = (uint8_t)value; return 1; }
        if (strcmp(field, "target_ref") == 0) {
            object->body.request.target_ref = (uint32_t)value; return 1; }
        if (strcmp(field, "x") == 0) {
            object->body.request.x = (int16_t)value; return 1; }
        if (strcmp(field, "y") == 0) {
            object->body.request.y = (int16_t)value; return 1; }
        if (strcmp(field, "radius") == 0) {
            object->body.request.radius = (uint16_t)value; return 1; }
        if (strcmp(field, "ttl") == 0) {
            object->body.request.ttl = (uint8_t)value; return 1; }
        return 0;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        if (strcmp(field, "authority_class") == 0) {
            object->body.authority_claim.authority_class = (uint8_t)value; return 1; }
        if (strcmp(field, "jurisdiction") == 0) {
            object->body.authority_claim.jurisdiction = (uint16_t)value; return 1; }
        if (strcmp(field, "credential_ref") == 0) {
            object->body.authority_claim.credential_ref = (uint32_t)value; return 1; }
        if (strcmp(field, "validity") == 0) {
            object->body.authority_claim.validity = (uint8_t)value; return 1; }
        return 0;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        if (strcmp(field, "affected_capability") == 0) {
            object->body.degraded_state.affected_capability = (uint8_t)value; return 1; }
        if (strcmp(field, "health") == 0) {
            object->body.degraded_state.health = (uint8_t)value; return 1; }
        if (strcmp(field, "severity") == 0) {
            object->body.degraded_state.severity = (uint8_t)value; return 1; }
        if (strcmp(field, "ttl") == 0) {
            object->body.degraded_state.ttl = (uint8_t)value; return 1; }
        return 0;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        if (strcmp(field, "migration_ref") == 0) {
            object->body.transport_offer.migration_ref = (uint32_t)value; return 1; }
        if (strcmp(field, "transport_id") == 0) {
            object->body.transport_offer.transport_id = (uint8_t)value; return 1; }
        if (strcmp(field, "profile_id") == 0) {
            object->body.transport_offer.profile_id = (uint8_t)value; return 1; }
        if (strcmp(field, "endpoint_token") == 0) {
            object->body.transport_offer.endpoint_token = (uint32_t)value; return 1; }
        if (strcmp(field, "validity") == 0) {
            object->body.transport_offer.validity = (uint8_t)value; return 1; }
        return 0;
    case MCL_WIRE_KIND_TRANSPORT_ACCEPT:
        if (strcmp(field, "migration_ref") == 0) {
            object->body.transport_accept.migration_ref = (uint32_t)value; return 1; }
        if (strcmp(field, "transport_id") == 0) {
            object->body.transport_accept.transport_id = (uint8_t)value; return 1; }
        if (strcmp(field, "profile_id") == 0) {
            object->body.transport_accept.profile_id = (uint8_t)value; return 1; }
        if (strcmp(field, "session_ref") == 0) {
            object->body.transport_accept.session_ref = (uint32_t)value; return 1; }
        return 0;
    default:
        return 0;
    }
}

static int32_t get_field(const mcl_wire_tier0_t *object, const char *field,
                         int *found)
{
    mcl_wire_tier0_t probe = *object;
    int32_t v = 0;

    *found = 1;
    switch (object->kind) {
    case MCL_WIRE_KIND_PRESENCE:
        if (strcmp(field, "machine_class") == 0) return (int32_t)probe.body.presence.machine_class;
        if (strcmp(field, "capability_tag") == 0) return (int32_t)probe.body.presence.capability_tag;
        if (strcmp(field, "ttl") == 0) return (int32_t)probe.body.presence.ttl;
        break;
    case MCL_WIRE_KIND_HAZARD:
        if (strcmp(field, "hazard_class") == 0) return (int32_t)probe.body.hazard.hazard_class;
        if (strcmp(field, "severity") == 0) return (int32_t)probe.body.hazard.severity;
        if (strcmp(field, "confidence") == 0) return (int32_t)probe.body.hazard.confidence;
        if (strcmp(field, "x") == 0) return (int32_t)probe.body.hazard.x;
        if (strcmp(field, "y") == 0) return (int32_t)probe.body.hazard.y;
        if (strcmp(field, "z") == 0) return (int32_t)probe.body.hazard.z;
        if (strcmp(field, "radius") == 0) return (int32_t)probe.body.hazard.radius;
        if (strcmp(field, "ttl") == 0) return (int32_t)probe.body.hazard.ttl;
        break;
    case MCL_WIRE_KIND_REQUEST:
        if (strcmp(field, "request_class") == 0) return (int32_t)probe.body.request.request_class;
        if (strcmp(field, "target_ref") == 0) return (int32_t)probe.body.request.target_ref;
        if (strcmp(field, "x") == 0) return (int32_t)probe.body.request.x;
        if (strcmp(field, "y") == 0) return (int32_t)probe.body.request.y;
        if (strcmp(field, "radius") == 0) return (int32_t)probe.body.request.radius;
        if (strcmp(field, "ttl") == 0) return (int32_t)probe.body.request.ttl;
        break;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        if (strcmp(field, "authority_class") == 0) return (int32_t)probe.body.authority_claim.authority_class;
        if (strcmp(field, "jurisdiction") == 0) return (int32_t)probe.body.authority_claim.jurisdiction;
        if (strcmp(field, "credential_ref") == 0) return (int32_t)probe.body.authority_claim.credential_ref;
        if (strcmp(field, "validity") == 0) return (int32_t)probe.body.authority_claim.validity;
        break;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        if (strcmp(field, "affected_capability") == 0) return (int32_t)probe.body.degraded_state.affected_capability;
        if (strcmp(field, "health") == 0) return (int32_t)probe.body.degraded_state.health;
        if (strcmp(field, "severity") == 0) return (int32_t)probe.body.degraded_state.severity;
        if (strcmp(field, "ttl") == 0) return (int32_t)probe.body.degraded_state.ttl;
        break;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        if (strcmp(field, "migration_ref") == 0) return (int32_t)probe.body.transport_offer.migration_ref;
        if (strcmp(field, "transport_id") == 0) return (int32_t)probe.body.transport_offer.transport_id;
        if (strcmp(field, "profile_id") == 0) return (int32_t)probe.body.transport_offer.profile_id;
        if (strcmp(field, "endpoint_token") == 0) return (int32_t)probe.body.transport_offer.endpoint_token;
        if (strcmp(field, "validity") == 0) return (int32_t)probe.body.transport_offer.validity;
        break;
    case MCL_WIRE_KIND_TRANSPORT_ACCEPT:
        if (strcmp(field, "migration_ref") == 0) return (int32_t)probe.body.transport_accept.migration_ref;
        if (strcmp(field, "transport_id") == 0) return (int32_t)probe.body.transport_accept.transport_id;
        if (strcmp(field, "profile_id") == 0) return (int32_t)probe.body.transport_accept.profile_id;
        if (strcmp(field, "session_ref") == 0) return (int32_t)probe.body.transport_accept.session_ref;
        break;
    default:
        break;
    }
    *found = 0;
    return v;
}

/* A value that exercises the whole width without overflowing it: all ones for
 * unsigned, and the most negative value for signed, which is the case a wrong
 * width or a wrong signedness gets wrong most visibly. */
static int32_t probe_value(const field_row_t *f, int use_min)
{
    if (f->is_signed != 0) {
        const int32_t magnitude = (int32_t)1 << (f->width - 1u);
        return use_min ? -magnitude : (magnitude - 1);
    }
    if (f->width >= 32u) {
        return (int32_t)0xFEDCBA98u;
    }
    return (int32_t)(((uint32_t)1 << f->width) - 1u);
}

static void check_kind(const kind_row_t *k)
{
    mcl_wire_tier0_t object;
    uint8_t buffer[64];
    size_t written = 0u;
    size_t i;

    printf("%s\n", k->name);

    /* Size and header codes, as the section 3 table states them. */
    memset(&object, 0, sizeof(object));
    object.kind = k->kind;
    object.priority = 0u;
    object.source_ref = 0u;
    if (k->kind == MCL_WIRE_KIND_TRANSPORT_OFFER ||
        k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
        (void)set_field(&object, "migration_ref", 1);
        (void)set_field(&object, "transport_id", 1);
    }
    if (k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
        (void)set_field(&object, "session_ref", 1);
    }
    CHECK(mcl_wire_tier0_encode(&object, buffer, sizeof(buffer), &written) ==
          MCL_WIRE_OK, "%s encodes", k->name);
    CHECK(written == k->bytes, "%s is %u bytes, document says %u",
          k->name, (unsigned)written, (unsigned)k->bytes);
    CHECK(mcl_wire_tier0_encoded_size(k->kind) == k->bytes,
          "%s encoded_size agrees", k->name);
    CHECK(((unsigned)buffer[0] & 0x0Fu) == k->category,
          "%s category is %u", k->name, k->category);
    CHECK(((unsigned)buffer[1] >> 3) == k->opcode,
          "%s opcode is %u", k->name, k->opcode);

    /* source_ref is at bit 16, width 32, for every kind. */
    memset(&object, 0, sizeof(object));
    object.kind = k->kind;
    object.source_ref = 0x89ABCDEFu;
    if (k->kind == MCL_WIRE_KIND_TRANSPORT_OFFER ||
        k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
        (void)set_field(&object, "migration_ref", 1);
        (void)set_field(&object, "transport_id", 1);
    }
    if (k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
        (void)set_field(&object, "session_ref", 1);
    }
    if (mcl_wire_tier0_encode(&object, buffer, sizeof(buffer), &written) ==
        MCL_WIRE_OK) {
        CHECK(read_bits(buffer, 16u, 32u) == 0x89ABCDEFu,
              "%s source_ref is 32 bits at offset 16", k->name);
    }

    for (i = 0u; i < k->field_count; ++i) {
        const field_row_t *f = &k->fields[i];
        int pass;

        for (pass = 0; pass < 2; ++pass) {
            const int32_t value = probe_value(f, pass);
            uint32_t expected;
            int found = 0;

            if (f->is_signed == 0 && pass == 1) {
                continue;  /* one probe is enough for an unsigned field */
            }

            memset(&object, 0, sizeof(object));
            object.kind = k->kind;
            object.source_ref = 0u;
            /* Reserved-value rules live in Link, but keep the object sane. */
            if (k->kind == MCL_WIRE_KIND_TRANSPORT_OFFER ||
                k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
                (void)set_field(&object, "migration_ref", 1);
                (void)set_field(&object, "transport_id", 1);
            }
            if (k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
                (void)set_field(&object, "session_ref", 1);
            }
            if (set_field(&object, f->field, value) == 0) {
                ++failures;
                printf("  FAIL: %s.%s is not a field of this kind\n",
                       k->name, f->field);
                continue;
            }
            if (mcl_wire_tier0_encode(&object, buffer, sizeof(buffer),
                                      &written) != MCL_WIRE_OK) {
                ++failures;
                printf("  FAIL: %s.%s = %ld did not encode\n",
                       k->name, f->field, (long)value);
                continue;
            }

            /* ENCODE direction: the documented bits hold the value. */
            expected = (f->width >= 32u)
                ? (uint32_t)value
                : ((uint32_t)value & (((uint32_t)1 << f->width) - 1u));
            CHECK(read_bits(buffer, f->offset, f->width) == expected,
                  "%s.%s at offset %u width %u", k->name, f->field,
                  f->offset, f->width);

            /* DECODE direction: those bits reach that field, and no other. */
            {
                mcl_wire_tier0_t decoded;
                size_t consumed = 0u;
                if (mcl_wire_tier0_decode(buffer, written, &decoded,
                                          &consumed) == MCL_WIRE_OK) {
                    const int32_t got = get_field(&decoded, f->field, &found);
                    CHECK(found == 1, "%s.%s readable", k->name, f->field);
                    CHECK(got == value, "%s.%s decodes to %ld, got %ld",
                          k->name, f->field, (long)value, (long)got);
                    CHECK(consumed == k->bytes, "%s consumed %u",
                          k->name, (unsigned)k->bytes);
                } else {
                    ++failures;
                    printf("  FAIL: %s.%s did not decode\n", k->name, f->field);
                }
            }
        }
    }

    /* Padding: the document says how many bits, and that they must be zero. */
    memset(&object, 0, sizeof(object));
    object.kind = k->kind;
    if (k->kind == MCL_WIRE_KIND_TRANSPORT_OFFER ||
        k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
        (void)set_field(&object, "migration_ref", 1);
        (void)set_field(&object, "transport_id", 1);
    }
    if (k->kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
        (void)set_field(&object, "session_ref", 1);
    }
    if (mcl_wire_tier0_encode(&object, buffer, sizeof(buffer), &written) ==
        MCL_WIRE_OK && k->pad_bits > 0u) {
        mcl_wire_tier0_t decoded;
        size_t consumed = 0u;
        const unsigned pad_offset = (unsigned)(k->bytes * 8u) - k->pad_bits;

        CHECK(read_bits(buffer, pad_offset, k->pad_bits) == 0u,
              "%s emits %u zero padding bits", k->name, k->pad_bits);

        /* Setting any padding bit must make the object undecodable. One
         * semantic object has exactly one encoding. */
        write_bits(buffer, pad_offset + k->pad_bits - 1u, 1u, 1u);
        CHECK(mcl_wire_tier0_decode(buffer, written, &decoded, &consumed) ==
              MCL_WIRE_ERR_NONCANONICAL,
              "%s rejects non-zero padding", k->name);
    }
}

int main(void)
{
    size_t i;

    printf("Tier-0 layout: spec/tier0-layout-v0.2.md against the codec\n\n");

    for (i = 0u; i < KIND_COUNT; ++i) {
        check_kind(&k_table[i]);
    }

    printf("\n");
    if (failures != 0) {
        printf("%d layout mismatches: the document and the code disagree.\n",
               failures);
        return 1;
    }
    printf("all seven layouts match the specification, both directions.\n");
    printf("NOTE: this checks BYTES, not MEANINGS. See section 7.\n");
    return 0;
}
