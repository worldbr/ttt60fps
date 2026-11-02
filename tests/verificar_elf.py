#!/usr/bin/env python3
"""Opcional: confirmar a instrução original no ELF citado no README."""
import sys, struct, hashlib
from pathlib import Path
file = Path(sys.argv[1]) if len(sys.argv) > 1 else None
if not file or not file.is_file():
    raise SystemExit('Uso: python tests/verificar_elf.py EBOOT.OLD.ELF')
data = file.read_bytes()
print('SHA256:', hashlib.sha256(data).hexdigest())
# ELF PT_LOAD: virtual=0x08804040, arquivo=0x1018.
va = 0x0887B9D4
pos = 0x1018 + (va - 0x08804040)
word = struct.unpack_from('<I', data, pos)[0]
print('0x%08X: 0x%08X (esperado 0x0E240784)' % (va, word))
assert word == 0x0E240784, 'Este ELF nao contem o opcode esperado'
