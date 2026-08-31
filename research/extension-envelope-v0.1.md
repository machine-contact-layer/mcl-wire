# Extension Envelope Study v0.1

Status: **Research Candidate, non-normative**

Long-lived interoperability requires both forward compatibility and deterministic failure. A blanket `ignore unknown fields` rule is unsafe for machine semantics, while rejecting every unknown optional field prevents compatible evolution.

This study proposes an explicit criticality + length-delimited extension envelope.

## 1. Candidate item format

When the common header indicates extensions are present, the base semantic object is followed by zero or more canonical extension items until the enclosing Wire/Link object boundary.

Each item is:

```text
extension_key = uvarint((extension_id << 1) | critical)
length        = uvarint(value_length_bytes)
value         = opaque bytes interpreted by extension specification
```

`critical = 1` means a receiver must understand the extension to interpret the object safely.

`critical = 0` means an unknown receiver may skip exactly `length` bytes and continue.

## 2. Canonicality

For deterministic bytes:

- unsigned varints use the shortest legal representation;
- extension ID 0 is reserved;
- items are sorted by increasing extension ID;
- duplicate extension IDs are invalid unless a future extension framework explicitly defines repeatability;
- an item whose declared length crosses the enclosing object boundary is invalid.

## 3. Unknown extension behavior

Unknown non-critical extension:

```text
read key -> read length -> skip value -> continue
```

Unknown critical extension:

```text
reject semantic object as UNSUPPORTED_CRITICAL_EXTENSION
```

A decoder MUST NOT guess the extension meaning or reinterpret the bytes under another schema.

## 4. Overhead

For common standardized extension IDs `1-63` and values shorter than 128 bytes:

- key = 1 byte;
- length = 1 byte;
- total envelope overhead = **2 bytes per extension**.

Larger IDs naturally expand without changing the base header. For example, IDs requiring a two-byte key have 3 bytes of envelope overhead when the value length remains below 128 bytes.

This keeps the most common standardized extension space compact while avoiding a fixed small global namespace.

## 5. Why not put every future field in Core

Extensions are appropriate for:

- domain-specific semantics;
- optional evidence/metadata;
- credential formats;
- experimental fields;
- transport-independent features not universal enough for Core.

An extension should not be used to redefine a Core field or bypass the assigned-number review process.

## 6. Test result

The reference research implementation currently passes:

- 10,000 random unsigned-varint round trips;
- 2,000 random canonical extension-block round trips;
- unknown optional-extension skip;
- unknown critical-extension rejection;
- duplicate rejection;
- non-minimal varint rejection;
- truncated-length/value rejection.

These are deterministic simulation/conformance tests (`E1`), not independent interoperability evidence.

## 7. Open questions before promotion

- final maximum extension ID / implementation bound;
- exact enclosing object-length contract;
- whether any extension types are repeatable;
- public extension-ID allocation ranges;
- how extension capability is advertised during Link negotiation;
- whether very constrained Tier-0 applicability profiles prohibit extensions entirely.
