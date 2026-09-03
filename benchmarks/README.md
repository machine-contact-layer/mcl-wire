# Source Codec Benchmark

This benchmark measures representation size for **41 Tier-0-candidate normalized events** derived from the MCL Core scenario corpus. One generic STATUS example remains excluded because the current Core registry treats STATUS as non-Tier-0.

The benchmark is a representation-overhead study, not a semantic rate-distortion result.

## C reproduction

Build the repository and run:

```text
./build/mcl_wire_benchmark_source
```

The C benchmark calculates the general-format baseline sizes directly from their encoding rules and passes every MCL case through the current C Tier-0 encoder/decoder before reporting the fixed-size result.

Expected means:

| Representation | Mean |
|---|---:|
| compact JSON, readable keys | 121.927 B |
| restricted CBOR, string keys | 87.439 B |
| restricted CBOR, integer schema keys | 26.463 B |
| proto3-equivalent typed scalar encoding | 22.488 B |
| MCL fixed Tier-0 candidate | 15.073 B |
| historical fixed + established-context candidate | 12.073 B |

These are what the program prints today. Two changes moved them:

- the MCL rows grew when `TRANSPORT_ACCEPT` was added — a seventh object with
  its own body raises the mean;
- the two rows that spell field names out, compact JSON and string-key CBOR,
  each fell by 9 bytes when `capability_digest` was renamed to `capability_tag`.
  A shorter key is cheaper in any format that carries its names, so this
  **narrows** MCL's advantage. Reported in that direction on purpose.

`results/source-codec-summary-v0.2.json` is the retained v0.2 study and is not
rewritten to match. It records what was measured then, and its `fixed_v0` mean
already disagrees with its own `fixed_total` — noted here rather than quietly
corrected, because re-deriving a dated study to make it look current is exactly
what must not happen to a retained result.

The historical context result replaces a 32-bit source reference with an 8-bit synchronized context ID. It is illegal before explicit context establishment and is not evidence for delta coding.

Link headers, integrity/authentication fields, FEC, and physical framing remain outside this source-size comparison.

Source corpus SHA-256: `305b16c96d951404066f686533be00e9f4dc199daeec0e618a04f2385aebe4ec`
