# Source Codec Benchmark v0.1

This benchmark measures representation size for **41 Tier-0-candidate normalized events** derived from the MCL Core scenario corpus. One generic `STATUS` example is excluded because the current Core registry treats `STATUS` as non-Tier-0.

This is **not** a semantic rate-distortion result yet. Every representation carries the same already-quantized scalar meaning, so this stage isolates representation overhead.

## Baselines and current mean size

| Representation | Mean |
|---|---:|
| compact JSON, readable keys | 122.15 B |
| CBOR subset, string keys | 87.66 B |
| CBOR subset, integer schema keys | 26.46 B |
| actual proto3 typed deterministic serialization | 22.49 B |
| MCL fixed Tier-0 candidate | 14.68 B |
| MCL fixed + established context | 11.68 B |

Against the strongest tested general typed baseline here, Protobuf, the fixed candidate is **34.7% smaller** on this corpus. Established context reduces the current mean to 11.68 B by replacing the 32-bit source reference with an 8-bit synchronized context ID.

These are corpus-specific early results, not general compression claims.

## Reproduction

```bash
python benchmarks/benchmark_source.py
```

The script performs a fixed-layout encode/decode round trip for every case before publishing sizes.

## Fairness notes

- Protobuf uses typed scalar fields and normal proto3 default omission.
- Integer-key CBOR is the stronger CBOR size baseline; string-key CBOR is retained as a schema-light reference.
- Link headers, CRC/MAC/signatures, FEC, and physical framing are deliberately excluded.
- `fixed_context` is illegal before explicit context establishment.
- Learned semantic selection is not in this stage because the deterministic baseline must be known first.

Source corpus SHA-256: `ac1f8ba0474d2def22b736a737305681144d6fae86ce327ce978ec1b24c783d5`
