# MCL Wire Tier-0 Body Layout v0.2

**Status:** Research Draft — **normative for the seven objects below at Wire major 0**
**Supersedes:** [tier0-candidate-layout-v0.1.md](tier0-candidate-layout-v0.1.md), which is
marked "do not implement from this document" and describes a design that was not built.

This document defines, bit for bit, the seven Tier-0 object bodies the reference
implementation actually encodes.

---

## 0. Why this document exists

Until now an independent implementer had no authoritative source for these
layouts. The candidate document says not to implement from it; `wire-v0.md`
describes the common header and calls the body assignments experimental. The
only complete statement of the layouts was the reference C and the conformance
vectors.

That inverts the project's own rule:

> The specification is authoritative. The reference implementation is
> subordinate.

An implementer who must reverse-engineer a codec to write a second one is not
reading a specification, and a second implementation derived that way tests
nothing: it will reproduce whatever the first one does, including its mistakes.

Everything below was derived from the reference implementation and then checked
against the conformance vectors. Where the two disagreed, that would be a defect
to report rather than a choice to make; they did not disagree.

## 1. Scope, and what is NOT frozen here

**Frozen here:** field order, bit width, signedness, padding, canonicality, and
the exact encoded size of each of the seven implemented objects. Two
implementations following this document produce identical bytes.

**NOT frozen here:** what several of the field VALUES mean. §7 says which, and
says so explicitly rather than leaving it to be discovered. Bit-perfect decoding
without shared meaning is not interoperability, and pretending otherwise is
worse than an admitted gap: it produces two implementations that agree on every
byte and disagree on what was said.

The distinction matters most for the coordinate fields. Their encoding is
settled; their **frame of reference is not defined**, and until it is, HAZARD
and REQUEST are not fully interoperable objects. See §7.2.

## 2. Common structure

Every Tier-0 object is:

```
  common header      16 bits    common-header-v0.2.md
  source_ref         32 bits    unsigned
  body               varies     this document
  padding            0-7 bits   zero
```

All fields are packed **big-endian, most significant bit first**, with no
alignment between fields — a 12-bit field may start mid-byte. There is no
padding *between* fields; the only padding is at the end.

`source_ref` is a correlation reference for the SEMANTIC ORIGIN of this object.
It is not identity, not authority, not trust, and it is not the same field as a
Link frame's `source_ref` — see `mcl-link/spec/link-v0.md` §3.3.

### 2.1 Padding MUST be zero, and MUST be checked

Where a body does not end on a byte boundary, the remaining bits of the final
byte are padding and MUST be zero. A decoder MUST reject a non-zero padding bit.

This is canonicality, not tidiness. If padding were ignored, one semantic object
would have up to 128 distinct valid encodings, and any future use of the bytes —
a hash, a digest, a signature, a cache key, a byte-equality conformance check —
would produce different answers for objects that mean the same thing. The
reference implementation enforces this (`mcl_require_zero_padding`).

### 2.2 Length is implied by the kind, not carried

There is no length field. The category/opcode pair in the header selects the
kind, and the kind fixes the size exactly. This is what makes the encoding
self-delimiting: a decoder reports exactly how many bytes an object occupied, so
successive objects can be read from one buffer with no rescanning.

A consequence worth stating: a decoder MUST NOT accept a buffer that is longer
than the object it decoded as a mistake in the object. Trailing bytes are the
caller's business — but a Link frame's `payload_len` IS an exact boundary, and
carriage there rejects a payload the object does not fill.

## 3. Encoded sizes

| Kind | Category | Opcode | Body bits | Total bits | Bytes | Pad bits |
|---|---|---|---|---|---|---|
| `PRESENCE` | 0 | 0 | 40 | 88 | **11** | 0 |
| `HAZARD` | 3 | 1 | 70 | 118 | **15** | 2 |
| `REQUEST` | 5 | 1 | 82 | 130 | **17** | 6 |
| `AUTHORITY_CLAIM` | 1 | 1 | 58 | 106 | **14** | 6 |
| `DEGRADED_STATE` | 6 | 1 | 26 | 74 | **10** | 6 |
| `TRANSPORT_OFFER` | 8 | 0 | 88 | 136 | **17** | 0 |
| `TRANSPORT_ACCEPT` | 8 | 1 | 80 | 128 | **16** | 0 |

"Total bits" includes the 16-bit common header and the 32-bit `source_ref`.
Largest Tier-0 object: **17 bytes**.

A category/opcode pair not in this table MUST be rejected with "unsupported
semantic". It is not a malformation — the bytes may be a perfectly good object
this implementation does not implement — and the two must be reported
differently so an integrator can tell "you sent something wrong" from "I do not
speak that".

## 4. The seven bodies

Bit offsets are from the start of the object, so they include the 16-bit header
and the 32-bit `source_ref`.

### 4.1 PRESENCE — category 0, opcode 0

**PRESENCE has two layouts. They differ by one field, and the major says
which.**

**Major 0 (experimental) — 11 bytes. Frozen as history; never changes.**

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48      8       u  machine_class
    56     24       u  capability_tag
    80      8       u  ttl
                       total 88 bits = 11 bytes, no padding
```

**Major 1 (Stable) — 10 bytes. `machine_class` is removed.**

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48     24       u  capability_tag
    72      8       u  ttl
                       total 80 bits = 10 bytes, no padding
```

**Why `machine_class` is not in the Stable layout.** A necessity audit
(`mcl-core/governance/MACHINE_CLASS_AUDIT.md`) asked what Stable-v1 decision
becomes impossible without it and found none: no code in any of the eight
repositories dispatches, filters or negotiates on the value — every one of its
21 references is a constant write, codec plumbing, or a round-trip assert, using
five different constants with no shared meaning because none exists. The
research corpus contains three `PRESENCE` messages carrying two distinct
classes, which is no basis for a 256-value cross-vendor taxonomy, and both
specifications that mentioned the field already marked it optional.

This is §4.1 of the v1 scope applied consistently: *a Stable field nobody may
act on is an invitation to act on it.* A `machine_class` with no assigned values
is exactly that field — a receiver learns the sender considers itself class 7
and has no way to look 7 up.

**No replacement taxonomy is defined.** Machine typing belongs to capability
metadata exchanged after contact, where a vocabulary can be domain-scoped and
versioned rather than universal and frozen into a first-contact object. An
implementation needing it before capability exchange must show the requirement
first; none exists in the tree.

**Major 0 is untouched.** Its layout, its published vectors and the E3/E4
over-air evidence recorded against it all stand exactly as they are. Nothing
here rewrites history to make the new decision look like it was always true.

### 4.2 HAZARD — category 3, opcode 1

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48      8       u  hazard_class
    56      3       u  severity            0..7
    59      7       u  confidence          0..127
    66     12       s  x                   -2048..2047
    78     12       s  y                   -2048..2047
    90     10       s  z                    -512..511
   100     10       u  radius              0..1023
   110      8       u  ttl
   118      2       -  padding, MUST be zero
                       total 120 bits = 15 bytes
```

### 4.3 REQUEST — category 5, opcode 1

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48      8       u  request_class
    56     32       u  target_ref
    88     12       s  x                   -2048..2047
   100     12       s  y                   -2048..2047
   112     10       u  radius              0..1023
   122      8       u  ttl
   130      6       -  padding, MUST be zero
                       total 136 bits = 17 bytes
```

`REQUEST` has no `z`. That is deliberate rather than an omission: the object was
built for the two-dimensional case and adding a third axis later is an extension
or a new opcode, not a silent widening.

### 4.4 AUTHORITY_CLAIM — category 1, opcode 1

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48      6       u  authority_class     0..63
    54     12       u  jurisdiction        0..4095
    66     32       u  credential_ref
    98      8       u  validity
   106      6       -  padding, MUST be zero
                       total 112 bits = 14 bytes
```

**Receiving this object confers no authority whatever.** It is a claim, and a
claim is an input to local policy. Every field crosses an observable medium in
the clear; anyone in range can transmit an identical one. `credential_ref` is a
reference to a credential, not a credential — MCL has no mechanism to verify
one, and charter §2.11.2 requires that the absence of the mechanism cause
refusal, not acceptance.

### 4.5 DEGRADED_STATE — category 6, opcode 1

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48      8       u  affected_capability
    56      7       u  health              0..127
    63      3       u  severity            0..7
    66      8       u  ttl
    74      6       -  padding, MUST be zero
                       total 80 bits = 10 bytes
```

Note the field order: `health` precedes `severity` here, while in `HAZARD` the
3-bit `severity` precedes the 7-bit `confidence`. An implementer working from
the C structs alone could easily transpose them, since both objects hold a
3-bit and a 7-bit field adjacent. They are ordered as shown.

### 4.6 TRANSPORT_OFFER — category 8, opcode 0

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48     32       u  migration_ref       MUST NOT be 0
    80      8       u  transport_id        MUST NOT be 0
    88      8       u  profile_id
    96     32       u  endpoint_token
   128      8       u  validity
                       total 136 bits = 17 bytes, no padding
```

`migration_ref` correlates one transport-change transaction and nothing else.
Without it, a delayed acceptance from an abandoned offer is indistinguishable
from the acceptance of the live one, because `transport_id` and `profile_id` are
normally identical across a retry.

Zero is reserved in both `migration_ref` and `transport_id` so that a zeroed or
uninitialised field can never name a live transaction or a real bearer. Wire
encodes what it is given; the reserved-value rules are enforced by the Link
contact state machine, which is where the transaction lives.

### 4.7 TRANSPORT_ACCEPT — category 8, opcode 1

```
offset  width  signed  field
------  -----  ------  --------------------------------------
     0     16       -  common header
    16     32       u  source_ref
    48     32       u  migration_ref       MUST NOT be 0
    80      8       u  transport_id        MUST NOT be 0
    88      8       u  profile_id
    96     32       u  session_ref         MUST NOT be 0
                       total 128 bits = 16 bytes, no padding
```

`session_ref` is the accepting peer's chosen reference for the CONTINUING
contact. It is bound once and persists for the life of the contact, across every
migration — see `mcl-link/spec/link-v0.md` §3.3.

It is a correlation reference and **not a secret**. It travels in the clear over
an observable medium, so anyone in range can read it and quote it back. It lets
an honest peer recognise a continuing contact; it establishes nothing about who
that peer is.

## 5. Signed fields

`x`, `y` and `z` are the only signed fields. They are **two's complement in the
stated width**, so a 12-bit `x` covers -2048..2047 and a 10-bit `z` covers
-512..511.

An encoder MUST reject a value outside the representable range rather than
truncating it. Truncation would turn a position 2100 units away into one 1996
units in the opposite direction — a silent sign flip, at the exact moment a
hazard is far enough away to matter.

## 6. Range enforcement

Every field narrower than its C storage type has a range, and an encoder MUST
reject a value that does not fit. This is not defensive programming; it is what
makes the encoding canonical. A `severity` of 8 in a 3-bit field would otherwise
be transmitted as 0 — the lowest severity, from a value meaning the highest.

A decoder cannot detect this, because after truncation the bits are a valid
encoding of a different value. The check has to be on the encode side, and it is.

## 7. WHAT THESE VALUES MEAN — the open half

The layouts above are complete. The **meanings** below are not, and this section
exists so that an implementer discovers that here rather than after shipping.

Field by field, this is recorded in
[`mcl-core/registries/tier0-fields-v0.1.json`](../../mcl-core/registries/tier0-fields-v0.1.json),
which states for each of the 21 Tier-0 fields whether its meaning is `assigned`,
`provisional` or `open`, and for the unfinished ones what specifically is
missing. The current count is **5 assigned, 10 provisional, 6 open** — that is,
16 of 21 fields carry values two independent implementations would not agree on.
The counts are checked against the entries by
`mcl-core/tools/validate_field_registry.c`, because a count kept by hand drifts
in the direction that flatters the project.

### 7.1 Enumerations with no registry

These fields are encoded but have no governing registry assigning values to
meanings:

```
machine_class          PRESENCE
hazard_class           HAZARD
request_class          REQUEST
authority_class        AUTHORITY_CLAIM
jurisdiction           AUTHORITY_CLAIM
affected_capability    DEGRADED_STATE
```

Two implementations can exchange these bit-perfectly today and disagree entirely
about what was said. `hazard_class = 3` currently means whatever each vendor
decided.

They MUST be treated as **provisional at Wire major 0**, and MUST NOT be relied
on for interoperability between independently developed implementations until
each has a registry with an assigned range, an allocation policy and a change
controller. The mechanism exists —
`mcl-core/registries/semantic-codes-v0.2.json` does exactly this for
category/opcode — and has not been applied to these.

### 7.2 Coordinates: encoded, but with NO defined frame of reference

`x`, `y`, `z` and `radius` in `HAZARD` and `REQUEST` are the most serious gap in
this document, and the most likely to cause harm.

What is defined: width, signedness, range, and that
`mcl-core/spec/shared-primitives-v0.1.md` §1 makes canonical length metres.

What is **not** defined:

- **The origin.** Relative to what? The sender? The receiver? A shared frame?
  MCL's premise is first contact between machines with no prior relationship,
  and such machines have no shared origin by construction.
- **The axes.** Which way is +x? North? The sender's heading? If it is the
  sender's heading, the receiver cannot use the value without knowing that
  heading, which is not in the object.
- **The quantization.** The superseded candidate document proposed 0.25 m per
  unit. The implementation encodes plain integers. Neither is normative, so a
  12-bit x currently means "±2047 of something".

The consequence is unavoidable and is stated rather than hidden: **if these
coordinates need an external frame of reference to be interpreted, then HAZARD
and REQUEST are not context-free Tier-0 objects**, and Tier-0's defining property
is that it is decodable with no prior shared state.

Until this is resolved, a conforming implementation MUST NOT act on these
coordinates as though they name a location in a shared frame. Carrying them,
storing them and passing them to local policy is fine; navigating by them is
not. A hazard placed at the wrong origin is worse than one not reported.

The resolution is a decision, not more code, and there are three honest options:
define a sender-relative frame and carry the sender's reference direction; drop
the coordinates from Tier-0 and move them to an extension that can carry a frame
declaration; or keep them and mark them permanently advisory. That decision is
open.

### 7.3 TTL and validity

`ttl` and `validity` are 8-bit and unitless in the implementation.
`mcl-core/spec/shared-primitives-v0.1.md` §2.1 defines `ttl` semantically — the
maximum local interval after successful decode during which a receiver may treat
the object as current — but does not fix the unit or the scale, and the
superseded candidate document proposed an exponent/mantissa encoding that was
not built.

So: 8 bits, meaning is "some duration", scale undefined. **Provisional.** A
receiver MUST NOT infer seconds.

`ttl` is not freshness and not replay protection. It bounds how long a receiver
may consider a decoded object current; it says nothing about when the object was
sent, and nothing prevents a listener from re-transmitting an object within its
own TTL window.

### 7.4 severity, confidence, health

Three unitless scales with defined ranges and undefined calibration:

```
severity     0..7      HAZARD, DEGRADED_STATE
confidence   0..127    HAZARD
health       0..127    DEGRADED_STATE
```

Monotonic in the obvious direction — higher severity is more severe, higher
health is healthier, higher confidence is more confident — and beyond that,
uncalibrated. One vendor's severity 5 is not another's. They are **provisional**
and are usable within one deployment, or between vendors that have agreed
externally, and not otherwise.

`confidence` in particular MUST NOT be read as a probability. Nothing states
that 64/127 means 50%.

## 8. Conformance

An implementation conforms to this document if, for every one of the seven
kinds:

1. it encodes to byte-identical output for identical field values;
2. it rejects out-of-range field values on encode rather than truncating;
3. it rejects non-zero padding on decode;
4. it rejects an unassigned category/opcode as unsupported-semantic, distinctly
   from malformed;
5. it reports the exact consumed length so successive objects can be decoded
   from one buffer.

The conformance vectors in `conformance/vectors/` are the executable form of 1.
They are **immutable**: an existing vector is never edited to make new code look
compatible.

Conforming to this document says nothing about §7. An implementation can satisfy
every point above and still mean something different by `hazard_class = 3`.

## 9. Status

Normative for the layouts. Provisional for the meanings identified in §7.

Before a stable Wire major:

- registries for the six enumerated fields in §7.1
- the coordinate frame-of-reference decision in §7.2
- unit and scale for `ttl` and `validity` in §7.3
- a decision on whether `severity` / `confidence` / `health` can be calibrated
  at all, or should be marked permanently local
- a second independent implementation built **from this document** rather than
  from the reference source, which is the only test of whether it is sufficient

The seven kinds here are the ones implemented. The registry marks fifteen as
Tier-0 candidates; the other eight are not implemented and MUST NOT be promoted
for the appearance of completeness.
