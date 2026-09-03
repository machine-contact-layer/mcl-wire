# Tier-0 Candidate Layout v0.1

Status: **Research Candidate, non-normative. Superseded — retained as a record.**

This document records the first bit-budget candidate produced from the v0.1 scenario corpus. It exists to be falsified by benchmarks and additional scenarios, and it was.

**Do not implement from this document.** The header model below (2-bit version, 4-bit semantic type) was replaced by the 16-bit major/category/opcode/priority/extension model in [`common-header-v0.2.md`](common-header-v0.2.md), which is what the reference codec implements. A 4-bit semantic type allowed 16 meanings for the lifetime of the wire version; the current model separates category from opcode so the namespace does not have to be rationed.

It is kept unedited because a superseded candidate is evidence of how a decision was reached, and rewriting it would destroy that. Current conformance vectors are in [`../conformance/vectors/`](../conformance/vectors/); the newest is `tier0-v0.3.json`.

## 1. Common header

Candidate common header: **16 bits**.

```text
wire_version        2 bits
semantic_type       4 bits
priority            2 bits
flags/reserved      8 bits
```

A non-context frame then carries a 32-bit local `source_ref`.

An established-context frame may replace that source reference with an 8-bit `context_id`. This is valid only when both peers have explicit synchronized context. It is never a first-contact shortcut.

## 2. Working quantization

| Primitive | Candidate representation | Coverage |
|---|---:|---|
| local source reference | 32 bits | ephemeral local reference, not identity proof |
| relative x/y | signed 12 bits each @ 0.25 m | -512.00 to +511.75 m |
| relative z | signed 10 bits @ 0.25 m | -128.00 to +127.75 m |
| radius | unsigned 10 bits @ 0.25 m | 0 to 255.75 m |
| confidence | 7 bits | 128 levels |
| severity | 3 bits | 8 levels |
| TTL / short validity | 8-bit exponent/mantissa | up to 39.68 s |

The TTL candidate uses `eee mmmmm` with:

```text
decoded_ms = mantissa * 10 ms * 2^exponent
```

On a 10 ms grid from 10 ms through 39.68 s the current encoder has approximately 1.0% median relative quantization error, 2.3% 95th-percentile error, and 3.1% worst-case relative error.

## 3. Resulting candidate sizes

Before channel coding or link-layer integrity:

| Semantic type | Fixed v0 | Established context |
|---|---:|---:|
| PRESENCE | 11 B | 8 B |
| HAZARD | 15 B | 12 B |
| REQUEST | 17 B | 14 B |
| AUTHORITY_CLAIM | 14 B | 11 B |
| DEGRADED_STATE | 10 B | 7 B |
| TRANSPORT_OFFER | 13 B | 10 B |

These layouts are deliberately small enough to test the earlier 16–64 byte Tier-0 hypothesis rather than assuming it.

## 4. Spatial scope warning

The local Cartesian candidate is a compact *contact-space* representation, not a global coordinate system. A session must establish or inherit the reference frame. Global geodetic coordinates, large areas, complex polygons, and maps belong in richer representations or extensions.

## 5. 32-bit ephemeral references

A 32-bit local random reference is a size/reuse candidate, not a credential. Approximate birthday-collision probabilities are:

- 100 simultaneously sampled references: ~1.15e-6
- 1,000: ~1.16e-4
- 10,000: ~1.16e-2

A 24-bit space reaches ~2.93% collision probability by 1,000 samples, which is too aggressive for a general default. Longer authenticated identities/credential references remain separate.

## 6. Promotion gates

No field width becomes normative until:

1. scenario coverage is expanded;
2. round-trip golden vectors exist;
3. boundary values are tested;
4. alternative typed encodings are benchmarked;
5. semantic ambiguity from quantization is reviewed;
6. link/security overhead is accounted separately.
