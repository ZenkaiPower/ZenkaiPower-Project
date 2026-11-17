"""Inspect built-in model packs and construct synthetic test fixtures."""
import struct


def unpack(data):
    count = struct.unpack_from("<I", data)[0]
    if not 1 <= count <= 4096 or len(data) < (count + 2) * 4:
        raise ValueError("Invalid model pack header")
    offsets = struct.unpack_from(f"<{count + 1}I", data, 4)
    if any((a & ~3) > (b & ~3) or (b & ~3) > len(data) for a, b in zip(offsets, offsets[1:])):
        raise ValueError("Invalid model pack offsets")
    return [(a & 3, data[a & ~3:b & ~3]) for a, b in zip(offsets, offsets[1:])]


def repack(entries):
    result = bytearray((len(entries) + 2) * 4)
    struct.pack_into("<I", result, 0, len(entries))
    for number, (flags, payload) in enumerate(entries, 1):
        result.extend(bytes((-len(result)) % 16))
        struct.pack_into("<I", result, number * 4, len(result) | flags)
        result.extend(payload)
    struct.pack_into("<I", result, (len(entries) + 1) * 4, len(result))
    result.extend(bytes((-len(result)) % 2048))
    return bytes(result)


def model_parts(model):
    offset = struct.unpack_from("<I", model, 0x6C)[0]
    first = offset
    parts = []
    while True:
        size = struct.unpack_from("<I", model, offset)[0]
        if size < 64 or offset + size > len(model):
            raise ValueError("Invalid model part")
        parts.append(model[offset:offset+size])
        if struct.unpack_from("<H", model, offset+6)[0]:
            return first, offset+size, parts
        offset += size


def vertex_offsets(part, palette=None):
    # Fighter meshes send V4-32 positions, normals and UVs in groups of three
    # vectors to VU TOPS+5. Skip payloads rather than interpreting float bits.
    offset = 96
    current_palette = None
    while offset+4 <= len(part):
        command = struct.unpack_from("<I", part, offset)[0]
        if command == 0x6C058000:
            current_palette = (struct.unpack_from("<I", part, offset+24)[0] >> 5) & 0x3FFF
            offset += 84
            continue
        if command & 0xFF00FFFF == 0x6C008005:
            count = (command >> 16) & 255
            if not count or count % 3 or offset+4+count*16 > len(part):
                raise ValueError("Unexpected fighter vertex stream")
            if palette is None or current_palette == palette:
                yield from range(offset+4, offset+4+count*16, 48)
            offset += 4+count*16
        else:
            offset += 4


def texture_palettes(data):
    """Return real indexed-texture palettes, excluding GS control entries."""
    count, table = struct.unpack_from("<II", data)
    for index in range(count):
        entry = table * 4 + index * 64
        tex0 = struct.unpack_from("<Q", data, entry + 48)[0]
        psm = (tex0 >> 20) & 63
        if psm not in (19, 20) or struct.unpack_from("<I", data, entry + 24)[0] >> 8:
            continue
        start = struct.unpack_from("<I", data, entry + 4)[0] * 4 + 96
        yield index, start, 256 if psm == 19 else 16
