<p align="center">
  <img src=".github/banner.png" alt="OJOBIT" width="100%">
</p>

<h1 align="center">MCL Wire</h1>

<p align="center"><strong>The exact bytes. One canonical encoding for every MCL object, on every transport.</strong></p>

<p align="center">
  <a href="https://github.com/machine-contact-layer/mcl-wire/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/machine-contact-layer/mcl-wire/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/machine-contact-layer/mcl-wire/blob/main/LICENSE"><img alt="License Apache-2.0" src="https://img.shields.io/badge/license-Apache--2.0-blue"></a>
  <img alt="Wire major 1" src="https://img.shields.io/badge/wire%20major-1%20Stable-brightgreen">
  <img alt="C99 freestanding" src="https://img.shields.io/badge/C99-freestanding-informational">
</p>

<p align="center">
  <a href="https://github.com/machine-contact-layer/mcl-sdk"><b>Use the SDK instead</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-core"><b>Specifications</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-link"><b>mcl-link</b></a>
</p>

---

> ### Most people should start with the SDK, not here
>
> This repository is a **specification**. If you are building a product, start
> with [**mcl-sdk**](https://github.com/machine-contact-layer/mcl-sdk): its quickstart runs two machines making
> contact, and the release ships a self-contained developer SDK — one CMake
> project, no sibling checkout. Come back here when you need to know exactly
> what a byte means, or when you are writing an independent implementation.

## Why this exists

Two machines can only agree on meaning if they first agree, byte for byte, on
representation. MCL Wire is that agreement: one canonical encoding, no
optional orderings, no implementation-defined padding, and the same result on a
microcontroller and a server.

An encoder that produces different bytes for the same object is wrong, and the
conformance vectors in this repository are how you find out.

**Wire major 1 is Stable.** Major 0 remains for experimental objects; a Stable
major carries only Stable semantics, and the encoder refuses anything else.

The specification is language-neutral. The v1.0 reference implementation is **portable freestanding C99** so the same byte contract can be used on bare-metal microcontrollers, RTOS targets, larger embedded systems, and hosted systems.

```text
MCL Core semantic object
        |
        v
MCL Wire
canonical bytes / extensions / future context forms
        |
        v
MCL Link
contact / state / QoS / binding handoff
```

## Reference implementation contract

The C reference implementation:

- requires no operating system;
- requires no dynamic allocation;
- uses no hidden mutable global protocol state;
- requires no C bitfields or packed-struct serialization;
- encodes and decodes byte and bit positions explicitly;
- uses fixed-width protocol-facing integers;
- returns deterministic status codes;
- compiles with a C99 compiler without language extensions;
- supports freestanding builds with no required libc symbols;
- keeps the wire specification authoritative over the code.

## Major-0 research and historical subset

The C reference implements seven Tier-0 layouts:

- PRESENCE
- HAZARD
- REQUEST
- AUTHORITY_CLAIM
- DEGRADED_STATE
- TRANSPORT_OFFER
- TRANSPORT_ACCEPT

`TRANSPORT_ACCEPT` is the other half of a transport change. Without it the
offering peer never learns which transport was selected, so two independent
implementations could agree on an offer and then complete nothing — which is
exactly the test for whether something belongs in the specification.

The layouts are specified bit for bit in
[`spec/tier0-layout-v0.2.md`](spec/tier0-layout-v0.2.md), independently of this
code, and `tools/validate_tier0_layout.c` checks that specification against the
codec on every test run.

**Layout interoperability is substantially solved; meaning interoperability is
not.** A significant fraction of the primitive field meanings are still
unsettled. The authoritative count is not restated here, because a number
written into prose rots the moment a field closes: it is derived from
[`tier0-fields-v0.1.json`](../mcl-core/registries/tier0-fields-v0.1.json), which
is machine-checked, and printed by the local gate run. Three
of the seven objects are Stable at major 1; see
[`V1_SCOPE.md`](../mcl-core/governance/V1_SCOPE.md).

It uses the Stable major-1 v0.2 16-bit common header:

```text
4 bits major wire version
4 bits semantic category
5 bits opcode
2 bits priority
1 bit extension-present
```

The encoded sizes differ where the Stable major-1 body removes `machine_class`;
the table makes both wire majors explicit:

| Object | Major 0 bytes | Major 1 bytes |
|---|---:|---:|
| PRESENCE | 11 | 10 |
| HAZARD | 15 | — |
| REQUEST | 17 | — |
| AUTHORITY_CLAIM | 14 | — |
| DEGRADED_STATE | 10 | — |
| TRANSPORT_OFFER | 13 | 17 |
| TRANSPORT_ACCEPT | 16 | 16 |

`TRANSPORT_OFFER` uses an 8-bit transport identifier and is 17 bytes.

## Canonical extension envelope

The C reference includes the Candidate/Experimental extension envelope described
in [`spec/tier0-extensions-v0.1.md`](spec/tier0-extensions-v0.1.md):

```text
extension_key = uvarint((extension_id << 1) | critical)
length        = uvarint(value_length)
value         = exact value bytes
```

The parser exposes criticality explicitly. Unknown critical meaning is rejected by the semantic layer; unknown non-critical values can be skipped by exact length. Duplicate/out-of-order IDs, non-canonical varints, zero extension IDs, and truncation are rejected.

## Build and test

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The host test suite currently covers:

- all 65,536 possible common headers;
- 120,000 seeded randomized Tier-0 round trips;
- exact v0.2 vectors for all seven implemented layouts;
- one million randomized decoder inputs under sanitizer runs;
- 10,000 uvarint round trips;
- 2,000 randomized extension blocks;
- canonicality, truncation, range, and buffer-boundary failures;
- the published extension vectors pinned against the decoder, rather than only
  through C arrays, which prove nothing about the JSON an independent
  implementation would actually read;
- the Tier-0 body layout specification checked against the codec;
- the source-size benchmark, run as a test so its cases cannot rot.

The protocol library itself is also compiled in freestanding mode for small ARM and 32-bit RISC-V targets during portability checks.

## Source-size benchmark

`benchmarks/benchmark_source.c` reproduces the retained 41-event source-size study without a hosted-language runtime or serialization dependency. It independently calculates compact JSON, restricted CBOR, and proto3-equivalent scalar wire sizes and validates every MCL event through the C codec.

The expected means are:

```text
compact JSON                   121.927 B
CBOR string keys                87.439 B
CBOR integer keys               26.463 B
typed proto3-equivalent         22.488 B
MCL fixed Tier-0                15.073 B
historical context candidate    12.073 B
```

The 12.073-byte number remains a historical established-context research result. It is not delta coding and is not a first-contact encoding.

These are the figures the program prints today, and they have moved twice for reasons worth recording. The MCL rows grew when `TRANSPORT_ACCEPT` was added to the codec — a seventh object with its own body raises the mean. The two name-carrying baselines shrank by 9 bytes each when `capability_digest` was renamed to `capability_tag`, because a shorter key costs fewer bytes in any format that spells its field names out. That narrows MCL's own advantage slightly, which is the honest direction to report it in.

`benchmarks/results/source-codec-summary-v0.2.json` is the **retained v0.2 study** and is deliberately not rewritten to match. It records what was measured then.

## Stable v1.0 status

**Wire major 1 is cut and is part of MCL v1.0.** It carries `PRESENCE`,
`TRANSPORT_OFFER` and `TRANSPORT_ACCEPT` and refuses every other kind: a
Candidate object presented at the Stable major is rejected, not decoded. Major-1
`PRESENCE` drops `machine_class`, so it is 10 bytes where major 0 is 11. The
frozen bytes are [`vectors/tier0-major1-v1.0.json`](vectors/tier0-major1-v1.0.json),
which records expected **field values** and not only lengths and hex — a
clean-room implementation once passed a length check while misreading every
field after `source_ref`.

`mcl_wire_tier0_encode` still emits major 0, because v1.0 promises source
compatibility. Use `mcl_wire_tier0_encode_at_major` to choose.

Major 0 is permanent and does not change. `HAZARD`, `REQUEST`,
`AUTHORITY_CLAIM` and `DEGRADED_STATE` stay there: their layouts and vectors
exist, their *meanings* may still change, and that is exactly what a frozen
major may not contain.

Passing the local C tests is implementation evidence, not independent
interoperability or field validation. What independent evidence exists is
`mcl-core/conformance/independent/` — a clean-room implementation sharing no
code, no language and no build system — and v1.0 does not claim that anyone
outside this project has implemented or reviewed these specifications.
