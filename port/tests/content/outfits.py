#!/usr/bin/env python3
"""Compile and test built-in outfit loading without installed game data."""
from pathlib import Path
import os
import hashlib
import json
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[3]
import model_pack as outfit
import sys
sys.path.insert(0, str(ROOT / "port/tools"))
from assets.build import generate
generate()
asset_root = ROOT / "assets/characters/future-gohan"
manifest = json.loads((asset_root / "manifest.json").read_text())
visuals = [outfit.unpack((asset_root / r["asset"]).read_bytes()) for r in manifest["resources"]]
assert len(visuals) == 8
for original, alternate in zip(visuals[:4], visuals[4:]):
    assert original[2] == alternate[2]  # identical one-armed geometry in both colors
    assert [i for i, (a,b) in enumerate(zip(original,alternate)) if a != b] == [11]
    palette_rgb = {p+j*4+k for _,p,n in outfit.texture_palettes(original[11][1])
                   for j in range(n) for k in range(3)}
    assert all(i in palette_rgb for i,(a,b) in enumerate(zip(original[11][1],alternate[11][1])) if a != b)
    assert len(original[11][1]) == len(alternate[11][1])
for form, (normal, damaged) in enumerate(((visuals[0], visuals[1]), (visuals[2], visuals[3]))):
    assert not normal[0][1] and not damaged[0][1]  # never replace a HUD portrait
    assert normal[2][1] != damaged[2][1]
    assert normal[11][1] != damaged[11][1]  # clean and torn material sets
    _, _, parts = outfit.model_parts(normal[2][1])
    nodes = {int.from_bytes(p[10:12],"little"): p for p in parts}
    # The clean model hides the missing arm while retaining the complete sleeve,
    # normals and UVs.
    expected_models = ('b51f887f60b1a5004ecb0e7d47507efdbed815c395bae8f58dbd094a3db3b9ea', '79f1d9e7c52e2b6fe2acbfac5f1f71dec7c32b228e1d874181ca73ec749a2565')
    assert hashlib.sha256(normal[2][1]).hexdigest() == expected_models[form]
    for node in range(34,45):
        assert all(struct.unpack_from("<4f", nodes[node], at) == (0.,0.,0.,0.)
                   for at in outfit.vertex_offsets(nodes[node]))
    sleeve = nodes[33]
    assert hashlib.sha256(b"".join(sleeve[at:at+48] for at in outfit.vertex_offsets(sleeve,palette=4))).hexdigest() == "ba54ad037c2bd89b578a33b297f406e3d2149a00f31206eeb8206633cddae518"
    blue_sleeve = list(outfit.vertex_offsets(sleeve,palette=6))
    assert len(blue_sleeve) == 219
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    data = work / "data/pzs3us1"
    data.mkdir(parents=True)
    for resource, visual in zip(manifest["resources"], visuals):
        index = int(Path(resource["target"]).stem)
        original = [(i % 4, bytes([i]) * 32) for i in range(1, 53)]
        (data / Path(resource["base"]).name).write_bytes(outfit.repack(original))
        for entry in range(3, 18):
            original[entry-1] = visual[entry-1]
        (work / f"{index:05d}.bin").write_bytes(outfit.repack(original))
        (data / f"{index:05d}.bin").write_bytes(bytes([0x7b])*64)
    # Manifest errors fail at build time instead of becoming broken runtime packs.
    fixture = work / "fixture/assets/characters/sample"
    fixture.mkdir(parents=True)
    (fixture / "asset.bin").write_bytes(outfit.repack([(0,b"test"*4)]))
    config = {"schema":1, "costumes":{"159":3}, "resources":[{
        "target":"pzs3us1/03015.bin", "base":"pzs3us1/03013.bin",
        "asset":"asset.bin", "replace_entries":[1,1]}]}
    manifest_path = fixture / "manifest.json"
    manifest_path.write_text(json.dumps(config))
    generated = generate(work / "fixture")
    previous = generated.stat().st_mtime_ns
    generate(work / "fixture")
    assert generated.stat().st_mtime_ns == previous
    config["resources"][0]["base"] = config["resources"][0]["target"]
    manifest_path.write_text(json.dumps(config))
    try:
        generate(work / "fixture")
        raise AssertionError("recursive resource accepted")
    except ValueError:
        pass
    for enabled in (0,1):
        binary = work / f"check-{enabled}"
        subprocess.run([os.environ.get("CC", "gcc"), "-O1", "-g", "-fsanitize=address,undefined",
                        f"-DPORT_ZENKAI_EXTRA_OUTFITS={enabled}",
                        "-I" + str(ROOT / "port/src"), str(Path(__file__).with_suffix(".c")), "-o", str(binary)], check=True)
        subprocess.run([str(binary), str(work / "data"), str(work)], check=True)
