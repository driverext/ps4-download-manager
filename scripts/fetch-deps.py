#!/usr/bin/env python3
from pathlib import Path
import urllib.request,hashlib
root=Path(__file__).resolve().parents[1]
path=root/'include/nlohmann/json.hpp'
expected='9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6'
if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
 data=urllib.request.urlopen('https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp',timeout=60).read()
 if hashlib.sha256(data).hexdigest()!=expected:raise SystemExit('Dependency checksum mismatch')
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
print('nlohmann/json 3.11.3 verified')
