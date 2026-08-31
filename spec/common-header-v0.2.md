# MCL Common Header Candidate v0.2

Status: **Research Candidate, non-normative**

This study supersedes the direct `semantic_type` allocation in the v0.1 Tier-0 candidate while preserving a two-byte common header.

## 1. Why v0.1 does not scale

The v0.1 candidate allocated:

```text
wire_version   2 bits
semantic_type  4 bits
priority       2 bits
flags          8 bits
```

A 4-bit direct semantic type space has only 16 values.
MCL Core already defines 20 semantic message classes, including 15 current Tier-0 candidates.

Using nearly the entire globally shared type space before v0.1 would make decades-long extensibility dependent on ad-hoc escape behavior.

## 2. v0.2 candidate

```text
15            12 11             8 7              3 2       1 0
+---------------+----------------+----------------+-----------+-+
| major_version | category       | opcode         | priority  |E|
+---------------+----------------+----------------+-----------+-+
      4 bits          4 bits           5 bits         2 bits  1
```

Total: **16 bits**.

`E` indicates that a standardized extension envelope follows according to the object layout.

## 3. Version semantics

`major_version = 0` is reserved for pre-standard experimental wire work.

Compatible registry additions and optional extensions do not require a major-version change.

An incompatible reinterpretation of existing canonical bytes requires a new major version.

`major_version = 15` is reserved for future version-space escape if ever required.

## 4. Category/opcode separation

Category identifies the semantic family.
Opcode identifies the act/object within that family.

Initial provisional categories:

| Code | Category |
|---:|---|
| 0x0 | CONTACT |
| 0x1 | IDENTITY |
| 0x2 | CAPABILITY |
| 0x3 | WORLD_STATE |
| 0x4 | INTENT |
| 0x5 | COORDINATION |
| 0x6 | STATE |
| 0x7 | LINK_STATE |
| 0x8 | TRANSPORT |
| 0x9-0xD | reserved standardized categories |
| 0xE | EXPERIMENTAL |
| 0xF | EXTENDED namespace escape |

Example assignments:

- `CONTACT/PRESENCE`
- `IDENTITY/AUTHORITY_CLAIM`
- `WORLD_STATE/HAZARD`
- `COORDINATION/REQUEST`
- `STATE/DEGRADED_STATE`
- `TRANSPORT/TRANSPORT_OFFER`

The exact assignments live in the machine-readable Core registry.

## 5. Capacity

The Standards-Action range provisionally reserves 24 opcodes per standardized category across 14 standard categories:

`14 * 24 = 336` centrally governed semantic slots.

Additional Specification-Required, Experimental, and Extended namespace mechanisms exist without consuming those stable slots.

This is not a goal to fill hundreds of Core acts. Core should remain small. The point is to avoid designing scarcity into the first draft.

## 6. Priority

Two bits encode the existing four priority classes.

Priority affects scheduling/protection policy.
It MUST NOT imply trust, authority, or semantic validity.

## 7. Unknown codes

A receiver encountering an unknown standardized category/opcode:
- MUST NOT guess its meaning;
- MUST preserve framing;
- MAY report `UNSUPPORTED_SEMANTIC`;
- MAY continue processing later independently framed objects.

Unknown critical extensions cause object rejection.
Unknown non-critical extensions may be skipped when the extension envelope permits deterministic skipping.

## 8. Benchmark impact

This candidate remains 16 bits, so the previously measured v0.1 frame byte counts do not increase solely because of the category/opcode split.

The existing source-codec benchmark remains useful for size comparison, but future golden vectors should use v0.2 header semantics.

## 9. Promotion gates

This header does not become canonical until:
- registry policy is reviewed;
- negative unknown-code tests exist;
- extension framing is defined;
- context/version negotiation is integrated;
- at least two independent implementations round-trip the same vectors.
