#!/usr/bin/env python3
"""Confirm the exact byte layout of the ULUS10537 EBOOT original used in the research."""
import hashlib
import struct
import sys
from pathlib import Path

SHA = "834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e"
CALLS = [
    (0x0885AC9C, 0x0E21EE6B, 0x00000000, 0x0887B9AC),
    (0x0887B9D4, 0x0E240784, 0x8C840000, 0x08901E10),
    (0x0885ACAC, 0x0E21EE94, 0x00000000, 0x0887BA50),
    (0x0887BB0C, 0x0E24084D, 0x8C840000, 0x08902134),
    (0x08804728, 0x0E20DEDA, 0x00000000, 0x08837B68),
]


def check(p):
    b = Path(p).read_bytes()
    digest = hashlib.sha256(b).hexdigest()
    print("SHA-256:", digest)
    assert digest == SHA, "ELF não corresponde à versão usada na pesquisa"
    for site, word, delay, target in CALLS:
        pos = 0x1018 + site - 0x08804040
        got, delay_got = struct.unpack_from('<II', b, pos)
        decoded = ((site + 4) & 0xF0000000) | ((got & 0x3FFFFFF) << 2)
        assert got == word, f"opcode divergente em {site:08X}: {got:08X}"
        assert delay_got == delay, f"delay slot divergente em {site:08X}: {delay_got:08X}"
        assert decoded == target, f"destino divergente em {site:08X}: {decoded:08X}"
        print(f"OK 0x{site:08X} = 0x{got:08X}, delay 0x{delay:08X} -> 0x{target:08X}")
    print("OK: cinco pontos de hook verificados no EBOOT original.")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Uso: python tests/verificar_elf.py <arquivo-ELF-original>')
    check(sys.argv[1])
