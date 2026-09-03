/*
 * Checks the published Tier-0 extension vectors against the decoder.
 *
 * mcl-wire/tests/test_extension.c exercises the same behaviour through C byte
 * arrays, which is readable but proves nothing about the JSON an independent
 * implementer would actually download. The two renderings can drift, and the
 * drift is invisible: the C suite stays green while the published artifact
 * describes a protocol nobody implements.
 *
 * A host tool. It uses stdio and is not part of the freestanding path, exactly
 * like mcl-link/tools/validate_handoff_vectors.c.
 */

#define _CRT_SECURE_NO_WARNINGS
#include "mcl/extension.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE 65536
#define MAX_HEX_CHARS 1024

static char g_file[MAX_FILE_SIZE];
static int g_checked;
static int g_failed;

static void fail(const char *name, const char *why, int got, int expected)
{
    ++g_failed;
    fprintf(stderr, "FAIL vector \"%s\": %s (got %d, expected %d)\n",
            name, why, got, expected);
}

static const char *find_string_value(
    const char *from,
    const char *end,
    const char *key,
    char *out,
    size_t out_size)
{
    char pattern[64];
    const char *p;
    size_t len = 0u;

    if (strlen(key) + 4u >= sizeof(pattern)) {
        return NULL;
    }
    sprintf(pattern, "\"%s\"", key);

    p = from;
    for (;;) {
        p = strstr(p, pattern);
        if (p == NULL || p >= end) {
            return NULL;
        }
        p += strlen(pattern);
        while (p < end && (*p == ' ' || *p == ':' || *p == '\t' ||
                           *p == '\n' || *p == '\r')) {
            ++p;
        }
        if (p < end && *p == '\"') {
            break;
        }
    }

    ++p;
    while (p < end && *p != '\"') {
        if (len + 1u >= out_size) {
            return NULL;
        }
        out[len++] = *p++;
    }
    if (p >= end) {
        return NULL;
    }
    out[len] = '\0';
    return p + 1;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return 10 + (c - 'a'); }
    if (c >= 'A' && c <= 'F') { return 10 + (c - 'A'); }
    return -1;
}

static int decode_hex(const char *hex, uint8_t *out, size_t capacity,
                      size_t *written)
{
    size_t len = strlen(hex);
    size_t i;

    if ((len % 2u) != 0u || (len / 2u) > capacity) {
        return 0;
    }
    for (i = 0u; i < len; i += 2u) {
        const int hi = hex_value(hex[i]);
        const int lo = hex_value(hex[i + 1u]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i / 2u] = (uint8_t)((hi << 4) | lo);
    }
    *written = len / 2u;
    return 1;
}

static mcl_wire_status_t status_from_name(const char *name)
{
    if (strcmp(name, "TRUNCATED") == 0) { return MCL_WIRE_ERR_TRUNCATED; }
    if (strcmp(name, "RANGE") == 0) { return MCL_WIRE_ERR_RANGE; }
    if (strcmp(name, "NONCANONICAL") == 0) { return MCL_WIRE_ERR_NONCANONICAL; }
    if (strcmp(name, "UNSUPPORTED_SEMANTIC") == 0) {
        return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
    }
    if (strcmp(name, "BUFFER_TOO_SMALL") == 0) {
        return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
    }
    return MCL_WIRE_ERR_INVALID_ARGUMENT;
}

static void check_section(const char *from, const char *end, int positive)
{
    const char *cursor = from;

    for (;;) {
        char name[128];
        char hex[MAX_HEX_CHARS];
        char expect[64];
        char decoder[64];
        uint8_t bytes[512];
        size_t byte_count = 0u;
        size_t consumed = 0u;
        mcl_wire_tier0_t object;
        mcl_wire_extension_reader_t reader;
        mcl_wire_status_t status;
        const char *after_name;
        const char *after_hex;

        after_name = find_string_value(cursor, end, "name", name, sizeof(name));
        if (after_name == NULL) {
            return;
        }
        after_hex = find_string_value(after_name, end, "bytes_hex", hex,
                                      sizeof(hex));
        if (after_hex == NULL) {
            return;
        }
        cursor = after_hex;
        ++g_checked;

        if (!decode_hex(hex, bytes, sizeof(bytes), &byte_count)) {
            fail(name, "bytes_hex is not valid hex or is too long", 0, 0);
            continue;
        }

        if (positive) {
            char declared[64];
            size_t expected_size = 0u;

            status = mcl_wire_tier0_decode_ext(bytes, byte_count, &object,
                                               &reader, &consumed);
            if (status != MCL_WIRE_OK) {
                fail(name, "a positive vector did not decode", (int)status,
                     (int)MCL_WIRE_OK);
                continue;
            }
            if (consumed != byte_count) {
                fail(name, "consumed length differs from the vector length",
                     (int)consumed, (int)byte_count);
                continue;
            }

            /* The declared size must agree with the bytes, so the file cannot
             * describe one object while carrying another. */
            if (find_string_value(after_name, cursor, "size", declared,
                                  sizeof(declared)) == NULL) {
                /* "size" is numeric, so the string search will not find it.
                 * Parse it directly instead. */
                const char *p = strstr(after_name, "\"size\"");
                if (p != NULL && p < cursor) {
                    p += 6;
                    while (*p == ' ' || *p == ':') { ++p; }
                    expected_size = (size_t)strtoul(p, NULL, 10);
                }
            }
            if (expected_size != 0u && expected_size != byte_count) {
                fail(name, "declared size differs from the byte count",
                     (int)byte_count, (int)expected_size);
                continue;
            }

            /* Walking the whole block must succeed, since decode already
             * validated it. A disagreement here means the validation pass and
             * the reader do not see the same block. */
            for (;;) {
                mcl_wire_extension_t extension;
                uint8_t has_extension = 0u;
                mcl_wire_status_t st =
                    mcl_wire_extension_reader_next(&reader, &extension,
                                                   &has_extension);
                if (st != MCL_WIRE_OK) {
                    fail(name, "the reader rejected a block decode accepted",
                         (int)st, (int)MCL_WIRE_OK);
                    break;
                }
                if (has_extension == 0u) {
                    break;
                }
            }
        } else {
            const char *after_expect =
                find_string_value(cursor, end, "expect", expect,
                                  sizeof(expect));
            mcl_wire_status_t wanted;
            int use_plain = 0;

            if (after_expect == NULL) {
                fail(name, "negative vector has no expect field", 0, 0);
                continue;
            }
            cursor = after_expect;
            wanted = status_from_name(expect);

            /*
             * One vector deliberately targets the extension-unaware decoder,
             * to pin that it refuses rather than silently drops.
             *
             * The search window runs to the NEXT vector's "name", not to the
             * cursor: optional fields may appear after "expect", and a window
             * that stopped at the cursor would silently miss them. It did, and
             * the vector passed against the wrong decoder.
             */
            {
                const char *next_name = strstr(cursor, "\"name\"");
                const char *bound = (next_name != NULL && next_name < end)
                                        ? next_name : end;
                if (find_string_value(after_name, bound, "decoder", decoder,
                                      sizeof(decoder)) != NULL &&
                    strcmp(decoder, "mcl_wire_tier0_decode") == 0) {
                    use_plain = 1;
                }
            }

            if (use_plain) {
                status = mcl_wire_tier0_decode(bytes, byte_count, &object,
                                               &consumed);
            } else {
                status = mcl_wire_tier0_decode_ext(bytes, byte_count, &object,
                                                   &reader, &consumed);
            }

            if (status != wanted) {
                fail(name, "negative vector produced the wrong status",
                     (int)status, (int)wanted);
            }
        }
    }
}

int main(int argc, char **argv)
{
    const char *path;
    FILE *f;
    size_t size;
    const char *positive_start;
    const char *negative_start;
    const char *end;

    if (argc < 2) {
        fprintf(stderr,
                "usage: validate_extension_vectors <extensions-v0.1.json>\n");
        return 2;
    }
    path = argv[1];

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        return 2;
    }
    size = fread(g_file, 1u, sizeof(g_file) - 1u, f);
    fclose(f);
    if (size == 0u || size >= sizeof(g_file) - 1u) {
        fprintf(stderr, "%s is empty or larger than this tool expects\n", path);
        return 2;
    }
    g_file[size] = '\0';
    end = g_file + size;

    positive_start = strstr(g_file, "\"positive\"");
    negative_start = strstr(g_file, "\"negative\"");
    if (positive_start == NULL || negative_start == NULL ||
        negative_start <= positive_start) {
        fprintf(stderr, "%s does not contain the expected sections\n", path);
        return 2;
    }

    check_section(positive_start, negative_start, 1);
    check_section(negative_start, end, 0);

    if (g_checked == 0) {
        fprintf(stderr, "no vectors found in %s -- the file shape changed and "
                        "this check silently stopped checking\n", path);
        return 2;
    }

    printf("extension vectors: %d checked, %d failed (%s)\n",
           g_checked, g_failed, path);
    return g_failed == 0 ? 0 : 1;
}
