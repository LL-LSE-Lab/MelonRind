"""Generate 9px HUD sprites from code masks, without build-time asset dependencies.

The saturation mask follows the black outline of the target client's vanilla
textures/ui/hunger_background.png (Minecraft 1.26.10.4): meat upper-left,
bone lower-right. Its exact alignment matters because this overlays that icon.
"""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1] / 'resource_packs/melonrind/textures/melonrind'

def png(name, rows, colors):
    assert len(rows) == 9 and all(len(row) == 9 for row in rows)
    raw = b''.join(b'\0' + bytes(v for c in row for v in colors[c]) for row in rows)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 9, 9, 8, 6, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    ROOT.mkdir(parents=True, exist_ok=True)
    (ROOT / (name + '.png')).write_bytes(data)

png('saturation', [
    '..GG.....', '.G..G....', 'G....G...', 'G.....G..', '.G....G..',
    '..G...G..', '...GGG.GG', '......G.G', '......GG.',
], {'.': (0, 0, 0, 0), 'G': (255, 207, 52, 255)})
png('regen', [
    '.GG...GG.', 'GLLG.GLLG', 'GLLLGLLLG', 'GLLLLLLLG', '.GLLLLLG.',
    '..GLLLG..', '...GLG...', '....G....', '.........',
], {'.': (0, 0, 0, 0), 'G': (86, 220, 153, 255), 'L': (164, 255, 207, 220)})
