from __future__ import annotations


class ExtensionError(ValueError):
    pass


class UnknownCriticalExtension(ExtensionError):
    pass


def enc_uvarint(n: int) -> bytes:
    if n < 0:
        raise ValueError("negative")
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def dec_uvarint(data: bytes, pos: int = 0):
    start = pos
    n = 0
    shift = 0
    while True:
        if pos >= len(data):
            raise ExtensionError("truncated varint")
        b = data[pos]
        pos += 1
        n |= (b & 0x7F) << shift
        if not b & 0x80:
            raw = data[start:pos]
            if enc_uvarint(n) != raw:
                raise ExtensionError("non-canonical varint")
            return n, pos
        shift += 7
        if shift > 35:
            raise ExtensionError("varint too large")


def encode_extensions(items):
    """Encode `(extension_id, critical, value_bytes)` tuples canonically."""
    prev = -1
    out = bytearray()
    for ext_id, critical, value in sorted(items, key=lambda x: x[0]):
        if ext_id <= 0:
            raise ExtensionError("extension id 0 reserved")
        if ext_id == prev:
            raise ExtensionError("duplicate extension")
        prev = ext_id
        key = (ext_id << 1) | int(bool(critical))
        out += enc_uvarint(key)
        out += enc_uvarint(len(value))
        out += value
    return bytes(out)


def decode_extensions(data: bytes, known_ids=frozenset()):
    pos = 0
    prev = -1
    known = {}
    skipped = []
    while pos < len(data):
        key, pos = dec_uvarint(data, pos)
        ext_id, critical = key >> 1, bool(key & 1)
        if ext_id <= 0:
            raise ExtensionError("extension id 0 reserved")
        if ext_id <= prev:
            raise ExtensionError("non-canonical extension order or duplicate")
        prev = ext_id
        length, pos = dec_uvarint(data, pos)
        end = pos + length
        if end > len(data):
            raise ExtensionError("extension exceeds object boundary")
        value = data[pos:end]
        pos = end
        if ext_id in known_ids:
            known[ext_id] = value
        elif critical:
            raise UnknownCriticalExtension(ext_id)
        else:
            skipped.append(ext_id)
    return known, skipped
