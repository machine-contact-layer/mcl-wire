#include "mcl/extension.h"
#include <stdint.h>

#define MCL_TRY(expression) do { \
    const mcl_wire_status_t mcl_status__ = (expression); \
    if (mcl_status__ != MCL_WIRE_OK) { \
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

mcl_wire_status_t mcl_wire_uvarint_encode(
    uint32_t value,
    uint8_t *out,
    size_t out_capacity,
    size_t *written)
{
    uint8_t bytes[5];
    size_t count = 0u;
    size_t i;

    if (out == NULL || written == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    do {
        bytes[count] = (uint8_t)(value & 0x7fu);
        value >>= 7;
        ++count;
    } while (value != 0u);

    if (out_capacity < count) {
        return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
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
    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_uvarint_decode(
    const uint8_t *data,
    size_t data_size,
    uint32_t *value,
    size_t *consumed)
{
    uint32_t result = 0u;
    size_t i;

    if (data == NULL || value == NULL || consumed == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if (data_size == 0u) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    for (i = 0u; i < data_size && i < 5u; ++i) {
        const uint8_t byte = data[i];
        if (result > (UINT32_MAX >> 7)) {
            return MCL_WIRE_ERR_RANGE;
        }
        result = (result << 7) | (uint32_t)(byte & 0x7fu);

        if ((byte & 0x80u) == 0u) {
            const size_t length = i + 1u;
            if (mcl_uvarint_size(result) != length) {
                return MCL_WIRE_ERR_NONCANONICAL;
            }
            *value = result;
            *consumed = length;
            return MCL_WIRE_OK;
        }
    }

    if (data_size < 5u) {
        return MCL_WIRE_ERR_TRUNCATED;
    }
    return MCL_WIRE_ERR_RANGE;
}

mcl_wire_status_t mcl_wire_extensions_encode(
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
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
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
            return MCL_WIRE_ERR_RANGE;
        }
        if (i != 0u && extension->id <= previous_id) {
            return MCL_WIRE_ERR_NONCANONICAL;
        }

        key = (extension->id << 1) | (uint32_t)extension->critical;

        if (offset > out_capacity) {
            return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
        }
        MCL_TRY(mcl_wire_uvarint_encode(
            key, out + offset, out_capacity - offset, &field_size));
        offset += field_size;

        if (offset > out_capacity) {
            return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
        }
        MCL_TRY(mcl_wire_uvarint_encode(
            (uint32_t)extension->value_size,
            out + offset,
            out_capacity - offset,
            &field_size));
        offset += field_size;

        if (extension->value_size > out_capacity - offset) {
            return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
        }
        for (j = 0u; j < extension->value_size; ++j) {
            out[offset + j] = extension->value[j];
        }
        offset += extension->value_size;
        previous_id = extension->id;
    }

    *written = offset;
    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_extensions_encoded_size(
    const mcl_wire_extension_t *extensions,
    size_t extension_count,
    size_t *size)
{
    size_t total = 0u;
    size_t i;
    uint32_t previous_id = 0u;

    if ((extension_count != 0u && extensions == NULL) || size == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    for (i = 0u; i < extension_count; ++i) {
        const mcl_wire_extension_t *extension = &extensions[i];
        uint32_t key;
        size_t field;

        /* Validation identical to mcl_wire_extensions_encode, so that a
         * successful size call means the encode will succeed given the room.
         * The two must not drift; a size that disagreed with the encoder would
         * produce a length prefix that did not match the block after it. */
        if (extension->id == 0u || extension->id > (UINT32_MAX >> 1) ||
            extension->critical > 1u ||
            (extension->value_size != 0u && extension->value == NULL) ||
            extension->value_size > UINT32_MAX) {
            return MCL_WIRE_ERR_RANGE;
        }
        if (i != 0u && extension->id <= previous_id) {
            return MCL_WIRE_ERR_NONCANONICAL;
        }

        key = (extension->id << 1) | (uint32_t)extension->critical;
        field = mcl_uvarint_size(key) +
                mcl_uvarint_size((uint32_t)extension->value_size);

        /*
         * Bounded before every addition rather than after the sum, so the
         * total cannot wrap on a 32-bit size_t. This implementation never
         * encodes a block larger than the largest it will accept.
         */
        if (field > (size_t)MCL_WIRE_EXTENSION_BLOCK_MAX - total) {
            return MCL_WIRE_ERR_RANGE;
        }
        total += field;
        if (extension->value_size > (size_t)MCL_WIRE_EXTENSION_BLOCK_MAX - total) {
            return MCL_WIRE_ERR_RANGE;
        }
        total += extension->value_size;

        previous_id = extension->id;
    }

    *size = total;
    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_tier0_encode_ext(
    const mcl_wire_tier0_t *object,
    const mcl_wire_extension_t *extensions,
    size_t extension_count,
    uint8_t *out,
    size_t out_capacity,
    size_t *written)
{
    size_t body = 0u;
    size_t block = 0u;
    size_t length_size = 0u;
    size_t block_written = 0u;

    if (object == NULL || out == NULL || written == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    if (extension_count == 0u) {
        /*
         * The empty case has exactly one encoding, and it is the one without a
         * block. Emitting extension_present with a zero-length block would give
         * the same object two valid encodings, which is what a canonical format
         * exists to prevent.
         */
        return mcl_wire_tier0_encode(object, out, out_capacity, written);
    }
    if (extensions == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    MCL_TRY(mcl_wire_extensions_encoded_size(extensions, extension_count,
                                             &block));
    MCL_TRY(mcl_wire_tier0_encode(object, out, out_capacity, &body));

    MCL_TRY(mcl_wire_uvarint_encode((uint32_t)block, out + body,
                                    out_capacity - body, &length_size));

    MCL_TRY(mcl_wire_extensions_encode(extensions, extension_count,
                                       out + body + length_size,
                                       out_capacity - body - length_size,
                                       &block_written));
    if (block_written != block) {
        /* The size function and the encoder disagreed, which would leave a
         * length prefix describing a different block than the one written. */
        return MCL_WIRE_ERR_RANGE;
    }

    /* extension_present is bit 0 of the second header byte. Set after the body
     * is encoded, because mcl_wire_tier0_encode always clears it. */
    out[1] = (uint8_t)(out[1] | 0x01u);

    *written = body + length_size + block;
    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_tier0_decode_ext(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    mcl_wire_extension_reader_t *reader,
    size_t *consumed)
{
    /* No callback means nothing is accepted, so every critical extension is
     * refused. */
    return mcl_wire_tier0_decode_ext_accept(data, data_size, object, reader,
                                            NULL, NULL, consumed);
}

mcl_wire_status_t mcl_wire_tier0_decode_ext_accept(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    mcl_wire_extension_reader_t *reader,
    mcl_wire_extension_accept_fn accept,
    void *user,
    size_t *consumed)
{
    mcl_wire_header_t header;
    mcl_wire_extension_reader_t scan;
    mcl_wire_extension_t extension;
    size_t body = 0u;
    size_t length_size = 0u;
    uint32_t block_length = 0u;
    uint8_t has_extension = 0u;

    if (data == NULL || object == NULL || reader == NULL || consumed == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if (data_size < MCL_WIRE_COMMON_HEADER_SIZE) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    MCL_TRY(mcl_wire_header_decode(data, &header));
    MCL_TRY(mcl_wire_tier0_decode_body(data, data_size, object, &body));

    if (header.extension_present == 0u) {
        mcl_wire_extension_reader_init(reader, data + body, 0u);
        *consumed = body;
        return MCL_WIRE_OK;
    }

    MCL_TRY(mcl_wire_uvarint_decode(data + body, data_size - body,
                                    &block_length, &length_size));

    if (block_length == 0u) {
        /* extension_present with nothing in it. The empty case already has an
         * encoding, with the bit clear, so this one is not canonical. */
        return MCL_WIRE_ERR_NONCANONICAL;
    }
    if (block_length > MCL_WIRE_EXTENSION_BLOCK_MAX) {
        return MCL_WIRE_ERR_RANGE;
    }
    if ((size_t)block_length > data_size - body - length_size) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    /*
     * The whole block is validated before anything is returned. Validating
     * lazily during iteration would let a caller act on the object and the
     * first few extensions before discovering that a later critical one made
     * the object undecodable in the first place.
     */
    mcl_wire_extension_reader_init(&scan, data + body + length_size,
                                   (size_t)block_length);
    for (;;) {
        MCL_TRY(mcl_wire_extension_reader_next(&scan, &extension,
                                               &has_extension));
        if (has_extension == 0u) {
            break;
        }
        if (extension.critical != 0u) {
            /*
             * The caller decides what it accepts, and it sees the VALUE, not
             * just the id. CRITICAL means "do not act on this object unless you
             * understand this extension", and understanding one means
             * understanding its contents. Without a callback nothing is
             * accepted and the object is refused -- skipping it would turn the
             * sender's requirement into a suggestion.
             *
             * Non-critical extensions never reach here: an unknown one is
             * skipped by definition and stays readable through the reader.
             */
            if (accept == NULL ||
                accept(user, extension.id, extension.value,
                       extension.value_size) == 0u) {
                return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
            }
        }
    }

    mcl_wire_extension_reader_init(reader, data + body + length_size,
                                   (size_t)block_length);
    *consumed = body + length_size + (size_t)block_length;
    return MCL_WIRE_OK;
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

mcl_wire_status_t mcl_wire_extension_reader_next(
    mcl_wire_extension_reader_t *reader,
    mcl_wire_extension_t *extension,
    uint8_t *has_extension)
{
    uint32_t key;
    uint32_t value_size;
    size_t consumed;
    uint32_t id;

    if (reader == NULL || extension == NULL || has_extension == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if (reader->offset == reader->size) {
        *has_extension = 0u;
        return MCL_WIRE_OK;
    }
    if (reader->data == NULL || reader->offset > reader->size) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    MCL_TRY(mcl_wire_uvarint_decode(
        reader->data + reader->offset,
        reader->size - reader->offset,
        &key,
        &consumed));
    reader->offset += consumed;

    id = key >> 1;
    if (id == 0u) {
        return MCL_WIRE_ERR_NONCANONICAL;
    }
    if (reader->has_previous != 0u && id <= reader->previous_id) {
        return MCL_WIRE_ERR_NONCANONICAL;
    }

    MCL_TRY(mcl_wire_uvarint_decode(
        reader->data + reader->offset,
        reader->size - reader->offset,
        &value_size,
        &consumed));
    reader->offset += consumed;

    if ((size_t)value_size > reader->size - reader->offset) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    extension->id = id;
    extension->critical = (uint8_t)(key & 1u);
    extension->value = reader->data + reader->offset;
    extension->value_size = (size_t)value_size;

    reader->offset += (size_t)value_size;
    reader->previous_id = id;
    reader->has_previous = 1u;
    *has_extension = 1u;
    return MCL_WIRE_OK;
}
