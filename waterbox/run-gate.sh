#!/bin/sh
# run-gate.sh - the Retro Engine v3 core's tests: the native reference and
# core.wbx are the same machine, on the core's own test game
# (tests/make-test-game.py) and, when one is given, on Sonic CD itself.
#
# Every test says what it compared. The tests:
#   equivalence  run-native and run-wbx: every frame's picture, sound, length
#                and lag, the frame count and every memory domain
#   rerecord     the sandbox saved and loaded before every frame = without
#   session      saved at a frame, a new host, loaded, finished = without
#   turbo        the first half not drawn: the rest the same, and the first
#                half's pictures really not drawn
#   input        the movie's presses reach the game's script: three presses of
#                A counted, Right and Left move the object 2 pixels a frame;
#                the frame that loads the stage is the only lag frame
#   saves        the game's save written (Start) and read back (C) in the same
#                run, in both builds, and across a session started after the
#                write
#   pokes        a property written before a frame lands in the game: the
#                object's position, a global variable by its name
#   settings     randomSeed is what the game's random numbers follow (the same
#                seed the same numbers, another seed others); screenWidth is
#                the picture's width
#   refusals     no data file, a file that is not one, a screen width out of
#                range: each says why
#   clock        the guest has no time() of its own: the one it calls is the
#                core's (the setting)
#   teeth        the equivalence comparison sees a one-frame difference
#   firmware     waterbox.config's firmware is the file the core opens
#   game         (with -g) the equivalence, rerecord and session tests on
#                Sonic CD's data file, which is never in the repository: by
#                default 3600 frames that start a game from the Android
#                release's title screen and run into Palmtree Panic, or the
#                frames of the movie -M names
#
# usage: run-gate.sh [-m <miniBox dir>] [-g <Data.rsdk>] [-M <movie>]
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-$HOME/chimera/extern/chimera-common-minibox}"
game=""
game_movie=""
while getopts "m:g:M:" opt; do
	case "$opt" in
		m) mb="$OPTARG" ;;
		g) game="$OPTARG" ;;
		M) game_movie="$OPTARG" ;;
		*) exit 2 ;;
	esac
done
mb="$(cd "$mb" && pwd)"

echo "== build (miniBox $mb)"
mkdir -p "$root/build"
make -C "$here" -f native.mk MB="$mb" -j"$(nproc)" > "$root/build/gate-native.log" 2>&1 || {
	tail -20 "$root/build/gate-native.log"; echo "native build failed"; exit 1; }
make -C "$here" -f guest.mk MB="$mb" -j"$(nproc)" > "$root/build/gate-guest.log" 2>&1 || {
	tail -20 "$root/build/gate-guest.log"; echo "guest build failed"; exit 1; }
native="$root/build/native/run-native"
wbx="$root/build/native/run-wbx"
core="$root/build/guest/core.wbx"
movie="$here/tests/test.movie"
frames=100

work="$root/build/gate"
rm -rf "$work"
mkdir -p "$work"
fails=0
pass() { echo "PASS $*"; }
fail() { echo "FAIL $*"; fails=$((fails + 1)); }

# the digests only (miniBox's own log lines go to the same stdout)
digests() { grep -E '^[A-Za-z]+(\[[^]]*\])?=' "$1" || true; }
same() { # same <test> <a> <b> <what>
	digests "$2" > "$2.d"
	digests "$3" > "$3.d"
	if cmp -s "$2.d" "$3.d"; then
		pass "$1: $4"
	else
		fail "$1: $4"; diff "$2.d" "$3.d" | head -20
	fi
}
value() { digests "$1" | sed -n "s/^$2=//p"; }
# a traced property's value at a frame (a trace line: frame rate read values...)
at() { awk -v f="$2" -v col="$3" '$1 == f { print $(3 + col) }' "$1"; }

mkgame() { # mkgame <dir>
	mkdir -p "$work/$1"
	python3 "$here/tests/make-test-game.py" "$work/$1/Data.rsdk"
}

mkgame synth
echo "== the test game ($frames frames, $movie)"
"$native" "$work/synth" --frames $frames --movie "$movie" > "$work/n.txt"
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" > "$work/w.txt" 2> "$work/w.err"
same equivalence "$work/n.txt" "$work/w.txt" "native = sandbox, $frames frames (lag $(value "$work/n.txt" lagFrames))"
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --rerecord > "$work/r.txt" 2> "$work/r.err"
same rerecord "$work/r.txt" "$work/w.txt" "save+load before every frame = straight ($(sed -n 's/stateBytes=//p' "$work/r.err") state bytes)"
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --session-at 75 > "$work/s.txt" 2> "$work/s.err"
same session "$work/s.txt" "$work/w.txt" "saved at frame 75 (after the game's save was written), loaded into a new host = straight"

"$native" "$work/synth" --frames $frames --movie "$movie" --turbo > "$work/t.txt"
digests "$work/t.txt" | grep -v '^videoHash=' > "$work/t.d"
digests "$work/n.txt" | grep -v '^videoHash=' > "$work/n.d"
if cmp -s "$work/t.d" "$work/n.d" && [ "$(value "$work/t.txt" videoHash)" != "$(value "$work/n.txt" videoHash)" ]; then
	pass "turbo: first half undrawn - the second half's pictures, the sound, the frames and memory the same; the first half's pictures not"
else
	fail "turbo"; diff "$work/t.d" "$work/n.d" | head
fi

echo "== input and saves"
props="test.presses,test.random,test.saved,Object.X[32]"
"$native" "$work/synth" --frames $frames --movie "$movie" --trace "$work/in.n" --trace-props "$props" > /dev/null
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --trace "$work/in.w" --trace-props "$props" > /dev/null 2>&1
"$wbx" "$core" "$work/synth" --frames $frames --movie "$movie" --session-at 75 --trace "$work/in.s" --trace-props "$props" > /dev/null 2>&1
# the object starts at x 100: Right 20 frames (+40), Left 10 frames (-20)
if [ "$(at "$work/in.n" 79 1)" = 3 ] && [ "$(at "$work/in.n" 99 1)" = 4 ] && [ "$(at "$work/in.n" 49 4)" = $((140 << 16)) ] &&
   [ "$(at "$work/in.n" 99 4)" = $((120 << 16)) ] && [ "$(value "$work/n.txt" lagFrames)" = 1 ] && [ "$(at "$work/in.n" 0 0)" = 0 ]; then
	pass "input: A counted 3 times by frame 79 and 4 by 99; Right then Left move the object to x 140, then 120; one lag frame, the stage's load"
else
	fail "input: presses $(at "$work/in.n" 79 1)/$(at "$work/in.n" 99 1), x $(at "$work/in.n" 49 4)/$(at "$work/in.n" 99 4), lag $(value "$work/n.txt" lagFrames)"
fi
if [ "$(at "$work/in.n" 89 3)" = -1 ] && [ "$(at "$work/in.n" 90 3)" = 3 ] && cmp -s "$work/in.n" "$work/in.w"; then
	pass "saves: the count written at frame 70 (3) is read back at frame 90, in both builds"
else
	fail "saves: before the read $(at "$work/in.n" 89 3), after $(at "$work/in.n" 90 3)"
fi
if [ "$(at "$work/in.s" 90 3)" = 3 ]; then
	pass "saves: a session started at frame 75 reads back the save written at 70"
else
	fail "saves: after a session, read back $(at "$work/in.s" 90 3)"
fi

echo "== pokes"
"$native" "$work/synth" --frames 30 --poke "20:Object.X[32]=13107200" --poke "25:test.presses=40" --trace "$work/pk.n" --trace-props "Object.X[32],test.presses" > /dev/null
"$wbx" "$core" "$work/synth" --frames 30 --poke "20:Object.X[32]=13107200" --poke "25:test.presses=40" --trace "$work/pk.w" --trace-props "Object.X[32],test.presses" > /dev/null 2>&1
if [ "$(at "$work/pk.n" 19 1)" = $((100 << 16)) ] && [ "$(at "$work/pk.n" 20 1)" = 13107200 ] && [ "$(at "$work/pk.n" 25 2)" = 40 ] && cmp -s "$work/pk.n" "$work/pk.w"; then
	pass "pokes: Object.X[32] and test.presses (a global variable by its name) written before a frame are what the game has after it, in both builds"
else
	fail "pokes: x $(at "$work/pk.n" 20 1), presses $(at "$work/pk.n" 25 2)"
fi

echo "== settings"
seeded() { # seeded <dir> <seed>
	mkdir -p "$work/$1"
	cp "$work/synth/Data.rsdk" "$work/$1/"
	printf '{"randomSeed": %s}' "$2" > "$work/$1/settings"
	"$native" "$work/$1" --frames $frames --movie "$movie" --trace "$work/$1/t" --trace-props test.random > /dev/null
	"$wbx" "$core" "$work/$1" --frames $frames --movie "$movie" --trace "$work/$1/tw" --trace-props test.random > /dev/null 2>&1
	awk 'NR > 1 { print $4 }' "$work/$1/t" | uniq | tr '\n' ' '
}
s0="$(seeded seed0 0)"
s0b="$(seeded seed0b 0)"
s7="$(seeded seed7 7)"
if [ "$s0" = "$s0b" ] && [ "$s0" != "$s7" ] && cmp -s "$work/seed7/t" "$work/seed7/tw"; then
	pass "settings: randomSeed 0 draws ${s0% } twice, 7 draws ${s7% }; both builds the same"
else
	fail "settings: randomSeed 0 ${s0}/${s0b}, 7 ${s7}"
fi
mkdir -p "$work/narrow"
cp "$work/synth/Data.rsdk" "$work/narrow/"
printf '{"screenWidth": 320}' > "$work/narrow/settings"
"$native" "$work/narrow" --frames 10 --screenshot 9:"$work/narrow/s.tga" > "$work/narrow/n" 2>&1 || true
"$wbx" "$core" "$work/narrow" --frames 10 > "$work/narrow/w" 2>&1 || true
width="$(python3 -c "import sys; d = open(sys.argv[1], 'rb').read(18); print(d[12] | d[13] << 8)" "$work/narrow/s.tga" 2>/dev/null || echo none)"
if [ "$width" = 320 ]; then
	same settings "$work/narrow/w" "$work/narrow/n" "screenWidth 320 draws a 320-pixel picture; native = sandbox"
else
	fail "settings: screenWidth 320 drew $width pixels"
fi

echo "== refusals"
refuse() { # refuse <dir> <expected text> <what>
	out="$("$native" "$work/$1" --frames 1 2>&1 || true)"
	outw="$("$wbx" "$core" "$work/$1" --frames 1 2>&1 || true)"
	if echo "$out" | grep -q "loadError=.*$2" && echo "$outw" | grep -q "loadError=.*$2"; then
		pass "refusals: $3 - $(echo "$out" | sed -n 's/^loadError=//p')"
	else
		fail "refusals: $3"; echo "$out" | tail -2
	fi
}
mkdir -p "$work/none" "$work/junk" "$work/wide"
head -c 100000 /dev/zero > "$work/junk/Data.rsdk"
cp "$work/synth/Data.rsdk" "$work/wide/"
printf '{"screenWidth": 640}' > "$work/wide/settings"
refuse none "error.game_s_data_file_data_rsdk" "no data file"
refuse junk "error.engine_did_not_start" "a file of zeros"
refuse wide "error.setting_screenwidth_is_it" "screenWidth 640"

echo "== clock"
if nm "$core" | grep -qE ' (T|W|t) (time|__real_time)$'; then
	fail "clock: the guest links a time() of the C library's"
else
	pass "clock: time() is only the core's (the randomSeed setting); SDL's counters are the frame count"
fi

echo "== teeth"
# frame 30 is in the Right stretch: Left instead
awk '!/^#/ { n++ } n == 31 && !/^#/ { print "L"; next } { print }' "$movie" > "$work/teeth.movie"
"$native" "$work/synth" --frames $frames --movie "$work/teeth.movie" > "$work/teeth.txt"
if digests "$work/teeth.txt" | cmp -s - "$work/n.txt.d"; then
	fail "teeth: a movie one frame different digests the same"
else
	pass "teeth: a movie one frame different digests differently"
fi

echo "== firmware"
fw="$(python3 -c "import json, sys; print(' '.join(f['id'] for f in json.load(open(sys.argv[1]))['firmware']))" "$here/waterbox.config")"
if [ "$fw" = "Data.rsdk" ] && grep -q 'const char \*name = "Data.rsdk";' "$here/rsdk-files.c"; then
	pass "firmware: waterbox.config's one firmware, Data.rsdk, is the name the core opens"
else
	fail "firmware: waterbox.config declares '$fw'"
fi

if [ -n "$game" ]; then
	echo "== Sonic CD ($game${game_movie:+, $game_movie})"
	mkdir -p "$work/game"
	cp "$game" "$work/game/Data.rsdk"
	if [ -n "$game_movie" ]; then
		gn=$(grep -vc '^#' "$game_movie")
		set -- --movie "$game_movie"
	else
		# the Android release: Start on the title, a direction and A on the menu
		# (Start Game), A through the save select, then Right and jumps in
		# Palmtree Panic
		gn=3600
		set -- --press 1200:S:5 --press 1700:D:5 --press 1800:U:5 --press 1900:A:5 --press 2300:A:5 --press 2700:A:5 \
			--press 3000:S:5 --press 3100:R:200 --press 3150:A:10 --press 3400:R:200 --press 3420:B:20
	fi
	gat=$((gn * 9 / 10))
	"$native" "$work/game" --frames $gn "$@" --trace "$work/g.t" --trace-props "Stage.List,Stage.Position" --audio "$work/g.pcm" > "$work/g.n"
	# Sound (chimera#302): the engine turned all its sound off when SDL could
	# not convert samples, and the run was silent from start to end. Now at
	# least half of every 5 seconds after the first 10 holds sound.
	quiet="$(python3 - "$work/g.pcm" <<'PY'
import array, sys
a = array.array("h")
a.frombytes(open(sys.argv[1], "rb").read())
step = 44100 * 2 * 5
blocks = [a[i:i + step] for i in range(2 * step, len(a) - step + 1, step)]
print(sum(1 for b in blocks if sum(1 for v in b if v) < len(b) // 2) if blocks else -1)
PY
)"
	if [ "$quiet" = 0 ]; then
		pass "game: sound - every 5 seconds after the first 10 holds sound"
	else
		fail "game: sound - $quiet stretch(es) of 5 seconds after the first 10 are mostly silent (all of them were, when SDL had no sample converters)"
	fi
	"$wbx" "$core" "$work/game" --frames $gn "$@" > "$work/g.w" 2>/dev/null
	stage="$(tail -1 "$work/g.t" | awk '{ print $4 "/" $5 }')"
	same game "$work/g.w" "$work/g.n" "native = sandbox, $gn frames (stage list/position at the end: $stage)"
	"$wbx" "$core" "$work/game" --frames $gn "$@" --rerecord > "$work/g.r" 2>/dev/null
	same game "$work/g.r" "$work/g.w" "rerecord = straight"
	"$wbx" "$core" "$work/game" --frames $gn "$@" --session-at $gat > "$work/g.s" 2>/dev/null
	same game "$work/g.s" "$work/g.w" "session at $gat = straight"

	# Save data (chimera#302): the route writes SData.bin; Export Save Data
	# gives it; a project given it back starts with it - the save select shows
	# slot 1 in use, where a fresh game shows four new games - and a file
	# with a name the game does not keep is refused
	mkdir -p "$work/sd" "$work/sdin" "$work/sdin-out" "$work/sdx"
	"$native" "$work/game" --frames $gn "$@" --savedata-out "$work/sd" > "$work/sd.n"
	cp "$game" "$work/sdin/Data.rsdk"
	cp "$work/sd/SData.bin" "$work/sdin/" 2>/dev/null
	printf '{"savedata":["SData.bin"]}' > "$work/sdin/slots"
	"$native" "$work/sdin" --frames 1 --savedata-out "$work/sdin-out" > "$work/sdin1.n"
	sel="--press 1200:S:5 --press 1700:D:5 --press 1800:U:5 --press 1900:A:5"
	# shellcheck disable=SC2086
	"$native" "$work/game" --frames 2200 $sel > "$work/fresh.n"
	# shellcheck disable=SC2086
	"$native" "$work/sdin" --frames 2200 $sel > "$work/seeded.n"
	# shellcheck disable=SC2086
	"$wbx" "$core" "$work/sdin" --frames 2200 $sel > "$work/seeded.w" 2>/dev/null
	cp "$game" "$work/sdx/Data.rsdk"
	cp "$work/sd/SData.bin" "$work/sdx/Other.bin" 2>/dev/null
	printf '{"savedata":["Other.bin"]}' > "$work/sdx/slots"
	"$native" "$work/sdx" --frames 1 > "$work/sdx.n" 2>&1 || true
	if [ ! -s "$work/sd/SData.bin" ]; then
		fail "game: save data - the route wrote no SData.bin to export"
	elif ! cmp -s "$work/sd/SData.bin" "$work/sdin-out/SData.bin"; then
		fail "game: save data - a project given SData.bin does not start with those bytes"
	elif [ "$(grep videoHash "$work/fresh.n")" = "$(grep videoHash "$work/seeded.n")" ]; then
		fail "game: save data - the save select with the save given is the fresh game's: the game did not read it"
	elif ! grep -q "error.save_data_is_not_a_file_this.Other.bin" "$work/sdx.n"; then
		fail "game: save data - a save named Other.bin was not refused ($(tail -1 "$work/sdx.n"))"
	else
		pass "game: save data - SData.bin ($(stat -c %s "$work/sd/SData.bin") bytes) exported, given back and read (the save select differs from a fresh game's), a wrong name refused"
		same game "$work/seeded.w" "$work/seeded.n" "save data given: native = sandbox, 2200 frames"
	fi

	# The developer menu (chimera#302): with the Developer Menu setting on,
	# Dev Menu (Escape) opens it; with the setting off the same press changes
	# nothing, as the control is not there
	for dm in on off; do
		mkdir -p "$work/dm-$dm"
		cp "$game" "$work/dm-$dm/Data.rsdk"
		if [ "$dm" = on ]; then printf '{"devMenu": true}' > "$work/dm-$dm/settings"; fi
		"$native" "$work/dm-$dm" --frames 800 --press 600:E:5 > "$work/dm-$dm.n"
	done
	"$wbx" "$core" "$work/dm-on" --frames 800 --press 600:E:5 > "$work/dm-on.w" 2>/dev/null
	"$native" "$work/dm-off" --frames 800 > "$work/dm-none.n"
	if [ "$(grep videoHash "$work/dm-on.n")" = "$(grep videoHash "$work/dm-off.n")" ]; then
		fail "game: developer menu - Dev Menu with the setting on made no difference to the picture"
	elif [ "$(grep videoHash "$work/dm-off.n")" != "$(grep videoHash "$work/dm-none.n")" ]; then
		fail "game: developer menu - with the setting off, Dev Menu still changed the game"
	else
		pass "game: developer menu - Dev Menu opens it with the setting on, and does nothing with the setting off"
		same game "$work/dm-on.w" "$work/dm-on.n" "developer menu: native = sandbox"
	fi
else
	echo "SKIP game: no Sonic CD data file given (-g <Data.rsdk>); it is never in the repository"
fi

echo
if [ $fails -eq 0 ]; then
	echo "gate: all tests passed"
else
	echo "gate: $fails test(s) failed"
	exit 1
fi
