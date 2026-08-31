#include "mcl/extension.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
        assert(mcl_wire_uvarint_encode(value, encoded, sizeof(encoded), &written) == MCL_OK);
        assert(mcl_wire_uvarint_decode(encoded, written, &decoded, &consumed) == MCL_OK);
        assert(decoded == value);
        assert(consumed == written);
    }

    {
        const uint8_t noncanonical[2] = {0x80u, 0x00u};
        uint32_t value;
        size_t consumed;
        assert(mcl_wire_uvarint_decode(noncanonical, sizeof(noncanonical), &value, &consumed) == MCL_ERR_NONCANONICAL);
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

        assert(mcl_wire_extensions_encode(input, count, encoded, sizeof(encoded), &written) == MCL_OK);
        mcl_wire_extension_reader_init(&reader, encoded, written);

        for (i = 0u; i < count; ++i) {
            mcl_wire_extension_t output;
            uint8_t has_extension = 0u;
            assert(mcl_wire_extension_reader_next(&reader, &output, &has_extension) == MCL_OK);
            assert(has_extension == 1u);
            assert(output.id == input[i].id);
            assert(output.critical == input[i].critical);
            assert(output.value_size == input[i].value_size);
            assert(memcmp(output.value, input[i].value, output.value_size) == 0);
        }

        {
            mcl_wire_extension_t output;
            uint8_t has_extension = 1u;
            assert(mcl_wire_extension_reader_next(&reader, &output, &has_extension) == MCL_OK);
            assert(has_extension == 0u);
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
    assert(mcl_wire_extensions_encode(duplicate, 2u, encoded, sizeof(encoded), &written) == MCL_ERR_NONCANONICAL);

    {
        const uint8_t truncated[] = {0x02u, 0x05u, 0x01u};
        mcl_wire_extension_reader_t reader;
        mcl_wire_extension_t extension;
        uint8_t has_extension;
        mcl_wire_extension_reader_init(&reader, truncated, sizeof(truncated));
        assert(mcl_wire_extension_reader_next(&reader, &extension, &has_extension) == MCL_ERR_TRUNCATED);
    }

    {
        const uint8_t optional_value[] = {'f','u','t','u','r','e'};
        const mcl_wire_extension_t optional = {7u, 0u, optional_value, sizeof(optional_value)};
        mcl_wire_extension_reader_t reader;
        mcl_wire_extension_t decoded;
        uint8_t has_extension;
        assert(mcl_wire_extensions_encode(&optional, 1u, encoded, sizeof(encoded), &written) == MCL_OK);
        mcl_wire_extension_reader_init(&reader, encoded, written);
        assert(mcl_wire_extension_reader_next(&reader, &decoded, &has_extension) == MCL_OK);
        assert(has_extension == 1u && decoded.id == 7u && decoded.critical == 0u);
    }

    {
        const uint8_t critical_value[] = {'m','u','s','t'};
        const mcl_wire_extension_t critical = {7u, 1u, critical_value, sizeof(critical_value)};
        mcl_wire_extension_reader_t reader;
        mcl_wire_extension_t decoded;
        uint8_t has_extension;
        assert(mcl_wire_extensions_encode(&critical, 1u, encoded, sizeof(encoded), &written) == MCL_OK);
        mcl_wire_extension_reader_init(&reader, encoded, written);
        assert(mcl_wire_extension_reader_next(&reader, &decoded, &has_extension) == MCL_OK);
        assert(has_extension == 1u && decoded.id == 7u && decoded.critical == 1u);
    }
}

int main(void)
{
    test_uvarint();
    test_extension_blocks();
    test_negative_cases();
    puts("uvarint random round trips: 10000 PASS");
    puts("extension block random round trips: 2000 PASS");
    puts("canonicality/length/criticality exposure: PASS");
    return 0;
}
