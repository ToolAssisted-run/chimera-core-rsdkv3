#!/usr/bin/env python3
"""A Retro Engine v3 game made for the core's tests, written out from
scratch: no part of any Sega game is in it.

    make-test-game.py <out Data.rsdk>

What is in the data file (a pack in the engine's own format, encrypted as
the engine reads it, Reader.cpp):

  Data/Game/GameConfig.bin  the game: three global variables (test.presses,
                            test.random, test.saved), one global sound effect, one
                            stage in the presentation list
  Data/Palettes/MasterPalette.act
  Data/SoundFX/Test/Beep.wav  a short square wave
  Data/Music/Tune.ogg       tests/tune.ogg, two seconds of two tones
  Data/Stages/Test/StageConfig.bin, Act1.bin  the stage: one object
  Data/Scripts/Test/Box.txt the object's script, in the engine's script
                            language, which the engine compiles at load

The object, Box, is the test: it starts the stage's timer and the music, moves with Left and
Right, and on each press of A plays the beep, counts the press in
test.presses and draws a random number into test.random (the engine's
Rand, the random seed's). Start saves the count in the game's save
(SaveRAM[0], WriteSaveRAM); C reads the save back (ReadSaveRAM) into
test.saved. It draws itself as a rectangle whose colour
changes with the presses, and a second rectangle as wide as the random
number, so each shows in the picture.
"""
import math
import struct
import sys
import os

HERE = os.path.dirname(os.path.abspath(__file__))

ENC_A = b"4RaS9D7KaEbxcp2o5r6t"
ENC_B = b"3tRaUxLmEaSn"


def encrypt(data):
    """The inverse of Reader.cpp FileRead for one file of the pack."""
    size = len(data)
    no = (size & 0x1FC) >> 2
    pos_b = (no % 9) + 1
    pos_a = (no % pos_b) + 1
    nyb = 0
    out = bytearray()
    for p in data:
        t = p ^ ENC_A[pos_a]
        if nyb:
            t = ((t & 0xF) << 4) | (t >> 4)
        out.append(t ^ ENC_B[pos_b] ^ no)
        pos_a += 1
        pos_b += 1
        if pos_a <= 19 or pos_b <= 11:
            if pos_a > 19:
                pos_a = 1
                nyb ^= 1
            if pos_b > 11:
                pos_b = 1
                nyb ^= 1
        else:
            no = (no + 1) & 0x7F
            if nyb:
                nyb = 0
                pos_a = (no % 12) + 6
                pos_b = (no % 5) + 4
            else:
                nyb = 1
                pos_a = (no % 15) + 3
                pos_b = (no % 7) + 1
    return bytes(out)


def pack(files):
    """files: {"Data/Dir/Name": bytes} -> the Data.rsdk bytes"""
    dirs = {}
    for path in sorted(files):
        d, name = path.rsplit("/", 1)
        dirs.setdefault(d + "/", []).append((name, files[path]))
    header_size = 4 + 2 + sum(1 + len(d) + 4 for d in dirs)
    body = bytearray()
    entries = bytearray()
    for d, members in dirs.items():
        n = len(d)
        entries += bytes([n]) + bytes((c ^ (0xFF - n)) & 0xFF for c in d.encode()) + struct.pack("<I", len(body))
        for name, data in members:
            nb = name.encode()
            body += bytes([len(nb)]) + bytes((~c) & 0xFF for c in nb) + struct.pack("<I", len(data)) + encrypt(data)
    return struct.pack("<IH", header_size, len(dirs)) + bytes(entries) + bytes(body)


def s8(text):
    b = text.encode()
    return bytes([len(b)]) + b


def game_config():
    out = s8("Chimera Test") + s8("Data") + s8("A game made for the core's tests")
    out += bytes([0])                                   # no global objects
    variables = [("test.presses", 0), ("test.random", 0), ("test.saved", -1)]
    out += bytes([len(variables)])
    for name, value in variables:
        out += s8(name) + struct.pack(">i", value)
    out += bytes([1]) + s8("Test/Beep.wav")             # global sound effects
    out += bytes([0])                                   # players
    # the four stage lists in the file's order: presentation, regular,
    # special, bonus; each stage: folder, act id, name, highlighted
    out += bytes([1]) + s8("Test") + s8("1") + s8("TEST STAGE") + bytes([0])
    out += bytes([0]) + bytes([0]) + bytes([0])
    return out


def palette():
    out = bytearray()
    for i in range(256):
        out += bytes([(i * 37) & 0xFF, (i * 91) & 0xFF, (i * 53) & 0xFF])
    out[0:3] = b"\x10\x20\x40"
    return bytes(out)


def stage_config():
    out = bytes([0])                                    # no global scripts
    for i in range(32):                                 # palette 96..127
        out += bytes([i * 8, 255 - i * 8, 128])
    out += bytes([1]) + s8("Box")                       # one object type
    out += s8("Test/Box.txt")                           # its script
    out += bytes([0])                                   # no stage sound effects
    return out


def act():
    out = s8("TEST")
    out += bytes([9, 9, 9, 9]) + bytes([3])             # no tile layers drawn
    out += bytes([1, 1]) + b"\x00\x00"                  # a 1x1 layout, empty
    out += bytes([0])                                   # no type names
    out += struct.pack(">H", 1)                         # one object
    out += bytes([1, 0]) + struct.pack(">HH", 100, 120) # Box at (100, 120)
    return out


BOX = """// Box: the core's test object
#alias Object.Value0 : Box.Frames
#alias Object.Value1 : Box.Colour

sub ObjectMain
	Box.Frames++
	if KeyDown.Left == 1
		Object.XPos -= 0x20000
	end if
	if KeyDown.Right == 1
		Object.XPos += 0x20000
	end if
	if KeyPress.ButtonA == 1
		PlaySfx(0, 0)
		test.presses++
		Rand(TempValue0, 200)
		test.random = TempValue0
		Box.Colour += 40
	end if
	if KeyPress.Start == 1
		SaveRAM[0] = test.presses
		WriteSaveRAM()
	end if
	if KeyPress.ButtonC == 1
		SaveRAM[0] = 0
		ReadSaveRAM()
		test.saved = SaveRAM[0]
	end if
end sub

sub ObjectDraw
	ClearScreen(0)
	TempValue1 = Box.Colour
	TempValue1 &= 255
	DrawRect(Object.iXPos, Object.iYPos, 16, 16, 255, TempValue1, 0, 255)
	DrawRect(8, 8, test.random, 4, 255, 255, 255, 255)
end sub

sub ObjectStartup
	Stage.TimeEnabled = 1
	SetMusicTrack("Tune.ogg", 0, 1)
	PlayMusic(0)
end sub
"""


def beep():
    rate, n = 44100, 4410
    samples = b"".join(struct.pack("<h", 6000 if (i // 50) % 2 else -6000) for i in range(n))
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    data = b"RIFF" + struct.pack("<I", 36 + len(samples)) + b"WAVE"
    data += b"fmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(samples)) + samples
    return data


def main():
    files = {
        "Data/Game/GameConfig.bin": game_config(),
        "Data/Palettes/MasterPalette.act": palette(),
        "Data/SoundFX/Test/Beep.wav": beep(),
        "Data/Music/Tune.ogg": open(os.path.join(HERE, "tune.ogg"), "rb").read(),
        "Data/Stages/Test/StageConfig.bin": stage_config(),
        "Data/Stages/Test/Act1.bin": act(),
        "Data/Scripts/Test/Box.txt": BOX.replace("\n", "\r\n").encode(),
    }
    open(sys.argv[1], "wb").write(pack(files))


if __name__ == "__main__":
    main()
