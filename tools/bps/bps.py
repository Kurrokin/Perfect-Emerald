#!/usr/bin/env python3
"""
bps.py - create / apply BPS patches (byuu's "beat" format). Python 3, no
dependencies. Output works with Floating IPS, Rom Patcher JS, beat, etc.

  python3 tools/bps/bps.py create  clean.gba  hack.gba  hack.bps
  python3 tools/bps/bps.py apply   clean.gba  hack.bps  out.gba

Encoder: SourceRead (unchanged bytes), SourceCopy (moved data, found through a
hash index of the clean ROM and extended both ways), TargetCopy (fill runs),
TargetRead (new bytes). Every patch is decoded again after creation and
checked byte-for-byte before it is written.
"""
import sys, struct, zlib

MAGIC = b'BPS1'
KEY = 16            # match key length (bytes)
STEP = 4            # index every STEP-th source offset
MIN_MATCH = 12      # shorter matches are cheaper as literal bytes


def _enc(n):
    out = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(0x80 | x)
            return bytes(out)
        out.append(x)
        n -= 1


def _dec(buf, pos):
    data, shift = 0, 1
    while True:
        x = buf[pos]
        pos += 1
        data += (x & 0x7F) * shift
        if x & 0x80:
            return data, pos
        shift <<= 7
        data += shift


def _signed(d):
    return (abs(d) << 1) | (1 if d < 0 else 0)


def create(src, tgt):
    src, tgt = bytes(src), bytes(tgt)
    ns, nt = len(src), len(tgt)
    out = bytearray(MAGIC)
    out += _enc(ns) + _enc(nt) + _enc(0)

    index = {}
    for off in range(0, ns - KEY + 1, STEP):
        index.setdefault(src[off:off + KEY], off)

    pos = 0
    lit = 0                 # start of pending TargetRead bytes
    src_rel = tgt_rel = 0

    def flush(end):
        if end > lit:
            out.extend(_enc(((end - lit - 1) << 2) | 1))
            out.extend(tgt[lit:end])

    while pos < nt:
        # 1) unchanged bytes at the same offset -> SourceRead
        if pos < ns and src[pos] == tgt[pos]:
            run = 1
            limit = min(ns, nt) - pos
            while run + 64 <= limit and src[pos + run:pos + run + 64] == tgt[pos + run:pos + run + 64]:
                run += 64
            while run < limit and src[pos + run] == tgt[pos + run]:
                run += 1
            if run >= MIN_MATCH:
                flush(pos)
                out.extend(_enc(((run - 1) << 2) | 0))
                pos += run
                lit = pos
                continue

        # 2) repeated fill byte -> overlapping TargetCopy from pos-1
        if pos > 0 and tgt[pos] == tgt[pos - 1]:
            b = tgt[pos - 1:pos]
            rep = 1
            while pos + rep + 64 <= nt and tgt[pos + rep:pos + rep + 64] == b * 64:
                rep += 64
            while pos + rep < nt and tgt[pos + rep] == tgt[pos - 1]:
                rep += 1
            if rep >= MIN_MATCH:
                flush(pos)
                out.extend(_enc(((rep - 1) << 2) | 3))
                out.extend(_enc(_signed((pos - 1) - tgt_rel)))
                tgt_rel = pos - 1 + rep
                pos += rep
                lit = pos
                continue

        # 3) moved data -> SourceCopy
        s_off = index.get(tgt[pos:pos + KEY]) if pos + KEY <= nt else None
        if s_off is not None:
            start, s0 = pos, s_off
            while start > lit and s0 > 0 and src[s0 - 1] == tgt[start - 1]:
                start -= 1
                s0 -= 1
            end, s1 = pos + KEY, s_off + KEY
            while end + 64 <= nt and s1 + 64 <= ns and src[s1:s1 + 64] == tgt[end:end + 64]:
                end += 64
                s1 += 64
            while end < nt and s1 < ns and src[s1] == tgt[end]:
                end += 1
                s1 += 1
            length = end - start
            if length >= MIN_MATCH:
                flush(start)
                out.extend(_enc(((length - 1) << 2) | 2))
                out.extend(_enc(_signed(s0 - src_rel)))
                src_rel = s0 + length
                pos = end
                lit = pos
                continue

        pos += 1                # literal byte, stays pending

    flush(nt)
    out += struct.pack('<I', zlib.crc32(src) & 0xFFFFFFFF)
    out += struct.pack('<I', zlib.crc32(tgt) & 0xFFFFFFFF)
    out += struct.pack('<I', zlib.crc32(bytes(out)) & 0xFFFFFFFF)
    patch = bytes(out)
    if apply(src, patch) != tgt:
        raise RuntimeError('internal error: patch does not reproduce the target')
    return patch


def apply(src, patch):
    src, patch = bytes(src), bytes(patch)
    if patch[:4] != MAGIC:
        raise ValueError('not a BPS patch')
    if zlib.crc32(patch[:-4]) & 0xFFFFFFFF != struct.unpack('<I', patch[-4:])[0]:
        raise ValueError('patch file is corrupted (CRC mismatch)')
    pos = 4
    ssize, pos = _dec(patch, pos)
    tsize, pos = _dec(patch, pos)
    msize, pos = _dec(patch, pos)
    pos += msize
    if len(src) != ssize or zlib.crc32(src) & 0xFFFFFFFF != struct.unpack('<I', patch[-12:-8])[0]:
        raise ValueError('wrong source ROM: needs a clean Pokemon Emerald (U) ROM, CRC32 1F1C08FB')
    tgt = bytearray()
    s_rel = t_rel = 0
    end = len(patch) - 12
    while pos < end:
        data, pos = _dec(patch, pos)
        cmd, length = data & 3, (data >> 2) + 1
        if cmd == 0:                                   # SourceRead
            tgt += src[len(tgt):len(tgt) + length]
        elif cmd == 1:                                 # TargetRead
            tgt += patch[pos:pos + length]
            pos += length
        elif cmd == 2:                                 # SourceCopy
            d, pos = _dec(patch, pos)
            s_rel += -(d >> 1) if d & 1 else (d >> 1)
            tgt += src[s_rel:s_rel + length]
            s_rel += length
        else:                                          # TargetCopy (may overlap)
            d, pos = _dec(patch, pos)
            t_rel += -(d >> 1) if d & 1 else (d >> 1)
            for _ in range(length):
                tgt.append(tgt[t_rel])
                t_rel += 1
    if len(tgt) != tsize or zlib.crc32(tgt) & 0xFFFFFFFF != struct.unpack('<I', patch[-8:-4])[0]:
        raise ValueError('output CRC mismatch')
    return bytes(tgt)


def main():
    if len(sys.argv) != 5 or sys.argv[1] not in ('create', 'apply'):
        print(__doc__)
        sys.exit(1)
    a = open(sys.argv[2], 'rb').read()
    b = open(sys.argv[3], 'rb').read()
    if sys.argv[1] == 'create':
        p = create(a, b)
        open(sys.argv[4], 'wb').write(p)
        print(f'{sys.argv[4]}: {len(p):,} bytes (verified against the target)')
    else:
        open(sys.argv[4], 'wb').write(apply(a, b))
        print(f'{sys.argv[4]}: written, CRC OK')


if __name__ == '__main__':
    main()
