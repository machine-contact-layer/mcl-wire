import os
import random
from extension_envelope import *


def main():
    for _ in range(10000):
        n = random.randrange(0, 1 << 28)
        raw = enc_uvarint(n)
        got, p = dec_uvarint(raw)
        assert got == n and p == len(raw)

    for _ in range(2000):
        ids = sorted(random.sample(range(1, 5000), random.randrange(0, 12)))
        items = [(i, bool(random.getrandbits(1)), os.urandom(random.randrange(0, 40))) for i in ids]
        raw = encode_extensions(items)
        known_ids = {i for i, _, _ in items}
        known, skipped = decode_extensions(raw, known_ids)
        assert skipped == []
        assert known == {i: v for i, _, v in items}

    raw = encode_extensions([(7, False, b"future")])
    known, skipped = decode_extensions(raw, set())
    assert known == {} and skipped == [7]

    raw = encode_extensions([(7, True, b"must-understand")])
    try:
        decode_extensions(raw, set())
        raise AssertionError("critical extension was accepted")
    except UnknownCriticalExtension:
        pass

    try:
        encode_extensions([(3, False, b"a"), (3, False, b"b")])
        raise AssertionError("duplicate accepted")
    except ExtensionError:
        pass

    try:
        dec_uvarint(bytes([0x80, 0x00]))
        raise AssertionError("noncanonical varint accepted")
    except ExtensionError:
        pass

    try:
        decode_extensions(bytes([0x02, 0x05, 0x01]))
        raise AssertionError("truncated extension accepted")
    except ExtensionError:
        pass

    print("uvarint random roundtrips: 10000")
    print("extension block random roundtrips: 2000")
    print("unknown optional skip: PASS")
    print("unknown critical reject: PASS")
    print("canonicality/length negatives: PASS")


if __name__ == "__main__":
    main()
