"""Read-only UE5.8 saved package name-table audit, not a general asset loader."""
import json
from pathlib import Path
import struct
import sys

def read_names(path):
    data=Path(path).read_bytes()
    tag,legacy,ue3,ue4,ue5,licensee=struct.unpack_from('<I5i',data)
    assert tag==0x9E2A83C1 and legacy==-9, (hex(tag),legacy,ue4,ue5)
    assert ue4==522 and ue5>=1016, (ue4,ue5)
    # PackageFileSummary.cpp: saved hash (20 bytes), total header size, optimized custom versions.
    pos=24+20
    header_size,custom_count=struct.unpack_from('<ii',data,pos)
    assert 0<custom_count<200 and 0<header_size<=len(data)
    pos+=8+custom_count*20
    def fstring(offset):
        count=struct.unpack_from('<i',data,offset)[0];offset+=4
        assert abs(count)<10000
        size=abs(count)*(2 if count<0 else 1)
        text=data[offset:offset+size].decode('utf-16-le' if count<0 else 'utf-8').rstrip('\0')
        return text,offset+size
    package,pos=fstring(pos)
    flags,name_count,name_offset=struct.unpack_from('<Iii',data,pos)
    assert 0<name_count<20000 and pos<name_offset<header_size
    pos=name_offset;names=[]
    for _ in range(name_count):
        name,pos=fstring(pos);names.append(name);pos+=4 # two uint16 name hashes
    assert pos<=header_size
    needle='/Game/Village/Meshes/MAT_Leaf'.encode('utf-16-le');offsets=[]
    found=data.find(needle)
    while found>=0:
        offsets.append(found);found=data.find(needle,found+len(needle))
    return {'file':str(path),'ue4':ue4,'ue5':ue5,'package':package,
            'name_count':name_count,'name_offset':name_offset,'names_end':pos,'header_size':header_size,
            'package_names':[n for n in names if n.startswith('/Game/')],
            'legacy_leaf_names':[n for n in names if 'MAT_Leaf' in n],
            'legacy_leaf_package_in_name_table':'/Game/Village/Meshes/MAT_Leaf' in names,
            'legacy_leaf_utf16_offsets':offsets}

if __name__=='__main__':
    print(json.dumps(read_names(sys.argv[1]),ensure_ascii=False,indent=2))
