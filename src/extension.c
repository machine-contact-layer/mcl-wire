#include "mcl/extension.h"
#include <stdint.h>

#define MCL_TRY(expression) do { \
    const mcl_status_t mcl_status__ = (expression); \
    if (mcl_status__ != MCL_OK) { \
        return mcl_status__; \
    } \
} while (0)

static size_t mcl_uvarint_size(uint32_t value)
{
    size_t size = 1u;
    while (value >= 0x80u) {
        value >>= 7;
        ++size;
    }
    return size;
}

mcl_status_t mcl_wire_uvarint_encode(
    uint32_t value,
    uint8_t *out,
    size_t out_capacity,
    size_t *written)
{
    uint8_t bytes[5];
    size_t count = 0u;
    size_t i;

    if (out == NULL || written == NULL) {
        return MCL_ERR_INVALID_ARGUMENT;
    }

    do {
        bytes[count] = (uint8_t)(value & 0x7fu);
        value >>= 7;
        ++count;
    } while (value != 0u);

    if (out_capacity < count) {
        return MCL_ERR_BUFFER_TOO_SMALL;
    }

    for (i = 0u; i < count; ++i) {
        const size_t source = count - 1u - i;
        uint8_t byte = bytes[source];
        if (i + 1u < count) {
            byte = (uint8_t)(byte | 0x80u);
        }
        out[i] = byte;
    }

    *written = count;
    return MCL_OK;
}

mcl_status_t mcl_wire_uvarint_decode(
    const uint8_t *data,
    size_t data_size,
    uint32_t *value,
    size_t *consumed)
{
    uint32_t result = 0u;
    size_t i;

    if (data == NULL || value == NULL || consumed == NULL) {
        return MCL_ERR_INVALID_ARGUMENT;
    }
    if (data_size == 0u) {
        return MCL_ERR_TRUNCATED;
    }

    for (i = 0u; i < data_size && i < 5u; ++i) {
        const uint8_t byte = data[i];
        if (result > (UINT32_MAX >> 7)) {
            return MCL_ERR_RANGE;
        }
        result = (result << 7) | (uint32_t)(byte & 0x7fu);

        if ((byte & 0x80u) == 0u) {
            const size_t length = i + 1u;
            if (mcl_uvarint_size(result) != length) {
                return MCL_ERR_NONCANONICAL;
            }
            *value = result;
            *consumed = length;
            return MCL_OK;
        }
    }

    if (data_size < 5u) {
        return MCL_ERR_TRUNCATED;
    }
    return MCL_ERR_RANGE;
}

mcl_status_t mcl_wire_extensions_encode(
    const mcl_wire_extension_t *extensions,
    size_t extension_count,
    uint8_t *out,
    size_t out_capacity,
    size_t *written)
{
    size_t offset = 0u;
    size_t i;
    uint32_t previous_id = 0u;

    if ((extension_count != 0u && extensions == NULL) || out == NULL || written == NULL) {
        return MCL_ERR_INVALID_ARGUMENT;
    }

    for (i = 0u; i < extension_count; ++i) {
        const mcl_wire_extension_t *extension = &extensions[i];
        uint32_t key;
        size_t field_size;
        size_t j;

        if (extension->id == 0u || extension->id > (UINT32_MAX >> 1) ||
            extension->critical > 1u ||
            (extension->value_size != 0u && extension->value == NULL) ||
            extension->value_size > UINT32_MAX) {
            return MCL_ERR_RANGE;
        }
        if (i != 0u && extension->id <= previous_id) {
            return MCL_ERR_NONCANONICAL;
        }

        key = (extension->id << 1) | (uint32_t)extension->critical;

        if (offset > out_capacity) {
            return MCL_ERR_BUFFER_TOO_SMALL;
        }
        MCL_TRY(mcl_wire_uvarint_encode(
            key, out + offset, out_capacity - offset, &field_size));
        offset += field_size;

        if (offset > out_capacity) {
            return MCL_ERR_BUFFER_TOO_SMALL;
        }
        MCL_TRY(mcl_wire_uvarint_encode(
            (uint32_t)extension->value_size,
            out + offset,
            out_capacity - offset,
            &field_size));
        offset += field_size;

        if (extension->value_size > out_capacity - offset) {
            return MCL_ERR_BUFFER_TOO_SMALL;
        }
        for (j = 0u; j < extension->value_size; ++j) {
            out[offset + j] = extension->value[j];
        }
        offset += extension->value_size;
        previous_id = extension->id;
    }

    *written = offset;
    return MCL_OK;
}

void mcl_wire_extension_reader_init(
    mcl_wire_extension_reader_t *reader,
    const uint8_t *data,
    size_t data_size)
{
    if (reader == NULL) {
        return;
    }

    reader->data = data;
    reader->size = data_size;
    reader->offset = 0u;
    reader->previous_id = 0u;
    reader->has_previous = 0u;
}

mcl_status_t mcl_wire_extension_reader_next(
    mcl_wire_extension_reader_t *reader,
    mcl_wire_extension_t *extension,
    uint8_t *has_extension)
{
    uint32_t key;
    uint32_t value_size;
    size_t consumed;
    uint32_t id;

    if (reader == NULL || extension == NULL || has_extension == NULL) {
        return MCL_ERR_INVALID_ARGUMENT;
    }
    if (reader->offset == reader->size) {
        *has_extension = 0u;
        return MCL_OK;
    }
    if (reader->data == NULL || reader->offset > reader->size) {
        return MCL_ERR_TRUNCATED;
    }

    MCL_TRY(mcl_wire_uvarint_decode(
        reader->data + reader->offset,
        reader->size - reader->offset,
        &key,
        &consumed));
    reader->offset += consumed;

    id = key >> 1;
    if (id == 0u) {
        return MCL_ERR_NONCANONICAL;
    }
    if (reader->has_previous != 0u && id <= reader->previous_id) {
        return MCL_ERR_NONCANONICAL;
    }

    MCL_TRY(mcl_wire_uvarint_decode(
        reader->data + reader->offset,
        reader->size - reader->offset,
        &value_size,
        &consumed));
    reader->offset += consumed;

    if ((size_t)value_size > reader->size - reader->offset) {
        return MCL_ERR_TRUNCATED;
    }

    extension->id = id;
    extension->critical = (uint8_t)(key & 1u);
    extension->value = reader->data + reader->offset;
    extension->value_size = (size_t)value_size;

    reader->offset += (size_t)value_size;
    reader->previous_id = id;
    reader->has_previous = 1u;
    *has_extension = 1u;
    return MCL_OK;
}
