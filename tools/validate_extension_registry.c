/*
 * Check registries/extension-ids-v0.1.json against itself and against the
 * encoding it allocates for.
 *
 * WHY THIS EXISTS
 *
 * An extension registry is a promise that two independent implementers reading
 * it will not collide. That promise is kept by properties nobody can hold in
 * their head while editing JSON by hand:
 *
 *   - the range table must PARTITION the identifier space. A gap is a value
 *     with no policy, and an overlap is a value with two, and both are
 *     discovered by whoever requests that number rather than by whoever wrote
 *     the table.
 *   - the space must end exactly at 2^31-1, because the key is
 *     uvarint((id << 1) | critical) and a wider id would not survive the shift.
 *   - zero must stay reserved. That is a property of the encoding -- a zeroed
 *     buffer must never decode as an extension -- so a registry that reassigned
 *     it would contradict the decoder rather than merely the policy.
 *   - every assignment must fall inside a declared range, carry every required
 *     field, and be unique.
 *   - the summary must count what is actually there. A summary counted by hand
 *     is a number that drifts the first time an assignment is added, and a
 *     WRONG count in a registry is exactly the class of defect this project has
 *     already found twice.
 *
 * The table is currently EMPTY, and this tool passing on an empty table is not
 * a formality. It is what makes the first real assignment safe: the checks are
 * in place before the pressure to add something is.
 *
 * This is a tool, not a test of the protocol. It tests that the registry and
 * the encoding agree.
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE 65536
#define MAX_RANGES 32
#define MAX_ASSIGNMENTS 256
#define MAX_STR 96

/* The id field is shifted left by one to make room for the criticality bit, so
 * the largest representable id is bounded by the 32-bit key rather than by the
 * registry's preference. */
#define MCL_EXTENSION_ID_MAX 2147483647L

static int failures = 0;

static void fail(const char *fmt, ...)
{
    va_list ap;
    ++failures;
    printf("  FAIL: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

typedef struct {
    long first;
    long last;
    char policy[MAX_STR];
} range_t;

typedef struct {
    long id;
    char name[MAX_STR];
    char status[MAX_STR];
} assignment_t;

static char g_buf[MAX_FILE_SIZE];

/*
 * A deliberately small scanner. This reads one known document shape, not
 * arbitrary JSON: a full parser here would be more code than the thing it
 * checks, and the failure it would catch -- malformed JSON -- is already caught
 * by every other consumer of the file.
 */
static const char *find_key(const char *from, const char *key)
{
    char pattern[MAX_STR];
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(from, pattern);
}

static int read_long_after(const char *at, const char *key, long *out)
{
    const char *k = find_key(at, key);
    const char *colon;
    if (k == NULL) {
        return 0;
    }
    colon = strchr(k, ':');
    if (colon == NULL) {
        return 0;
    }
    *out = strtol(colon + 1, NULL, 10);
    return 1;
}

static int read_string_after(const char *at, const char *key, char *out, size_t cap)
{
    const char *k = find_key(at, key);
    const char *colon, *open, *close;
    size_t len;

    if (k == NULL) {
        return 0;
    }
    colon = strchr(k, ':');
    if (colon == NULL) {
        return 0;
    }
    open = strchr(colon, '"');
    if (open == NULL) {
        return 0;
    }
    close = strchr(open + 1, '"');
    if (close == NULL) {
        return 0;
    }
    len = (size_t)(close - open - 1);
    if (len >= cap) {
        len = cap - 1u;
    }
    memcpy(out, open + 1, len);
    out[len] = '\0';
    return 1;
}

/*
 * Find the extent of the array that follows `key`, by counting brackets.
 *
 * Without this the scan for the next "id" runs off the end of an EMPTY array
 * and matches the word elsewhere in the document -- which it did, reporting a
 * phantom assignment 0 in a registry with none. A scanner that cannot tell
 * "no entries" from "the next entry is somewhere later in the file" is worse
 * than no scanner, because it fails loudest on the correct input.
 */
static int array_span(const char *at, const char *key,
                      const char **begin, const char **end)
{
    const char *k = find_key(at, key);
    const char *p;
    int depth = 0;

    if (k == NULL) {
        return 0;
    }
    p = strchr(k, '[');
    if (p == NULL) {
        return 0;
    }
    *begin = p;
    for (; *p != '\0'; ++p) {
        if (*p == '[') { ++depth; }
        else if (*p == ']') {
            --depth;
            if (depth == 0) {
                *end = p;
                return 1;
            }
        }
    }
    return 0;
}

static int compare_ranges(const void *a, const void *b)
{
    const range_t *ra = (const range_t *)a;
    const range_t *rb = (const range_t *)b;
    if (ra->first < rb->first) { return -1; }
    if (ra->first > rb->first) { return 1; }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    size_t size;
    const char *cursor;
    const char *ranges_start;
    const char *assignments_start;
    const char *assignments_begin = NULL;
    const char *assignments_end = NULL;
    range_t ranges[MAX_RANGES];
    assignment_t assignments[MAX_ASSIGNMENTS];
    size_t range_count = 0u, assignment_count = 0u, i;
    long declared_assigned = 0, declared_deprecated = 0, declared_total = 0;
    long counted_assigned = 0, counted_deprecated = 0;

    if (argc < 2) {
        printf("usage: %s <extension-ids-v0.1.json>\n", argv[0]);
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (f == NULL) {
        printf("cannot open %s\n", argv[1]);
        return 2;
    }
    size = fread(g_buf, 1u, sizeof(g_buf) - 1u, f);
    fclose(f);
    g_buf[size] = '\0';

    printf("Wire extension registry: %s\n\n", argv[1]);

    /* ---- ranges ---- */
    ranges_start = find_key(g_buf, "ranges");
    if (ranges_start == NULL) {
        fail("no `ranges` table");
        printf("\n1 problem.\n");
        return 1;
    }

    assignments_start = find_key(ranges_start, "assignments");
    cursor = ranges_start;
    while (range_count < MAX_RANGES) {
        const char *entry = find_key(cursor, "first");
        long first = 0, last = 0;

        if (entry == NULL) {
            break;
        }
        if (assignments_start != NULL && entry > assignments_start) {
            break;
        }
        if (!read_long_after(entry, "first", &first) ||
            !read_long_after(entry, "last", &last)) {
            fail("a range entry is missing `first` or `last`");
            break;
        }
        ranges[range_count].first = first;
        ranges[range_count].last = last;
        ranges[range_count].policy[0] = '\0';
        (void)read_string_after(entry, "policy", ranges[range_count].policy,
                                sizeof(ranges[range_count].policy));
        ++range_count;
        cursor = entry + 1;
    }

    printf("  %u ranges declared\n", (unsigned)range_count);
    if (range_count == 0u) {
        fail("the range table is empty, so no identifier has a policy");
    }

    qsort(ranges, range_count, sizeof(ranges[0]), compare_ranges);

    for (i = 0u; i < range_count; ++i) {
        if (ranges[i].last < ranges[i].first) {
            fail("range %ld..%ld runs backwards", ranges[i].first, ranges[i].last);
        }
    }

    /*
     * THE PARTITION CHECK. A gap is an identifier with no policy; an overlap is
     * an identifier with two. Both are found by the requester, not the author,
     * unless something checks here.
     */
    if (range_count > 0u) {
        if (ranges[0].first != 0) {
            fail("the range table starts at %ld, not 0", ranges[0].first);
        }
        for (i = 1u; i < range_count; ++i) {
            if (ranges[i].first == ranges[i - 1u].last + 1) {
                continue;
            }
            if (ranges[i].first <= ranges[i - 1u].last) {
                fail("ranges %ld..%ld and %ld..%ld overlap",
                     ranges[i - 1u].first, ranges[i - 1u].last,
                     ranges[i].first, ranges[i].last);
            } else {
                fail("identifiers %ld..%ld have no declared policy",
                     ranges[i - 1u].last + 1, ranges[i].first - 1);
            }
        }
        if (ranges[range_count - 1u].last != MCL_EXTENSION_ID_MAX) {
            fail("the range table ends at %ld; the id field ends at %ld",
                 ranges[range_count - 1u].last, MCL_EXTENSION_ID_MAX);
        }
    }

    /* Zero belongs to the encoding, not to policy. */
    for (i = 0u; i < range_count; ++i) {
        if (ranges[i].first <= 0 && ranges[i].last >= 0) {
            if (strcmp(ranges[i].policy, "Reserved") != 0) {
                fail("identifier 0 is in a `%s` range; it is permanently "
                     "reserved so a zeroed buffer cannot decode as an extension",
                     ranges[i].policy);
            }
        }
    }

    /* ---- assignments ---- */
    if (array_span(g_buf, "assignments", &assignments_begin, &assignments_end)) {
        cursor = assignments_begin;
        while (assignment_count < MAX_ASSIGNMENTS) {
            const char *entry = find_key(cursor, "id");
            long id = 0;

            if (entry == NULL || entry > assignments_end) {
                break;
            }
            if (!read_long_after(entry, "id", &id)) {
                break;
            }
            assignments[assignment_count].id = id;
            assignments[assignment_count].name[0] = '\0';
            assignments[assignment_count].status[0] = '\0';

            if (!read_string_after(entry, "name",
                                   assignments[assignment_count].name,
                                   sizeof(assignments[assignment_count].name))) {
                fail("assignment id %ld has no `name`", id);
            }
            if (!read_string_after(entry, "status",
                                   assignments[assignment_count].status,
                                   sizeof(assignments[assignment_count].status))) {
                fail("assignment id %ld has no `status`", id);
            }
            if (find_key(entry, "specification") == NULL) {
                fail("assignment id %ld has no permanent specification reference",
                     id);
            }
            if (find_key(entry, "criticality_guidance") == NULL) {
                fail("assignment id %ld does not say what rejecting it costs", id);
            }

            ++assignment_count;
            cursor = entry + 1;
        }
    }

    printf("  %u assignments\n", (unsigned)assignment_count);

    for (i = 0u; i < assignment_count; ++i) {
        size_t j;
        int inside = 0;

        if (assignments[i].id <= 0 || assignments[i].id > MCL_EXTENSION_ID_MAX) {
            fail("assignment id %ld is outside the encodable range 1..%ld",
                 assignments[i].id, MCL_EXTENSION_ID_MAX);
        }
        for (j = 0u; j < range_count; ++j) {
            if (assignments[i].id >= ranges[j].first &&
                assignments[i].id <= ranges[j].last) {
                inside = 1;
                if (strcmp(ranges[j].policy, "Reserved") == 0) {
                    fail("assignment id %ld falls in a Reserved range",
                         assignments[i].id);
                }
                break;
            }
        }
        if (!inside) {
            fail("assignment id %ld is in no declared range", assignments[i].id);
        }
        for (j = i + 1u; j < assignment_count; ++j) {
            if (assignments[i].id == assignments[j].id) {
                fail("identifier %ld is assigned twice; stable identifiers are "
                     "never reused", assignments[i].id);
            }
        }

        if (strcmp(assignments[i].status, "deprecated") == 0) {
            ++counted_deprecated;
        } else {
            ++counted_assigned;
        }
    }

    /* ---- summary ---- */
    (void)read_long_after(g_buf, "assigned", &declared_assigned);
    (void)read_long_after(g_buf, "deprecated", &declared_deprecated);
    (void)read_long_after(g_buf, "total", &declared_total);

    printf("\n  counted:  assigned %ld, deprecated %ld, total %u\n",
           counted_assigned, counted_deprecated, (unsigned)assignment_count);
    printf("  declared: assigned %ld, deprecated %ld, total %ld\n",
           declared_assigned, declared_deprecated, declared_total);

    if (declared_assigned != counted_assigned) {
        fail("summary claims %ld assigned, there are %ld",
             declared_assigned, counted_assigned);
    }
    if (declared_deprecated != counted_deprecated) {
        fail("summary claims %ld deprecated, there are %ld",
             declared_deprecated, counted_deprecated);
    }
    if (declared_total != (long)assignment_count) {
        fail("summary claims %ld total, there are %u",
             declared_total, (unsigned)assignment_count);
    }

    printf("\n");
    if (failures != 0) {
        printf("%d problem%s.\n", failures, (failures == 1) ? "" : "s");
        return 1;
    }

    printf("extension registry is self-consistent and partitions the id space.\n");
    if (assignment_count == 0u) {
        printf("NOTE: zero assignments. That is the intended state at v1.0 --\n"
               "      the mechanism and its governance are ready, and no\n"
               "      extension has yet earned a permanent number.\n");
    }
    return 0;
}
