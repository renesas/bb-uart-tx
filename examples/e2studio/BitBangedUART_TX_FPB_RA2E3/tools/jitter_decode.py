#!/usr/bin/env python3
"""
Bit-Banged UART TX -- logic analyzer capture decoder and jitter analyzer.

REWRITTEN to match the corrected, three-part jitter specification derived
from RA2E3_Bit_Banged_UART_TX_Jitter_Specification.txt (see PhasePlan.md
for the full derivation and re-analysis this rewrite is based on). The
earlier version of this script treated every adjacent bit-period pair
identically, including the pair spanning from one byte's stop bit into
the next byte's start bit -- which is NOT correct: a UART receiver
re-synchronizes its timing reference at every start-bit falling edge, so
the "stop bit -> next start bit" gap is not something that needs to match
a nominal bit period at all; it only needs to be at least one bit period
long. Lumping it in with genuine intra-character jitter produced years'
worth of "failing" jitter numbers driven entirely by the stop bit
running LONGER than nominal -- which is not a defect, it's the preferred
direction ("preferably >= 1.0 * Tbit" per the spec doc).

This script now reports three separate things, matching the spec doc's
own structure:

  1. AVERAGE BAUD-RATE ERROR -- byte-start to next-byte-start, across a
     whole burst. Target: <= +/-1% (recommended), <= +/-0.5% (preferred).

  2. INTRA-CHARACTER EDGE JITTER -- every adjacent bit-period pair
     WITHIN one byte (start bit through the transition into the stop
     bit; i.e. positions 0..9, 9 pairs per byte). This is what should be
     held to a tight jitter-vs-nominal target. Default target here is
     <=10% (this project's own chosen design target -- more lenient than
     the spec doc's suggested <=1-2%, a deliberate choice for a
     bit-banged rather than hardware-peripheral implementation), reported
     alongside <=2% and <=1% counts for context.

  3. STOP-BIT DURATION -- a MINIMUM-duration check, not a jitter-vs-
     nominal check. Tstop >= Tbit * (1 - allowed_error). Default
     allowed_error = 1% (i.e. Tstop >= 0.99*Tbit), matching the spec
     doc's example. A stop bit that runs LONGER than nominal is not a
     violation; only a stop bit that runs SHORT is.

TX-request-to-start-bit latency (a fourth, separate, application-specific
metric per the spec doc) is not measured here -- it requires knowing when
the software Write() call happened, which is not recoverable from a pure
electrical capture of the TX pin alone.

Usage: python3 jitter_decode.py <capture.csv>

Expects a 2-column CSV: "Time [s],Channel N" header, then rows of
ISO-8601 timestamps (fractional seconds up to nanosecond precision) and
a 0/1 level, one row per signal transition (not a fixed-rate sample
capture).
"""
import re, sys, statistics

NOMINAL_BAUD = 115200
NOMINAL_PERIOD_NS = 1_000_000_000.0 / NOMINAL_BAUD
BITS_PER_FRAME = 10  # start + 8 data (LSB first) + stop
IDLE_GAP_THRESHOLD_PERIODS = 3.0

# ---- Spec thresholds (see file-level docstring) ----
INTRA_CHARACTER_JITTER_TARGET_PCT = 10.0   # this project's chosen design target
STOP_BIT_ALLOWED_ERROR_PCT = 1.0           # Tstop >= Tbit * (1 - 1%) = 0.99*Tbit
BAUD_ERROR_RECOMMENDED_PCT = 1.0
BAUD_ERROR_PREFERRED_PCT = 0.5


def parse_ts_to_ns(ts):
    """Manual regex parse to preserve full (sub-microsecond) precision --
    Python's datetime.fromisoformat() truncates to microseconds, which
    matters at ~8.68us bit periods."""
    m = re.match(r'(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})\.(\d+)([+-]\d{2}:\d{2})', ts)
    assert m, f"bad timestamp: {ts}"
    yyyy, mm, dd, hh, mi, ss, frac, tz = m.groups()
    frac_ns = frac.ljust(9, '0')[:9]
    day_seconds = int(hh) * 3600 + int(mi) * 60 + int(ss)
    return day_seconds * 1_000_000_000 + int(frac_ns)


def load_segments(fname):
    rows = []
    with open(fname) as f:
        f.readline()  # header
        for line in f:
            line = line.strip()
            if not line:
                continue
            ts, level = line.split(',')
            rows.append((parse_ts_to_ns(ts), int(level)))
    t0 = rows[0][0]
    edges = [(r[0] - t0, r[1]) for r in rows]
    if len(edges) >= 2 and edges[-1][1] == edges[-2][1]:
        edges = edges[:-1]  # drop trailing non-transition end-of-capture marker
    segments = []
    for i in range(len(edges)):
        t_start, level = edges[i]
        t_end = edges[i + 1][0] if i + 1 < len(edges) else None
        segments.append([t_start, t_end, level])
    return segments


def level_at(segments, t_ns):
    lo, hi = 0, len(segments) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        s, e, l = segments[mid]
        if t_ns < s:
            hi = mid - 1
        elif e is not None and t_ns >= e:
            lo = mid + 1
        else:
            return l, mid
    raise ValueError(f"time {t_ns} not found in any segment")


def decode_frames(segments):
    """Anchor the nominal sampling grid fresh at each byte's own start-bit
    edge (not one grid for the whole capture) to bound baud-error drift to
    one byte's worth."""
    frames = []
    search_from_ns = 0
    while True:
        found = None
        for idx in range(len(segments)):
            s, e, l = segments[idx]
            if s < search_from_ns:
                continue
            if l == 0 and (idx == 0 or segments[idx - 1][2] == 1):
                found = idx
                break
        if found is None:
            break
        frame_start = segments[found][0]
        bits = []
        for k in range(BITS_PER_FRAME):
            sample_t = frame_start + (k + 0.5) * NOMINAL_PERIOD_NS
            lvl, _ = level_at(segments, sample_t)
            bits.append(lvl)
        ok = (bits[0] == 0 and bits[9] == 1)
        byte_val = 0
        for b in range(8):
            byte_val |= (bits[1 + b] << b)
        frames.append({'start_ns': frame_start, 'bits': bits, 'byte': byte_val, 'ok': ok})
        search_from_ns = frame_start + (BITS_PER_FRAME - 0.5) * NOMINAL_PERIOD_NS
    return frames


def find_message_boundaries(frames):
    boundaries = set()
    for idx in range(1, len(frames)):
        gap = frames[idx]['start_ns'] - (frames[idx - 1]['start_ns'] + BITS_PER_FRAME * NOMINAL_PERIOD_NS)
        if gap > IDLE_GAP_THRESHOLD_PERIODS * NOMINAL_PERIOD_NS:
            boundaries.add(idx)
    return boundaries


def build_intra_frame_boundaries(segments, frame):
    """Bit-position boundary timestamps (0..9) WITHIN one frame only,
    clipped to that frame's own nominal 10-period window. Real edge
    timestamp where one exists; even subdivision of the run's real
    measured span for interior (non-edge) positions within a multi-bit
    run -- see PhasePlan.md for the rationale (unchanged from the
    previous version of this script)."""
    bits = frame['bits']
    frame_start = frame['start_ns']
    frame_end_bound = frame_start + BITS_PER_FRAME * NOMINAL_PERIOD_NS
    times = [None] * BITS_PER_FRAME
    k = 0
    while k < BITS_PER_FRAME:
        run_val = bits[k]
        run_start_k = k
        while k < BITS_PER_FRAME and bits[k] == run_val:
            k += 1
        run_end_k = k
        run_len = run_end_k - run_start_k
        sample_t = frame_start + (run_start_k + 0.5) * NOMINAL_PERIOD_NS
        lvl, seg_idx = level_at(segments, sample_t)
        seg_start, seg_end, seg_lvl = segments[seg_idx]
        real_run_end = seg_end if seg_end is not None else frame_end_bound
        real_run_start_clipped = max(seg_start, frame_start)
        real_run_end_clipped = min(real_run_end, frame_end_bound)
        run_duration = real_run_end_clipped - real_run_start_clipped
        per_bit = run_duration / run_len
        for j in range(run_len):
            pos = run_start_k + j
            times[pos] = real_run_start_clipped + j * per_bit
    return times


def measure_stop_bit_duration(segments, frame):
    """The REAL, unclipped duration the line stays high starting from this
    frame's stop bit (position 9), out to whatever the next real edge is
    -- the next byte's start bit if back-to-back, or a long idle gap
    before the next message if not. Either way, a longer duration is
    fine; this function is only used for a MINIMUM-duration check."""
    times = build_intra_frame_boundaries(segments, frame)
    pos9_t = times[9]
    _, seg_idx = level_at(segments, pos9_t + NOMINAL_PERIOD_NS * 0.1)
    seg_start, seg_end, seg_lvl = segments[seg_idx]
    if seg_end is None:
        return None  # capture ended before the stop bit's real end was observed
    return seg_end - pos9_t


def stats_line(lst, label, unit_ns=False, pct=False):
    if not lst:
        print(f"{label}: (no data)")
        return
    if pct:
        print(f"{label}: n={len(lst)}  max={max(lst):.2f}%  mean={statistics.mean(lst):.2f}%  "
              f"stdev={statistics.pstdev(lst):.2f}%")
    elif unit_ns:
        print(f"{label}: n={len(lst)}  max={max(lst):.0f}ns  mean={statistics.mean(lst):.0f}ns  "
              f"min={min(lst):.0f}ns")
    else:
        print(f"{label}: n={len(lst)}  {lst}")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    fname = sys.argv[1]

    segments = load_segments(fname)
    frames = decode_frames(segments)
    bad = [f for f in frames if not f['ok']]

    print(f"Decoded {len(frames)} frames. Invalid start/stop patterns: {len(bad)}")
    text = bytes(f['byte'] for f in frames)
    print("---- Decoded content (verify this looks sensible before trusting timing stats!) ----")
    print(repr(text))
    if bad:
        print("WARNING: invalid frames present -- do not trust timing stats until resolved.")
        for f in bad[:5]:
            print("  BAD:", f)
        return

    message_boundaries = find_message_boundaries(frames)
    print(f"\nDetected {len(message_boundaries) + 1} separate messages "
          f"(idle gaps before frame indices: {sorted(message_boundaries)})")

    # ==== 1. Average baud-rate error ====
    byte_starts = [fr['start_ns'] for fr in frames]
    eff_baud_errs = []
    for idx in range(1, len(byte_starts)):
        if idx in message_boundaries:
            continue
        dt = byte_starts[idx] - byte_starts[idx - 1]
        nominal_dt = BITS_PER_FRAME * NOMINAL_PERIOD_NS
        eff_baud_errs.append((dt - nominal_dt) / nominal_dt * 100.0)

    print("\n==== 1. AVERAGE BAUD-RATE ERROR ====")
    print(f"(target: <= +/-{BAUD_ERROR_RECOMMENDED_PCT}%, preferred: <= +/-{BAUD_ERROR_PREFERRED_PCT}%)")
    if eff_baud_errs:
        mean_err = statistics.mean(eff_baud_errs)
        stdev_err = statistics.pstdev(eff_baud_errs)
        status = "PASS (preferred)" if abs(mean_err) <= BAUD_ERROR_PREFERRED_PCT else \
                 "PASS (recommended)" if abs(mean_err) <= BAUD_ERROR_RECOMMENDED_PCT else "FAIL"
        print(f"n={len(eff_baud_errs)}  mean={mean_err:+.3f}%  stdev={stdev_err:.3f}%  -> {status}")

    # ==== 2. Intra-character edge jitter ====
    intra_jitter = []
    for fr in frames:
        times = build_intra_frame_boundaries(segments, fr)
        for n in range(len(times) - 1):
            T = times[n + 1] - times[n]
            intra_jitter.append(abs(T - NOMINAL_PERIOD_NS) / NOMINAL_PERIOD_NS * 100.0)

    print(f"\n==== 2. INTRA-CHARACTER EDGE JITTER ====")
    print(f"(start bit through the transition into the stop bit; excludes the")
    print(f" stop-bit-to-next-start-bit gap entirely -- see file docstring for why)")
    print(f"(design target: <= {INTRA_CHARACTER_JITTER_TARGET_PCT}%, shown alongside <=2%/<=1% for context)")
    if intra_jitter:
        viol_target = sum(1 for x in intra_jitter if x > INTRA_CHARACTER_JITTER_TARGET_PCT)
        viol_2 = sum(1 for x in intra_jitter if x > 2.0)
        viol_1 = sum(1 for x in intra_jitter if x > 1.0)
        status = "PASS" if viol_target == 0 else "FAIL"
        print(f"n={len(intra_jitter)}  max={max(intra_jitter):.2f}%  mean={statistics.mean(intra_jitter):.2f}%  "
              f"stdev={statistics.pstdev(intra_jitter):.2f}%")
        print(f"  violations > {INTRA_CHARACTER_JITTER_TARGET_PCT}% (design target): "
              f"{viol_target}/{len(intra_jitter)} ({100.0*viol_target/len(intra_jitter):.1f}%)  -> {status}")
        print(f"  violations > 2% (context only): {viol_2}/{len(intra_jitter)} ({100.0*viol_2/len(intra_jitter):.1f}%)")
        print(f"  violations > 1% (context only): {viol_1}/{len(intra_jitter)} ({100.0*viol_1/len(intra_jitter):.1f}%)")

    # ==== 3. Stop-bit duration (minimum, not jitter) ====
    stop_min_threshold = NOMINAL_PERIOD_NS * (1.0 - STOP_BIT_ALLOWED_ERROR_PCT / 100.0)
    stop_durations = []
    for fr in frames:
        d = measure_stop_bit_duration(segments, fr)
        if d is not None:
            stop_durations.append(d)

    print(f"\n==== 3. STOP-BIT DURATION (minimum-duration check, NOT jitter-vs-nominal) ====")
    print(f"(requirement: Tstop >= Tbit * (1 - {STOP_BIT_ALLOWED_ERROR_PCT}%) = "
          f"{stop_min_threshold:.1f}ns; a LONGER stop bit is fine/preferred, only")
    print(f" a SHORTER one is a violation)")
    if stop_durations:
        below = sum(1 for d in stop_durations if d < stop_min_threshold)
        status = "PASS" if below == 0 else "FAIL"
        min_pct = min(stop_durations) / NOMINAL_PERIOD_NS * 100.0
        print(f"n={len(stop_durations)}  min={min(stop_durations):.0f}ns ({min_pct:.1f}% of Tbit)")
        print(f"  violations (stop bit too SHORT): {below}/{len(stop_durations)}  -> {status}")

    print("\nNote: TX-request-to-start-bit latency is a separate, application-specific")
    print("metric per the jitter specification and is not measured by this script --")
    print("it requires knowing when the software Write() call happened, which this")
    print("purely-electrical capture of the TX pin does not record.")


if __name__ == "__main__":
    main()
