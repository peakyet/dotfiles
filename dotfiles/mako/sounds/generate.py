#!/usr/bin/env python3
"""Render macOS/iPhone-flavoured notification chimes for mako.

Everything here is synthesised from scratch with the standard library only
(``wave`` + ``math`` + ``struct``), so the chimes can be regenerated on any
machine with no audio tooling and no third-party samples. Output is 16-bit
mono WAV; ffmpeg, when present, transcodes the result to Ogg Vorbis so the
repository only carries small files.

    python3 generate.py              # write <name>.oga (and .wav on --keep-wav)

The chimes are *inspired by* the Apple alert tones rather than copies of
them -- those are Apple's copyrighted recordings and are deliberately not
redistributed here. What is reproduced is the feel, and mostly by way of
organic struck/plucked timbres (marimba, kalimba, plucked string) rather
than the bright FM bells of the macOS set.
"""

import math
import os
import random
import shutil
import struct
import subprocess
import sys
import wave

SR = 44100  # sample rate, Hz
PEAK = 0.95  # ceiling, about -0.4 dBFS
RMS_TARGET = 0.16  # about -16 dBFS


# ── synthesis primitives ────────────────────────────────────────────


def add_tone(buf, freq, start, amp, decay, partials=((1.0, 1.0),),
             attack=0.0015, glide=1.0):
    """Mix one struck tone into ``buf``, in place.

    ``partials`` is a sequence of ``(frequency ratio, amplitude)`` pairs --
    a marimba bar, for instance, rings at roughly 1 : 4 : 10, while a
    kalimba tine is closer to 1 : 6.3 : 17.5. ``glide`` is the ratio the
    pitch bends towards; values under 1.0 give a falling blip, over 1.0 a
    rising one. Each partial decays exponentially and gets a short
    raised-cosine attack so the onset does not click.
    """
    i0 = int(start * SR)
    atk = max(int(attack * SR), 1)
    for ratio, pamp in partials:
        f0 = freq * ratio
        if f0 >= SR / 2:  # above Nyquist, would alias
            continue
        f1 = f0 * glide
        phase = 0.0
        for i in range(int(decay * 7 * SR)):  # ~7 time constants
            j = i0 + i
            if j >= len(buf):
                break
            t = i / SR
            # pitch settles quickly, amplitude decays slowly
            f = f1 + (f0 - f1) * math.exp(-t / (decay * 0.25))
            phase += 2 * math.pi * f / SR
            e = math.exp(-t / decay)
            if i < atk:
                e *= 0.5 - 0.5 * math.cos(math.pi * i / atk)
            buf[j] += amp * pamp * math.sin(phase) * e


def add_click(buf, start, amp, decay, seed=1):
    """Add the mallet/beater transient: a short low-passed noise thud."""
    i0 = int(start * SR)
    rng = random.Random(seed)
    prev = 0.0
    for i in range(int(decay * 5 * SR)):
        j = i0 + i
        if j >= len(buf):
            break
        t = i / SR
        prev = prev * 0.55 + (rng.random() * 2 - 1) * 0.45  # one-pole LP
        buf[j] += amp * prev * math.exp(-t / decay)


def add_plucked_string(buf, freq, start, amp, decay, harmonics=14, tilt=1.2,
                       seed=1):
    """A plucked string built from an explicit harmonic stack.

    Harmonic k is given amplitude 1/k**tilt -- the spectrum of a plucked
    string, and what makes it read as a string rather than a bell -- and
    decays faster than the one below it, so the tone darkens as it rings.
    Karplus-Strong would be the cheaper way to get here, but its one-period
    noise excitation has a ragged spectrum that can leave an arbitrary
    harmonic louder than the fundamental; stating the harmonics outright
    removes that lottery. A short noise transient stands in for the plectrum.
    """
    i0 = int(start * SR)
    atk = max(int(0.002 * SR), 1)
    for k in range(1, harmonics + 1):
        f = freq * k
        if f >= SR / 2:  # above Nyquist, would alias
            break
        d = decay / (1 + 0.55 * (k - 1))
        phase = 0.0
        for i in range(int(d * 7 * SR)):
            j = i0 + i
            if j >= len(buf):
                break
            t = i / SR
            e = math.exp(-t / d)
            if i < atk:
                e *= 0.5 - 0.5 * math.cos(math.pi * i / atk)
            buf[j] += amp * (k ** -tilt) * math.sin(phase) * e
            phase += 2 * math.pi * f / SR
    add_click(buf, start, amp * 0.10, 0.006, seed=seed)


# ── the chimes ──────────────────────────────────────────────────────


def render_tri_tone():
    """iPhone alert: an ascending 1-5-8 marimba arpeggio, D5 - A5 - D6.

    The original is Kelly Jacklin's ``158-marimba``, written for SoundJam MP
    in 1998 and inherited by iTunes and then the iPhone. Those digits are
    scale degrees: root, fifth, octave -- not the root-third-fifth that the
    "tri-tone" name suggests (Jacklin has called the name musically wrong).
    """
    buf = [0.0] * int(1.35 * SR)
    for freq, start in ((587.33, 0.00), (880.00, 0.10), (1174.66, 0.21)):
        add_tone(buf, freq, start, 0.85, 0.42,
                 partials=((1.0, 1.0), (4.0, 0.34), (10.0, 0.09)))
        add_click(buf, start, 0.055, 0.012, seed=int(freq))
    return buf


def render_kalimba():
    """Same 1-5-8 melody as the tri-tone, but on thumb piano.

    Softer and rounder than the marimba: a strong fundamental with only
    faint, fast-decaying upper partials, which is how a plucked metal tine
    rings. Slower spacing makes it read as a phrase rather than a ping.
    """
    buf = [0.0] * int(1.55 * SR)
    for freq, start in ((587.33, 0.00), (880.00, 0.13), (1174.66, 0.26)):
        add_tone(buf, freq, start, 0.85, 0.50, attack=0.004,
                 partials=((1.0, 1.0), (6.27, 0.13), (17.5, 0.035)))
    return buf


def render_pluck():
    """A harp-like rising arpeggio: a plucked string on each note."""
    buf = [0.0] * int(1.70 * SR)
    for freq, start in ((587.33, 0.00), (880.00, 0.075),
                        (1174.66, 0.150), (1479.98, 0.225)):
        add_plucked_string(buf, freq, start, 0.55, 0.80, seed=int(freq))
    return buf


def render_aurora():
    """A soft two-note swell, D5 - A5: warm, slow, unhurried.

    Deliberately the least attention-grabbing of the set -- a gentle rise
    for completions you do not want to be pulled back to the desk by.
    """
    buf = [0.0] * int(1.90 * SR)
    for freq, start in ((587.33, 0.00), (880.00, 0.17)):
        add_tone(buf, freq, start, 0.80, 0.95, attack=0.018,
                 partials=((1.0, 1.0), (2.0, 0.22), (3.0, 0.07)))
    return buf


def render_wood():
    """A dry wooden double-knock, low and short: percussive but not harsh."""
    buf = [0.0] * int(0.60 * SR)
    for freq, start in ((220.00, 0.00), (293.66, 0.095)):
        add_tone(buf, freq, start, 0.90, 0.055,
                 partials=((1.0, 1.0), (4.0, 0.55), (10.0, 0.18)))
        add_click(buf, start, 0.12, 0.008, seed=int(freq))
    return buf


def render_drip():
    """A short rising droplet blip -- the upward counterpart to macOS Ping."""
    buf = [0.0] * int(0.45 * SR)
    add_tone(buf, 620.0, 0.0, 0.90, 0.10, glide=2.10,
             partials=((1.0, 1.0), (2.0, 0.10)))
    return buf


CHIMES = {
    "tri-tone": render_tri_tone,
    "kalimba": render_kalimba,
    "pluck": render_pluck,
    "aurora": render_aurora,
    "wood": render_wood,
    "drip": render_drip,
}


# ── output ──────────────────────────────────────────────────────────


def normalize(buf, fade=0.006):
    """Scale to a common loudness, fade the tail, and never clip.

    Aiming at RMS alone would let a spiky chime clip, and aiming at peak
    alone leaves a percussive one much quieter to the ear than a sustained
    one. Taking whichever scale is smaller keeps every chime under the
    ceiling while pulling the set towards a common perceived level.
    """
    peak = max(abs(v) for v in buf) or 1.0
    rms = math.sqrt(sum(v * v for v in buf) / len(buf)) or 1.0
    scale = min(PEAK / peak, RMS_TARGET / rms)
    n = len(buf)
    f = max(int(fade * SR), 1)
    out = []
    for i, v in enumerate(buf):
        v *= scale
        if i >= n - f:
            v *= 0.5 - 0.5 * math.cos(math.pi * (n - i) / f)
        out.append(max(-1.0, min(1.0, v)))
    return out


def write_wav(path, buf):
    frames = b"".join(struct.pack("<h", int(v * 32767)) for v in buf)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(frames)


def main():
    outdir = os.path.dirname(os.path.abspath(__file__))
    keep_wav = "--keep-wav" in sys.argv
    ffmpeg = shutil.which("ffmpeg")
    for name, render in CHIMES.items():
        wav = os.path.join(outdir, name + ".wav")
        write_wav(wav, normalize(render()))
        if not ffmpeg:
            print("wrote", wav, "(no ffmpeg: left as WAV)")
            continue
        oga = os.path.join(outdir, name + ".oga")
        subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-i", wav,
                        "-c:a", "libvorbis", "-q:a", "7", oga], check=True)
        if not keep_wav:
            os.remove(wav)
        print("wrote", oga)


if __name__ == "__main__":
    main()
