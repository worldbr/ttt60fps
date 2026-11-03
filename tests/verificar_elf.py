#!/usr/bin/env python3
from pathlib import Path
import hashlib,struct,sys
p=Path(sys.argv[1]);b=p.read_bytes()
assert hashlib.sha256(b).hexdigest()=="834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e"
for addr,w in [(0x0885AD5C,0x0E201A3F),(0x0885AD60,0x02002025)]:
 off=0x1018+(addr-0x08804040);v=struct.unpack_from('<I',b,off)[0]
 assert v==w,(hex(addr),hex(v),hex(w))
 print(hex(addr),hex(v),'OK')
