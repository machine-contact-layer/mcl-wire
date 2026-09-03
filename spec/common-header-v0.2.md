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

**Status.** Major 1 is defined (`MCL_WIRE_STABLE_MAJOR`) and **not yet cut**.
The decoder refuses it along with every unassigned major, because accepting
frames under a major whose bodies are not frozen would be the cutting, and that
is gated on the Stable meanings closing (`V1_SCOPE.md` §5.8). The rule is
written and tested now — `mcl_wire_kind_allowed_at_major`,
`mcl-wire/tests/test_major_rule.c` — so that it exists before the first major-1
vector is generated. A vector frozen under an ambiguous rule fixes the ambiguity
into the artifacts that define the release.

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
