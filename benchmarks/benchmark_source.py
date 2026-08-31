#!/usr/bin/env python3
"""Reproduce the MCL v0.1 source-codec size benchmark.

Inputs are already-normalized scalar events derived from mcl-core's scenario corpus.
This benchmark is non-normative. It compares representation overhead, not task quality.
"""
from __future__ import annotations
import argparse, csv, json, statistics, struct
from pathlib import Path

FIELD_NUM = {
    "type":1,"priority":2,"source_ref":3,"machine_class":4,"capability_digest":5,"ttl":6,
    "hazard_class":7,"severity":8,"confidence":9,"x":10,"y":11,"z":12,"radius":13,
    "request_class":14,"target_ref":15,"authority_class":16,"jurisdiction":17,
    "credential_ref":18,"validity":19,"affected_capability":20,"health":21,
    "transport_id":23,"profile_id":24,"endpoint_token":25,
}
SIGNED = {"x","y","z"}

def _cbor_head(major: int, n: int) -> bytes:
    if n < 24: return bytes([(major << 5) | n])
    if n <= 0xff: return bytes([(major << 5) | 24, n])
    if n <= 0xffff: return bytes([(major << 5) | 25]) + struct.pack(">H", n)
    if n <= 0xffffffff: return bytes([(major << 5) | 26]) + struct.pack(">I", n)
    return bytes([(major << 5) | 27]) + struct.pack(">Q", n)

def restricted_canonical_cbor(obj) -> bytes:
    """Minimal deterministic CBOR encoder for the integer/string/map corpus used here."""
    if isinstance(obj, int):
        return _cbor_head(0, obj) if obj >= 0 else _cbor_head(1, -1-obj)
    if isinstance(obj, str):
        b = obj.encode("utf-8"); return _cbor_head(3, len(b)) + b
    if isinstance(obj, dict):
        pairs = [(restricted_canonical_cbor(k), restricted_canonical_cbor(v)) for k,v in obj.items()]
        pairs.sort(key=lambda kv: (len(kv[0]), kv[0]))
        return _cbor_head(5, len(pairs)) + b"".join(k+v for k,v in pairs)
    raise TypeError(type(obj))

class BitWriter:
    def __init__(self): self.bits = []
    def u(self, value: int, width: int):
        if not 0 <= value < (1 << width): raise ValueError((value, width))
        self.bits.extend((value >> i) & 1 for i in range(width-1, -1, -1))
    def s(self, value: int, width: int):
        if value < 0: value = (1 << width) + value
        self.u(value, width)
    def finish(self) -> bytes:
        while len(self.bits) % 8: self.bits.append(0)
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            v = 0
            for bit in self.bits[i:i+8]: v = (v << 1) | bit
            out.append(v)
        return bytes(out)

class BitReader:
    def __init__(self, data: bytes):
        self.bits = [(b >> i) & 1 for b in data for i in range(7,-1,-1)]
        self.p = 0
    def u(self, width: int) -> int:
        v = 0
        for _ in range(width):
            v = (v << 1) | self.bits[self.p]; self.p += 1
        return v
    def s(self, width: int) -> int:
        v = self.u(width)
        return v - (1 << width) if v & (1 << (width-1)) else v

def fixed_encode(d: dict, context: bool=False) -> bytes:
    w = BitWriter()
    w.u(0,2); w.u(d["type"],4); w.u(d["priority"],2)
    w.u(0x80 if context else 0, 8)
    if context: w.u(1,8)
    else: w.u(d["source_ref"],32)
    t = d["type"]
    if t == 0:
        w.u(d["machine_class"],8); w.u(d["capability_digest"],24); w.u(d["ttl"],8)
    elif t == 1:
        w.u(d["hazard_class"],8); w.u(d["severity"],3); w.u(d["confidence"],7)
        w.s(d["x"],12); w.s(d["y"],12); w.s(d["z"],10); w.u(d["radius"],10); w.u(d["ttl"],8)
    elif t == 2:
        w.u(d["request_class"],8); w.u(d["target_ref"],32)
        w.s(d["x"],12); w.s(d["y"],12); w.u(d["radius"],10); w.u(d["ttl"],8)
    elif t == 3:
        w.u(d["authority_class"],6); w.u(d["jurisdiction"],12); w.u(d["credential_ref"],32); w.u(d["validity"],8)
    elif t == 4:
        w.u(d["affected_capability"],8); w.u(d["health"],7); w.u(d["severity"],3); w.u(d["ttl"],8)
    elif t == 5:
        w.u(d["transport_id"],4); w.u(d["profile_id"],8); w.u(d["endpoint_token"],32); w.u(d["validity"],8)
    else: raise ValueError(f"unknown type {t}")
    return w.finish()

def fixed_decode(data: bytes) -> dict:
    r = BitReader(data)
    version = r.u(2); typ = r.u(4); priority = r.u(2); flags = r.u(8)
    context = bool(flags & 0x80)
    d = {"type":typ, "priority":priority}
    if context: d["context_id"] = r.u(8); d["source_ref"] = None
    else: d["source_ref"] = r.u(32)
    if typ == 0:
        d.update(machine_class=r.u(8), capability_digest=r.u(24), ttl=r.u(8))
    elif typ == 1:
        d.update(hazard_class=r.u(8), severity=r.u(3), confidence=r.u(7), x=r.s(12), y=r.s(12), z=r.s(10), radius=r.u(10), ttl=r.u(8))
    elif typ == 2:
        d.update(request_class=r.u(8), target_ref=r.u(32), x=r.s(12), y=r.s(12), radius=r.u(10), ttl=r.u(8))
    elif typ == 3:
        d.update(authority_class=r.u(6), jurisdiction=r.u(12), credential_ref=r.u(32), validity=r.u(8))
    elif typ == 4:
        d.update(affected_capability=r.u(8), health=r.u(7), severity=r.u(3), ttl=r.u(8))
    elif typ == 5:
        d.update(transport_id=r.u(4), profile_id=r.u(8), endpoint_token=r.u(32), validity=r.u(8))
    d["_wire_version"] = version
    return d

def protobuf_factory():
    try:
        from google.protobuf import descriptor_pb2, descriptor_pool, message_factory
    except ImportError:
        return None
    fd=descriptor_pb2.FileDescriptorProto(); fd.name="mcl_bench.proto"; fd.package="mclbench"; fd.syntax="proto3"
    m=fd.message_type.add(); m.name="Event"
    for name,num in FIELD_NUM.items():
        f=m.field.add(); f.name=name; f.number=num
        f.label=descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
        f.type=(descriptor_pb2.FieldDescriptorProto.TYPE_SINT32 if name in SIGNED else descriptor_pb2.FieldDescriptorProto.TYPE_UINT32)
    pool=descriptor_pool.DescriptorPool(); pool.Add(fd)
    return message_factory.GetMessageClass(pool.FindMessageTypeByName("mclbench.Event"))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--cases", type=Path, default=Path(__file__).with_name("cases-v0.1.json"))
    ap.add_argument("--out", type=Path, default=Path(__file__).parent/"results"/"source-codec-v0.1.csv")
    args=ap.parse_args(); cases=json.loads(args.cases.read_text())["events"]; Event=protobuf_factory(); rows=[]
    for i,d in enumerate(cases):
        fixed=fixed_encode(d,False); decoded=fixed_decode(fixed); decoded.pop("_wire_version")
        if decoded != d: raise AssertionError((i,d,decoded))
        compact={FIELD_NUM[k]:v for k,v in d.items()}
        rows.append({"event_index":i,"type":d["type"],"json_bytes":len(json.dumps(d,separators=(",",":"),sort_keys=True).encode()),"cbor_string_keys_bytes":len(restricted_canonical_cbor(d)),"cbor_integer_keys_bytes":len(restricted_canonical_cbor(compact)),"protobuf_typed_bytes":len(Event(**d).SerializeToString(deterministic=True)) if Event else -1,"fixed_v0_bytes":len(fixed),"fixed_context_bytes":len(fixed_encode(d,True))})
    args.out.parent.mkdir(parents=True,exist_ok=True)
    with args.out.open("w",newline="") as fp:
        w=csv.DictWriter(fp,fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
    for k in [x for x in rows[0] if x.endswith("_bytes")]:
        vals=[r[k] for r in rows if r[k] >= 0]; print(f"{k}: mean={statistics.mean(vals):.2f} B")
    print(f"roundtrip vectors: {len(rows)} / {len(rows)}")

if __name__ == "__main__": main()
