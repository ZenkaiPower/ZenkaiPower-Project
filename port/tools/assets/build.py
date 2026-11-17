"""Validate manifests and embed binary content for native builds."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def generate(root=ROOT, output=None):
    output = output or root / "port/build/generated/content_assets.inc"
    resources, costumes, targets, blobs = [], {}, set(), {}
    lines = ["/* Generated from assets/characters manifests. Do not edit. */\n"]
    for manifest in sorted((root / "assets/characters").glob("*/manifest.json")):
        config = json.loads(manifest.read_text())
        if config["schema"] != 1:
            raise ValueError(f"Unsupported content manifest: {manifest}")
        for character, count in config.get("costumes", {}).items():
            character = int(character)
            if character in costumes or not 0 <= character < 161 or not 1 <= count <= 4:
                raise ValueError("Invalid or duplicate costume registration")
            costumes[character] = count
        for resource in config["resources"]:
            target, base = resource["target"], resource["base"]
            if target in targets or target == base:
                raise ValueError("Duplicate or recursive content resource")
            for relative in (target, base):
                if len(relative) >= 128 or Path(relative).is_absolute() or ":" in relative or ".." in Path(relative).parts or "\\" in relative:
                    raise ValueError("Content resource must be a relative game path")
            asset = (manifest.parent / resource["asset"]).resolve()
            if not asset.is_relative_to(manifest.parent.resolve()):
                raise ValueError("Asset escapes its content directory")
            data = asset.read_bytes()
            first, last = resource["replace_entries"]
            if len(data) < 12:
                raise ValueError("Truncated content pack")
            count, = struct.unpack_from("<I", data)
            if not 1 <= first <= last <= count <= 4096 or (count+2)*4 > len(data):
                raise ValueError("Invalid replacement range")
            offsets = struct.unpack_from(f"<{count+1}I", data, 4)
            if any((a & ~3) > (b & ~3) or (b & ~3) > len(data) for a,b in zip(offsets,offsets[1:])):
                raise ValueError("Invalid content pack offsets")
            digest = hashlib.sha256(data).digest()
            symbol = blobs.get(digest)
            if symbol is None:
                symbol = f"content_blob_{len(blobs)}"
                blobs[digest] = symbol
                lines.append(f"static const unsigned char {symbol}[] = {{\n")
                lines.extend("    " + ",".join(f"0x{b:02x}" for b in data[o:o+24]) + ",\n" for o in range(0,len(data),24))
                lines.append("};\n")
            resources.append((target,base,symbol,first,last))
            targets.add(target)
    if any(base in targets for _,base,_,_,_ in resources):
        raise ValueError("Content resources cannot use another generated resource as their base")
    lines.append("static const ContentAsset content_assets[] = {\n")
    lines.extend(f"    {{{json.dumps(t)},{json.dumps(b)},{s},sizeof({s}),{f},{l}}},\n" for t,b,s,f,l in resources)
    lines.append("};\nstatic const ContentCostume content_costumes[] = {\n")
    lines.extend(f"    {{{c},{n}}},\n" for c,n in sorted(costumes.items()))
    lines.append("};\n")
    text = "".join(lines)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text() != text:
        with tempfile.NamedTemporaryFile(mode="w", dir=output.parent, prefix=output.name, suffix=".tmp", delete=False) as stream:
            stream.write(text)
            temporary = Path(stream.name)
        try:
            temporary.replace(output)
        finally:
            temporary.unlink(missing_ok=True)
    return output


if __name__ == "__main__":
    print(generate())
