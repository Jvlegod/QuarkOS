#!/usr/bin/env python3
"""Convert a PNG image to a C array (.h file) for desktop icons."""

import sys
from pathlib import Path
from PIL import Image


def make_var_name(filename: str) -> str:
    """Generate a valid C variable name from the file name."""
    stem = Path(filename).stem
    stem = stem.replace("-", "_").replace(" ", "_")
    if not stem[0].isalpha():
        stem = "icon_" + stem
    return f"{stem}_icon_data"


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 4:
        print("Usage: icon2c.py input.png [width height]")
        return 1

    inp = Path(sys.argv[1])
    width = int(sys.argv[2]) if len(sys.argv) >= 3 else 48
    height = int(sys.argv[3]) if len(sys.argv) >= 4 else width

    out_dir = Path("src/user/icons")
    out_dir.mkdir(parents=True, exist_ok=True)
    outp = out_dir / (inp.stem + ".h")

    img = Image.open(inp).convert("RGBA").resize((width, height))
    data = list(img.getdata())

    var_name = make_var_name(inp.name)

    with open(outp, "w") as f:
        f.write("#include \"ktypes.h\"\n\n")
        f.write(f"// Generated from {inp.name}, size {width}x{height}\n")
        f.write(f"static const uint32_t {var_name}[{width * height}] = {{\n")
        for i, (r, g, b, a) in enumerate(data):
            val = (b | (g << 8) | (r << 16) | (a << 24))  # ARGB
            f.write(f"0x{val:08X},")
            if (i + 1) % width == 0:
                f.write("\n")
        f.write("};\n")

    print(f"Icon converted: {outp} (variable: {var_name})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
