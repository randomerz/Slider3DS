"""Renders the Village's FMOD events to Ogg Vorbis for the port: assets/audio/*.ogg + audio.json.

    python tools/make_audio.py [--keep-export]

Sources (the Slider repo next to this one):
  - Slider/Assets/Prefabs/Managers/AudioManager.prefab: AudioManager's sounds / music / ambience
    tables (name -> FMOD event path), the names the game plays them by;
  - Slider/FMODAssets/Slider: the FMOD Studio project (events, and the source audio in its Assets/).
FMOD Studio's command line tool runs tools/fmod_export.js on the project, which dumps the events
with every property resolved; this script then renders each event's timeline (its sounds at their
positions, trims, fades and track volumes) at the port's rate with ffmpeg + numpy, and writes what
it can't bake as data. Anything else an event uses fails the export (or warns, where the port can
do without it), so a changed FMOD project can't slip through unnoticed. Needs FMOD Studio 2.02
(FMOD_STUDIO below), ffmpeg on PATH, numpy.

audio.json (the port's game/audio/audio_bank reads it):
  {"rate": 32000,
   "sounds": {name: {"clips": [file, ...] (a multi instrument's playlist: one picked at random),
       "volume": dB (the event's master volume), "pitchRandom": semitones (FMOD's randomizer: a
       random pitch within +-half of it), "pitchParam": the "Pitch" parameter (0.5-2) sets the pitch,
       "spatial": [min, max] distance (the spatializer, linear-squared rolloff; absent: 2D),
       "canPause"}},
   "music" / "ambience": {name: {"file", "volume": dB, "attack" / "release": s (the volume AHDSR:
       in from / out to silence, linear in dB; 0: none), "length": the timeline's frames (playing
       up to it ends the event; the file goes on with the crossfades, reached only by jumps),
       "jumps": [[at, to] or [at, to, parameter, min, max], ...] (frames, in order of `at`: when
       playback reaches `at` (landing there by a jump doesn't count), it continues from `to` if the
       global parameter is in [min, max]; the first such jump wins)}}}
Clips are mono (sounds are spatialized or centered); streams are stereo. Track and instrument
volumes, trims and fades are baked; the event's master volume is data (less whatever a render
had to be turned down by to fit, so it plays as loud). A transition with a
crossfade is baked too: the stream jumps at its marker into the crossfade, rendered past the
timeline's end, and from there back to the destination plus the fade's length. Fade curves are
rendered linear (FMOD's curve shapes aren't modelled).
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UNITY = os.path.join(os.path.dirname(ROOT), 'Slider', 'Slider')
PREFAB = os.path.join(UNITY, 'Assets', 'Prefabs', 'Managers', 'AudioManager.prefab')
PROJECT = os.path.join(UNITY, 'FMODAssets', 'Slider')
FMOD_STUDIO = r'C:\Program Files\FMOD SoundSystem\FMOD Studio 2.02.29\fmodstudiocl.exe'
OUT = os.path.join(ROOT, 'assets', 'audio')
RATE = 32000
PEAK = 0.98 # renders louder than this are scaled down (Vorbis overshoots a little)
QUALITY = {'sounds': 4, 'music': 2, 'ambience': 2} # Vorbis -q

# What the Village and the title screen play, by AudioManager name.
SOUNDS = ['UI Click']
MUSIC = ['Main Menu', 'Village']
AMBIENCE = []

# Effects that change nothing by default, and the bus faders.
NEUTRAL_EFFECTS = {'MixerBusFader'}
TRANSITION_REGIONS = {'TransitionSourceSound', 'TransitionDestinationSound'}


def audio_manager_tables():
    """AudioManager.prefab's sounds / music / ambience: {table: {name: [event path, canPause]}}."""
    tables, table, name = {}, None, None
    for line in open(PREFAB, encoding='utf-8'):
        m = re.match(r'^  (sounds|music|ambience):', line)
        if m:
            table = tables.setdefault(m.group(1), {})
            continue
        if re.match(r'^  [A-Za-z]', line):
            table = None
        if table is None:
            continue
        m = re.match(r'^  - name: (.*)$', line)
        if m:
            name = m.group(1).strip()
        m = re.match(r'^\s+Path: (event:/.*)$', line)
        if m:
            table[name] = [m.group(1).strip(), True]
        if re.match(r'^\s+canPause: 0', line):
            table[name][1] = False
    return tables


def fmod_export(paths, keep):
    """Runs tools/fmod_export.js on the project (see there for what it returns)."""
    tmp = tempfile.mkdtemp(prefix='slider3ds_audio_')
    out = os.path.join(tmp, 'fmod.json').replace('\\', '/')
    script = open(os.path.join(ROOT, 'tools', 'fmod_export.js'), encoding='utf-8').read()
    script = script.replace('%OUT%', out).replace('%EVENTS%', json.dumps(paths))
    js = os.path.join(tmp, 'export.js')
    open(js, 'w', encoding='utf-8').write(script)
    r = subprocess.run([FMOD_STUDIO, '-script', js, os.path.join(PROJECT, 'Slider.fspro')],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if not os.path.exists(out):
        sys.exit('FMOD Studio export failed:\n' + r.stdout + r.stderr)
    data = json.load(open(out, encoding='utf-8'))
    if keep:
        print('export kept at', out)
    else:
        shutil.rmtree(tmp)
    return data


class Project:
    """The exported objects, with helpers to walk them."""

    def __init__(self, data):
        self.objects = data['objects']
        self.events = data['events']
        self.parameter_names = data['parameterNames']
        self.warnings = []

    def get(self, oid):
        return self.objects[oid]

    def rel(self, o, name):
        return [self.objects[i] for i in o['rels'].get(name, [])]

    def one(self, o, name):
        r = self.rel(o, name)
        return r[0] if r else None

    def automation(self, automator):
        """(property, parameter name, [(x, y), ...]) for each of an automator's curves."""
        out = []
        for curve in self.rel(automator, 'automationCurves'):
            param = self.one(curve, 'parameter')
            name = self.parameter_names.get(curve['rels']['parameter'][0], param['entity']) if param else None
            pts = sorted((p['props']['position'], p['props']['value']) for p in self.rel(curve, 'automationPoints'))
            out.append((automator['props']['nameOfPropertyBeingAutomated'], name, pts))
        return out

    def automators(self, o):
        return [x for a in self.rel(o, 'automators') for x in self.automation(a)]


class Fail(Exception):
    pass


def db(x):
    return 10 ** (x / 20)


def decode(path, channels):
    """A source file as float32 frames at RATE (shape (frames, channels))."""
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'f32le', '-ac', str(channels), '-ar', str(RATE), '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, channels).copy()


def headroom(clips):
    """dB to scale `clips` by so none clips (0 if none would); the entry's volume gains it back."""
    peak = max(float(np.abs(c).max()) for c in clips if c.size)
    return -20 * np.log10(peak / PEAK) if peak > PEAK else 0.0


def encode(samples, path, quality):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(RATE), '-ac', str(samples.shape[1]), '-i', '-',
                    '-c:a', 'libvorbis', '-q:a', str(quality),
                    '-fflags', '+bitexact', '-flags:a', '+bitexact', path], # (reproducible: no random serial)
                   input=samples.astype(np.float32).tobytes(), check=True)
    # The port seeks by frame: the file must decode to exactly as many as were rendered.
    probe = subprocess.run(['ffprobe', '-v', 'error', '-count_packets', '-show_entries', 'stream=duration_ts',
                            '-of', 'csv=p=0', path], capture_output=True, text=True, check=True).stdout.strip()
    if int(probe) != len(samples):
        raise Fail(f'{os.path.basename(path)} decodes to {probe} frames, not {len(samples)}')


def frames(t):
    return int(round(t * RATE))


class Event:
    """One event, interpreted: its tracks' sounds and what the port plays as data."""

    def __init__(self, proj, path):
        self.proj, self.path = proj, path
        self.ev = proj.get(proj.events[path])
        self.warn = lambda msg: proj.warnings.append(f'{path}: {msg}')
        P = proj
        master = P.one(self.ev, 'masterTrack')
        mm = P.one(master, 'mixerGroup')
        self.volume = mm['props']['volume']
        if mm['props']['pitch'] != 0:
            raise Fail(f'{path}: a master pitch')
        if P.rel(mm, 'modulators'):
            raise Fail(f'{path}: a modulator on the master track')
        self.bus = self.bus_name()
        self.spatial = None
        self.pitch_param = False
        props = P.one(self.ev, 'automatableProperties')['props']
        for fx in P.rel(P.one(mm, 'effectChain'), 'effects'):
            kind = fx['entity']
            if kind == 'SpatialiserEffect':
                p = fx['props']
                if p['distanceRolloffType'] != 0:
                    raise Fail(f'{path}: a spatializer rolloff other than linear squared')
                self.spatial = [p['minimumDistance'], p['maximumDistance']] if p['overrideRange'] else \
                    [props['minimumDistance'], props['maximumDistance']]
            elif kind == 'PitchShifterEffect':
                # Bound to "Pitch": the port changes the playback rate instead (also the length).
                if not self.binds_pitch(P.automators(fx), 'pitch'):
                    raise Fail(f'{path}: a pitch shifter not bound to the Pitch parameter')
                self.pitch_param = True
            elif kind not in NEUTRAL_EFFECTS:
                self.warn(f'{kind} on the master track is not ported')
        # The event's own pitch, set by "Pitch" one to one (a pitch shifter's curve is its own: Pop's
        # tops out at 1.8; the port plays both as the parameter's value).
        for at in P.rel(master, 'automationTracks'):
            for a in P.rel(at, 'automator'):
                for prop, param, pts in P.automation(a):
                    if prop != 'pitch' or param != 'Pitch':
                        raise Fail(f'{path}: automation of {prop} by {param}')
                    self.pitch_param = True
        self.attack = self.release = 0
        self.pitch_random = 0
        self.tracks = [self.track(t) for t in P.rel(self.ev, 'groupTracks')]
        # The AHDSR is played as the whole event's: one track's, with no others beside it.
        if len(self.tracks) > 1 and (self.attack or self.release):
            raise Fail(f'{path}: an AHDSR on one of several tracks')
        if P.rel(self.ev, 'returnTracks'):
            raise Fail(f'{path}: return tracks')

    def binds_pitch(self, autos, prop):
        return autos and all(p == prop and n == 'Pitch' for p, n, _ in autos)

    def bus_name(self):
        """The top bus under Master the event plays into (Unity's volume settings are per bus)."""
        chain, o = [], self.proj.one(self.proj.one(self.ev, 'mixerInput'), 'output')
        while o is not None and o['entity'] != 'MixerMaster':
            if o['props']['volume'] != 0:
                raise Fail(f'{self.path}: bus {o["props"]["name"]} has a volume')
            chain.append(o['props']['name'])
            o = self.proj.one(o, 'output')
        if len(chain) < 2 or chain[-1] != 'Master':
            raise Fail(f'{self.path}: not under bus:/Master')
        return chain[-2]

    def track(self, t):
        P = self.proj
        g = P.one(t, 'mixerGroup')
        if g['props']['pitch'] != 0:
            raise Fail(f'{self.path}: a track pitch')
        for fx in P.rel(P.one(g, 'effectChain'), 'effects'):
            if fx['entity'] not in NEUTRAL_EFFECTS:
                self.warn(f'{fx["entity"]} on a track is not ported')
        for m in P.rel(g, 'modulators'):
            p = m['props']
            if m['entity'] != 'ADSRModulator' or p['nameOfPropertyBeingModulated'] != 'volume':
                raise Fail(f'{self.path}: a {m["entity"]} on the track')
            if p['initialValue'] > -80 or p['finalValue'] > -80 or p['peakValue'] != 0 or p['sustainValue'] != 0:
                raise Fail(f'{self.path}: an AHDSR other than in from / out to silence')
            self.attack, self.release = p['attackTime'] / 1000, p['releaseTime'] / 1000
        if P.rel(t, 'automationTracks'):
            raise Fail(f'{self.path}: track automation')
        # (A transition's source / destination regions sit on the tracks too: read with its marker.)
        return {'volume': g['props']['volume'], 'modules': [self.module(m) for m in P.rel(t, 'modules')
                                                             if m['entity'] not in TRANSITION_REGIONS]}

    def module(self, m):
        P = self.proj
        p = m['props']
        if m['entity'] not in ('SingleSound', 'MultiSound'):
            raise Fail(f'{self.path}: a {m["entity"]}')
        if p['pitch'] != 0 or p['looping'] or p['triggerProbabilityEnabled'] or P.rel(m, 'triggerConditions'):
            raise Fail(f'{self.path}: an instrument with pitch, looping or trigger conditions')
        for mod in P.rel(m, 'modulators'):
            mp = mod['props']
            if mod['entity'] == 'RandomizerModulator' and mp['nameOfPropertyBeingModulated'] == 'pitch':
                self.pitch_random = mp['amount']
            else:
                raise Fail(f'{self.path}: a {mod["entity"]} on {mp.get("nameOfPropertyBeingModulated")}')
        fades = []
        for name in ('fadeInCurve', 'fadeOutCurve'):
            c = P.one(m, name)
            if c:
                a, b = P.one(c, 'startPoint')['props'], P.one(c, 'endPoint')['props']
                fades.append((a['position'], a['value'], b['position'], b['value']))
        if m['entity'] == 'SingleSound':
            files = [P.one(m, 'audioFile')['props']['assetPath']]
        else:
            entries = P.rel(m, 'sounds')
            if any(e['entity'] != 'SingleSound' for e in entries):
                raise Fail(f'{self.path}: a playlist entry other than a sound')
            files = [P.one(e, 'audioFile')['props']['assetPath'] for e in entries]
        return {'start': p['start'], 'length': p['length'], 'offset': p['timelockedOffset'] + p['startOffset'],
                'volume': p['volume'], 'fades': fades, 'files': files}

    def render_module(self, mod, file, channels, track_volume):
        """One instrument (one of its files) as placed: (start frame, samples)."""
        src = decode(os.path.join(PROJECT, 'Assets', file), channels)
        a = frames(mod['offset'])
        out = src[a:a + frames(mod['length'])] * db(mod['volume'] + track_volume)
        t = np.arange(len(out)) / RATE
        for t0, v0, t1, v1 in mod['fades']: # positions from the instrument's start
            ramp = np.clip((t - t0) / max(t1 - t0, 1e-9), 0, 1)
            out *= (v0 + (v1 - v0) * ramp)[:, None]
        return frames(mod['start']), out

    def render_timeline(self, channels):
        parts = []
        for tr in self.tracks:
            for mod in tr['modules']:
                if len(mod['files']) != 1:
                    raise Fail(f'{self.path}: a playlist on a stream')
                parts.append(self.render_module(mod, mod['files'][0], channels, tr['volume']))
        end = max(s + len(x) for s, x in parts)
        out = np.zeros((end, channels), np.float32)
        for s, x in parts:
            out[s:s + len(x)] += x
        return out

    # -- sounds --

    def sound_clips(self):
        """Each clip a sound can play: its instruments mixed (a playlist: one clip per entry)."""
        mods = [(tr['volume'], m) for tr in self.tracks for m in tr['modules']]
        if self.proj.rel(self.proj.one(self.ev, 'timeline'), 'markers') and any(
                mk['entity'] != 'TempoMarker' for mk in self.proj.rel(self.proj.one(self.ev, 'timeline'), 'markers')):
            raise Fail(f'{self.path}: a sound with markers')
        if len(mods) == 1:
            vol, mod = mods[0]
            clips = []
            for f in mod['files']:
                start, x = self.render_module(mod, f, 1, vol)
                clips.append(np.concatenate([np.zeros((start, 1), np.float32), x]))
            return clips
        return [self.render_timeline(1)]

    # -- streams --

    def stream(self):
        """The timeline (stereo) with its crossfades appended, its length (frames) and the jumps
        that play it."""
        P = self.proj
        audio = self.render_timeline(2)
        transitions = [] # (at, to, condition, crossfade frames)
        for mk in P.rel(P.one(self.ev, 'timeline'), 'markers'):
            kind = mk['entity']
            if kind in ('TempoMarker', 'NamedMarker'):
                continue
            if kind != 'TransitionMarker':
                raise Fail(f'{self.path}: a {kind}')
            if mk['props']['triggerProbabilityEnabled']:
                raise Fail(f'{self.path}: a transition with a probability')
            at, to = frames(mk['props']['position']), frames(P.one(mk, 'destination')['props']['position'])
            cond = []
            conds = P.rel(mk, 'triggerConditions')
            if len(conds) > 1 or (conds and (conds[0]['entity'] != 'ParameterCondition' or conds[0]['props']['isInverted'])):
                raise Fail(f'{self.path}: a transition condition the port does not read')
            if conds:
                c = conds[0]
                cond = [P.parameter_names[c['rels']['parameter'][0]], c['props']['minimum'], c['props']['maximum']]
            tl = P.one(mk, 'transitionTimeline')
            fade = frames(tl['props']['length']) if tl else 0
            transitions.append((at, to, cond, fade, self.crossfade(tl) if fade else None))

        # The crossfades, past the timeline: the source from `at` (fading out) over the destination
        # from `to` (fading in); the stream jumps in at `at` and back out to `to` + the fade.
        jumps, pieces, pos = [], [audio], len(audio)
        for at, to, cond, fade, ramps in transitions:
            if fade == 0:
                jumps.append([at, to] + cond)
                continue
            ramp = (np.arange(fade) / fade)[:, None].astype(np.float32)

            def window(s):
                w = audio[s:s + fade]
                return np.concatenate([w, np.zeros((fade - len(w), 2), np.float32)])

            (s0, s1), (d0, d1) = ramps
            pieces.append(window(at) * (s0 + (s1 - s0) * ramp) + window(to) * (d0 + (d1 - d0) * ramp))
            jumps.append([at, pos] + cond)
            pos += fade
            jumps.append([pos, to + fade])
            # The copy replaces the destination's first `fade` frames: a transition there is skipped.
            if any(to <= t[0] < to + fade for t in transitions):
                raise Fail(f'{self.path}: a transition inside a crossfade')
        return np.concatenate(pieces), len(audio), sorted(jumps, key=lambda j: j[0])

    def crossfade(self, tl):
        """A transition timeline's gain ramps, ((from, to) for the source, (from, to) for the
        destination): each side plays over the whole timeline, faded by its curve or at full."""
        P = self.proj
        length = tl['props']['length']
        ramps = {'TransitionSourceSound': (1, 1), 'TransitionDestinationSound': (1, 1)}
        spans = set()
        for m in P.rel(tl, 'modules'):
            if m['props']['length'] == 0: # (the zero-length regions mark where the tracks resume)
                continue
            if m['entity'] not in ramps or m['props']['start'] != 0 or abs(m['props']['length'] - length) > 1e-6:
                raise Fail(f'{self.path}: a transition region other than the whole timeline')
            spans.add(m['entity'])
            c = P.one(m, 'fadeOutCurve') or P.one(m, 'fadeInCurve')
            if c is None:
                continue
            a, b = P.one(c, 'startPoint')['props'], P.one(c, 'endPoint')['props']
            if a['position'] != 0 or abs(b['position'] - length) > 1e-6:
                raise Fail(f'{self.path}: a transition fade shorter than its timeline')
            ramps[m['entity']] = (a['value'], b['value'])
        if spans != set(ramps):
            raise Fail(f'{self.path}: a transition without both a source and a destination')
        return ramps['TransitionSourceSound'], ramps['TransitionDestinationSound']

def file_name(table, name, variant=None):
    base = {'sounds': 'sfx', 'music': 'music', 'ambience': 'amb'}[table] + '_' + re.sub(r'[^a-z0-9]+', '_', name.lower()).strip('_')
    return base + (f'_{variant}' if variant is not None else '') + '.ogg'


def main():
    keep = '--keep-export' in sys.argv
    tables = audio_manager_tables()
    wanted = [('sounds', n) for n in SOUNDS] + [('music', n) for n in MUSIC] + [('ambience', n) for n in AMBIENCE]
    paths = []
    for table, name in wanted:
        if name not in tables.get(table, {}):
            sys.exit(f'AudioManager has no {table} entry "{name}"')
        paths.append(tables[table][name][0])
    proj = Project(fmod_export(sorted(set(paths)), keep))

    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith('.ogg'):
            os.remove(os.path.join(OUT, f))
    bank = {'rate': RATE, 'sounds': {}, 'music': {}, 'ambience': {}}
    try:
        for table, name in wanted:
            path, can_pause = tables[table][name]
            ev = Event(proj, path)
            expected = {'sounds': 'SFX', 'music': 'Music', 'ambience': 'Ambience'}[table]
            if ev.bus != expected:
                raise Fail(f'{path}: plays on the {ev.bus} bus, not {expected}')
            if table == 'sounds':
                clips = ev.sound_clips()
                gain = headroom(clips)
                files = []
                for i, clip in enumerate(clips):
                    f = file_name(table, name, i if len(clips) > 1 else None)
                    encode(clip * db(gain), os.path.join(OUT, f), QUALITY[table])
                    files.append(f)
                entry = {'clips': files, 'volume': round(ev.volume - gain, 3)}
                if ev.pitch_random: entry['pitchRandom'] = round(ev.pitch_random, 4)
                if ev.pitch_param: entry['pitchParam'] = True
                if ev.spatial: entry['spatial'] = ev.spatial
                if ev.attack or ev.release: raise Fail(f'{path}: an AHDSR on a sound')
                entry['canPause'] = can_pause
            else:
                if ev.spatial or ev.pitch_param or ev.pitch_random:
                    raise Fail(f'{path}: a spatial or pitched stream')
                audio, length, jumps = ev.stream()
                f = file_name(table, name)
                gain = headroom([audio])
                encode(audio * db(gain), os.path.join(OUT, f), QUALITY[table])
                entry = {'file': f, 'volume': round(ev.volume - gain, 3), 'attack': ev.attack, 'release': ev.release,
                         'length': length, 'jumps': jumps}
            bank[table][name] = entry
            print(f'{table:8} {name:12} -> {entry.get("file") or ", ".join(entry["clips"])}')
    except Fail as e:
        sys.exit(f'make_audio: {e}')
    text = json.dumps(bank, indent=1, ensure_ascii=False)
    # Arrays of plain values on one line.
    text = re.sub(r'\[[^\[\]{}]*\]', lambda m: re.sub(r'\s+', ' ', m.group(0)).replace('[ ', '[').replace(' ]', ']'), text)
    with open(os.path.join(OUT, 'audio.json'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(text + '\n')
    for w in proj.warnings:
        print('warning:', w)
    total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
    print(f'assets/audio: {total // 1024} KB')


if __name__ == '__main__':
    main()
