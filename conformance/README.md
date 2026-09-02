# MCL Wire Conformance Vectors

Every frozen wire-version object must eventually ship with deterministic golden vectors.

## Vector contract

A vector records:

```text
semantic input
    ↓
context state (if any)
    ↓
expected canonical bytes
    ↓
decoder
    ↓
expected semantic output
```

Recommended machine-readable vector fields:

```yaml
id: hazard-basic-001
core_version: 0.0-draft
wire_version: 0.0-draft
context: null
semantic:
  type: HAZARD
  fields: {}
expected_hex: "..."
expected_decoded: {}
```

## Requirements

Once a wire version is frozen:

- identical input + context MUST produce identical bytes
- decoders MUST reject malformed encodings according to the spec
- context-dependent vectors MUST identify the exact context generation
- delta vectors MUST include their base object/reference
- canonical and non-canonical representations must be distinguishable
- extensions must have explicit unknown-field behavior

Transport repositories extend these vectors with framing and physical/link artifacts.

For example, MCL-AP will eventually provide:

```text
semantic object
→ canonical MCL Wire bytes
→ MCL Link frame
→ exact reference acoustic samples
→ channel/replay input
→ recovered Link/Wire bytes
→ recovered semantic object
```

Candidate hex vectors are committed under `vectors/`. They are reproducible research artifacts, not normative release vectors, while field IDs, quantization, and Tier-0 layouts remain under active benchmark design.
