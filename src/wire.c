#include "mcl/wire.h"
#include <limits.h>

#if CHAR_BIT != 8
#error "MCL Wire requires 8-bit bytes"
#endif

typedef struct {
    uint8_t *buffer;
    size_t capacity_bits;
    size_t position;
} mcl_bit_writer_t;

typedef struct {
    const uint8_t *buffer;
    size_t size_bits;
    size_t position;
} mcl_bit_reader_t;

static void mcl_zero_bytes(void *ptr, size_t count)
{
    uint8_t *bytes = (uint8_t *)ptr;
    size_t i;
    for (i = 0u; i < count; ++i) {
        bytes[i] = 0u;
    }
}

static mcl_wire_status_t mcl_write_u(mcl_bit_writer_t *writer, uint32_t value, unsigned width)
{
    unsigned i;

    if (writer == NULL || width > 32u) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if ((width < 32u) && (value >= (1u << width))) {
        return MCL_WIRE_ERR_RANGE;
    }
    if (writer->position > writer->capacity_bits ||
        (size_t)width > (writer->capacity_bits - writer->position)) {
        return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
    }

    for (i = 0u; i < width; ++i) {
        const unsigned shift = width - 1u - i;
        const uint8_t bit = (uint8_t)((value >> shift) & 1u);
        const size_t byte_index = writer->position >> 3;
        const unsigned bit_index = 7u - (unsigned)(writer->position & 7u);

        if (bit != 0u) {
            writer->buffer[byte_index] = (uint8_t)(
                writer->buffer[byte_index] | (uint8_t)(1u << bit_index));
        }
        ++writer->position;
    }

    return MCL_WIRE_OK;
}

static mcl_wire_status_t mcl_write_s(mcl_bit_writer_t *writer, int32_t value, unsigned width)
{
    int32_t minimum;
    int32_t maximum;
    uint32_t encoded;

    if (width == 0u || width >= 32u) {
        return MCL_WIRE_ERR_RANGE;
    }

    minimum = -(int32_t)(1u << (width - 1u));
    maximum = (int32_t)((1u << (width - 1u)) - 1u);
    if (value < minimum || value > maximum) {
        return MCL_WIRE_ERR_RANGE;
    }

    if (value < 0) {
        const uint32_t magnitude = (uint32_t)(-value);
        encoded = (1u << width) - magnitude;
    } else {
        encoded = (uint32_t)value;
    }

    return mcl_write_u(writer, encoded, width);
}

static mcl_wire_status_t mcl_read_u(mcl_bit_reader_t *reader, unsigned width, uint32_t *value)
{
    unsigned i;
    uint32_t result = 0u;

    if (reader == NULL || value == NULL || width > 32u) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if (reader->position > reader->size_bits ||
        (size_t)width > (reader->size_bits - reader->position)) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    for (i = 0u; i < width; ++i) {
        const size_t byte_index = reader->position >> 3;
        const unsigned bit_index = 7u - (unsigned)(reader->position & 7u);
        result = (result << 1) |
            (uint32_t)((reader->buffer[byte_index] >> bit_index) & 1u);
        ++reader->position;
    }

    *value = result;
    return MCL_WIRE_OK;
}

static mcl_wire_status_t mcl_read_s(mcl_bit_reader_t *reader, unsigned width, int32_t *value)
{
    uint32_t encoded;
    mcl_wire_status_t status;

    if (width == 0u || width >= 32u || value == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    status = mcl_read_u(reader, width, &encoded);
    if (status != MCL_WIRE_OK) {
        return status;
    }

    if ((encoded & (1u << (width - 1u))) != 0u) {
        *value = (int32_t)(encoded - (1u << width));
    } else {
        *value = (int32_t)encoded;
    }

    return MCL_WIRE_OK;
}

static mcl_wire_status_t mcl_require_zero_padding(mcl_bit_reader_t *reader)
{
    if (reader == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    while (reader->position < reader->size_bits) {
        const size_t byte_index = reader->position >> 3;
        const unsigned bit_index = 7u - (unsigned)(reader->position & 7u);
        if (((reader->buffer[byte_index] >> bit_index) & 1u) != 0u) {
            return MCL_WIRE_ERR_NONCANONICAL;
        }
        ++reader->position;
    }

    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_header_encode(
    const mcl_wire_header_t *header,
    uint8_t out[MCL_WIRE_COMMON_HEADER_SIZE])
{
    if (header == NULL || out == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if (header->major_version > 15u ||
        header->category > 15u ||
        header->opcode > 31u ||
        header->priority > 3u ||
        header->extension_present > 1u) {
        return MCL_WIRE_ERR_RANGE;
    }

    out[0] = (uint8_t)((header->major_version << 4) | header->category);
    out[1] = (uint8_t)((header->opcode << 3) |
                       (header->priority << 1) |
                       header->extension_present);
    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_header_decode(
    const uint8_t in[MCL_WIRE_COMMON_HEADER_SIZE],
    mcl_wire_header_t *header)
{
    if (in == NULL || header == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    header->major_version = (uint8_t)(in[0] >> 4);
    header->category = (uint8_t)(in[0] & 0x0fu);
    header->opcode = (uint8_t)(in[1] >> 3);
    header->priority = (uint8_t)((in[1] >> 1) & 0x03u);
    header->extension_present = (uint8_t)(in[1] & 0x01u);
    return MCL_WIRE_OK;
}

static void mcl_kind_to_code(
    mcl_wire_kind_t kind,
    uint8_t *category,
    uint8_t *opcode)
{
    switch (kind) {
    case MCL_WIRE_KIND_PRESENCE:
        *category = 0u;
        *opcode = 0u;
        break;
    case MCL_WIRE_KIND_HAZARD:
        *category = 3u;
        *opcode = 1u;
        break;
    case MCL_WIRE_KIND_REQUEST:
        *category = 5u;
        *opcode = 1u;
        break;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        *category = 1u;
        *opcode = 1u;
        break;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        *category = 6u;
        *opcode = 1u;
        break;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        *category = 8u;
        *opcode = 0u;
        break;
    default:
        *category = 0xffu;
        *opcode = 0xffu;
        break;
    }
}

static mcl_wire_status_t mcl_code_to_kind(
    uint8_t category,
    uint8_t opcode,
    mcl_wire_kind_t *kind)
{
    if (kind == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    if (category == 0u && opcode == 0u) {
        *kind = MCL_WIRE_KIND_PRESENCE;
    } else if (category == 3u && opcode == 1u) {
        *kind = MCL_WIRE_KIND_HAZARD;
    } else if (category == 5u && opcode == 1u) {
        *kind = MCL_WIRE_KIND_REQUEST;
    } else if (category == 1u && opcode == 1u) {
        *kind = MCL_WIRE_KIND_AUTHORITY_CLAIM;
    } else if (category == 6u && opcode == 1u) {
        *kind = MCL_WIRE_KIND_DEGRADED_STATE;
    } else if (category == 8u && opcode == 0u) {
        *kind = MCL_WIRE_KIND_TRANSPORT_OFFER;
    } else {
        return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
    }

    return MCL_WIRE_OK;
}

size_t mcl_wire_tier0_encoded_size(mcl_wire_kind_t kind)
{
    switch (kind) {
    case MCL_WIRE_KIND_PRESENCE:
        return 11u;
    case MCL_WIRE_KIND_HAZARD:
        return 15u;
    case MCL_WIRE_KIND_REQUEST:
        return 17u;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        return 14u;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        return 10u;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        return 13u;
    default:
        return 0u;
    }
}

#define MCL_TRY(expression) do { \
    const mcl_wire_status_t mcl_status__ = (expression); \
    if (mcl_status__ != MCL_WIRE_OK) { \
        return mcl_status__; \
    } \
} while (0)

mcl_wire_status_t mcl_wire_tier0_encode(
    const mcl_wire_tier0_t *object,
    uint8_t *out,
    size_t out_capacity,
    size_t *written)
{
    mcl_bit_writer_t writer;
    uint8_t category;
    uint8_t opcode;
    mcl_wire_header_t header;
    uint8_t header_bytes[MCL_WIRE_COMMON_HEADER_SIZE];
    size_t required;

    if (object == NULL || out == NULL || written == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }

    required = mcl_wire_tier0_encoded_size(object->kind);
    if (required == 0u) {
        return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
    }
    if (out_capacity < required) {
        return MCL_WIRE_ERR_BUFFER_TOO_SMALL;
    }

    mcl_zero_bytes(out, required);
    mcl_kind_to_code(object->kind, &category, &opcode);

    header.major_version = MCL_WIRE_EXPERIMENTAL_MAJOR;
    header.category = category;
    header.opcode = opcode;
    header.priority = object->priority;
    header.extension_present = 0u;

    MCL_TRY(mcl_wire_header_encode(&header, header_bytes));
    out[0] = header_bytes[0];
    out[1] = header_bytes[1];

    writer.buffer = out;
    writer.capacity_bits = required * 8u;
    writer.position = 16u;

    MCL_TRY(mcl_write_u(&writer, object->source_ref, 32u));

    switch (object->kind) {
    case MCL_WIRE_KIND_PRESENCE:
        MCL_TRY(mcl_write_u(&writer, object->body.presence.machine_class, 8u));
        MCL_TRY(mcl_write_u(&writer, object->body.presence.capability_digest, 24u));
        MCL_TRY(mcl_write_u(&writer, object->body.presence.ttl, 8u));
        break;
    case MCL_WIRE_KIND_HAZARD:
        MCL_TRY(mcl_write_u(&writer, object->body.hazard.hazard_class, 8u));
        MCL_TRY(mcl_write_u(&writer, object->body.hazard.severity, 3u));
        MCL_TRY(mcl_write_u(&writer, object->body.hazard.confidence, 7u));
        MCL_TRY(mcl_write_s(&writer, object->body.hazard.x, 12u));
        MCL_TRY(mcl_write_s(&writer, object->body.hazard.y, 12u));
        MCL_TRY(mcl_write_s(&writer, object->body.hazard.z, 10u));
        MCL_TRY(mcl_write_u(&writer, object->body.hazard.radius, 10u));
        MCL_TRY(mcl_write_u(&writer, object->body.hazard.ttl, 8u));
        break;
    case MCL_WIRE_KIND_REQUEST:
        MCL_TRY(mcl_write_u(&writer, object->body.request.request_class, 8u));
        MCL_TRY(mcl_write_u(&writer, object->body.request.target_ref, 32u));
        MCL_TRY(mcl_write_s(&writer, object->body.request.x, 12u));
        MCL_TRY(mcl_write_s(&writer, object->body.request.y, 12u));
        MCL_TRY(mcl_write_u(&writer, object->body.request.radius, 10u));
        MCL_TRY(mcl_write_u(&writer, object->body.request.ttl, 8u));
        break;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        MCL_TRY(mcl_write_u(&writer, object->body.authority_claim.authority_class, 6u));
        MCL_TRY(mcl_write_u(&writer, object->body.authority_claim.jurisdiction, 12u));
        MCL_TRY(mcl_write_u(&writer, object->body.authority_claim.credential_ref, 32u));
        MCL_TRY(mcl_write_u(&writer, object->body.authority_claim.validity, 8u));
        break;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        MCL_TRY(mcl_write_u(&writer, object->body.degraded_state.affected_capability, 8u));
        MCL_TRY(mcl_write_u(&writer, object->body.degraded_state.health, 7u));
        MCL_TRY(mcl_write_u(&writer, object->body.degraded_state.severity, 3u));
        MCL_TRY(mcl_write_u(&writer, object->body.degraded_state.ttl, 8u));
        break;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        MCL_TRY(mcl_write_u(&writer, object->body.transport_offer.transport_id, 8u));
        MCL_TRY(mcl_write_u(&writer, object->body.transport_offer.profile_id, 8u));
        MCL_TRY(mcl_write_u(&writer, object->body.transport_offer.endpoint_token, 32u));
        MCL_TRY(mcl_write_u(&writer, object->body.transport_offer.validity, 8u));
        break;
    default:
        return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
    }

    *written = required;
    return MCL_WIRE_OK;
}

mcl_wire_status_t mcl_wire_tier0_decode(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    size_t *consumed)
{
    mcl_bit_reader_t reader;
    mcl_wire_header_t header;
    mcl_wire_kind_t kind;
    uint32_t unsigned_value;
    int32_t signed_value;
    size_t required;

    if (data == NULL || object == NULL || consumed == NULL) {
        return MCL_WIRE_ERR_INVALID_ARGUMENT;
    }
    if (data_size < MCL_WIRE_COMMON_HEADER_SIZE) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    MCL_TRY(mcl_wire_header_decode(data, &header));
    if (header.major_version != MCL_WIRE_EXPERIMENTAL_MAJOR ||
        header.extension_present != 0u) {
        return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
    }

    MCL_TRY(mcl_code_to_kind(header.category, header.opcode, &kind));
    required = mcl_wire_tier0_encoded_size(kind);
    if (data_size < required) {
        return MCL_WIRE_ERR_TRUNCATED;
    }

    mcl_zero_bytes(object, sizeof(*object));
    object->kind = kind;
    object->priority = header.priority;

    reader.buffer = data;
    reader.size_bits = required * 8u;
    reader.position = 16u;

    MCL_TRY(mcl_read_u(&reader, 32u, &object->source_ref));

    switch (kind) {
    case MCL_WIRE_KIND_PRESENCE:
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.presence.machine_class = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 24u, &unsigned_value));
        object->body.presence.capability_digest = unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.presence.ttl = (uint8_t)unsigned_value;
        break;
    case MCL_WIRE_KIND_HAZARD:
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.hazard.hazard_class = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 3u, &unsigned_value));
        object->body.hazard.severity = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 7u, &unsigned_value));
        object->body.hazard.confidence = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_s(&reader, 12u, &signed_value));
        object->body.hazard.x = (int16_t)signed_value;
        MCL_TRY(mcl_read_s(&reader, 12u, &signed_value));
        object->body.hazard.y = (int16_t)signed_value;
        MCL_TRY(mcl_read_s(&reader, 10u, &signed_value));
        object->body.hazard.z = (int16_t)signed_value;
        MCL_TRY(mcl_read_u(&reader, 10u, &unsigned_value));
        object->body.hazard.radius = (uint16_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.hazard.ttl = (uint8_t)unsigned_value;
        break;
    case MCL_WIRE_KIND_REQUEST:
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.request.request_class = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 32u, &object->body.request.target_ref));
        MCL_TRY(mcl_read_s(&reader, 12u, &signed_value));
        object->body.request.x = (int16_t)signed_value;
        MCL_TRY(mcl_read_s(&reader, 12u, &signed_value));
        object->body.request.y = (int16_t)signed_value;
        MCL_TRY(mcl_read_u(&reader, 10u, &unsigned_value));
        object->body.request.radius = (uint16_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.request.ttl = (uint8_t)unsigned_value;
        break;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        MCL_TRY(mcl_read_u(&reader, 6u, &unsigned_value));
        object->body.authority_claim.authority_class = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 12u, &unsigned_value));
        object->body.authority_claim.jurisdiction = (uint16_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 32u, &object->body.authority_claim.credential_ref));
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.authority_claim.validity = (uint8_t)unsigned_value;
        break;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.degraded_state.affected_capability = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 7u, &unsigned_value));
        object->body.degraded_state.health = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 3u, &unsigned_value));
        object->body.degraded_state.severity = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.degraded_state.ttl = (uint8_t)unsigned_value;
        break;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.transport_offer.transport_id = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.transport_offer.profile_id = (uint8_t)unsigned_value;
        MCL_TRY(mcl_read_u(&reader, 32u, &object->body.transport_offer.endpoint_token));
        MCL_TRY(mcl_read_u(&reader, 8u, &unsigned_value));
        object->body.transport_offer.validity = (uint8_t)unsigned_value;
        break;
    default:
        return MCL_WIRE_ERR_UNSUPPORTED_SEMANTIC;
    }

    MCL_TRY(mcl_require_zero_padding(&reader));
    *consumed = required;
    return MCL_WIRE_OK;
}
