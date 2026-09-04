/*
 * Check the major-1 vector family against the codec.
 *
 * Release gate item 12.
 *
 * WHY THIS CHECKS FIELDS AND NOT ONLY BYTES
 *
 * Earlier vector families recorded name, length and hex. That is not enough to
 * catch a wrong layout, and this project has the proof: the clean-room
 * implementation decoded AUTHORITY_CLAIM with an 8-bit authority_class and a
 * 16-bit jurisdiction instead of 6 and 12, and because that object carries 6
 * bits of padding BOTH layouts consume exactly 14 bytes. The vector passed. The
 * length check passed. Round-tripping passed. Every field after source_ref was
 * being misread.
 *
 * So this validator decodes each vector and compares every recorded field
 * value, and it refuses to pass a vector file whose positive entries carry no
 * "fields" object at all -- a vector without expected values is a vector that
 * cannot catch that defect.
 *
 * It also runs the negative vectors, because a decoder that accepts everything
 * passes any positive-only suite.
 *
 * A minimal scanner over the shapes this file actually uses. No JSON library:
 * the protocol repositories take no dependencies.
 */

#define _CRT_SECURE_NO_WARNINGS
#include "mcl/wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_text[65536];
static size_t g_len;
static int g_failures;
static int g_checks;

static size_t find_from(const char *needle, size_t from)
{
    const char *hit;
    if (from >= g_len) {
        return (size_t)-1;
    }
    hit = strstr(g_text + from, needle);
    if (hit == NULL) {
        return (size_t)-1;
    }
    return (size_t)(hit - g_text);
}

/* Value of "key": "..." after `from`, bounded by `limit`. */
static int read_string(const char *key, size_t from, size_t limit,
                       char *out, size_t out_size)
{
    char pattern[64];
    size_t pos;
    size_t i = 0u;

    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = find_from(pattern, from);
    if (pos == (size_t)-1 || pos > limit) {
        return 0;
    }
    pos += strlen(pattern);
    while (pos < g_len && g_text[pos] != '"') {
        if (g_text[pos] == ',' || g_text[pos] == '}') {
            return 0;
        }
        ++pos;
    }
    if (pos >= g_len) {
        return 0;
    }
    ++pos;
    while (pos < g_len && g_text[pos] != '"' && i + 1u < out_size) {
        out[i++] = g_text[pos++];
    }
    out[i] = '\0';
    return 1;
}

static int read_number(const char *key, size_t from, size_t limit,
                       unsigned long *out)
{
    char pattern[64];
    size_t pos;

    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = find_from(pattern, from);
    if (pos == (size_t)-1 || pos > limit) {
        return 0;
    }
    pos += strlen(pattern);
    while (pos < g_len && (g_text[pos] == ':' || g_text[pos] == ' ')) {
        ++pos;
    }
    if (pos >= g_len || g_text[pos] < '0' || g_text[pos] > '9') {
        return 0;
    }
    *out = strtoul(g_text + pos, NULL, 10);
    return 1;
}

static size_t from_hex(const char *text, uint8_t *out, size_t capacity)
{
    size_t len = strlen(text);
    size_t i;

    if ((len % 2u) != 0u || (len / 2u) > capacity) {
        return 0u;
    }
    for (i = 0u; i < len; i += 2u) {
        unsigned value = 0u;
        if (sscanf(text + i, "%2x", &value) != 1) {
            return 0u;
        }
        out[i / 2u] = (uint8_t)value;
    }
    return len / 2u;
}

static void check_field(const char *object, const char *field,
                        size_t block_start, size_t block_end,
                        unsigned long actual)
{
    unsigned long expected = 0u;
    ++g_checks;
    if (read_number(field, block_start, block_end, &expected) == 0) {
        printf("  FAIL %s: vector records no expected %s\n", object, field);
        ++g_failures;
        return;
    }
    if (expected != actual) {
        printf("  FAIL %s.%s: decoded %lu, vector says %lu\n",
               object, field, actual, expected);
        ++g_failures;
    }
}

int main(int argc, char **argv)
{
    FILE *file;
    const char *path;
    size_t pos;
    size_t negatives_start;
    int positives = 0;
    int negatives = 0;

    path = (argc > 1) ? argv[1]
                      : "conformance/vectors/tier0-major1-v1.0.json";
    file = fopen(path, "rb");
    if (file == NULL) {
        printf("cannot open %s\n", path);
        return 1;
    }
    g_len = fread(g_text, 1u, sizeof(g_text) - 1u, file);
    fclose(file);
    g_text[g_len] = '\0';

    printf("Major-1 Tier-0 vectors: %s\n\n", path);

    negatives_start = find_from("\"negative_vectors\"", 0u);
    if (negatives_start == (size_t)-1) {
        printf("  FAIL the vector file records no negative vectors\n");
        ++g_failures;
        negatives_start = g_len;
    }

    /* ---------------- positive vectors ---------------- */
    pos = find_from("\"vectors\"", 0u);
    if (pos == (size_t)-1) {
        printf("no vectors\n");
        return 1;
    }

    for (;;) {
        char name[64];
        char hex[256];
        size_t name_pos;
        size_t block_end;
        size_t fields_pos;
        uint8_t bytes[64];
        size_t size;
        size_t consumed = 0u;
        mcl_wire_tier0_t object;
        unsigned long declared_len = 0u;

        name_pos = find_from("\"name\"", pos);
        if (name_pos == (size_t)-1 || name_pos > negatives_start) {
            break;
        }
        if (read_string("name", name_pos - 1u, name_pos + 64u,
                        name, sizeof(name)) == 0) {
            break;
        }
        block_end = find_from("\"name\"", name_pos + 6u);
        if (block_end == (size_t)-1 || block_end > negatives_start) {
            block_end = negatives_start;
        }

        if (read_string("hex", name_pos, block_end, hex, sizeof(hex)) == 0) {
            printf("  FAIL %s: no hex\n", name);
            ++g_failures;
            pos = name_pos + 6u;
            continue;
        }
        size = from_hex(hex, bytes, sizeof(bytes));
        ++positives;
        ++g_checks;
        if (size == 0u) {
            printf("  FAIL %s: unreadable hex\n", name);
            ++g_failures;
            pos = name_pos + 6u;
            continue;
        }

        ++g_checks;
        if (read_number("length", name_pos, block_end, &declared_len) == 0 ||
            declared_len != (unsigned long)size) {
            printf("  FAIL %s: declared length disagrees with the bytes\n",
                   name);
            ++g_failures;
        }

        ++g_checks;
        if (mcl_wire_tier0_decode(bytes, size, &object, &consumed) !=
                MCL_WIRE_OK) {
            printf("  FAIL %s: does not decode\n", name);
            ++g_failures;
            pos = name_pos + 6u;
            continue;
        }
        ++g_checks;
        if (consumed != size) {
            printf("  FAIL %s: %u bytes consumed of %u\n", name,
                   (unsigned)consumed, (unsigned)size);
            ++g_failures;
        }

        /*
         * A positive vector with no "fields" block cannot catch a wrong layout
         * whose widths happen to sum correctly. Refused rather than skipped.
         */
        fields_pos = find_from("\"fields\"", name_pos);
        ++g_checks;
        if (fields_pos == (size_t)-1 || fields_pos > block_end) {
            printf("  FAIL %s: records no expected field values\n", name);
            ++g_failures;
            pos = name_pos + 6u;
            continue;
        }

        check_field(name, "priority", fields_pos, block_end, object.priority);
        check_field(name, "source_ref", fields_pos, block_end,
                    object.source_ref);

        if (object.kind == MCL_WIRE_KIND_PRESENCE) {
            check_field(name, "capability_tag", fields_pos, block_end,
                        object.body.presence.capability_tag);
            check_field(name, "ttl", fields_pos, block_end,
                        object.body.presence.ttl);
            /* machine_class is absent at major 1 and must decode to zero. */
            ++g_checks;
            if (object.body.presence.machine_class != 0u) {
                printf("  FAIL %s: machine_class is not absent at major 1\n",
                       name);
                ++g_failures;
            }
        } else if (object.kind == MCL_WIRE_KIND_TRANSPORT_OFFER) {
            check_field(name, "migration_ref", fields_pos, block_end,
                        object.body.transport_offer.migration_ref);
            check_field(name, "transport_id", fields_pos, block_end,
                        object.body.transport_offer.transport_id);
            check_field(name, "profile_id", fields_pos, block_end,
                        object.body.transport_offer.profile_id);
            check_field(name, "endpoint_token", fields_pos, block_end,
                        object.body.transport_offer.endpoint_token);
            check_field(name, "validity", fields_pos, block_end,
                        object.body.transport_offer.validity);
        } else if (object.kind == MCL_WIRE_KIND_TRANSPORT_ACCEPT) {
            check_field(name, "migration_ref", fields_pos, block_end,
                        object.body.transport_accept.migration_ref);
            check_field(name, "transport_id", fields_pos, block_end,
                        object.body.transport_accept.transport_id);
            check_field(name, "profile_id", fields_pos, block_end,
                        object.body.transport_accept.profile_id);
            check_field(name, "session_ref", fields_pos, block_end,
                        object.body.transport_accept.session_ref);
        } else {
            printf("  FAIL %s: not a Stable object, so it may not be here\n",
                   name);
            ++g_failures;
        }

        printf("  ok   %-20s %u bytes, fields verified\n", name,
               (unsigned)size);
        pos = name_pos + 6u;
    }

    /* ---------------- negative vectors ---------------- */
    printf("\n");
    pos = negatives_start;
    for (;;) {
        char name[64];
        char hex[256];
        size_t name_pos;
        size_t block_end;
        uint8_t bytes[64];
        size_t size;
        size_t consumed = 0u;
        mcl_wire_tier0_t object;

        name_pos = find_from("\"name\"", pos);
        if (name_pos == (size_t)-1) {
            break;
        }
        if (read_string("name", name_pos - 1u, name_pos + 64u,
                        name, sizeof(name)) == 0) {
            break;
        }
        block_end = find_from("\"name\"", name_pos + 6u);
        if (block_end == (size_t)-1) {
            block_end = g_len;
        }
        if (read_string("hex", name_pos, block_end, hex, sizeof(hex)) == 0) {
            pos = name_pos + 6u;
            continue;
        }
        size = from_hex(hex, bytes, sizeof(bytes));
        ++negatives;
        ++g_checks;
        if (size == 0u) {
            printf("  FAIL %s: unreadable hex\n", name);
            ++g_failures;
            pos = name_pos + 6u;
            continue;
        }
        if (mcl_wire_tier0_decode(bytes, size, &object, &consumed) ==
                MCL_WIRE_OK && consumed == size) {
            printf("  FAIL %s: ACCEPTED, and it must be refused\n", name);
            ++g_failures;
        } else {
            printf("  ok   %-32s refused\n", name);
        }
        pos = name_pos + 6u;
    }

    printf("\n%d positive, %d negative, %d checks, %d failed.\n",
           positives, negatives, g_checks, g_failures);

    if (positives == 0) {
        printf("no positive vectors were read, which is itself a defect\n");
        return 1;
    }
    if (negatives == 0) {
        printf("no negative vectors were read; a positive-only suite passes\n"
               "a decoder that accepts everything\n");
        return 1;
    }
    return (g_failures == 0) ? 0 : 1;
}
