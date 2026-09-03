# Wire duration codes v0.1

**Status:** Research Draft
**Layer:** Wire
**Reference implementation:** `mcl-wire/include/mcl/wire.h`, `mcl-wire/src/wire.c`
**Tests:** `mcl-wire/tests/test_duration.c`

This document is normative for the bytes. Where it and the reference
implementation disagree, this document is correct and the code is a defect.

---

## 1. What this closes

`ttl` and `validity` are 8-bit duration fields appearing in five Tier-0 objects.
Neither had a unit or a scale. The field registry recorded the consequence
plainly: eight bits meaning "some duration", with a normative rule that a
receiver **MUST NOT** infer seconds — which made the fields undecidable rather
than merely imprecise. Two implementations could agree on every bit of a
`PRESENCE` and disagree completely on how long it was good for.

This document assigns one encoding, and **both fields use it**. Two duration
fields with different scales in one protocol is a defect waiting to be written.

## 2. Encoding

One octet. A 4-bit exponent in the high nibble, a 4-bit mantissa in the low.

```text
  7   6   5   4   3   2   1   0
+---+---+---+---+---+---+---+---+
|    exponent   |    mantissa   |
+---+---+---+---+---+---+---+---+
```

```text
e == 0    seconds = m                      0 .. 15,  in 1 s steps
e >= 1    seconds = (16 + m) << (e - 1)    16 .. 507904
```

The unit is the **second**, as required by
[`mcl-core/spec/shared-primitives-v0.1.md`](../../mcl-core/spec/shared-primitives-v0.1.md)
§1. There is no negotiated scale and no locale-dependent interpretation.

All 256 codes are valid. Decoding a duration cannot fail.

### 2.1 Why the implied leading one

Above `e == 0` the mantissa carries an implied leading 1, so the value is
`(16 + m)` rather than `m`. This is what makes the mapping **injective**: every
representable duration has exactly one code.

A plain `m << e` would encode 64 seconds five different ways. In a protocol
whose premise is that bytes mean one thing, a decoder with two answers and an
encoder with two choices is not acceptable, and canonical form here costs
nothing but this paragraph.

### 2.2 The bands do not touch

Band `e` covers `16<<(e-1)` to `31<<(e-1)`. The next band starts at
`32<<(e-1)`. Between them sits a gap of exactly one step:

```text
band 14 top     253952 s
                        <- nothing representable here
band 15 bottom  262144 s
```

This is a property of the encoding, not an oversight, and an implementer must
handle it. See §3.2.

### 2.3 Range and precision

| | |
|---|---|
| Minimum | 0 s |
| Maximum | 507904 s (5 d 21 h 5 min 4 s) |
| Resolution below 16 s | 1 s |
| Worst quantization loss above 60 s | **5.88%** |
| Largest gap between representable values | 16384 s |

The worst case sits just below a band bottom, where the step is 1/16 of the
value. Measured across the whole range rather than estimated.

The upper end is deliberate. A duration field that cannot express "for the rest
of the shift" gets worked around, and a work-around is a second scale — the
exact defect §1 exists to prevent.

## 3. Required behaviour

### 3.1 Zero means zero

Code `0x00` is a real value meaning **zero seconds**: decode the object, then
stop treating it as current.

It does **not** mean "unset", "unknown" or "unlimited". A receiver MUST NOT
substitute a default for it.

### 3.2 Encoders round DOWN

An encoder given a duration that is not exactly representable MUST select the
largest representable value **at or below** it. This includes durations landing
in a band gap (§2.2), which round to the top of the band below.

Rounding MUST NOT be to nearest and MUST NOT be upward.

The direction is a safety property, not a convention. These fields bound how
long a receiver may keep treating information as current. Rounding up extends
the life of stale information by up to one quantum, and does so again on every
hop that re-encodes the value. Rounding down can only shorten a bound.

### 3.3 Durations above the maximum are REFUSED

An encoder given a duration greater than 507904 s MUST fail. It MUST NOT clamp.

Silently turning thirty days into six days is a rounding error that surfaces as
a field incident rather than as a build failure. A sender that needs longer than
this encoding provides has a requirement the encoding does not meet, and needs
to know that at the call site.

### 3.4 There is no code for "forever"

Deliberately. An unbounded lifetime is the opposite of what a TTL is for, and a
sentinel meaning it would be reached for by every sender that did not want to
think about expiry.

A deployment needing indefinite validity must re-assert it, which is also what
makes the assertion observably current.

## 4. What this does not do

`ttl` remains what
[`shared-primitives-v0.1.md`](../../mcl-core/spec/shared-primitives-v0.1.md)
§2.1 says it is: the maximum local interval after successful decode during which
a receiver may treat the object as current, subject to local policy.

Assigning it a unit changes none of the following:

- **`ttl` is not freshness.** It says nothing about when the object was sent.
- **`ttl` is not replay protection.** Nothing prevents a listener retransmitting
  an object within its own TTL window, and the interval restarts on decode at
  each receiver.
- **`ttl` requires no synchronized clock**, and MCL still forbids first contact
  depending on one. The interval is measured locally from local receipt, which
  is why it can be honoured by a machine with no wall clock at all.

`validity` in `TRANSPORT_OFFER` remains retained for the **caller** to enforce.
The Link library has no clock, and giving it one would tie a freestanding
protocol library to a platform's notion of time. This document tells the caller
what the number means; it does not move the enforcement.

## 5. Conformance

An implementation conforms when:

1. decoding any of the 256 codes yields the seconds in §2;
2. encoding a representable duration yields its own code;
3. encoding a non-representable duration in range yields the largest
   representable value at or below it, including across a band gap;
4. encoding a duration above the maximum fails rather than clamping;
5. code `0x00` is treated as zero seconds and not as an absent value.

`mcl-wire/tests/test_duration.c` checks 1 and 2 exhaustively over all 256 codes,
3 over the full range at fine granularity, and 4 and 5 directly.
