# MCL Wire

`mcl-wire` defines the canonical deterministic representation of Machine Contact Layer semantic objects.

The specification is language-neutral. The primary pre-v0.1 reference implementation is **portable freestanding C99** so the same byte contract can be used on bare-metal microcontrollers, RTOS targets, larger embedded systems, and hosted systems.

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

## Current executable research subset

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
not.** Sixteen of the 21 primitive field meanings are still unsettled, recorded
field by field in
[`tier0-fields-v0.1.json`](../mcl-core/registries/tier0-fields-v0.1.json). Three
of the seven objects are proposed Stable for v1 on that basis; see
[`V1_SCOPE.md`](../mcl-core/governance/V1_SCOPE.md).

It uses the current v0.2 16-bit common-header candidate:

```text
4 bits major wire version
4 bits semantic category
5 bits opcode
2 bits priority
1 bit extension-present
```

The current encoded sizes remain:

| Object | Bytes |
|---|---:|
| PRESENCE | 11 |
| HAZARD | 15 |
| REQUEST | 17 |
| AUTHORITY_CLAIM | 14 |
| DEGRADED_STATE | 10 |
| TRANSPORT_OFFER | 13 |

`TRANSPORT_OFFER` uses an 8-bit transport identifier and remains 13 bytes after padding.

## Canonical extension envelope

The C reference includes the research extension envelope described in `research/extension-envelope-v0.1.md`:

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
compact JSON                   122.146 B
CBOR string keys                87.659 B
CBOR integer keys               26.463 B
typed proto3-equivalent         22.488 B
MCL fixed Tier-0                14.683 B
historical context candidate    11.683 B
```

The 11.683-byte number remains a historical established-context research result. It is not delta coding and is not a first-contact encoding.

## Status

Private research repository. Pre-v0.1 candidate specification and reference implementation. Passing the local C tests is implementation evidence, not independent interoperability or field validation.
