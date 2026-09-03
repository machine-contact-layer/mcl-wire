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

/*
 * Exact encoded size of an extension block, or an error if the block is not
 * encodable. Applies the same validation as mcl_wire_extensions_encode, so a
 * successful size call means the encode will succeed given the room.
 */
mcl_wire_status_t mcl_wire_extensions_encoded_size(
    const mcl_wire_extension_t *extensions,
    size_t extension_count,
    size_t *size);

/* ============================================================
 * Extensions on Tier-0 objects.
 *
 * The extension framework above has existed since early in the project and was
 * unreachable: mcl_wire_tier0_encode hard-set extension_present to 0 and
 * mcl_wire_tier0_decode refused any object that set it. The mechanism was
 * implemented, tested, and impossible to use, which is worse than not having
 * it -- the manifest recorded "extensions supported" for a capability no
 * caller could reach.
 *
 * CANONICAL LAYOUT
 *
 *   [2-byte common header, extension_present = 1]
 *   [fixed Tier-0 body, exactly as without extensions]
 *   [uvarint block_length]
 *   [block_length bytes of extension TLVs]
 *
 * The block carries its own length so that a Tier-0 object stays
 * SELF-DELIMITING. Without it, an object could only be decoded where an outer
 * boundary already exists -- true for a Link frame, false for the raw-Wire
 * path that MCL-AP uses, where bytes arrive from the air with no envelope.
 *
 * CANONICAL FORM
 *
 * There is exactly one encoding of any object. An empty extension list encodes
 * with extension_present = 0 and no block, never as a zero-length block;
 * ids are strictly increasing, so a set of extensions has one order; uvarints
 * are minimal-length. Two implementations encoding the same object produce the
 * same bytes, which is what makes the vectors meaningful.
 *
 * CRITICALITY
 *
 * An unknown CRITICAL extension makes the whole object undecodable. An unknown
 * non-critical extension is skipped and remains readable through the reader.
 * This implementation currently recognises NO extension ids -- none are
 * registered -- so every critical extension is rejected. That is the honest
 * behaviour rather than a limitation to work around: an id becomes known by
 * being registered and implemented, not by being tolerated.
 * ============================================================ */

/*
 * Largest extension block this implementation will encode or accept.
 *
 * A bound is required because the decoder hands out borrowed pointers into a
 * caller-owned buffer and must never invite an allocation. 256 bytes is
 * comfortably more than the fixed bodies it accompanies and still fits the
 * transports MCL actually runs on; the Link frame's own payload limit is what
 * governs above this.
 */
#define MCL_WIRE_EXTENSION_BLOCK_MAX 256u

/* Largest encodable Tier-0 object once extensions are included: the biggest
 * fixed body, the two-byte uvarint that can describe a 256-byte block, and the
 * block itself. */
#define MCL_WIRE_TIER0_EXT_MAX_SIZE \
    (MCL_WIRE_TIER0_MAX_SIZE + 2u + MCL_WIRE_EXTENSION_BLOCK_MAX)

/*
 * Encode a Tier-0 object with extensions.
 *
 * `extension_count` of 0 produces bytes identical to mcl_wire_tier0_encode,
 * with extension_present clear. Extensions must be supplied with strictly
 * increasing ids; anything else is refused as non-canonical rather than
 * silently sorted, because sorting would let two callers disagree about what
 * they sent while both believing they succeeded.
 */
mcl_wire_status_t mcl_wire_tier0_encode_ext(
    const mcl_wire_tier0_t *object,
    const mcl_wire_extension_t *extensions,
    size_t extension_count,
    uint8_t *out,
    size_t out_capacity,
    size_t *written);

/*
 * Does this caller understand extension `extension_id`?
 *
 * Returns 1 for an id the caller implements, 0 otherwise. Called during
 * decoding for every CRITICAL extension in the block; a 0 makes the whole
 * object undecodable.
 *
 * WHY THIS IS A CALLBACK AND NOT A TABLE
 *
 * "Known" is a property of the implementation doing the decoding, not of the
 * codec. Two programs linking this same library legitimately support different
 * extension sets, and a library-owned table would force them to agree. It would
 * also have to live somewhere, and this library has no heap and no globals.
 *
 * A predicate keeps the library ignorant of which extensions exist -- which is
 * the correct division, because the registry is a governance artifact and the
 * codec is not. It also means adding an extension registry later requires no
 * change to this decode contract, which is exactly the problem this closes:
 * before it existed, every critical extension was refused unconditionally and
 * there was no way to ever accept one without changing the API.
 *
 * It MUST be a pure predicate. It is called during validation, before the
 * object is returned, and must not decode, allocate, or act on anything.
 */
typedef uint8_t (*mcl_wire_extension_known_fn)(
    void *user,
    uint32_t extension_id);

/*
 * Decode a Tier-0 object that may carry extensions.
 *
 * On success `reader` is positioned at the start of the extension block, empty
 * when the object carried none, and borrows from `data` -- so it stays valid
 * only as long as `data` does. `consumed` reports the whole object including
 * the block, which is what lets successive objects be decoded from one buffer.
 *
 * The block is fully validated before this returns: every TLV is well formed,
 * ids strictly increase, lengths fit, and no CRITICAL extension is present.
 * Validating during iteration instead would let a caller act on the object and
 * the first few extensions before discovering that a later critical one made
 * the whole thing undecodable.
 */
mcl_wire_status_t mcl_wire_tier0_decode_ext(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    mcl_wire_extension_reader_t *reader,
    size_t *consumed);

/*
 * As above, but the caller states which extension ids it understands.
 *
 * `known` is consulted for every critical extension. Passing NULL means the
 * caller understands none, which is what mcl_wire_tier0_decode_ext does and why
 * that function refuses every critical extension. Non-critical extensions never
 * consult it -- an unknown one is skipped by definition and stays readable
 * through the reader.
 *
 * The whole block is still validated before the object is returned, so a
 * critical extension the caller does not know makes the object undecodable even
 * if it appears after ones the caller does know. Discovering that halfway
 * through iteration would be too late: the caller would already have acted.
 */
mcl_wire_status_t mcl_wire_tier0_decode_ext_known(
    const uint8_t *data,
    size_t data_size,
    mcl_wire_tier0_t *object,
    mcl_wire_extension_reader_t *reader,
    mcl_wire_extension_known_fn known,
    void *user,
    size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif
