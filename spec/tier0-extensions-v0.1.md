# Tier-0 Extensions v0.1

**Status:** Research Draft
**Layer:** Wire
**Reference implementation:** `mcl-wire/include/mcl/extension.h`, `mcl-wire/src/extension.c`
**Vectors:** `mcl-wire/conformance/vectors/extensions-v0.1.json`

This document is normative for the bytes. Where it and the reference
implementation disagree, this document is correct and the code is a defect.

---

## 1. What this closes

The extension framework — uvarint coding, a TLV envelope, explicit criticality,
a forward-only reader — has existed since early in this project. It was also
**unreachable**: `mcl_wire_tier0_encode` hard-set `extension_present` to 0, and
`mcl_wire_tier0_decode` refused any object that set it.

The mechanism was implemented, unit-tested, and impossible to use from the
public API. That is worse than not having it, because the release manifest
recorded extension support for a capability no caller could reach. Charter
invariant 6 requires extensions with explicit criticality; a framework nothing
can call does not satisfy it.

## 2. Canonical layout

```
[ 2-byte common header, extension_present = 1 ]
[ fixed Tier-0 body, byte-identical to the same object without extensions ]
[ uvarint  block_length ]
[ block_length bytes of extension TLVs ]
```

Each TLV is:

```
uvarint key         = (id << 1) | critical
uvarint value_length
value_length bytes
```

`id` is 1..2³¹-1. Zero is permanently reserved, so a zeroed buffer can never
decode as an extension.

### 2.1 Why the block carries its own length

A Tier-0 object must be **self-delimiting**: its total length knowable from its
own bytes.

Without `block_length`, an object could only be decoded where an outer boundary
already exists. That is true inside a Link frame, whose `payload_len` states
where the payload ends. It is **not** true on the raw-Wire path, which is what
MCL-AP carries: bytes arrive from the air with no envelope around them, and a
decoder that needed to be told where the object stopped could not work there at
all. One extra byte in the common case buys carriage on the transport MCL was
built to reach first.

## 3. Canonical form

There is exactly one valid encoding of any object.

- An empty extension list encodes with `extension_present = 0` and no block.
  A zero-length block with the bit set is **non-canonical and rejected**,
  because the empty case already has an encoding.
- Extension ids are **strictly increasing**. A set of extensions therefore has
  exactly one order.
- Uvarints are minimal-length; a longer encoding of the same value is rejected.
- The fixed body is byte-identical to the same object encoded without
  extensions, so adding this capability changed no existing vector.

An encoder given out-of-order or duplicate ids **rejects them rather than
sorting**. Sorting would let two callers disagree about what they sent while
both believed they had succeeded.

## 4. Criticality

| | Known id | Unknown id |
|---|---|---|
| **critical** | processed | **whole object rejected** |
| **non-critical** | processed | skipped, still readable |

An unknown critical extension makes the entire object undecodable. The sender
has said the object must not be acted on without it, and returning the body
while dropping the extension would turn *you must understand this* into *you may
ignore this*.

**This implementation recognises no extension ids**, because none are
registered. Every critical extension is therefore unknown to it and every object
carrying one is refused. That is the correct behaviour and not a gap to work
around: an id becomes known by being registered and implemented, not by being
tolerated.

### 4.1 Validation is complete before anything is returned

A decoder MUST validate the whole block — every TLV well formed, ids strictly
increasing, lengths within the block, no unknown critical extension — **before**
returning the object.

Validating lazily during iteration would let a caller act on the object and the
first few extensions before discovering that a later critical one meant the
object was never decodable.

## 5. Bounds

`MCL_WIRE_EXTENSION_BLOCK_MAX` is 256 bytes in this implementation.

A bound is required because the decoder hands out pointers borrowed from a
caller-owned buffer and must never invite an allocation. The value is
comfortably larger than the fixed bodies it accompanies and small enough for the
transports MCL runs on; above it, the Link frame's own payload limit governs.

A block longer than the maximum is refused **before** any TLV in it is read.

## 6. Decoder obligations

| Condition | Result |
|---|---|
| Buffer shorter than the fixed body | truncated |
| `extension_present` set, block length absent or short | truncated |
| Block length 0 with the bit set | non-canonical |
| Block length above the implementation maximum | rejected |
| Non-minimal uvarint | non-canonical |
| id 0, or an id not greater than the previous | non-canonical |
| Value length exceeding the block | truncated |
| Unknown **critical** extension | unsupported semantic |
| Trailing bytes inside the block after the last TLV | truncated |

A decoder that cannot read extensions at all MUST refuse an object whose header
sets `extension_present`, rather than decoding the body and discarding the rest.
`mcl_wire_tier0_decode` does exactly this; `mcl_wire_tier0_decode_ext` reads
them.

## 7. Registry policy

No extension id is assigned. Before any id becomes normative, the registry must
state: owner, range policy (Standards Action / Specification Required /
Experimental / Private Use), application procedure, change controller,
provisional-to-permanent promotion, deprecation behaviour, and the no-reuse
rule.

Until then, extensions are usable only between parties that have agreed an id
out of band, and neither party may claim interoperability from it.

## 8. What this does not do

- No extension is defined. The framework carries them; it names none.
- Extensions are not authenticated. A block crosses the medium in the clear like
  everything else in MCL, and `frame_check` is a CRC, not integrity. An
  extension is data a peer sent, never evidence of anything.
- Criticality is the sender's assertion about its own object. It confers no
  authority and is not a security control.

## 9. Status

Research draft at Wire major 0, which is reserved for pre-standard experimental
work. No independent implementation has read these bytes. Software conformance
and physical evidence advance separately.
