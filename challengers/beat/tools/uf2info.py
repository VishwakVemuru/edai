"""What a .uf2 firmware writes to flash: blocks, address range, family.   python3 uf2info.py file.uf2"""
import struct, sys
FAM = {0xE48BFF56: "RP2040", 0xE48BFF59: "RP2350 Arm secure", 0xE48BFF57: "absolute", 0xE48BFF5A: "RP2350 RISC-V"}
for p in sys.argv[1:]:
    b = open(p, "rb").read(); addrs = []; fams = set(); payload = 0
    for o in range(0, len(b), 512):
        m0, m1, flags, addr, size, bno, nb, fam = struct.unpack_from("<8I", b, o)
        if m0 != 0x0A324655: continue
        addrs.append(addr); payload += size
        if flags & 0x2000: fams.add(FAM.get(fam, hex(fam)))
    print(f"{p.split('/')[-1]}: {len(addrs)} blocks, {payload:,} B to flash at {min(addrs):#x}-{max(addrs)+256:#x}, {', '.join(sorted(fams))}")
