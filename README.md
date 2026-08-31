# MCL Wire

`mcl-wire` defines the canonical deterministic representation of Machine Contact Layer semantic objects.

It sits between `mcl-core` and `mcl-link`:

```text
MCL Core semantic object
        ↓
MCL Wire
canonical encoding / context / delta / priority
        ↓
MCL Link
framing / sessions / QoS / transport profile
```

## Scope

- canonical field ordering and type representation
- Tier-0 bounded microframes
- Tier-1 structured binary objects
- context establishment
- stateful/delta representation
- semantic priority metadata
- compatibility/version rules
- golden test vectors
- baseline comparisons against JSON, CBOR, typed binary formats, and fixed-layout bit fields

## Design rules

1. The same semantic object MUST have one canonical wire representation for a given version/context.
2. Wire decoding MUST NOT require an AI model.
3. Context compression may reduce transmission size but MUST provide deterministic resynchronization behavior.
4. Semantic priority is explicit metadata for downstream protection/scheduling; it does not authorize actions.
5. Rich or model-specific payloads belong in extensions, not the mandatory core.

## Status

Private research repository. Pre-v0.1 candidate specification.

See [`spec/wire-v0.md`](spec/wire-v0.md).
