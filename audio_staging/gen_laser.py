"""
Stdlib-only laser-zap synthesizer (no numpy). Produces mono 48kHz 16-bit PCM WAVs.
Classic sci-fi "pew": descending-pitch sine sweep + a short noise/zap transient,
wrapped in a fast-attack / exponential-decay envelope.

Run:  python gen_laser.py
Writes laser_zap_v1.wav, laser_zap_v2.wav, laser_zap_v3.wav next to this file.
"""
import wave, struct, math, random, os

SR = 48000          # 48 kHz (matches Wwise default conversion SR)
random.seed(1234)    # deterministic output

def env_attack_decay(t, dur, attack=0.004, decay_k=None):
    """Fast linear attack, exponential decay to ~0 by end of dur."""
    if decay_k is None:
        decay_k = 5.0 / dur          # ~e^-5 (~0.7%) at the tail
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) * decay_k)

def write_wav(path, samples):
    # clamp + 16-bit
    frames = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        frames += struct.pack('<h', int(v * 32767))
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(bytes(frames))
    print("wrote", path, "(%.3fs)" % (len(samples)/SR))

def synth(dur, f_start, f_end, noise_amt, sweep_pow=2.0, vibrato=0.0, drive=1.0):
    """
    f_start->f_end pitch sweep (exponential-ish via sweep_pow), plus a 2nd/3rd
    harmonic for body, a decaying noise burst at the head for the 'zap', light
    soft-clip drive for grit. Returns float samples.
    """
    n = int(dur * SR)
    out = [0.0] * n
    phase = 0.0
    noise_k = 30.0 / dur          # noise burst decays much faster than the tone
    for i in range(n):
        t = i / SR
        frac = i / n
        # descending sweep, curved so most of the drop happens early
        f = f_end + (f_start - f_end) * ((1.0 - frac) ** sweep_pow)
        if vibrato:
            f *= 1.0 + vibrato * math.sin(2*math.pi*55.0*t)
        phase += 2*math.pi*f / SR
        tone = (math.sin(phase)
                + 0.35*math.sin(2*phase)
                + 0.18*math.sin(3*phase))
        # head transient: filtered-ish noise (averaged) that dies fast
        nz = (random.uniform(-1, 1) + random.uniform(-1, 1)) * 0.5
        nz *= noise_amt * math.exp(-t * noise_k)
        s = (tone * 0.7 + nz)
        # soft clip for a little energy-weapon grit
        s = math.tanh(s * drive)
        out[i] = s * env_attack_decay(t, dur)
    # normalize to -1.5 dBFS
    peak = max(1e-6, max(abs(x) for x in out))
    g = (10 ** (-1.5/20)) / peak
    return [x * g for x in out]

HERE = os.path.dirname(os.path.abspath(__file__))

# v1: classic short pew — high zap dropping fast, bright
write_wav(os.path.join(HERE, "laser_zap_v1.wav"),
          synth(dur=0.32, f_start=2200, f_end=420, noise_amt=0.6, sweep_pow=2.4, drive=1.6))

# v2: heavier charged blast — lower, longer, more grit/vibrato
write_wav(os.path.join(HERE, "laser_zap_v2.wav"),
          synth(dur=0.45, f_start=1500, f_end=180, noise_amt=0.8, sweep_pow=2.0, vibrato=0.03, drive=2.2))

# v3: tight snappy zap — very short, crisp, less noise (good for rapid fire)
write_wav(os.path.join(HERE, "laser_zap_v3.wav"),
          synth(dur=0.22, f_start=2600, f_end=600, noise_amt=0.4, sweep_pow=2.8, drive=1.3))
