# Field-Width Review v0.2

Status: **Research Note**

This review asks whether early v0.1 bit allocations are suitable for a protocol family intended to evolve for decades.

## 1. Direct semantic type space

The v0.1 common header used a 4-bit direct `semantic_type`, giving only 16 global values.

MCL Core already defines 20 semantic classes, so this allocation is not suitable as the long-term global namespace even though only six event layouts were exercised by the current benchmark.

The v0.2 common-header study replaces this with category + opcode while preserving a two-byte common header.

## 2. Transport identifier

The v0.1 benchmark used a 4-bit `transport_id`, allowing 16 transport bindings.

For the current fixed `TRANSPORT_OFFER` layout:

```text
16-bit common header
32-bit source_ref
4-bit transport_id
8-bit profile_id
32-bit endpoint_token
8-bit validity
= 100 bits -> 13 bytes after byte padding
```

Expanding `transport_id` to 8 bits gives:

```text
16 + 32 + 8 + 8 + 32 + 8 = 104 bits -> 13 bytes
```

Therefore the transport namespace can expand from 16 to 256 values with **zero byte-size increase** for this candidate layout.

Recommendation: future v0.2 Wire work SHOULD use an 8-bit transport identifier unless a stronger end-to-end benchmark shows a reason not to.

## 3. Profile identifiers

The current 8-bit `profile_id` provides 256 values per transport when identifiers are transport-scoped.

This is provisionally sufficient because:
- profile IDs are not global across every transport;
- incompatible profiles receive new IDs;
- experimental/private ranges can be defined per binding;
- extended profile negotiation can be introduced later if genuinely required.

No AP research-family name should receive a permanent numeric profile ID before its conformance contract is frozen.

## 4. Source/context references

The 32-bit ephemeral source reference remains a plausible first-contact candidate, but context mode needs its own deterministic envelope and generation/digest guarding.

The existing 11.68-byte established-context benchmark MUST NOT be interpreted as a first-contact size or as proof of a final context envelope.

## 5. Rule for future bit allocation

Bit widths should be reviewed against three costs simultaneously:

1. on-air bytes after actual byte/frame padding;
2. long-term namespace exhaustion/collision risk;
3. compatibility cost of expanding the field later.

Saving bits that do not reduce transmitted bytes is usually a poor trade when it creates a scarce global namespace.
