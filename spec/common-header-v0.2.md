# MCL Common Header v0.2

Status: **Stable** for the major-1 common header. Major 0 remains Experimental and permanent.

Promoted 2026-09-06 under `mcl-core/governance/V1_SCOPE.md` §6 on the §5.9
three-part evidence: a clean-room implementation cross-decoding in both
directions (C4, 803 checks), over-air between distinct devices on three
bearers, and the stack compiled by a different toolchain for a different
architecture. The status previously read "Research Candidate, non-normative"
while `V1_SCOPE.md` §4 called the same header Stable at major 1, so a reader
who believed the document and a reader who believed the scope table disagreed
about whether they could implement against it.

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

## 2. v0.2 layout

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

### 3.1 A Stable major carries only Stable semantics

**Normative.** `mcl-core/governance/V1_SCOPE.md` §4.7.

> Wire major 1 carries only semantic objects whose body contract is part of the
> major-1 Stable set: `PRESENCE`, `TRANSPORT_OFFER` and `TRANSPORT_ACCEPT`.
>
> A major-1 decoder receiving a category/opcode that is not assigned in the
> major-1 Stable set **MUST** reject it. It MUST NOT decode the body, and MUST
> NOT infer a layout from the fact that one exists at another major.
>
> `HAZARD`, `REQUEST`, `AUTHORITY_CLAIM` and `DEGRADED_STATE` are Candidate.
> They continue to be carried under the experimental major until they are
> separately promoted.

**Why.** Those four objects have layouts, codes, vectors and passing tests —
and they are Candidate precisely so that their *meanings* may still change. If
one changed while being carried inside major 1, two decoders both correctly
implementing "major 1" would read the same category/opcode under different
layouts. A major version exists to make exactly that impossible, so a Candidate
body inside a frozen major contradicts the guarantee the major provides.

**Rejected alternative.** Carrying Candidate objects inside major 1 under a
code range documented as unstable. It makes the major version insufficient to
determine whether a layout can be trusted — a decoder would have to consult a
range table to learn what its own version guarantees. MCL already draws this
line for extension IDs, where Experimental Use values are explicitly not
globally interoperable assignments.

**Consequence, stated because implementers will meet it.** A node that performs
first contact under major 1 and also reports hazards emits objects of two
different majors. This is permitted: `major_version` is a per-object header
field, not a per-link property. What a peer **MUST NOT** do is infer support for
one major from having observed the other.

**Status.** Major 1 (`MCL_WIRE_STABLE_MAJOR`) is **cut** for v1.0. The codec
encodes and decodes it, and the immutable major-1 vector family
(`mcl-wire/conformance/vectors/tier0-major1-v1.0.json`) is frozen against it. Majors other
than 0 and 1 are unassigned and are refused, never guessed at.

Cutting was gated on the Stable meanings closing (`V1_SCOPE.md` §5.8), because
accepting frames under a major whose bodies were not frozen would itself have
been the cutting. The rule above was written and tested **before** the first
major-1 vector was generated — `mcl_wire_kind_allowed_at_major`,
`mcl-wire/tests/test_major_rule.c` — so that no vector was frozen under an
ambiguous rule.

A decoder MUST apply both halves of the check. That an object arrives at an
assigned major does not make it admissible **at** that major: a Candidate object
presented at major 1 is refused with `UNSUPPORTED_SEMANTIC`, and a major-0
`PRESENCE` presented at major 1 is refused on length, because major 1 drops
`machine_class` and the body is 10 bytes rather than 11.

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

## 9. Promotion record

Major 1 is canonical and Stable for the scope stated above. Its promotion was
supported by registry governance, negative unknown-code tests, immutable
major-1 vectors, the independently implemented C4 decoder, and the Stable
negotiation contract. Extension framing remains Candidate/Experimental and is
not a prerequisite for interpreting a major-1 object with `E = 0`.
