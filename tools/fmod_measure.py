"""Measures FMOD's curve shapes by rendering events with the game's own FMOD runtime, for
tools/make_audio.py (which can read every property of an event but not how FMOD shapes its curves:
the curveShape of automation points and the AHDSR's default shape aren't documented).

    python tools/fmod_measure.py        -> tools/fmod_measured.json (committed; re-run if the events change)

Uses Unity's FMOD plugin (fmodstudio.dll, Studio API 2.02.13) and the banks Unity built
(Assets/StreamingAssets), rendering non-realtime to WAV (FMOD's WAVWRITER_NRT output), paced so its
stream thread keeps up. Each level is the render's RMS over the source's, both on the same audio:
  "ahdsr": the AHDSR's shape (shape 0, from 0 dB to -80): dB at x = 0, 0.05 .. 1 of its time — the
      Village music's release after a stop (its attack is the same curve, mirrored);
  "curves": {"<event>|<parameter>": {"points": the automation points measured ([[x, dB], ...] as the
      project has them, so make_audio can tell they're unchanged), "levels": [[value, dB], ...]}}.
Needs Windows (the plugin's x86_64 DLL), numpy, soundfile, ffmpeg.
"""
import ctypes as C
import json
import os
import subprocess
import sys
import tempfile
import time

import numpy as np
import soundfile as sf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UNITY = os.path.join(os.path.dirname(ROOT), 'Slider', 'Slider', 'Assets')
DLL = os.path.join(UNITY, 'Plugins', 'FMOD', 'platforms', 'win', 'lib', 'x86_64', 'fmodstudio.dll')
BANKS = os.path.join(UNITY, 'StreamingAssets')
PROJECT_ASSETS = os.path.join(os.path.dirname(UNITY), 'FMODAssets', 'Slider', 'Assets')
OUT = os.path.join(ROOT, 'tools', 'fmod_measured.json')
VERSION = 0x00020213
RATE = 48000
BLOCK = 1024  # FMOD's DSP buffer: one mix per Update in NRT mode
PACE = 0.025
RELEASE_S = 1.0  # Village BGM's AHDSR releaseTime (make_audio exports it: 1000 ms)  # s of sleep per mix (slower than real time: the stream thread never starves)

# The parameter curves the port plays: (event, parameter, its automation points as the project has them).
CURVES = [('event:/Ambience/Waterfall', 'AmbienceDistToWater', [[0, 0], [15, -80]])]


def ok(r, what):
    if r != 0:
        raise RuntimeError(f'{what}: FMOD error {r}')


def render(dll, event, seconds, out, params=None, stop_at=None):
    """`event` rendered for `seconds` (stopped, fading out, at `stop_at`) to `out`; returns the
    frame the stop happened at."""
    sysh = C.c_void_p()
    ok(dll.FMOD_Studio_System_Create(C.byref(sysh), VERSION), 'create')
    core = C.c_void_p()
    ok(dll.FMOD_Studio_System_GetCoreSystem(sysh, C.byref(core)), 'core')
    ok(dll.FMOD_System_SetOutput(core, 5), 'output')  # FMOD_OUTPUTTYPE_WAVWRITER_NRT
    ok(dll.FMOD_System_SetSoftwareFormat(core, RATE, 3, 0), 'format')  # stereo
    ok(dll.FMOD_Studio_System_Initialize(sysh, 256, 2, 0, C.c_char_p(out.encode())), 'init')  # SYNCHRONOUS_UPDATE
    for b in ['Master.bank', 'Master.strings.bank', 'BGM.bank', 'Ambience.bank', 'SFX.bank']:
        bank = C.c_void_p()
        ok(dll.FMOD_Studio_System_LoadBankFile(sysh, os.path.join(BANKS, b).encode(), 0, C.byref(bank)), b)
    desc = C.c_void_p()
    ok(dll.FMOD_Studio_System_GetEvent(sysh, event.encode(), C.byref(desc)), event)
    ok(dll.FMOD_Studio_EventDescription_LoadSampleData(desc), 'load')
    ok(dll.FMOD_Studio_System_FlushSampleLoading(sysh), 'flush')
    inst = C.c_void_p()
    ok(dll.FMOD_Studio_EventDescription_CreateInstance(desc, C.byref(inst)), 'instance')
    for k, v in (params or {}).items():
        ok(dll.FMOD_Studio_System_SetParameterByName(sysh, k.encode(), C.c_float(v), 1), k)
    ok(dll.FMOD_Studio_EventInstance_Start(inst), 'start')
    stop_block = int(stop_at * RATE / BLOCK) if stop_at is not None else -1
    for i in range(int(seconds * RATE / BLOCK)):
        if i == stop_block:
            ok(dll.FMOD_Studio_EventInstance_Stop(inst, 0), 'stop')  # FMOD_STUDIO_STOP_ALLOWFADEOUT
        ok(dll.FMOD_Studio_System_Update(sysh), 'update')
        time.sleep(PACE)
    dll.FMOD_Studio_System_Release(sysh)
    return stop_block * BLOCK


def decode(path):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'f32le', '-ac', '2', '-ar', str(RATE), '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2)


def lag_of(render, src):
    """Where the source's 2 s mark sits in the render (FMOD's stream starts late)."""
    seg = src[2 * RATE:2 * RATE + RATE // 2, 0]
    best = max(range(RATE), key=lambda lag: float(np.dot(render[2 * RATE + lag:2 * RATE + lag + len(seg), 0], seg))
               if 2 * RATE + lag + len(seg) <= len(render) else -1e9)
    return best


def rms(x):
    return float(np.sqrt((x ** 2).mean()))


def measure_ahdsr(dll, tmp):
    """The Village music stopped 3 s in: its release, dB against the source at each 5% of it."""
    wav = os.path.join(tmp, 'release.wav')
    stop = render(dll, 'event:/Music/Village BGM', 5, wav, stop_at=3)
    out, _ = sf.read(wav, dtype='float32')
    src = decode(os.path.join(PROJECT_ASSETS, 'Music', 'Connection - Eclipse _イクリプス.mp3'))
    lag = lag_of(out, src)
    win = RATE // 50  # 20 ms
    level = []  # (render frame, gain) per window from just before the stop
    for f in range(stop - RATE // 10, stop + 2 * RATE, win):
        a, b = out[f:f + win], src[f - lag:f - lag + win]
        level.append((f + win // 2, rms(a) / max(rms(b), 1e-9)))
    full = np.median([g for f, g in level if f < stop])
    # The release's end is sharp (it reaches -80 dB: off) where its start hides in the music's own
    # ups and downs: measure back from the end by its time (the event's releaseTime, 1 s).
    end = next(f for f, g in level if f > stop and g < full * 3e-4)
    length = int(RELEASE_S * RATE)
    onset = end - length
    table = []
    for i in range(21):
        f = onset + int(i / 20 * length)
        g = np.interp(f, [p for p, _ in level], [g for _, g in level]) / full
        table.append(round(float(max(20 * np.log10(max(g, 1e-4)), -80)), 2))
    table[0], table[-1] = 0.0, -80.0
    print(f'AHDSR release: {length / RATE:.3f} s from {(onset - stop) / RATE:.3f} s after the stop: {table}')
    return table


def measure_curve(dll, tmp, event, param, points):
    lo, hi = points[0][0], points[-1][0]
    levels, ref = [], None
    for i in range(16):
        v = lo + (hi - lo) * i / 15
        wav = os.path.join(tmp, f'curve{i}.wav')
        render(dll, event, 2.5, wav, params={param: v})
        out, _ = sf.read(wav, dtype='float32')
        r = rms(out[RATE:2 * RATE])
        ref = ref if ref is not None else r
        levels.append([round(v, 4), round(float(max(20 * np.log10(max(r / ref, 1e-4)), -80)), 2)])
    print(f'{event} by {param}: {levels}')
    return {'points': points, 'levels': levels}


def main():
    if sys.platform != 'win32':
        sys.exit('fmod_measure: needs Windows (FMOD\'s x86_64 DLL)')
    dll = C.WinDLL(DLL)
    tmp = tempfile.mkdtemp(prefix='slider3ds_measure_')
    data = {'fmod': '2.02.13', 'ahdsr': measure_ahdsr(dll, tmp), 'curves': {}}
    for event, param, points in CURVES:
        data['curves'][f'{event}|{param}'] = measure_curve(dll, tmp, event, param, points)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, indent=1)
        f.write('\n')
    print('wrote', OUT)


if __name__ == '__main__':
    main()
