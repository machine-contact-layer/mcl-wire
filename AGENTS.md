# MCL Wire implementation rules

Read `spec/common-header-v0.2.md`, `spec/wire-v0.md`, `research/field-width-review-v0.2.md`, `research/extension-envelope-v0.1.md`, and the conformance material before changing protocol-facing code.

## Authority

Wire owns canonical deterministic bytes. Core owns semantic meaning and assigned values. Link owns context authorization and session behavior.

The specification and reviewed registries are authoritative. The C reference implementation must never silently redefine them.

## Language and runtime baseline

The primary reference implementation is portable freestanding C99.

Protocol-facing code must:

- require no OS;
- require no heap or allocator;
- avoid hidden mutable globals;
- use caller-owned state and buffers;
- use fixed-width integer types for protocol values;
- avoid C bitfields, packed structs, unaligned loads, aliasing tricks, and native-struct serialization;
- encode byte order and bit order explicitly;
- return explicit deterministic status values;
- compile with `-ffreestanding -fno-builtin` without required libc symbols;
- require no compiler extensions;
- keep C++ compatibility through `extern "C"` headers.

Do not add a higher-level runtime, package manager, asynchronous framework, or object model to the primary Wire implementation.

## Current pre-v0.1 scope

Implement only the six measured Tier-0 layouts unless a reviewed specification change expands the set:

PRESENCE, HAZARD, REQUEST, AUTHORITY_CLAIM, DEGRADED_STATE, TRANSPORT_OFFER.

Use the v0.2 common header exactly as specified. Do not revive the direct 4-bit semantic-type header.

Use the 8-bit transport identifier direction for new TRANSPORT_OFFER vectors.

## Canonical rejection

Reject malformed or ambiguous input. At minimum preserve tests for:

- truncated input;
- output buffer too small;
- numeric fields outside their assigned bit width;
- unsupported category/opcode;
- non-zero canonical padding;
- duplicate/out-of-order extensions;
- extension ID zero;
- non-canonical uvarints;
- truncated extension values.

Unknown critical extensions must never be treated as safely ignorable by a semantic decoder.

## Testing gate

Before each protocol-facing commit run strict GCC and Clang host builds, CTest, sanitizer-backed randomized decode testing, and freestanding portability compilation.

Do not weaken warning flags to make a change pass. Fix the conversion, aliasing, lifetime, bounds, or portability issue.

Historical vectors and benchmark outputs must not be edited to make new code appear compatible. Create new versioned artifacts when bytes legitimately change.
