"""Read only this mod's resolved game API import addresses for target-version research.

Requires pefile/capstone and a running, user-started test client. No remote writes,
injection, world data or arbitrary player memory are accessed.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import struct
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--mod-base', type=int, required=True)
    parser.add_argument('--game-base', type=int, required=True)
    parser.add_argument('--client', type=Path, required=True)
    args = parser.parse_args()
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    kernel.ReadProcessMemory.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x10, False, args.pid)  # PROCESS_VM_READ only
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    root = Path(__file__).resolve().parents[1]
    output = root / 'build' / 'inspection'
    output.mkdir(parents=True, exist_ok=True)
    mod = pefile.PE(str(args.client / 'mods/MelonRind/MelonRind.dll'), fast_load=True)
    mod.parse_data_directories(directories=[13])
    game = pefile.PE(str(args.client / 'Minecraft.Windows.exe'), fast_load=True)
    targets = {}
    disasm = Cs(CS_ARCH_X86, CS_MODE_64)
    try:
        for entry in mod.DIRECTORY_ENTRY_DELAY_IMPORT:
            if entry.dll != b'bedrock_runtime.dll':
                continue
            for imp in entry.imports:
                name = imp.name.decode() if imp.name else str(imp.ordinal)
                buf = ctypes.c_uint64()
                count = ctypes.c_size_t()
                address = args.mod_base + imp.address - mod.OPTIONAL_HEADER.ImageBase
                if not kernel.ReadProcessMemory(handle, address, ctypes.byref(buf), 8, ctypes.byref(count)) or count.value != 8:
                    raise ctypes.WinError(ctypes.get_last_error())
                target = buf.value
                for _ in range(8):
                    if args.game_base <= target < args.game_base + game.OPTIONAL_HEADER.SizeOfImage:
                        break
                    stub = ctypes.create_string_buffer(16)
                    if not kernel.ReadProcessMemory(handle, target, stub, 16, ctypes.byref(count)):
                        break
                    if stub.raw[:6] == b'\xff\x25\x00\x00\x00\x00':
                        target = struct.unpack_from('<Q', stub.raw, 6)[0]
                    elif stub.raw[0] == 0xe9:
                        target += 5 + struct.unpack_from('<i', stub.raw, 1)[0]
                    else:
                        break
                rva = target - args.game_base
                if 'Hud' in name or 'getAttribute' in name or 'SATURATION@' in name:
                    print(name, 'target', hex(target), 'gameRva', hex(rva), 'modRva', hex(target - args.mod_base))
                if 0 <= rva < game.OPTIONAL_HEADER.SizeOfImage:
                    targets[name] = rva
        (output / 'resolved-api.json').write_text(json.dumps(targets, indent=2), encoding='utf-8')
        for name, rva in targets.items():
            if any(token in name for token in ('HudHungerRenderer', 'HudHeartRenderer', '?SATURATION@', '?getAttribute@')):
                label = name.replace('?', '_').replace('$', '_').replace('@', '_')[:100]
                lines = [f'{i.address:#x}: {i.mnemonic} {i.op_str}' for i in disasm.disasm(game.get_data(rva, 3072), game.OPTIONAL_HEADER.ImageBase + rva)]
                (output / (label + '.asm')).write_text('\n'.join(lines), encoding='utf-8')
        print(f'Resolved {len(targets)} game APIs; inspection files: {output}')
    finally:
        kernel.CloseHandle(handle)

if __name__ == '__main__':
    main()
