#ifndef MCL_WIRE_EXTENSION_H
#define MCL_WIRE_EXTENSION_H

#include "mcl/wire.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    uint8_t critical;
    const uint8_t *value;
    size_t value_size;
} mcl_wire_extension_t;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
    uint32_t previous_id;
    uint8_t has_previous;
} mcl_wire_extension_reader_t;

mcl_wire_status_t mcl_wire_uvarint_encode(
    uint32_t value,
    uint8_t *out,
    size_t out_capacity,
    size_t *written);

mcl_wire_status_t mcl_wire_uvarint_decode(
    const uint8_t *data,
    size_t data_size,
    uint32_t *value,
    size_t *consumed);

mcl_wire_status_t mcl_wire_extensions_encode(
    const mcl_wire_extension_t *extensions,
    size_t extension_count,
    uint8_t *out,
    size_t out_capacity,
    size_t *written);

void mcl_wire_extension_reader_init(
    mcl_wire_extension_reader_t *reader,
    const uint8_t *data,
    size_t data_size);

mcl_wire_status_t mcl_wire_extension_reader_next(
    mcl_wire_extension_reader_t *reader,
    mcl_wire_extension_t *extension,
    uint8_t *has_extension);

#ifdef __cplusplus
}
#endif

#endif
