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
    session_ref?
    payload
    integrity_tag?
}
```

Exact bit assignments remain experimental until the scenario corpus and benchmark suite are frozen.

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
