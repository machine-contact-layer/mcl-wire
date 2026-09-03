# MCL Wire v0

Status: **Research Draft**

## 1. Objective

MCL Wire maps deterministic MCL Core objects to compact canonical bytes and back.

The research target is not compression for its own sake. It is to minimize the number of transmitted bits needed to preserve task-relevant machine meaning while retaining deterministic interoperability.

## 2. Encoding tiers

### Tier 0 — Contact microframe

Properties:

- bounded size
- deterministic layout
- no schema negotiation required
- decodable by small embedded implementations
- suitable for presence, basic hazard, compact request, capability digest, authority/identity references, transport offer, and acknowledgement

Initial size hypothesis: many Tier-0 objects should be evaluated in the 16–64 byte range before channel coding. This is a benchmark target, not a normative limit.

### Tier 1 — Structured semantic object

Properties:

- typed field representation
- optional fields
- context references
- delta representation
- domain extensions
- larger observations, intent, trajectory, constraints, diagnosis, and state

### Tier 2 — Rich extension payload

Large evidence, maps, embeddings, model-specific objects, or external references. Tier 2 should normally move to another transport when practical.

## 3. Canonical object envelope

Working logical envelope:

```text
WireObject {
    wire_version
    semantic_type
    flags
    priority
    context_id?
    sender_ref?
    payload
}
```

The implemented header is specified in
[`common-header-v0.2.md`](common-header-v0.2.md), and the seven implemented
Tier-0 object bodies are specified, bit for bit, in
[`tier0-layout-v0.2.md`](tier0-layout-v0.2.md).

Those two documents together are **authoritative and sufficient** to build a
second implementation of the current wire format without reading the reference
C. Until `tier0-layout-v0.2.md` existed, this section said only that the body
assignments were experimental, which left an independent implementer with
nothing to work from but the reference source and the vectors — inverting the
project's own rule that the specification is authoritative and the
implementation subordinate.

**What is authoritative is the LAYOUT, not every field's MEANING.** Sixteen of
the twenty-one Tier-0 fields carry values on which two independent
implementations would not agree, because no registry assigns them; the
coordinate fields have no defined frame of reference at all. That is recorded
field by field in
[`mcl-core/registries/tier0-fields-v0.1.json`](../../mcl-core/registries/tier0-fields-v0.1.json)
and summarised in `tier0-layout-v0.2.md` §7. Bit-perfect decoding without shared
meaning is not interoperability, and the gap is stated rather than left to be
discovered.

**Two fields were removed from this sketch rather than implemented, and the
reasons are load-bearing.**

`integrity_tag` is gone. The only integrity-shaped mechanism MCL has is the
Link frame's CRC-32, which detects accidental corruption and stops no attacker,
since one simply recomputes it over the altered bytes. It lives at the Link
layer where framing errors occur, it is named `frame_check` precisely so it
cannot be mistaken for cryptographic integrity, and Wire has no equivalent. A
field named `integrity_tag` in a Wire envelope would promise a property no part
of MCL provides. See Architecture Charter §2.11 and `mcl-core/SECURITY.md`.

`session_ref` is gone from the Wire envelope because it belongs to the Link
frame, not to a semantic object. A session reference correlates a contact
continuing across a transport change; it has nothing to do with what an object
*means*, and duplicating it here would create a second copy that could disagree
with the frame carrying it. Charter §2.1: a semantic object means the same thing
on every transport, which it cannot do if it carries transport-session state.

Note also that a `context_id` is not a `session_ref`. They have different
lifetimes and either can exist without the other; conflating them is a defect
this project has already shipped once.

## 4. Context

A context captures information known by both peers so it need not be retransmitted repeatedly.

Example first exchange:

```text
machine class
reference frame
coordinate convention
dimensions
capabilities
transport list
static identity references
```

Later exchanges may transmit only:

```text
Δ position
Δ velocity
intent code
confidence
TTL
priority
```

Context state MUST have:

- explicit identifier
- version or generation
- deterministic reset
- bounded lifetime
- resynchronization path
- safe behavior for unknown context

## 5. Delta representation

Delta encoding may be used when both peers share a valid base object/context.

A decoder MUST reject a delta whose base is unavailable rather than silently applying it to the wrong state.

## 6. Priority representation

Wire objects expose semantic importance to MCL Link and transport profiles.

Working classes:

- `P0_CRITICAL` — immediate safety/contact-critical fields
- `P1_HIGH` — authority, hazard, constraint, session-critical state
- `P2_NORMAL` — ordinary observations, intent, status
- `P3_BULK` — rich optional state/evidence

The exact field-level protection policy belongs to transport profiles. Priority does not imply trust or authorization.

## 7. Canonicalization requirements

For a fixed wire version and context:

- field order is deterministic
- integer representation is deterministic
- quantization rules are deterministic
- absent/default fields have deterministic handling
- extension ordering is deterministic
- malformed or non-canonical encodings are rejectable

Extension framing on Tier-0 objects is specified in
[`tier0-extensions-v0.1.md`](tier0-extensions-v0.1.md), with vectors in
`conformance/vectors/extensions-v0.1.json`. The block carries its own length so
that a Tier-0 object remains **self-delimiting**, which the raw-Wire path over
MCL-AP requires: bytes arrive from the air with no envelope stating where the
object ends.

An empty extension list encodes with the extension bit clear and no block, so
there is exactly one encoding of any object and adding this capability changed
no existing vector.

## 8. Baseline benchmark

MCL Wire MUST be evaluated against strong alternatives rather than only text JSON.

Required baseline family:

- JSON
- CBOR
- Protocol Buffers or equivalent typed binary
- fixed-layout bit fields
- CBOR + shared context
- custom context/delta encoding
- context + entropy coding
- learned semantic selection + deterministic codec, where applicable

Metrics:

- bytes per semantic event
- encode/decode latency
- memory
- resynchronization overhead
- semantic/task distortion
- behavior under packet/field loss

## 9. Golden vectors

Each normative object eventually requires a vector of the form:

```text
semantic object
    ↓
canonical bytes
    ↓
decode
    ↓
identical semantic object
```

Transport-specific repositories extend this into their own physical/link vectors.

## 10. Open questions

- Which Tier-0 fields deserve fixed bit positions?
- Which quantities should use absolute versus relative coordinates?
- Which fields can be quantized without operational ambiguity?
- How much compression comes from context versus entropy coding?
- How should field-level priority be represented without bloating Tier 0?
