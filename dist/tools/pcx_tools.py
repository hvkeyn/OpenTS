# -*- coding: utf-8 -*-
"""PCX helpers: read a picture out, or write one back in a form the game reads.

    python pcx_tools.py dump  <game.pcx>   <out.png>
    python pcx_tools.py write <out.pcx>    <in.png>          (24 bit colour)
    python pcx_tools.py write8 <out.pcx>   <in.png>          (8 bit, 256 colours)

The game reads both, but the 8 bit form is the one its own artwork uses, so it is the
safer one when a picture has to be put back exactly as the game expects it.
"""
import struct
import sys


def _rle(data):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        value = data[i]
        run = 1
        while i + run < n and data[i + run] == value and run < 63:
            run += 1
        if run > 1 or value >= 0xC0:
            out.append(0xC0 | run)
        out.append(value)
        i += run
    return bytes(out)


def write_pcx(path, im):
    """Writes a 24 bit PCX: three planes of eight bits a row."""
    im = im.convert("RGB")
    width, height = im.size

    header = bytearray(128)
    header[0] = 0x0A
    header[1] = 0x05
    header[2] = 0x01
    header[3] = 8
    struct.pack_into("<HHHH", header, 4, 0, 0, width - 1, height - 1)
    header[65] = 3
    struct.pack_into("<H", header, 66, width)
    struct.pack_into("<H", header, 68, 1)

    with open(path, "wb") as out:
        out.write(bytes(header))
        for y in range(height):
            row = [im.getpixel((x, y)) for x in range(width)]
            out.write(_rle(bytes(p[0] for p in row)))
            out.write(_rle(bytes(p[1] for p in row)))
            out.write(_rle(bytes(p[2] for p in row)))
        out.write(bytes((0x0C,)))


def write_pcx8(path, im):
    """Writes an 8 bit paletted PCX, the form the game's own artwork is stored in."""
    paletted = im.convert("RGB").quantize(colors=256, method=2)
    width, height = paletted.size
    palette = paletted.getpalette()[:768]
    while len(palette) < 768:
        palette.append(0)

    header = bytearray(128)
    header[0] = 0x0A
    header[1] = 0x05
    header[2] = 0x01
    header[3] = 8
    struct.pack_into("<HHHH", header, 4, 0, 0, width - 1, height - 1)
    header[65] = 1
    struct.pack_into("<H", header, 66, width)
    struct.pack_into("<H", header, 68, 1)

    data = paletted.tobytes()

    with open(path, "wb") as out:
        out.write(bytes(header))
        for y in range(height):
            out.write(_rle(data[y * width:(y + 1) * width]))
        out.write(bytes((0x0C,)))
        out.write(bytes(palette))


def main(argv):
    from PIL import Image

    if len(argv) == 3 and argv[0] == "dump":
        im = Image.open(argv[1])
        im.load()
        im.convert("RGB").save(argv[2])
        print("wrote", argv[2], im.size)
        return 0

    if len(argv) == 3 and argv[0] in ("write", "write8"):
        im = Image.open(argv[2])
        im.load()
        if argv[0] == "write":
            write_pcx(argv[1], im)
        else:
            write_pcx8(argv[1], im)
        check = Image.open(argv[1])
        check.load()
        print("wrote", argv[1], check.size, check.mode)
        return 0

    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
