#!/usr/bin/env python3
"""Differential native/Python extraction tests using entirely synthetic discs.

Build the driver:
 g++ -std=c++17 -O1 port/tests/installer/native_extract.cpp -lSDL3 -lpthread -o port/build/native-extract-test
Run: python3 port/tests/installer/native_extract.py
Python is only used by this developer test and the reference readers.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from extract_jp import extract as python_jp
from extract_wii_online import extract as python_wii


def pack(parts, endian='>'):
    offset = 4 * (len(parts) + 2)
    offsets = [offset]
    blobs = []
    for part in parts:
        blob = part + bytes(-len(part) % 4)
        blobs.append(blob)
        offset += len(blob)
        offsets.append(offset)
    return struct.pack(endian + f'{len(parts)+2}I', len(parts), *offsets) + b''.join(blobs)


def bpe(data, endian='>'):
    blocks = []
    for start in range(0, len(data), 65535):
        block = data[start:start+65535]
        blocks.append(b'\xff\x80\xfe' + struct.pack('>H', len(block)) + block)
    body = b''.join(blocks)
    return struct.pack(endian+'II', len(data), len(body)) + body


def afs(entries, count=None, names=None):
    count = count or max(entries)+1
    data = bytearray(8 + count*8 + 8)
    data[:8] = struct.pack('<4sI', b'AFS\0', count)
    for index, blob in entries.items():
        struct.pack_into('<II', data, 8+index*8, len(data), len(blob))
        data.extend(blob)
    if names:
        offset = len(data)
        for name in names:
            data.extend(name.encode().ljust(48, b'\0'))
        struct.pack_into('<II', data, 8+count*8, offset, count*48)
    return bytes(data)


def iso_record(name, sector, size, directory=False):
    length = 33 + len(name)
    length += length % 2
    data = bytearray(length)
    data[0] = length
    struct.pack_into('<I', data, 2, sector)
    struct.pack_into('>I', data, 6, sector)
    struct.pack_into('<I', data, 10, size)
    struct.pack_into('>I', data, 14, size)
    data[25] = 2 if directory else 0
    data[28:32] = b'\1\0\0\1'
    data[32] = len(name)
    data[33:33+len(name)] = name
    return data


def japanese_iso(path):
    logo = struct.pack('<I', 1) + bytes(132480-4)
    names = struct.pack('<I', 4) + bytes(431424-4)
    menus = afs({448: pack([bpe(pack([b'1234']*4+[logo]+[b'1234']*9, '<'), '<')], '<'),
                 447: pack([b'1234', bpe(pack([b'1234']*2+[names]*2, '<'), '<')], '<')})
    track = bytearray(2048)
    track[:8] = bytes.fromhex('800007fc03120402')
    struct.pack_into('>I', track, 8, 24000)
    struct.pack_into('>I', track, 36, 1)
    sounds = afs({63602+i: bytes(track) for i in range(24)})
    files = {'SLPS_258.15': b'fake serial', 'DATA/PZS3JP1.AFS': menus, 'DATA/PZS3JP2.AFS': sounds}
    for name in ('ZS3JPED.PSS','ZS3JPED.ADX','ZS3JPOP.PSS','ZS3JPOP.ADX'):
        prefix = b'\0\0\1\xba' if name.endswith('PSS') else b'\x80\0'
        files['DATA/'+name] = prefix + bytes(64-len(prefix))
    data = bytearray(22*2048)
    extents = {}
    for name, blob in files.items():
        extents[name] = (len(data)//2048, len(blob))
        data.extend(blob + bytes(-len(blob) % 2048))
    root = iso_record(b'\0',20,2048,True) + iso_record(b'\1',20,2048,True)
    root += iso_record(b'DATA',21,2048,True) + iso_record(b'SLPS_258.15;1',*extents['SLPS_258.15'])
    directory = iso_record(b'\0',21,2048,True)+iso_record(b'\1',20,2048,True)
    for name, extent in extents.items():
        if name.startswith('DATA/'):
            directory += iso_record((name[5:]+';1').encode(),*extent)
    data[20*2048:20*2048+len(root)] = root
    data[21*2048:21*2048+len(directory)] = directory
    pvd = bytearray(2048); pvd[:7] = b'\1CD001\1'
    record = iso_record(b'\0',20,2048,True)
    pvd[156:156+len(record)] = record
    data[16*2048:17*2048] = pvd
    path.write_bytes(data)


def textures(count, seed=0):
    data = bytearray(8+96*count)
    struct.pack_into('>II',data,0,count,2)
    for index in range(count):
        fmt = (0,8,9)[index%3]
        colors = 256 if fmt==9 else 16 if fmt==8 else 0
        pixel = len(data); pixels = bytes(((i+index+seed)%256 for i in range(64 if fmt==9 else 32)))
        data.extend(pixels)
        palette = len(data)
        data.extend(bytes((i+seed)%256 for i in range(colors*4)))
        at = 8+index*96
        struct.pack_into('>4I',data,at,pixel//4,palette//4,len(pixels),colors*4)
        data[at+40:at+44] = bytes((3,3,0,fmt))
    return bytes(data)


def matrix(x=0, y=0):
    bits = f'{16:05b}{int(x*20)&65535:016b}{int(y*20)&65535:016b}'
    bits += '0'*(-len(bits)%8)
    return b'\x24'+int(bits,2).to_bytes(len(bits)//8,'big')


def frame(name='', depth=None, x=0, y=0, alpha=None, scale=None, initial=False):
    actions=b''
    if depth is not None:
        flags=4 | (8 if alpha is not None else 0)
        actions=(struct.pack('>HHB',4,depth,6 if alpha is not None else 0)+bytes(2)
                 if initial else struct.pack('>HBH',26,flags,depth))
        if scale is None:
            actions+=matrix(x,y)
        else:
            bits=f'{18:05b}{int(scale*65536):018b}{int(scale*65536):018b}'
            bits+='0'*(-len(bits)%8)
            actions+=b'\x11'+int(bits,2).to_bytes(len(bits)//8,'big')
        if alpha is not None:
            bits=f'{10:04b}{int(alpha*256):010b}'
            bits+='0'*(-len(bits)%8)
            actions+=b'\x08'+int(bits,2).to_bytes(len(bits)//8,'big')
    actions += b'\0\1'
    return name.encode()+b'\0'+struct.pack('>H',len(actions))+actions


def movie():
    def record(frames): return bytes(4)+struct.pack('>I',len(frames))+frames
    def tag(code, records):
        data = b''.join(record(r) for r in records)
        return struct.pack('>BBHI',code,0,len(records),len(data))+data
    sprites = [b'']*12
    sprites[1] = frame(depth=1,alpha=0.5,initial=True)+frame(depth=2,alpha=0.25)+frame()*58
    sprites[3] = frame(depth=1,x=-12,y=32)+frame()*29
    sprites[6] = frame('fl_loop_start',1,y=256)+frame()*499
    sprites[11] = frame('fl_on_loop',depth=1,scale=1.25)+frame()*49
    root = frame(depth=20,x=531)+frame('fl_btn_in_1',10,x=-272)+frame()*16
    return b'FOD\x11BIG\0'+tag(6,sprites)+tag(7,[root])+bytes(8)


def wii_archives(directory):
    voice = bytearray(64); voice[:8] = bytes.fromhex('8000002403120401')
    struct.pack_into('>II',voice,8,24000,24000)
    sounds = {base+i: bytes(voice) for base in (30787,32159) for i in range(44)}
    (directory/'wzs3eu2.afs').write_bytes(afs(sounds,count=32203))
    lips = b'LIPS'+struct.pack('<IIII',2,3,0,0)+struct.pack('<6H',0,0,1,1,60,0)
    entries = {}
    for locale, name in enumerate(('en','es','de','fr','it')):
        dialog = pack([b'\xff\xfe'+(f'{name} dialogue {i} café\0').encode('utf-16le') for i in range(44)])
        menu = pack([b'\xff\xfe'+(f'{name} menu {i}\nline\0').encode('utf-16le') for i in range(66)])
        resources = [b'1234']*14
        resources[0] = movie()
        resources[3] = resources[4] = resources[5] = textures(5,locale)
        resources[6] = pack([lips]*88)
        resources[7] = dialog; resources[11] = menu
        resources[10] = pack([textures(2,locale)])
        resources[13] = pack([b'1234',b'1234',textures(2,locale)])
        battle = pack([b'1234']*3+[textures(43,locale),textures(4,locale)])
        entries[locale] = pack([bpe(pack(resources)), bpe(battle)])
    target = directory/'wzs3eu1.afs'
    target.write_bytes(afs(entries,names=[f'DragonNe-{i}' for i in range(5)]))
    return target


def gamecube_container(path, files):
    # A tiny unencrypted fixture exercises the disc-reader filesystem interface; its
    # contents are synthetic Wii archives, not Nintendo/game disc data.
    names = b"\0"; records = []
    cursor = 0x4000
    payloads = []
    for name, data in files.items():
        records.append(struct.pack('>III', len(names), cursor, len(data)))
        names += name.encode()+b"\0"
        payloads.append((cursor,data)); cursor += (len(data)+2047)//2048*2048
    fst = struct.pack('>III',0x01000000,0,len(records)+1)+b''.join(records)+names
    result = bytearray(cursor)
    result[:6] = b'RDSPAF'
    struct.pack_into('>I',result,0x1c,0xc2339f3d)
    struct.pack_into('>I',result,0x420,0x2800)
    struct.pack_into('>I',result,0x2800,0x100)  # DOL section offset
    struct.pack_into('>I',result,0x2890,4)      # DOL section size
    struct.pack_into('>III',result,0x424,0x1000,len(fst),len(fst))
    result[0x1000:0x1000+len(fst)] = fst
    for offset,data in payloads: result[offset:offset+len(data)] = data
    path.write_bytes(result)


def named_package(path):
    data=path.read_bytes(); assert data[:8]==b'BT3TEXT1'
    count,=struct.unpack_from('<I',data,8); at=12; result={}
    for _ in range(count):
        n,offset,size=struct.unpack_from('<IQQ',data,at); at+=20
        name=data[at:at+n].decode(); at+=n
        result[name]=data[offset:offset+size]
    return result


def png_pixels(data):
    at=8; compressed=b''
    while at<len(data):
        size,=struct.unpack_from('>I',data,at); kind=data[at+4:at+8]; payload=data[at+8:at+8+size]
        assert zlib.crc32(kind+payload)==struct.unpack_from('>I',data,at+8+size)[0]
        if kind==b'IDAT': compressed+=payload
        at+=12+size
    return zlib.decompress(compressed)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver',type=Path,default=ROOT/'port/build/native-extract-test')
    parser.add_argument('--runner', help='Wine executable for testing the Windows driver')
    args=parser.parse_args(); driver=args.driver.resolve()
    def native_run(kind, root, disc, **options):
        def path(p): return 'Z:' + str(p.resolve()).replace('/', '\\') if args.runner else str(p)
        command=([args.runner] if args.runner else [])+[str(driver),kind,path(root),path(disc)]
        options.setdefault("timeout",120)
        return subprocess.run(command,**options)
    def reject(kind, root, disc):
        result=native_run(kind,root,disc,capture_output=True)
        assert result.returncode==1, result.stderr
        assert b'Sanitizer' not in result.stderr and b'runtime error:' not in result.stderr, result.stderr
    with tempfile.TemporaryDirectory(prefix='zenkaipower-native-') as temporary:
        root=Path(temporary); native=root/'native with spaces'; reference=root/'reference'
        native.mkdir(); reference.mkdir()
        image=root/'Japanese disc.iso'; japanese_iso(image)
        python_jp(image,reference)
        native_run('jp',native,image,check=True)
        assert named_package(native/'gamedata/sparking-meteor.pak')==named_package(reference/'sparking-meteor.pak')
        custom_install=root/'custom install';custom_install.mkdir()
        custom_data=root/'custom extracted data';custom_data.mkdir()
        configured=('Z:'+str(custom_data.resolve()).replace('/','\\')) if args.runner else str(custom_data)
        (custom_install/'locations.ini').write_text('data='+configured+'\nsaves=\n')
        native_run('jp',custom_install,image,check=True)
        assert named_package(custom_data/'sparking-meteor.pak')==named_package(reference/'sparking-meteor.pak')
        assert not (custom_install/'gamedata').exists()
        previous=(native/'gamedata/sparking-meteor.pak').read_bytes()
        # A late read failure must not replace a previously installed package.
        bad=root/'truncated.iso'; bad.write_bytes(image.read_bytes()[:-2048])
        reject('jp',native,bad)
        assert (native/'gamedata/sparking-meteor.pak').read_bytes()==previous
        print('PASS: Japanese assets match reference; truncated image preserves installation')
        archives=root/'archives'; archives.mkdir(); menu=wii_archives(archives)
        catalogs=native/'gamedata/game-text/translations/ui'; catalogs.mkdir(parents=True)
        old={'locale':'es','labels':{'custom/id':{'original':'original','text':'edited'},
             'wii-dragon-net/dialog/0':{'original':'old','text':'user edit'},
             'wii-dragon-net/dialog/1':{'original':'old','text':'old'}},'labels_count':3}
        (catalogs/'es.json').write_text(json.dumps(old))
        python_wii(menu,reference/'dragon-net-battle',None,None,archives/'wzs3eu2.afs',True)
        native_run('wii',native,menu,check=True)
        native_run('wii',custom_install,menu,check=True)
        assert named_package(custom_data/'dragon-net-battle.pak')==named_package(native/'gamedata/dragon-net-battle.pak')
        output=named_package(native/'gamedata/dragon-net-battle.pak')
        for p in (reference/'dragon-net-battle').rglob('*'):
            if not p.is_file(): continue
            name=p.relative_to(reference/'dragon-net-battle').as_posix()
            if name in ('index.html','manifest.json'): continue
            assert name in output,name
            if name.endswith('.png'): assert png_pixels(output[name])==png_pixels(p.read_bytes()),name
            elif name.startswith('ui-fragment-'):
                assert json.loads('{'+output[name].decode()[1:]+'}')==json.loads('{'+p.read_text(encoding='utf-8')[1:]+'}'),name
            else: assert output[name]==p.read_bytes(),name
        catalog=json.loads((catalogs/'es.json').read_text(encoding='utf-8'))
        assert catalog['labels']['custom/id']['text']=='edited'
        assert catalog['labels']['wii-dragon-net/dialog/0']['text']=='user edit'
        assert catalog['labels']['wii-dragon-net/dialog/1']['text'].startswith('es dialogue 1')
        assert catalog['labels_count']==len(catalog['labels'])==111
        from dolphin_tool import find_dolphin_tool
        tool=Path(find_dolphin_tool(ROOT))
        assert not (native/'tools/dolphin').exists()
        image=root/'Wii archives.iso'
        gamecube_container(image,{'wzs3eu1.afs': menu.read_bytes(), 'wzs3eu2.afs': (archives/'wzs3eu2.afs').read_bytes()})
        for disc in (image,root/'Wii archives.rvz'):
            if disc.suffix=='.rvz':
                subprocess.run([str(tool),'convert','-i',str(image),'-o',str(disc),'-f','rvz','-c','zstd','-b','131072','-l','1','-u',str(root/'dolphin-user')],check=True,capture_output=True)
            native_run('wii',native,disc,check=True,env={**os.environ,'PATH':''})
            assert named_package(native/'gamedata/dragon-net-battle.pak')==output
        print('PASS: linked nod ISO/RVZ extraction works with an empty PATH and no Dolphin tool')
        previous=(native/'gamedata/dragon-net-battle.pak').read_bytes()
        previous_catalog=(catalogs/'es.json').read_bytes()
        menu_bytes=menu.read_bytes(); broken=bytearray(menu_bytes)
        locale_offset=struct.unpack_from('<I',broken,8+4*8)[0]
        bpe_offset=struct.unpack_from('>I',broken,locale_offset+4)[0]
        struct.pack_into('>I',broken,locale_offset+bpe_offset,0x4000001)
        menu.write_bytes(broken)
        reject('wii',native,menu)
        assert (native/'gamedata/dragon-net-battle.pak').read_bytes()==previous
        assert (catalogs/'es.json').read_bytes()==previous_catalog
        menu.write_bytes(menu_bytes)
        # Corrupt a late voice stream; a partially decoded result must never publish.
        voice=archives/'wzs3eu2.afs'; data=bytearray(voice.read_bytes()); offset=struct.unpack_from('<I',data,8+32202*8)[0]; data[offset]=0
        voice.write_bytes(data)
        reject('wii',native,menu)
        assert (native/'gamedata/dragon-net-battle.pak').read_bytes()==previous
        assert not list((native/'gamedata').glob('.zenkaipower-extract-*'))
        print('PASS: Wii runtime outputs match reference, PNG pixels match; edited catalogs and old package preserved')


if __name__=='__main__': main()
