"""The port's stream format (.adp): IMA ADPCM in self-contained 64-frame packets, so a seek is exact
and cheap (game/audio/adpcm decodes it: a few adds per sample, where Vorbis cost an Old 3DS ~15% of
a core per stream).

  header (16 bytes, little-endian): "SADP", u8 version (1), u8 channels, u16 0, u32 frames, u32 rate
  packets: packet k, channel c at 16 + (k * channels + c) * 36:
      s16 predictor, u8 step index (0-88), u8 0, 32 bytes of 64 nibbles (low nibble first)
  Each nibble is decoded from the state the last one left (the header's for the first): standard IMA
  (step = STEPS[index]; diff = step/8 + bit2 step + bit1 step/2 + bit0 step/4, bit3 the sign;
  predictor clamped to int16; index += INDEX_STEP[nibble], clamped). The last packet is padded.

The encoder works on all packets at once (numpy): each starts from its first sample with the best of a
few step indices for it (the least squared error).
"""
import struct

import numpy as np

PACKET_FRAMES = 64
PACKET_BYTES = 36
HEADER = 16
VERSION = 1

STEPS = np.array([
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767], np.int32)
INDEX_STEP = np.array([-1, -1, -1, -1, 2, 4, 6, 8] * 2, np.int32)


def _run(x, pred, idx):
    """Encodes x (packets, 64) from the given start states: (nibbles, decoded)."""
    nib = np.zeros(x.shape, np.uint8)
    out = np.zeros(x.shape, np.int32)
    pred, idx = pred.copy(), idx.copy()
    for i in range(PACKET_FRAMES):
        step = STEPS[idx]
        diff = x[:, i] - pred
        d = np.abs(diff)
        code = np.zeros_like(idx)
        vp = step >> 3
        b = d >= step
        code |= b * 4
        d = np.where(b, d - step, d)
        vp = vp + b * step
        b = d >= (step >> 1)
        code |= b * 2
        d = np.where(b, d - (step >> 1), d)
        vp = vp + b * (step >> 1)
        b = d >= (step >> 2)
        code |= b * 1
        vp = vp + b * (step >> 2)
        neg = diff < 0
        code |= neg * 8
        pred = np.clip(np.where(neg, pred - vp, pred + vp), -32768, 32767)
        idx = np.clip(idx + INDEX_STEP[code], 0, 88)
        nib[:, i] = code
        out[:, i] = pred
    return nib, out


def encode_channel(x):
    """x: int32 samples of one channel, padded to whole packets → (pred, idx, nibbles, decoded)."""
    x = x.reshape(-1, PACKET_FRAMES)
    pred = x[:, 0].copy()
    # The start index: near the packet's own first steps, then the best of a spread around it.
    first = np.maximum(np.abs(np.diff(x[:, :5], axis=1)).mean(axis=1), 1)
    guess = np.clip(np.searchsorted(STEPS, first * 2), 0, 88).astype(np.int32)
    best = None
    for off in (-12, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 12):
        idx = np.clip(guess + off, 0, 88).astype(np.int32)
        nib, out = _run(x, pred, idx)
        err = ((out - x).astype(np.int64) ** 2).sum(axis=1)
        if best is None:
            best = [idx, nib, out, err]
            continue
        better = err < best[3]
        best[0] = np.where(better, idx, best[0])
        best[1] = np.where(better[:, None], nib, best[1])
        best[2] = np.where(better[:, None], out, best[2])
        best[3] = np.where(better, err, best[3])
    return pred, best[0], best[1], best[2]


def encode(samples, rate):
    """samples: (frames, channels) float in [-1, 1] → (the file's bytes, the SNR in dB)."""
    n, channels = samples.shape
    padded = -(-n // PACKET_FRAMES) * PACKET_FRAMES
    q = np.zeros((padded, channels), np.int32)
    q[:n] = np.clip(np.round(samples * 32767), -32768, 32767).astype(np.int32)
    packets = padded // PACKET_FRAMES
    data = np.zeros((packets, channels, PACKET_BYTES), np.uint8)
    signal = noise = 0.0
    for c in range(channels):
        pred, idx, nib, out = encode_channel(q[:, c])
        data[:, c, 0:2] = pred.astype('<i2').view(np.uint8).reshape(-1, 2)
        data[:, c, 2] = idx
        data[:, c, 4:] = nib[:, 0::2] | (nib[:, 1::2] << 4)
        ref = q[:n, c].astype(np.float64)
        signal += float((ref ** 2).sum())
        noise += float(((out.reshape(-1)[:n] - ref) ** 2).sum())
    snr = 10 * np.log10(signal / max(noise, 1e-9))
    return struct.pack('<4sBBHII', b'SADP', VERSION, channels, 0, n, rate) + data.tobytes(), snr


def decode(blob):
    """The file's samples, (frames, channels) int16: the reference decoder (tests the encoder)."""
    magic, version, channels, _, n, _rate = struct.unpack_from('<4sBBHII', blob)
    assert magic == b'SADP' and version == VERSION
    data = np.frombuffer(blob, np.uint8, offset=HEADER).reshape(-1, channels, PACKET_BYTES)
    out = np.zeros((data.shape[0] * PACKET_FRAMES, channels), np.int16)
    for c in range(channels):
        d = data[:, c]
        pred = d[:, 0:2].copy().view('<i2').reshape(-1).astype(np.int32)
        idx = d[:, 2].astype(np.int32)
        nib = np.zeros((len(d), PACKET_FRAMES), np.int32)
        nib[:, 0::2] = d[:, 4:] & 15
        nib[:, 1::2] = d[:, 4:] >> 4
        for i in range(PACKET_FRAMES):
            step = STEPS[idx]
            code = nib[:, i]
            vp = (step >> 3) + (code & 4 > 0) * step + (code & 2 > 0) * (step >> 1) + (code & 1 > 0) * (step >> 2)
            pred = np.clip(np.where(code & 8 > 0, pred - vp, pred + vp), -32768, 32767)
            idx = np.clip(idx + INDEX_STEP[code], 0, 88)
            out[i::PACKET_FRAMES, c][:len(pred)] = pred
    return out[:n]
