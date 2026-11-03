#!/usr/bin/env python3
"""Verifica os JALs e delay slots originais antes de usar o projeto."""
import sys,struct,hashlib
from pathlib import Path
EXPECTED_HASH="834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e"
BASE=0x08804040
BASE_OFFSET=0x1018
CASES=[
 (0x089AB02C, 0x0E26AF95, "jal 0x089ABE54 - ramo A"),
 (0x089AB030, 0x02202825, "delay slot A"),
 (0x089AB734, 0x0E26AF95, "jal 0x089ABE54 - ramo B"),
 (0x089AB738, 0x02E02825, "delay slot B"),
 (0x089AB538, 0xE60C0070, "swc1 pos x"),
 (0x089AB54C, 0xE60C0074, "swc1 pos y"),
 (0x089AB560, 0xE60C0078, "swc1 pos z"),
]
if len(sys.argv)!=2: raise SystemExit('Uso: python tests/verificar_elf.py CAMINHO_DO_EBOOT.ELF')
b=Path(sys.argv[1]).read_bytes()
checksum=hashlib.sha256(b).hexdigest()
print('ELF SHA256:',checksum)
if checksum!=EXPECTED_HASH: raise SystemExit('ERRO: ELF nao corresponde a referencia')
for va,expected,name in CASES:
    off=BASE_OFFSET+(va-BASE)
    actual=struct.unpack_from('<I',b,off)[0]
    print(f'{name}: 0x{va:08X} -> 0x{actual:08X}')
    assert actual==expected,(name,hex(actual),hex(expected))
print('OK: todos os enderecos/opcodes conferem.')
