<p align="center">
  <img src="https://raw.githubusercontent.com/machine-contact-layer/.github/main/profile/banner.png" alt="Machine Contact Layer (MCL) banner: black and white checkerboard with the OJOBIT wordmark" width="100%">
</p>

<h1 align="center">MCL Wire</h1>

<p align="center"><strong>The exact bytes. One canonical encoding for every MCL object, on every transport.</strong></p>

<p align="center">
  Canonical binary encoding and deterministic decoding for the Machine Contact
  Layer (MCL): a compact, freestanding C99 codec for machine-to-machine messages
  on microcontrollers, embedded systems and servers.
</p>

<p align="center">
  <a href="https://github.com/machine-contact-layer/mcl-wire/actions/workflows/ci.yml"><img alt="CI status" src="https://github.com/machine-contact-layer/mcl-wire/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/machine-contact-layer/mcl-wire/blob/main/LICENSE"><img alt="License: Apache-2.0" src="https://img.shields.io/badge/license-Apache--2.0-blue"></a>
  <img alt="Wire major 1: Stable" src="https://img.shields.io/badge/wire%20major-1%20Stable-brightgreen">
  <img alt="Language: freestanding C99" src="https://img.shields.io/badge/C99-freestanding-informational">
</p>

<p align="center">
  <a href="https://github.com/machine-contact-layer/mcl-sdk"><b>SDK</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-core"><b>MCL overview</b></a> ·
  <a href="#specifications"><b>Specifications</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-link"><b>mcl-link</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-core/blob/main/REPORTING.md"><b>Report a defect</b></a>
</p>

---

Two machines can only agree on meaning if they first agree, byte for byte, on
representation. MCL Wire is that agreement: one canonical encoding, no optional
orderings, no implementation-defined padding, and the same bytes on a
microcontroller and a server.

It is part of the [Machine Contact Layer](https://github.com/machine-contact-layer/mcl-core),
an open protocol for machine-to-machine discovery, contact and transport
migration across BLE, IP and acoustic links.

> **Building a product?** Start with [**mcl-sdk**](https://github.com/machine-contact-layer/mcl-sdk),
> which uses this codec for you. Come here when you need to know exactly what a
> byte means, or when you are writing your own implementation.

## What MCL Wire provides

- **One canonical encoding** per object. An encoder that produces different
  bytes for the same object is wrong, and the conformance vectors say so.
- **Deterministic rejection.** Truncated, non-canonical, out-of-range and
  unknown-critical input is refused with an explicit status, never guessed at.
- **Compact fixed layouts.** A Stable `PRESENCE` is 10 bytes; a
  `TRANSPORT_OFFER` is 17.
- **A canonical extension envelope** that lets unknown non-critical data be
  skipped by exact length.
- **Conformance vectors with field values**, not only lengths and hex, so an
  independent implementation can check what it decoded.

## Stable surface: Wire major 1

Wire major 1 is Stable. It carries `PRESENCE`, `TRANSPORT_OFFER` and
`TRANSPORT_ACCEPT` and refuses every other kind: a Candidate object presented
at the Stable major is rejected, not decoded.

| Object | Maturity | Major 0 bytes | Major 1 bytes |
|---|---|---:|---:|
| `PRESENCE` | Stable | 11 | 10 |
| `TRANSPORT_OFFER` | Stable | 13 | 17 |
| `TRANSPORT_ACCEPT` | Stable | 16 | 16 |
| `HAZARD` | Candidate | 15 | — |
| `REQUEST` | Candidate | 17 | — |
| `AUTHORITY_CLAIM` | Candidate | 14 | — |
| `DEGRADED_STATE` | Candidate | 10 | — |

Major-1 `PRESENCE` drops `machine_class`, which is why it is one byte shorter.
Major 0 is permanent and never changes. The Candidate objects stay there: their
layouts and vectors are fixed, but their meanings may still change, which is
exactly what a frozen major may not contain.

**Choosing the major.** `mcl_wire_tier0_encode` still emits major 0, because
v1.0 promises source compatibility. Use `mcl_wire_tier0_encode_at_major` to emit
the Stable major.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The host test suite covers all 65,536 possible common headers, 120,000 seeded
randomized Tier-0 round trips, exact vectors for all seven layouts, one million
randomized decoder inputs under sanitizers, extension-block round trips,
canonicality and buffer-boundary failures, and the published vectors pinned
against the decoder. The library is also compiled freestanding for small ARM
and 32-bit RISC-V targets.

## Common header

Every object begins with a 16-bit header:

```text
4 bits major wire version
4 bits semantic category
5 bits opcode
2 bits priority
1 bit extension-present
```

## Extension envelope

```text
extension_key = uvarint((extension_id << 1) | critical)
length        = uvarint(value_length)
value         = exact value bytes
```

The parser exposes criticality explicitly. Unknown critical meaning is rejected
by the semantic layer; unknown non-critical values are skipped by exact length.
Duplicate or out-of-order IDs, non-canonical varints, zero extension IDs and
truncation are rejected. No extension IDs are assigned in v1.0: the mechanism
ships and the table is empty by design.

## Reference implementation contract

The C reference implementation:

- requires no operating system and no dynamic allocation;
- uses no hidden mutable global protocol state;
- uses no C bitfields or packed-struct serialization;
- encodes and decodes byte and bit positions explicitly, with fixed-width integers;
- returns deterministic status codes;
- compiles with a C99 compiler without language extensions;
- supports freestanding builds with no required libc symbols.

The specification is authoritative over the code.

## Encoded size

`benchmarks/benchmark_source.c` encodes the same 41-event source set in several
formats, with no hosted runtime or serialization dependency:

```text
compact JSON                   121.927 B
CBOR string keys                87.439 B
CBOR integer keys               26.463 B
typed proto3-equivalent         22.488 B
MCL fixed Tier-0                15.073 B
```

The retained v0.2 study is in
[`benchmarks/results/source-codec-summary-v0.2.json`](benchmarks/results/source-codec-summary-v0.2.json).

## Specifications

- [`spec/common-header-v0.2.md`](spec/common-header-v0.2.md) — the 16-bit common header
- [`spec/tier0-layout-v0.2.md`](spec/tier0-layout-v0.2.md) — Tier-0 layouts, bit for bit; `tools/validate_tier0_layout.c` checks it against the codec on every test run
- [`spec/tier0-extensions-v0.1.md`](spec/tier0-extensions-v0.1.md) — the extension envelope
- [`spec/duration-v0.1.md`](spec/duration-v0.1.md) — duration encoding
- [`conformance/vectors/tier0-major1-v1.0.json`](conformance/vectors/tier0-major1-v1.0.json) — the frozen major-1 vectors
- [`registries/extension-ids-v0.1.json`](registries/extension-ids-v0.1.json) — the extension ID registry

Per-document maturity is in
[`mcl-core/SPECIFICATION_INDEX.md`](https://github.com/machine-contact-layer/mcl-core/blob/main/SPECIFICATION_INDEX.md).

## Related repositories

[mcl-core](https://github.com/machine-contact-layer/mcl-core) (semantics and
registries) · [mcl-link](https://github.com/machine-contact-layer/mcl-link)
(framing, sessions, migration) · [mcl-sdk](https://github.com/machine-contact-layer/mcl-sdk)
(developer SDK)

## License

Apache-2.0. See [`LICENSE`](LICENSE).
