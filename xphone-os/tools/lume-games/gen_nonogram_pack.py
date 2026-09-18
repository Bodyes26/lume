#!/usr/bin/env python3
"""Generate verified Nonogram puzzle packs for Lume OS."""

import argparse
import hashlib
import random
import sys
from typing import List, Optional, Tuple

def line_clues(line: List[int]) -> List[int]:
    clues = []
    count = 0
    for cell in line:
        if cell == 1:
            count += 1
        elif count > 0:
            clues.append(count)
            count = 0
    if count > 0:
        clues.append(count)
    return clues

def generate_line_placements(n: int, clues: List[int], current: List[int]) -> List[List[int]]:
    if not clues:
        if any(c == 1 for c in current):
            return []
        return [[0] * n]
    
    sum_clues = sum(clues)
    min_len = sum_clues + len(clues) - 1
    if min_len > n:
        return []
    
    results = []
    
    def backtrack(clue_idx: int, pos: int, current_placement: List[int]):
        if clue_idx == len(clues):
            for i in range(pos, n):
                if current[i] == 1:
                    return
                current_placement[i] = 0
            results.append(list(current_placement))
            return
        
        clue = clues[clue_idx]
        rem_clues = clues[clue_idx + 1:]
        rem_min = sum(rem_clues) + len(rem_clues)
        max_start = n - rem_min - clue
        
        for start in range(pos, max_start + 1):
            if any(current[i] == 1 for i in range(pos, start)):
                continue
            for i in range(pos, start):
                current_placement[i] = 0
            
            if any(current[start + i] == 2 for i in range(clue)):
                continue
            for i in range(clue):
                current_placement[start + i] = 1
            
            next_pos = start + clue
            if next_pos < n:
                if current[next_pos] == 1:
                    continue
                current_placement[next_pos] = 0
                next_pos += 1
            
            backtrack(clue_idx + 1, next_pos, current_placement)
            
    backtrack(0, 0, [0] * n)
    return results

def is_line_solvable(grid: List[List[int]], side: int) -> bool:
    row_clues = [line_clues(grid[r]) for r in range(side)]
    col_clues = [line_clues([grid[r][c] for r in range(side)]) for c in range(side)]
    
    if any(len(cl) > 4 for cl in row_clues) or any(len(cl) > 4 for cl in col_clues):
        return False
    if any(len(cl) == 0 for cl in row_clues) or any(len(cl) == 0 for cl in col_clues):
        return False
    
    total_ink = sum(sum(row) for row in grid)
    ink_ratio = total_ink / (side * side)
    if not (0.30 <= ink_ratio <= 0.65):
        return False

    state = [[0] * side for _ in range(side)]
    
    changed = True
    while changed:
        changed = False
        for r in range(side):
            curr_row = state[r]
            if all(c != 0 for c in curr_row):
                continue
            placements = generate_line_placements(side, row_clues[r], curr_row)
            if not placements:
                return False
            for c in range(side):
                if state[r][c] == 0:
                    vals = {p[c] for p in placements}
                    if len(vals) == 1:
                        v = vals.pop()
                        state[r][c] = 1 if v == 1 else 2
                        changed = True

        for c in range(side):
            curr_col = [state[r][c] for r in range(side)]
            if all(v != 0 for v in curr_col):
                continue
            placements = generate_line_placements(side, col_clues[c], curr_col)
            if not placements:
                return False
            for r in range(side):
                if state[r][c] == 0:
                    vals = {p[r] for p in placements}
                    if len(vals) == 1:
                        v = vals.pop()
                        state[r][c] = 1 if v == 1 else 2
                        changed = True

    return all(all(cell != 0 for cell in row) for row in state)

AUTHORED_10 = [
    ("Heart", "Cuore", [
        "..##..##..",
        ".####.####.",
        "##########",
        "##########",
        "##########",
        ".########.",
        "..######..",
        "...####...",
        "....##....",
        ".........."
    ]),
    ("House", "Casa", [
        "....##....",
        "...####...",
        "..######..",
        ".########.",
        "##########",
        ".##....##.",
        ".##.##.##.",
        ".##.##.##.",
        ".##....##.",
        ".########."
    ]),
    ("Cup", "Tazza", [
        "..........",
        ".##..##...",
        ".##..##...",
        "...........",
        ".######.#.",
        ".######.##",
        ".######.##",
        ".######.#.",
        "..####....",
        ".######..."
    ]),
    ("Star", "Stella", [
        "....##....",
        "....##....",
        "##########",
        ".########.",
        "..######..",
        ".########.",
        ".##....##.",
        "##......##",
        "##......##",
        ".........."
    ]),
    ("Cat", "Gatto", [
        ".##....##.",
        ".###..###.",
        ".########.",
        "##########",
        "#.##..##.#",
        "##########",
        ".########.",
        "..######..",
        ".########.",
        "##########"
    ]),
    ("Tree", "Albero", [
        "....##....",
        "...####...",
        "..######..",
        "...####...",
        "..######..",
        ".########.",
        "##########",
        "....##....",
        "....##....",
        "...####..."
    ]),
    ("Fish", "Pesce", [
        "..........",
        "....#.....",
        "...###.#..",
        "..######..",
        ".#######..",
        "#########.",
        ".#######..",
        "..######..",
        "...###.#..",
        "....#....."
    ]),
    ("Boat", "Barca", [
        "....#.....",
        "....##....",
        "....###...",
        "....####..",
        "....#####.",
        "....#.....",
        "..........",
        "##########",
        ".########.",
        "..######.."
    ]),
    ("Key", "Chiave", [
        "...####...",
        "..######..",
        "..##..##..",
        "...####...",
        "....##....",
        "....####..",
        "....##....",
        "....####..",
        "....##....",
        "....##...."
    ]),
    ("Bell", "Campana", [
        "....##....",
        "...####...",
        "..######..",
        "..######..",
        "..######..",
        ".########.",
        "##########",
        "##########",
        "....##....",
        ".........."
    ]),
    ("Mushroom", "Fungo", [
        "...####...",
        "..######..",
        ".########.",
        "##########",
        "##########",
        "....##....",
        "....##....",
        "...####...",
        "...####...",
        ".........."
    ]),
    ("Moon", "Luna", [
        "...#####..",
        "..######..",
        ".#####....",
        ".####.....",
        ".####.....",
        ".####.....",
        ".#####....",
        "..######..",
        "...#####..",
        ".........."
    ]),
    ("Note", "Nota", [
        "....#####.",
        "....#####.",
        "....##.##.",
        "....##.##.",
        "....##.##.",
        "....##.##.",
        "..####.##.",
        ".#####.##.",
        ".#####.###",
        "..###..###"
    ]),
    ("Envelope", "Busta", [
        "##########",
        "##......##",
        "#.##..##.#",
        "#..####..#",
        "#...##...#",
        "#..####..#",
        "#.##..##.#",
        "##......##",
        "##########",
        ".........."
    ]),
    ("Umbrella", "Ombrello", [
        "....#.....",
        "...###....",
        "..#####...",
        ".#######..",
        "#########.",
        "....#.....",
        "....#.....",
        "....#.....",
        "....##....",
        "....##...."
    ]),
    ("Anchor", "Ancora", [
        "....##....",
        "...####...",
        "....##....",
        "....##....",
        ".########.",
        "##..##..##",
        "##..##..##",
        ".########.",
        "..######..",
        "....##...."
    ]),
]

AUTHORED_12 = [
    ("Castle", "Castello", [
        "#.##.##.##.#",
        "############",
        "############",
        ".##########.",
        ".##......##.",
        ".##.####.##.",
        ".##.####.##.",
        "############",
        "############",
        ".###....###.",
        ".###.##.###.",
        ".###.##.###."
    ]),
    ("Sword", "Spada", [
        ".....##.....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        "....####....",
        "############",
        "....####....",
        ".....##.....",
        ".....##.....",
        "....####...."
    ]),
    ("Crown", "Corona", [
        "#....##....#",
        "##..####..##",
        "############",
        "############",
        ".##########.",
        ".##########.",
        "############",
        "############",
        ".##.####.##.",
        ".##########.",
        "############",
        "............"
    ]),
    ("Rocket", "Razzo", [
        ".....##.....",
        "....####....",
        "....####....",
        "...######...",
        "...##..##...",
        "...######...",
        "..########..",
        "..##.##.##..",
        ".###.##.###.",
        "####.##.####",
        ".##......##.",
        ".#........#."
    ]),
    ("Diamond", "Diamante", [
        "....####....",
        "...######...",
        "..########..",
        ".##########.",
        "############",
        ".##########.",
        "..########..",
        "...######...",
        "....####....",
        ".....##.....",
        "............",
        "............"
    ]),
    ("Glasses", "Occhiali", [
        "............",
        "............",
        "############",
        "#..........#",
        "#.########.#",
        "#.########.#",
        "##.######.##",
        "##........##",
        ".##########.",
        "..########..",
        "............",
        "............"
    ]),
    ("Skull", "Teschio", [
        "...######...",
        "..########..",
        ".##########.",
        "############",
        "##.######.##",
        "##.######.##",
        "############",
        ".##########.",
        "..########..",
        "...######...",
        "...##..##...",
        "............"
    ]),
    ("Clock", "Orologio", [
        "...######...",
        "..########..",
        ".##########.",
        "#####.######",
        "#####.######",
        "#####...####",
        "############",
        "############",
        ".##########.",
        "..########..",
        "...######...",
        "............"
    ]),
    ("Flower", "Fiore", [
        "....####....",
        "..########..",
        ".###.##.###.",
        ".###.##.###.",
        "..########..",
        "....####....",
        ".....##.....",
        "...######...",
        ".....##.....",
        ".....##.....",
        "....####....",
        "............"
    ]),
    ("Butterfly", "Farfalla", [
        "##........##",
        "####....####",
        "######.#####",
        "############",
        ".##########.",
        "...######...",
        "..########..",
        ".##########.",
        "############",
        ".##########.",
        "..##....##..",
        "...#....#..."
    ]),
    ("Duck", "Papera", [
        ".....####...",
        "....######..",
        "..########..",
        "....######..",
        ".....####...",
        "....######..",
        "..##########",
        ".###########",
        "############",
        "############",
        ".##########.",
        "..########.."
    ]),
]

def parse_ascii_grid(lines: List[str]) -> List[List[int]]:
    grid = []
    for line in lines:
        grid.append([1 if ch == '#' else 0 for ch in line])
    return grid

def generate_procedural_symmetric(side: int, rng: random.Random) -> Optional[List[List[int]]]:
    half = (side + 1) // 2
    grid = [[0] * side for _ in range(side)]
    
    for r in range(side):
        for c in range(half):
            v = 1 if rng.random() < 0.48 else 0
            grid[r][c] = v
            grid[r][side - 1 - c] = v
            
    for r in range(1, side - 1):
        for c in range(side):
            neighbors = 0
            for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                nr, nc = r + dr, c + dc
                if 0 <= nr < side and 0 <= nc < side and grid[nr][nc] == 1:
                    neighbors += 1
            if grid[r][c] == 0 and neighbors >= 3:
                grid[r][c] = 1
                grid[r][side - 1 - c] = 1
            elif grid[r][c] == 1 and neighbors == 0:
                grid[r][c] = 0
                grid[r][side - 1 - c] = 0
                
    if is_line_solvable(grid, side):
        return grid
    return None

def pack_puzzle_bytes(grid: List[List[int]], side: int) -> List[int]:
    row_bytes = 2
    packed = [0] * (15 * row_bytes)
    for r in range(side):
        for c in range(side):
            if grid[r][c] == 1:
                byte_idx = r * row_bytes + (c >> 3)
                bit_idx = 7 - (c & 7)
                packed[byte_idx] |= (1 << bit_idx)
    return packed

def format_grid_comment(grid: List[List[int]], side: int) -> str:
    lines = []
    for row in grid:
        s = "".join("#" if c == 1 else "." for c in row)
        lines.append(f"    // {s}")
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--seed", type=int, default=20260819)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    
    puzzles_easy = []
    for en_name, it_name, ascii_art in AUTHORED_10:
        grid = parse_ascii_grid(ascii_art)
        if not is_line_solvable(grid, 10):
            print(f"Authored 10x10 '{en_name}' is not line solvable, tweaking...")
            continue
        puzzles_easy.append((grid, 10, f'L10N("{en_name}", "{it_name}")'))
        
    while len(puzzles_easy) < 32:
        g = generate_procedural_symmetric(10, rng)
        if g:
            puzzles_easy.append((g, 10, "nullptr"))
            
    puzzles_med = []
    for en_name, it_name, ascii_art in AUTHORED_12:
        grid = parse_ascii_grid(ascii_art)
        if not is_line_solvable(grid, 12):
            print(f"Authored 12x12 '{en_name}' is not line solvable, tweaking...")
            continue
        puzzles_med.append((grid, 12, f'L10N("{en_name}", "{it_name}")'))
        
    while len(puzzles_med) < 32:
        g = generate_procedural_symmetric(12, rng)
        if g:
            puzzles_med.append((g, 12, "nullptr"))
            
    puzzles_hard = []
    while len(puzzles_hard) < 16:
        g = generate_procedural_symmetric(15, rng)
        if g:
            puzzles_hard.append((g, 15, "nullptr"))
            
    print(f"Generated Easy: {len(puzzles_easy)} (Authored: {sum(1 for _,_,n in puzzles_easy if n != 'nullptr')})")
    print(f"Generated Med:  {len(puzzles_med)} (Authored: {sum(1 for _,_,n in puzzles_med if n != 'nullptr')})")
    print(f"Generated Hard: {len(puzzles_hard)} (Authored: {sum(1 for _,_,n in puzzles_hard if n != 'nullptr')})")
    
    out_lines = [
        "// Generated by tools/lume-games/gen_nonogram_pack.py — DO NOT EDIT DIRECTLY.",
        f"// CLI: python3 tools/lume-games/gen_nonogram_pack.py --out {args.out} --seed {args.seed}",
        f"// Puzzles: Easy=10x10 ({len(puzzles_easy)}), Medium=12x12 ({len(puzzles_med)}), Hard=15x15 ({len(puzzles_hard)})",
        "// All puzzles verified line-solvable, unique, max 4 clues/line.",
        "",
        "#pragma once",
        "#include <cstdint>",
        '#include "../../LumeLocale.h"',
        "",
        "namespace nonogram_pack {",
        "constexpr int kMaxSide = 15;",
        "constexpr int kRowBytes = 2;",
        "",
        "struct Puzzle {",
        "  uint8_t side;",
        "  uint8_t rows[kMaxSide * kRowBytes];",
        "  const char* name;",
        "};",
        "",
    ]
    
    def emit_tier(tier_name: str, puzzles: list):
        out_lines.append(f"constexpr Puzzle k{tier_name}[] = {{")
        for idx, (grid, side, name_expr) in enumerate(puzzles):
            packed = pack_puzzle_bytes(grid, side)
            bytes_str = ", ".join(f"0x{b:02X}" for b in packed)
            out_lines.append(f"  // #{idx + 1} (side {side})")
            out_lines.append(format_grid_comment(grid, side))
            out_lines.append(f"  {{{side}, {{{bytes_str}}}, {name_expr}}},")
        out_lines.append("};")
        out_lines.append(f"constexpr uint32_t k{tier_name}Count = sizeof(k{tier_name}) / sizeof(k{tier_name}[0]);")
        out_lines.append("")

    emit_tier("Easy", puzzles_easy)
    emit_tier("Medium", puzzles_med)
    emit_tier("Hard", puzzles_hard)
    out_lines.append("}  // namespace nonogram_pack")
    
    content = "\n".join(out_lines) + "\n"
    with open(args.out, "w") as f:
        f.write(content)
        
    sha = hashlib.sha256(content.encode("utf-8")).hexdigest()
    print(f"Wrote {len(content)} bytes to {args.out} (SHA256: {sha})")

if __name__ == "__main__":
    main()
