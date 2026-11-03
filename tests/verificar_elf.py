"""Opcional: verifica os sites do EBOOT original. Ignora se o ELF não estiver no projeto."""
import hashlib
import pathlib
import struct

paths = list(pathlib.Path('.').glob('*EBOOT.OLD*.ELF'))
if not paths:
    print('ELF nao encontrado, validacao estatica opcional ignorada.')
else:
    b = paths[0].read_bytes()
    print('sha256:', hashlib.sha256(b).hexdigest())
    for va, op, delay in [(0x089AC4E0, 0x0E26AF95, 0x02402825), (0x0885AD5C,0x0E201A3F,0x02002025)]:
        offset = 0x1018 + va-0x08804040
        got = struct.unpack_from('<II',b,offset)
        assert got == (op,delay),(hex(va),got)
        print(hex(va), 'JAL e delay conferem')
