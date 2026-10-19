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
      project has them, so make_audio can tell they're unchanged), "levels": [[value, dB], ...]}};
  "indoorMusicEq": AudioManager's IndoorMusic3Eq (its global parameters on the Music bus's ThreeEQ,
      set while the player is in a house): the music's change per band, measured, and the port's
      3-band EQ gains fitted to it ("lowHz"/"highHz" crossovers, "lowDb"/"highDb");
  "spatial": the spatializer as Slider places its listener: {"gain": a mono sound's total over its
      two sides (constant power), "pan": [[azimuth, theta], ...] deg (theta: the split's angle, 45
      centered, 90 hard right; azimuth: the sound's side over the listener's height)}.
Needs Windows (the plugin's x86_64 DLL), numpy, soundfile, ffmpeg.
"""
import ctypes as C
import json
import os
import re
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


class Vec(C.Structure):
    _fields_ = [('x', C.c_float), ('y', C.c_float), ('z', C.c_float)]


class Attributes3D(C.Structure):  # FMOD_3D_ATTRIBUTES
    _fields_ = [('position', Vec), ('velocity', Vec), ('forward', Vec), ('up', Vec)]


LISTENER_Z = 10  # AudioManager.ZLevel: the listener this far off the plane (Unity's camera axes)


def render(dll, event, seconds, out, params=None, stop_at=None, instance_params=None, restart_every=None, place=None):
    """`event` rendered for `seconds` (stopped, fading out, at `stop_at`) to `out`, with global
    `params` and the instance's own `instance_params` (`restart_every` s: a new instance starts,
    as a game playing it again; `place` (dx, dy): the listener LISTENER_Z off the plane, facing it
    as Slider's does, the sound at (dx, dy) on it); returns the frame the stop happened at."""
    sysh = C.c_void_p()
    ok(dll.FMOD_Studio_System_Create(C.byref(sysh), VERSION), 'create')
    core = C.c_void_p()
    ok(dll.FMOD_Studio_System_GetCoreSystem(sysh, C.byref(core)), 'core')
    ok(dll.FMOD_System_SetOutput(core, 5), 'output')  # FMOD_OUTPUTTYPE_WAVWRITER_NRT
    ok(dll.FMOD_System_SetSoftwareFormat(core, RATE, 3, 0), 'format')  # stereo
    ok(dll.FMOD_Studio_System_Initialize(sysh, 256, 2, 0, C.c_char_p(out.encode())), 'init')  # SYNCHRONOUS_UPDATE
    for b in ['Master.bank', 'Master.strings.bank', 'BGM.bank', 'Ambience.bank', 'SFX.bank', 'Dialogue.bank']:
        bank = C.c_void_p()
        ok(dll.FMOD_Studio_System_LoadBankFile(sysh, os.path.join(BANKS, b).encode(), 0, C.byref(bank)), b)
    desc = C.c_void_p()
    ok(dll.FMOD_Studio_System_GetEvent(sysh, event.encode(), C.byref(desc)), event)
    ok(dll.FMOD_Studio_EventDescription_LoadSampleData(desc), 'load')
    ok(dll.FMOD_Studio_System_FlushSampleLoading(sysh), 'flush')
    inst = C.c_void_p()
    ok(dll.FMOD_Studio_EventDescription_CreateInstance(desc, C.byref(inst)), 'instance')
    if place is not None:
        listener = Attributes3D(Vec(0, 0, LISTENER_Z), Vec(0, 0, 0), Vec(0, 0, 1), Vec(0, 1, 0))
        ok(dll.FMOD_Studio_System_SetListenerAttributes(sysh, 0, C.byref(listener), None), 'listener')
    source = Attributes3D(Vec(*(place or (0, 0)), 0), Vec(0, 0, 0), Vec(0, 0, 1), Vec(0, 1, 0))
    if place is not None:
        ok(dll.FMOD_Studio_EventInstance_Set3DAttributes(inst, C.byref(source)), '3d')
    for k, v in (params or {}).items():
        ok(dll.FMOD_Studio_System_SetParameterByName(sysh, k.encode(), C.c_float(v), 1), k)
    for k, v in (instance_params or {}).items(): # (one the event doesn't have is left out)
        dll.FMOD_Studio_EventInstance_SetParameterByName(inst, k.encode(), C.c_float(v), 1)
    ok(dll.FMOD_Studio_EventInstance_Start(inst), 'start')
    stop_block = int(stop_at * RATE / BLOCK) if stop_at is not None else -1
    every = int(restart_every * RATE / BLOCK) if restart_every else 0
    for i in range(int(seconds * RATE / BLOCK)):
        if every and i > 0 and i % every == 0:
            ok(dll.FMOD_Studio_EventDescription_CreateInstance(desc, C.byref(inst)), 'instance')
            if place is not None:
                ok(dll.FMOD_Studio_EventInstance_Set3DAttributes(inst, C.byref(source)), '3d')
            ok(dll.FMOD_Studio_EventInstance_Start(inst), 'start')
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


# AudioManager's IndoorMusic3Eq (SetListenerIsIndoor): global parameters on the Music bus's ThreeEQ.
INDOOR_ASSET = os.path.join(UNITY, 'Audio', 'Modifiers', 'IndoorMusic3Eq.asset')
EQ_LOW_HZ, EQ_HIGH_HZ = 280, 1650  # the Music bus ThreeEQ's crossovers (its slope 0: 12 dB / octave)
BANDS = [(40, 150), (150, 280), (280, 600), (600, 1650), (1650, 3000), (3000, 6000), (6000, 16000)]


def band_db(x, rate):
    X = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / rate)
    return np.array([10 * np.log10(X[(f >= lo) & (f < hi)].sum() + 1e-20) for lo, hi in BANDS])


def biquad(x, b, a):
    """Direct form I, as the port's mixer runs it."""
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        y[i] = b[0] * x[i] + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        x2, x1, y2, y1 = x1, x[i], y1, y[i]
    return y


def rbj(kind, hz, rate):
    """RBJ cookbook low / high pass, Q = 1/sqrt(2) (game/audio/mixer.cpp's Biquad::pass)."""
    w = 2 * np.pi * hz / rate
    alpha = np.sin(w) / (2 * np.sqrt(0.5))
    c = np.cos(w)
    b = [(1 - c) / 2, 1 - c, (1 - c) / 2] if kind == 'low' else [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    a0 = 1 + alpha
    return [v / a0 for v in b], [1, -2 * c / a0, (1 - alpha) / a0]


def port_eq(x, rate, low_db, high_db):
    """The port's split: low = LP(x), high = HP(x - low), mid = the rest (flat at 0 dB)."""
    low = biquad(x, *rbj('low', EQ_LOW_HZ, rate))
    rest = x - low
    high = biquad(rest, *rbj('high', EQ_HIGH_HZ, rate))
    return low * 10 ** (low_db / 20) + (rest - high) + high * 10 ** (high_db / 20)


def measure_indoor_eq(dll, tmp):
    """The Village music with IndoorMusic3Eq's values against without, band by band; the port's EQ
    gains fitted to the same changes."""
    text = open(INDOOR_ASSET, encoding='utf-8').read()
    params = {k: float(v) for k, v in re.findall(r'parameter: (\S+)\s+value: (\S+)', text)}
    out_wav, in_wav = os.path.join(tmp, 'eq_out.wav'), os.path.join(tmp, 'eq_in.wav')
    render(dll, 'event:/Music/Village BGM', 7, out_wav)
    render(dll, 'event:/Music/Village BGM', 7, in_wav, params=params)
    a, _ = sf.read(out_wav, dtype='float32')
    b, _ = sf.read(in_wav, dtype='float32')
    target = band_db(b[2 * RATE:6 * RATE, 0], RATE) - band_db(a[2 * RATE:6 * RATE, 0], RATE)
    # The fit, at the port's rate on a stretch of the source (band changes don't depend on the level).
    rate = 32000
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', '2', '-t', '1.5', '-i',
                          os.path.join(PROJECT_ASSETS, 'Music', 'Connection - Eclipse _イクリプス.mp3'),
                          '-f', 'f32le', '-ac', '1', '-ar', str(rate), '-'], capture_output=True, check=True).stdout
    src = np.frombuffer(raw, dtype=np.float32).astype(np.float64)
    base = band_db(src, rate)
    lowpass = biquad(src, *rbj('low', EQ_LOW_HZ, rate))
    highpass = biquad(src - lowpass, *rbj('high', EQ_HIGH_HZ, rate))
    best = None
    for low_db in np.arange(-15, 0.01, 0.25):
        for high_db in np.arange(-30, 0.01, 0.25):
            y = lowpass * 10 ** (low_db / 20) + (src - lowpass - highpass) + highpass * 10 ** (high_db / 20)
            err = float(((band_db(y, rate) - base - target) ** 2).sum())
            if best is None or err < best[0]:
                best = (err, float(low_db), float(high_db))
    print(f'indoor EQ {params}: measured {np.round(target, 2).tolist()} -> low {best[1]} dB, high {best[2]} dB (err {best[0]:.2f})')
    return {'parameters': params, 'lowHz': EQ_LOW_HZ, 'highHz': EQ_HIGH_HZ, 'lowDb': best[1], 'highDb': best[2],
            'measured': [round(float(t), 2) for t in target]}


def rolloff(d, lo, hi):
    """The spatializer's linear-squared distance attenuation."""
    return 1.0 if d <= lo else max(0.0, (hi - d) / (hi - lo)) ** 2


def measure_spatial(dll, tmp):
    """A mono sound placed on the plane, the listener above it (as Slider sets it up): each side's
    gain against the dry source, the distance attenuation divided out. FMOD's split is constant
    power (`gain` the total) at an angle (theta: 45 deg centered, 90 hard right) that follows the
    source's azimuth (its side over the listener's height; its height on screen doesn't pan it)."""
    event, lo, hi = 'event:/SFX/SFXR/Fall', 1, 300  # (mono, no randomizer, only the spatializer)
    src = decode(os.path.join(PROJECT_ASSETS, 'SFX', 'sfxr', 'fall.wav'))[:, 0]
    ref = float(np.abs(src).max())
    gains, pan = [], []
    for dx in [0, 2.5, 5, 7.5, 10, 15, 20, 30, 40]:
        wav = os.path.join(tmp, 'spatial.wav')
        render(dll, event, 1, wav, place=(dx, 0))
        x, _ = sf.read(wav, dtype='float32')
        r = rolloff((dx * dx + LISTENER_Z ** 2) ** 0.5, lo, hi)
        left, right = float(np.abs(x[:, 0]).max()) / ref / r, float(np.abs(x[:, 1]).max()) / ref / r
        gains.append((left * left + right * right) ** 0.5)
        pan.append([round(float(np.degrees(np.arctan2(dx, LISTENER_Z))), 3), round(float(np.degrees(np.arctan2(right, left))), 3)])
    gain = float(np.median(gains))
    print(f'spatial: gain {gain:.4f} (each {np.round(gains, 3).tolist()}), pan (azimuth, theta) {pan}')
    return {'gain': round(gain, 4), 'pan': pan + [[90.0, 90.0]]}


def main():
    if sys.platform != 'win32':
        sys.exit('fmod_measure: needs Windows (FMOD\'s x86_64 DLL)')
    dll = C.WinDLL(DLL)
    tmp = tempfile.mkdtemp(prefix='slider3ds_measure_')
    data = {'fmod': '2.02.13', 'ahdsr': measure_ahdsr(dll, tmp), 'curves': {}, 'indoorMusicEq': measure_indoor_eq(dll, tmp),
            'spatial': measure_spatial(dll, tmp)}
    for event, param, points in CURVES:
        data['curves'][f'{event}|{param}'] = measure_curve(dll, tmp, event, param, points)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, indent=1)
        f.write('\n')
    print('wrote', OUT)


if __name__ == '__main__':
    main()
