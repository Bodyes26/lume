#!/usr/bin/env python3
"""Generate src/games/pack/SudokuPack.h — the three sudoku tiers Lume ships in flash.

The device cannot generate sudoku: a single uniqueness proof costs more RAM and
more seconds than an ESP32-C3 has to spare while an e-ink panel waits, and a
board that is merely "probably unique" would let src/games/Sudoku.h's solved()
(complete + consistent == solved, no answer key stored) accept a wrong grid. So
every board is generated, proved unique and CLASSIFIED here, and flash carries
41 bytes per puzzle.

Pipeline per puzzle, deterministic from (--seed, tier, ordinal, attempt) so a
re-run reproduces the header byte for byte:
  1. complete grid by randomized backtracking;
  2. dig cells in a random order, keeping exactly one solution — checked by
     asking, for every OTHER digit still legal in the dug cell, whether a
     solution exists (a second solution can only differ there), which fails
     fast where counting all solutions from scratch would not;
  3. classify with a human-technique solver and keep the puzzle only if it
     lands in the tier being generated. A puzzle that misses its tier is
     DISCARDED, never relabelled — "facile" has to mean singles-only or the
     difficulty picker is a lie.
     Easy   naked singles + hidden singles, nothing else
     Medium also needs locked candidates (pointing/claiming) and/or naked pairs
     Hard   unique, but beyond all of the above
  4. reject anything outside the tier's given-count band, so the grids also
     LOOK like their tier on a 9x9 of 44 px cells.

Easy and Medium dig under their technique ceiling (a removal that would push the
puzzle past the tier is undone), which is why they hit their tier nearly every
attempt; Hard digs to exhaustion under uniqueness alone and is then required to
be beyond Medium's techniques.

Self-check (mandatory, runs before anything is written): every emitted puzzle is
decoded back from its 41 bytes and re-verified independently — consistent
givens, exactly one solution, the claimed tier, the tier's given band, and
distinct from every other entry. Any failure exits non-zero and writes nothing.

Regenerate from the xphone-os directory with:
  python3 tools/lume-games/gen_sudoku_pack.py --out src/games/pack/SudokuPack.h

The emitted text depends only on --easy/--medium/--hard/--seed (never on the
output path, never on a timestamp): the SHA-256 printed at the end is the
reproducibility check.
"""

import argparse
import hashlib
import multiprocessing
import os
import random
import sys
import time

SIDE = 9
CELLS = 81
PACK_BYTES = 41  # one nibble per cell, even cell in the high nibble
FULL = 0x1FF  # candidate mask for digits 1..9, digit d = bit d-1

BIT = [0] + [1 << (d - 1) for d in range(1, 10)]
POP = bytes(bin(m).count("1") for m in range(FULL + 1))
DIGIT = {1 << (d - 1): d for d in range(1, 10)}

ROW = [c // SIDE for c in range(CELLS)]
COL = [c % SIDE for c in range(CELLS)]
BOX = [(c // 27) * 3 + (c % SIDE) // 3 for c in range(CELLS)]

ROW_UNITS = [[r * SIDE + c for c in range(SIDE)] for r in range(SIDE)]
COL_UNITS = [[r * SIDE + c for r in range(SIDE)] for c in range(SIDE)]
BOX_UNITS = [
    [(b // 3 * 3 + dr) * SIDE + (b % 3 * 3 + dc) for dr in range(3) for dc in range(3)]
    for b in range(SIDE)
]
UNITS = ROW_UNITS + COL_UNITS + BOX_UNITS
PEERS = [
    tuple(
        p
        for p in set(ROW_UNITS[ROW[c]]) | set(COL_UNITS[COL[c]]) | set(BOX_UNITS[BOX[c]])
        if p != c
    )
    for c in range(CELLS)
]

CONTRA, SOLVED, STUCK = 0, 1, 2

# (technique ceiling while digging, required final level, given band). The
# ceiling is None for Hard: nothing to hold back, it digs as deep as uniqueness
# allows and must then be unsolvable by the Medium techniques.
TIERS = {
    "easy": (1, 1, (34, 42)),
    "medium": (2, 2, (28, 34)),
    "hard": (None, 3, (22, 28)),
}
TIER_ORDER = ("easy", "medium", "hard")
ARRAY_NAME = {"easy": "kEasy", "medium": "kMedium", "hard": "kHard"}
COUNT_NAME = {"easy": "kEasyCount", "medium": "kMediumCount", "hard": "kHardCount"}


# ---------------------------------------------------------------- brute force


def count_solutions(grid, limit=2):
    """Solutions of `grid` (list of 81 ints, 0 = empty), counted up to `limit`.

    Bitmask backtracking with minimum-remaining-values selection: the search
    always branches on the tightest cell, which is what keeps a 22-given board
    at a few thousand nodes instead of a few million.
    """
    rm = [0] * SIDE
    cm = [0] * SIDE
    bm = [0] * SIDE
    empties = set()
    for c in range(CELLS):
        v = grid[c]
        if v:
            b = BIT[v]
            rm[ROW[c]] |= b
            cm[COL[c]] |= b
            bm[BOX[c]] |= b
        else:
            empties.add(c)
    count = 0

    def rec():
        nonlocal count
        if not empties:
            count += 1
            return count >= limit
        best = -1
        best_cand = 0
        best_n = 10
        for c in empties:
            cand = FULL & ~(rm[ROW[c]] | cm[COL[c]] | bm[BOX[c]])
            n = POP[cand]
            if n < best_n:
                if n == 0:
                    return False
                best, best_cand, best_n = c, cand, n
                if n == 1:
                    break
        empties.discard(best)
        r, co, bo = ROW[best], COL[best], BOX[best]
        cand = best_cand
        while cand:
            b = cand & -cand
            cand ^= b
            rm[r] |= b
            cm[co] |= b
            bm[bo] |= b
            stop = rec()
            rm[r] ^= b
            cm[co] ^= b
            bm[bo] ^= b
            if stop:
                empties.add(best)
                return True
        empties.add(best)
        return False

    rec()
    return count


def random_solution(rng):
    """A complete valid grid, uniform enough: randomized value order per cell."""
    grid = [0] * CELLS
    rm = [0] * SIDE
    cm = [0] * SIDE
    bm = [0] * SIDE
    order = [list(range(1, 10)) for _ in range(CELLS)]
    for values in order:
        rng.shuffle(values)

    def rec(c):
        if c == CELLS:
            return True
        r, co, bo = ROW[c], COL[c], BOX[c]
        used = rm[r] | cm[co] | bm[bo]
        for v in order[c]:
            b = BIT[v]
            if used & b:
                continue
            grid[c] = v
            rm[r] |= b
            cm[co] |= b
            bm[bo] |= b
            if rec(c + 1):
                return True
            rm[r] ^= b
            cm[co] ^= b
            bm[bo] ^= b
        grid[c] = 0
        return False

    if not rec(0):  # unreachable: an empty grid always completes
        raise SystemExit("internal error: could not build a complete grid")
    return grid


# ------------------------------------------------------- human-technique solver


def _initial(grid):
    """(val, cand) with cand[c] = digits not already taken by c's peers."""
    val = list(grid)
    cand = [0] * CELLS
    for c in range(CELLS):
        if val[c]:
            continue
        used = 0
        for p in PEERS[c]:
            used |= BIT[val[p]]
        cand[c] = FULL & ~used
    return val, cand


def _place(val, cand, c, d):
    val[c] = d
    cand[c] = 0
    keep = ~BIT[d]
    for p in PEERS[c]:
        if val[p]:
            continue
        cand[p] &= keep
        if cand[p] == 0:
            return False
    return True


def _singles(val, cand):
    """Naked + hidden singles to exhaustion. CONTRA / SOLVED / STUCK."""
    moved = True
    while moved:
        moved = False
        for c in range(CELLS):
            if val[c]:
                continue
            m = cand[c]
            if m == 0:
                return CONTRA
            if POP[m] == 1:
                if not _place(val, cand, c, DIGIT[m]):
                    return CONTRA
                moved = True
        for unit in UNITS:
            for d in range(1, 10):
                b = BIT[d]
                spot = -1
                n = 0
                for c in unit:
                    if val[c] == d:
                        n = -1
                        break
                    if cand[c] & b:
                        n += 1
                        if n > 1:
                            break
                        spot = c
                if n == -1:
                    continue
                if n == 0:
                    return CONTRA
                if n == 1:
                    if not _place(val, cand, spot, d):
                        return CONTRA
                    moved = True
    return SOLVED if all(val) else STUCK


def _locked_candidates(val, cand):
    """Pointing (box -> line) and claiming (line -> box). True if it eliminated."""
    hit = False
    for b in range(SIDE):
        box = BOX_UNITS[b]
        for d in range(1, 10):
            bit = BIT[d]
            spots = [c for c in box if cand[c] & bit]
            if not spots or any(val[c] == d for c in box):
                continue
            rows = {ROW[c] for c in spots}
            cols = {COL[c] for c in spots}
            line = None
            if len(rows) == 1:
                line = ROW_UNITS[rows.pop()]
            elif len(cols) == 1:
                line = COL_UNITS[cols.pop()]
            if line is None:
                continue
            for c in line:
                if BOX[c] != b and cand[c] & bit:
                    cand[c] &= ~bit
                    hit = True
    for line in ROW_UNITS + COL_UNITS:
        for d in range(1, 10):
            bit = BIT[d]
            spots = [c for c in line if cand[c] & bit]
            if not spots or any(val[c] == d for c in line):
                continue
            boxes = {BOX[c] for c in spots}
            if len(boxes) != 1:
                continue
            b = boxes.pop()
            for c in BOX_UNITS[b]:
                if c not in spots and cand[c] & bit:
                    cand[c] &= ~bit
                    hit = True
    return hit


def _naked_pairs(val, cand):
    hit = False
    for unit in UNITS:
        pairs = {}
        for c in unit:
            m = cand[c]
            if m and POP[m] == 2:
                pairs.setdefault(m, []).append(c)
        for m, cells in pairs.items():
            if len(cells) != 2:
                continue
            keep = ~m
            for c in unit:
                if c not in cells and cand[c] & m:
                    cand[c] &= keep
                    hit = True
    return hit


def solvable_within(grid, level):
    """True when singles (level 1), or singles + locked candidates + naked pairs
    (level 2), finish `grid` on their own."""
    val, cand = _initial(grid)
    while True:
        state = _singles(val, cand)
        if state == SOLVED:
            return True
        if state == CONTRA:
            return False
        if level < 2:
            return False
        if not (_locked_candidates(val, cand) or _naked_pairs(val, cand)):
            return False


def human_level(grid):
    """1 singles only, 2 needs locked candidates / naked pairs, 3 beyond both."""
    if solvable_within(grid, 1):
        return 1
    if solvable_within(grid, 2):
        return 2
    return 3


# ------------------------------------------------------------------- digging


def _still_unique(puz, cell, removed):
    """`puz` has just had `cell` emptied. Any second solution must put a digit
    other than `removed` there, so only those need testing — and each test is a
    one-solution search that usually dies in the first few nodes."""
    used = 0
    for p in PEERS[cell]:
        used |= BIT[puz[p]]
    alt = FULL & ~used & ~BIT[removed]
    while alt:
        b = alt & -alt
        alt ^= b
        puz[cell] = DIGIT[b]
        found = count_solutions(puz, limit=1)
        puz[cell] = 0
        if found:
            return False
    return True


def dig(rng, solution, tier):
    """One digging attempt. (puzzle, givens) or None when it missed the tier."""
    ceiling, want_level, (lo, hi) = TIERS[tier]
    # Depth this attempt is WILLING to stop at, drawn inside the band: a Medium
    # that only ever appears at 28 givens is a tier nobody can tell from Hard,
    # and digging to exhaustion under a technique ceiling always bottoms out at
    # the band floor. Hard has no ceiling to hold it back, so it digs as deep as
    # uniqueness allows and stops at the floor.
    target = rng.randint(lo, hi) if ceiling is not None else lo
    puz = list(solution)
    givens = CELLS
    order = list(range(CELLS))
    rng.shuffle(order)
    # Removing a given never makes a puzzle easier, so "tier reached" is
    # monotone and worth caching. Easy is reached from the first dig: every
    # accepted removal below keeps the board singles-solvable.
    reached = want_level == 1
    for c in order:
        if givens <= lo:
            break
        if reached and givens <= target:
            break
        v = puz[c]
        puz[c] = 0
        if _still_unique(puz, c, v) and (ceiling is None or solvable_within(puz, ceiling)):
            givens -= 1
            # Nine attempts in ten used to be thrown away here: random digging
            # that merely PRESERVES uniqueness leaves a board the singles still
            # crack, i.e. an Easy wearing a Medium band. Keep digging until the
            # singles genuinely fail instead of discarding the attempt.
            if want_level == 2:
                reached = not solvable_within(puz, 1)
        else:
            puz[c] = v
    if not lo <= givens <= hi:
        return None
    if human_level(puz) != want_level:
        return None
    return puz, givens


def generate_one(job):
    """(tier, ordinal, seed) -> (tier, ordinal, packed bytes, givens, attempts).

    Seeded per puzzle rather than per run: workers finish out of order, and the
    header must not depend on which core got there first.
    """
    tier, ordinal, seed = job
    for attempt in range(4096):
        rng = random.Random("lume-sudoku|%d|%s|%d|%d" % (seed, tier, ordinal, attempt))
        got = dig(rng, random_solution(rng), tier)
        if got is not None:
            puz, givens = got
            return tier, ordinal, encode(puz), givens, attempt + 1
    raise SystemExit(f"{tier} #{ordinal}: no puzzle in 4096 attempts (tier unreachable?)")


# ------------------------------------------------------------------ encoding


def encode(puz):
    out = bytearray(PACK_BYTES)
    for i in range(CELLS):
        if i % 2 == 0:
            out[i >> 1] |= puz[i] << 4
        else:
            out[i >> 1] |= puz[i]
    return bytes(out)


def decode(blob):
    return [
        (blob[i >> 1] >> 4) if i % 2 == 0 else (blob[i >> 1] & 0xF) for i in range(CELLS)
    ]


# ---------------------------------------------------------------- self-check


def self_check(packs):
    """Re-verify every puzzle from its emitted BYTES, not from the grid that
    produced them: a packing bug would otherwise ship a valid puzzle as garbage.
    Returns the list of failure strings (empty means pass)."""
    bad = []
    seen = {}
    for tier in TIER_ORDER:
        _, want_level, (lo, hi) = TIERS[tier]
        for i, (blob, givens) in enumerate(packs[tier]):
            where = f"{tier} #{i}"
            if len(blob) != PACK_BYTES:
                bad.append(f"{where}: {len(blob)} bytes, expected {PACK_BYTES}")
                continue
            if blob in seen:
                bad.append(f"{where}: duplicate of {seen[blob]}")
                continue
            seen[blob] = where
            grid = decode(blob)
            if any(v > 9 for v in grid):
                bad.append(f"{where}: nibble out of range")
                continue
            filled = sum(1 for v in grid if v)
            if filled != givens:
                bad.append(f"{where}: {filled} givens decoded, {givens} recorded")
                continue
            if not lo <= filled <= hi:
                bad.append(f"{where}: {filled} givens outside {lo}..{hi}")
                continue
            consistent = True
            for unit in UNITS:
                digits = [grid[c] for c in unit if grid[c]]
                if len(digits) != len(set(digits)):
                    consistent = False
                    break
            if not consistent:
                bad.append(f"{where}: a digit repeats in a row, column or box")
                continue
            n = count_solutions(grid, limit=2)
            if n != 1:
                bad.append(f"{where}: {n if n < 2 else '2+'} solutions, expected 1")
                continue
            level = human_level(grid)
            if level != want_level:
                bad.append(f"{where}: technique level {level}, expected {want_level}")
    return bad


# ------------------------------------------------------------------- emission


def tier_stats(entries):
    givens = [g for _, g in entries]
    return len(entries), min(givens), sum(givens) / len(givens), max(givens)


def render(packs, args):
    lines = []
    a = lines.append
    a("/**")
    a(" * Lume sudoku pack — generated, do not hand-edit.")
    a(" * Regenerate from the xphone-os directory with:")
    a(
        " *   python3 tools/lume-games/gen_sudoku_pack.py --out src/games/pack/SudokuPack.h"
        f" --easy {args.easy} --medium {args.medium} --hard {args.hard} --seed {args.seed}"
    )
    a(f" * Seed {args.seed}. The emitted bytes depend on nothing else.")
    a(" *")
    a(" * Every entry was verified to have exactly ONE solution and was kept only")
    a(" * if the techniques a human needs match its tier:")
    a(" *   kEasy    naked singles + hidden singles, nothing else")
    a(" *   kMedium  also locked candidates (pointing/claiming) and/or naked pairs")
    a(" *   kHard    unique, but beyond those techniques")
    a(" *")
    a(" *   tier     puzzles  givens min/avg/max")
    for tier in TIER_ORDER:
        n, lo, avg, hi = tier_stats(packs[tier])
        a(f" *   {ARRAY_NAME[tier]:<9}{n:>7}  {lo}/{avg:.1f}/{hi}")
    a(" */")
    a("#pragma once")
    a("")
    a("#include <cstdint>")
    a("")
    a("namespace sudoku_pack {")
    a("")
    a(f"constexpr int kBytes = {PACK_BYTES};  // 81 cells, one nibble each, EVEN cell in the HIGH nibble, 0 = empty")
    for tier in TIER_ORDER:
        a("")
        a(f"constexpr uint8_t {ARRAY_NAME[tier]}[][kBytes] = {{")
        for i, (blob, givens) in enumerate(packs[tier]):
            cols = [f"0x{b:02X}" for b in blob]
            # 16 bytes a row keeps every line under 110 columns.
            chunks = [cols[j : j + 16] for j in range(0, len(cols), 16)]
            for k, chunk in enumerate(chunks):
                head = "    {" if k == 0 else "     "
                body = ", ".join(chunk)
                if k == len(chunks) - 1:
                    a(f"{head}{body}}},  // #{i} {givens} givens")
                else:
                    a(f"{head}{body},")
        a("};")
    a("")
    for tier in TIER_ORDER:
        a(f"constexpr uint32_t {COUNT_NAME[tier]} = sizeof({ARRAY_NAME[tier]}) / kBytes;")
    a("")
    a("}  // namespace sudoku_pack")
    a("")
    return "\n".join(lines)


# ----------------------------------------------------------------------- main


def main():
    ap = argparse.ArgumentParser(description="Generate src/games/pack/SudokuPack.h")
    ap.add_argument("--out", default="src/games/pack/SudokuPack.h")
    ap.add_argument("--easy", type=int, default=128)
    ap.add_argument("--medium", type=int, default=256)
    ap.add_argument("--hard", type=int, default=128)
    ap.add_argument("--seed", type=int, default=20260819)
    ap.add_argument(
        "--jobs",
        type=int,
        default=os.cpu_count() or 1,
        help="worker processes; every puzzle is seeded from (seed, tier, ordinal) "
        "so the result never depends on this",
    )
    args = ap.parse_args()

    counts = {"easy": args.easy, "medium": args.medium, "hard": args.hard}
    for tier, n in counts.items():
        if n < 1:
            raise SystemExit(f"--{tier} must be at least 1")

    jobs = [(tier, i, args.seed) for tier in TIER_ORDER for i in range(counts[tier])]
    t0 = time.time()
    results = {tier: {} for tier in TIER_ORDER}
    attempts = {tier: 0 for tier in TIER_ORDER}
    done = 0
    if args.jobs > 1:
        ctx = multiprocessing.get_context("fork" if sys.platform != "win32" else "spawn")
        pool = ctx.Pool(args.jobs)
        stream = pool.imap_unordered(generate_one, jobs, chunksize=1)
    else:
        pool = None
        stream = (generate_one(j) for j in jobs)
    try:
        for tier, ordinal, blob, givens, tries in stream:
            results[tier][ordinal] = (blob, givens)
            attempts[tier] += tries
            done += 1
            print(
                f"\r{done}/{len(jobs)} puzzles  {time.time() - t0:6.1f}s",
                end="",
                flush=True,
            )
    finally:
        if pool is not None:
            pool.close()
            pool.join()
    print()

    packs = {}
    for tier in TIER_ORDER:
        got = results[tier]
        if len(got) != counts[tier]:
            raise SystemExit(f"{tier}: produced {len(got)} of {counts[tier]}")
        packs[tier] = [got[i] for i in range(counts[tier])]

    bad = self_check(packs)
    if bad:
        print(f"SELF-CHECK FAILED ({len(bad)} problem(s)), nothing written:")
        for line in bad[:20]:
            print(f"  {line}")
        if len(bad) > 20:
            print(f"  ... and {len(bad) - 20} more")
        return 1
    total = sum(counts.values())
    print(f"self-check PASS: {total} puzzles decoded, unique, in tier, in band, distinct")

    text = render(packs, args)
    with open(args.out, "w") as fh:
        fh.write(text)
    data = text.encode()

    for tier in TIER_ORDER:
        n, lo, avg, hi = tier_stats(packs[tier])
        print(
            f"{tier:<7}{n:>5} puzzles  givens {lo}..{hi} (avg {avg:.1f})"
            f"  {attempts[tier] / n:.2f} attempts/puzzle"
        )
    print(f"{args.out}: {len(data)} bytes, sha256 {hashlib.sha256(data).hexdigest()}")
    print(f"wall time {time.time() - t0:.1f}s on {args.jobs} worker(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
