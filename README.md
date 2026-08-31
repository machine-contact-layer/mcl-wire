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

The first C reference slice implements the six Tier-0 layouts already supported by the measured source benchmark:

- PRESENCE
- HAZARD
- REQUEST
- AUTHORITY_CLAIM
- DEGRADED_STATE
- TRANSPORT_OFFER

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
- exact v0.2 vectors for all six implemented layouts;
- one million randomized decoder inputs under sanitizer runs;
- 10,000 uvarint round trips;
- 2,000 randomized extension blocks;
- canonicality, truncation, range, and buffer-boundary failures.

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
