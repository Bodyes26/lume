#!/usr/bin/env python3
"""
Lume Trail Story Compiler.

Compiles a story YAML file into a packed .story binary file.
Validates node graphs, compiles minigame assembly via trail_asm, and packs
all data structures according to docs/lume/15-trail-game-design.md.

Usage:
  python3 build_story.py --input stories/silk_road/story.yaml --output silk_road.story
"""

import argparse
import os
import struct
import sys
from typing import Any, Dict, List, Tuple

import yaml

# Import bytecode assembler
sys.path.insert(0, os.path.dirname(__file__))
from trail_asm import assemble

# Struct packing formats matching C++ definitions in TrailTypes.h & TrailFormat.h
# kStoryMagic = "LTRL", version = 1
HEADER_FORMAT = "<4sBBH24s32s"  # 64 bytes
META_FORMAT = "<BB2x"            # 4 bytes + 8 ResourceDefs + 6 CompanionDefs
# ResourceDef: char name[24], uint8 icon, 1x pad, int16 max, int16 start, int16 critical = 32 bytes
RES_DEF_FORMAT = "<24sBx3h"
# CompanionDef: char name[16], char trait[16] = 32 bytes
COMP_DEF_FORMAT = "<16s16s"

# ChapterHeader:
# uint16 nodeCount, minigameCount, stringCount, imageCount (8 B)
# uint32 nodesOffset, minigamesOffset, stringsOffset, imagesOffset, totalBytes (20 B) = 28 B
CHAPTER_HEADER_FORMAT = "<4H5I"

# MinigameDescriptor:
# uint8 gridW, gridH, stringCount, reserved (4 B)
# char cellChars[16] (16 B)
# uint8 cellStyles[16] (16 B)
# uint16 stringOffsets[16] (32 B)
# uint16 setupLen, uint16 keyLens[6] (14 B) = 82 B
MINIGAME_DESC_FORMAT = "<4B16s16B16H7H"

# Effect: uint8 type, uint8 id, int16 value = 4 B
EFFECT_FORMAT = "<BBh"
# Condition: uint8 type, uint8 id, int16 value = 4 B
COND_FORMAT = "<BBh"

# Choice header (14 bytes)
CHOICE_HEADER_FORMAT = "<3HBxH2bBx"

# Node header (26 bytes)
NODE_HEADER_FORMAT = "<Bb7Hbx2H3Bx"
EFFECT_TYPES = {
    "resource_delta": 0, "res_delta": 0, "resource": 0,
    "resource_set": 1, "res_set": 1,
    "companion_join": 2, "comp_join": 2,
    "companion_leave": 3, "comp_leave": 3,
    "companion_health": 4, "comp_health": 4,
    "flag_set": 5, "flag": 5,
    "flag_clear": 6,
    "distance": 7,
    "day": 8,
}

COND_TYPES = {
    "resource_min": 0, "res_min": 0, "min": 0,
    "resource_max": 1, "res_max": 1, "max": 1,
    "companion_alive": 2, "comp_alive": 2,
    "companion_dead": 3, "comp_dead": 3,
    "flag_set": 4, "flag": 4,
    "flag_clear": 5,
}

NODE_TYPES = {
    "narrative": 0,
    "event": 1,
    "check": 2,
    "minigame": 3,
    "shop": 4,
    "chapter_end": 5, "chapterend": 5,
    "game_over": 6, "gameover": 6,
}


def build_story(yaml_data: Dict[str, Any]) -> bytes:
    """Compiles a parsed YAML story dict into a binary byte string."""
    story_meta = yaml_data.get("story", yaml_data.get("meta", {}))
    story_id = story_meta.get("id", "untitled")
    story_title = story_meta.get("title", "Senza Titolo")

    # Parse resources
    resources = story_meta.get("resources", [])
    res_name_to_idx = {r.get("id", r.get("name", i)): i for i, r in enumerate(resources)}

    # Parse companions
    companions = story_meta.get("companions", [])
    comp_name_to_idx = {c.get("id", c.get("name", i)): i for i, c in enumerate(companions)}

    # Collect chapters
    chapter_keys = [k for k in yaml_data if k.startswith("chapter_") or k.startswith("chapter")]
    chapter_keys.sort(key=lambda k: int("".join(filter(str.isdigit, k)) or "0"))

    if not chapter_keys and "chapters" in yaml_data:
        chapters = yaml_data["chapters"]
    else:
        chapters = [yaml_data[k] for k in chapter_keys]

    chapter_count = len(chapters)

    # 1. Pack Header
    header_bytes = struct.pack(
        HEADER_FORMAT,
        b"LTRL",
        1,  # version
        0,  # flags
        chapter_count,
        story_id.encode("utf-8")[:23],
        story_title.encode("utf-8")[:31],
    )

    # 2. Pack Meta Block
    res_defs_bytes = bytearray()
    for i in range(8):
        if i < len(resources):
            r = resources[i]
            icon_type = 0 if r.get("icon") == "bar" else (1 if r.get("icon") == "number" else 2)
            name = r.get("name", f"Res_{i}").encode("utf-8")[:23]
            res_defs_bytes.extend(struct.pack(
                RES_DEF_FORMAT,
                name,
                icon_type,
                int(r.get("max", 100)),
                int(r.get("start", 100)),
                int(r.get("critical", 15)),
            ))
        else:
            res_defs_bytes.extend(b"\x00" * struct.calcsize(RES_DEF_FORMAT))

    comp_defs_bytes = bytearray()
    for i in range(6):
        if i < len(companions):
            c = companions[i]
            name = c.get("name", f"Comp_{i}").encode("utf-8")[:15]
            trait = c.get("trait", "").encode("utf-8")[:15]
            comp_defs_bytes.extend(struct.pack(COMP_DEF_FORMAT, name, trait))
        else:
            comp_defs_bytes.extend(b"\x00" * struct.calcsize(COMP_DEF_FORMAT))

    meta_bytes = struct.pack(META_FORMAT, len(resources), len(companions)) + res_defs_bytes + comp_defs_bytes

    # 3. Pack Chapters
    chapter_blocks: List[bytes] = []
    for ch_idx, ch_data in enumerate(chapters):
        ch_nodes_data = ch_data.get("nodes", [])
        ch_minigames_data = ch_data.get("minigames", {})

        # Node index mapping
        node_id_to_idx = {n.get("id", i): i for i, n in enumerate(ch_nodes_data)}
        # Minigame index mapping
        if isinstance(ch_minigames_data, dict):
            mg_name_to_idx = {k: i for i, k in enumerate(ch_minigames_data.keys())}
            mg_list = list(ch_minigames_data.values())
        else:
            mg_name_to_idx = {m.get("id", i): i for i, m in enumerate(ch_minigames_data)}
            mg_list = ch_minigames_data

        # Build String Pool for this chapter
        str_pool = bytearray()
        def add_str(s: str) -> Tuple[int, int]:
            if not s:
                return 0, 0
            encoded = s.strip().encode("utf-8") + b"\0"
            off = len(str_pool)
            str_pool.extend(encoded)
            return off, len(encoded) - 1

        # Helper to pack an Effect
        def pack_effect(eff: Dict[str, Any]) -> bytes:
            t_str = str(eff.get("type", "resource_delta")).lower()
            t_val = EFFECT_TYPES.get(t_str, 0)
            res_id = eff.get("resource", eff.get("id", 0))
            if isinstance(res_id, str):
                if t_val in (2, 3, 4):  # companion
                    res_id = comp_name_to_idx.get(res_id, 0)
                else:
                    res_id = res_name_to_idx.get(res_id, 0)
            val = int(eff.get("delta", eff.get("value", eff.get("set", 0))))
            return struct.pack(EFFECT_FORMAT, t_val, int(res_id) & 0xFF, val)

        # Helper to pack a Condition
        def pack_condition(cond: Dict[str, Any]) -> bytes:
            t_str = str(cond.get("type", "resource_min")).lower()
            t_val = COND_TYPES.get(t_str, 0)
            res_id = cond.get("resource", cond.get("id", cond.get("flag", 0)))
            if isinstance(res_id, str):
                if t_val in (2, 3):
                    res_id = comp_name_to_idx.get(res_id, 0)
                else:
                    res_id = res_name_to_idx.get(res_id, 0)
            val = int(cond.get("value", cond.get("min", cond.get("max", 1))))
            return struct.pack(COND_FORMAT, t_val, int(res_id) & 0xFF, val)

        # Helper to pack a Choice
        def pack_choice(ch: Dict[str, Any]) -> bytes:
            lbl_off, lbl_len = add_str(ch.get("label", ch.get("text", "")))
            next_target = ch.get("next", 0)
            next_idx = node_id_to_idx.get(next_target, int(next_target) if str(next_target).isdigit() else 0)
            prob = int(ch.get("probability", ch.get("prob", 0)))
            fail_target = ch.get("fail_next", next_idx)
            fail_idx = node_id_to_idx.get(fail_target, int(fail_target) if str(fail_target).isdigit() else 0)

            mg_name = ch.get("minigame")
            mg_idx = mg_name_to_idx.get(mg_name, -1) if mg_name else -1

            conds = ch.get("conditions", [ch.get("condition")] if "condition" in ch else [])
            effs = ch.get("effects", [])

            ch_hdr = struct.pack(
                CHOICE_HEADER_FORMAT,
                lbl_off, lbl_len, next_idx, prob, fail_idx, mg_idx,
                min(len(conds), 4), min(len(effs), 8),
            )
            conds_bytes = bytearray()
            for c_i in range(4):
                if c_i < len(conds):
                    conds_bytes.extend(pack_condition(conds[c_i]))
                else:
                    conds_bytes.extend(b"\x00" * 4)

            effs_bytes = bytearray()
            for e_i in range(8):
                if e_i < len(effs):
                    effs_bytes.extend(pack_effect(effs[e_i]))
                else:
                    effs_bytes.extend(b"\x00" * 4)

            return ch_hdr + conds_bytes + effs_bytes

        # Pack Minigames
        mg_bytes_list = bytearray()
        for mg in mg_list:
            gw = int(mg.get("grid", {}).get("w", mg.get("grid_w", 8)))
            gh = int(mg.get("grid", {}).get("h", mg.get("grid_h", 6)))
            mg_strings = mg.get("strings", [])
            mg_str_offsets = [add_str(s)[0] for s in mg_strings[:16]] + [0] * max(0, 16 - len(mg_strings))

            # Cell map
            cell_chars = bytearray(16)
            cell_styles = bytearray(16)
            for cid, cinfo in mg.get("cell_map", {}).items():
                idx = int(cid) & 0x0F
                ch = cinfo.get("char", ".") if isinstance(cinfo, dict) else str(cinfo)
                UNICODE_GLYPH_MAP = {
                    "·": ord("."), "◆": ord("#"), "♦": ord("#"), "✦": ord("*"),
                    "★": ord("*"), "▲": ord("^"), "▴": ord("^"), "≈": ord("~"),
                    "█": ord("@"), "▓": ord("#"), "■": ord("@"), "♠": ord("s"),
                    "◎": ord("O"), "○": ord("O"), "×": ord("x"), "▾": ord("v"),
                }
                if ch in UNICODE_GLYPH_MAP:
                    cell_chars[idx] = UNICODE_GLYPH_MAP[ch]
                elif ch:
                    cell_chars[idx] = ord(ch[0]) & 0x7F
                else:
                    cell_chars[idx] = ord(".")
                style = 1
                if isinstance(cinfo, dict):
                    st_str = cinfo.get("style", "black")
                    style = 0 if st_str == "white" else (2 if st_str == "inverted" else (3 if st_str == "border" else 1))
                cell_styles[idx] = style

            # Compile bytecode
            setup_asm = mg.get("setup", "HALT")
            setup_bc = assemble(setup_asm)

            keys_bc = []
            for k in ["on_up", "on_down", "on_left", "on_right", "on_confirm", "on_back"]:
                k_asm = mg.get(k, "HALT")
                keys_bc.append(assemble(k_asm))

            key_lens = [len(bc) for bc in keys_bc]
            mg_hdr = struct.pack(
                MINIGAME_DESC_FORMAT,
                gw, gh, len(mg_strings), 0,
                bytes(cell_chars),
                *cell_styles,
                *mg_str_offsets,
                len(setup_bc),
                *key_lens,
            )
            mg_bytes_list.extend(mg_hdr)
            mg_bytes_list.extend(setup_bc)
            for k_bc in keys_bc:
                mg_bytes_list.extend(k_bc)

        # Pack Nodes
        nodes_bytes = bytearray()
        for n in ch_nodes_data:
            nt_str = str(n.get("type", "narrative")).lower()
            nt_val = NODE_TYPES.get(nt_str, 0)
            t_off, t_len = add_str(n.get("title", ""))
            txt_off, txt_len = add_str(n.get("text", n.get("narrative", "")))

            next_idx = node_id_to_idx.get(n.get("next", 0), 0)
            pass_idx = node_id_to_idx.get(n.get("pass_next", next_idx), 0)
            fail_idx = node_id_to_idx.get(n.get("fail_next", next_idx), 0)

            mg_name = n.get("minigame")
            mg_idx = mg_name_to_idx.get(mg_name, -1) if mg_name else -1
            win_idx = node_id_to_idx.get(n.get("win_next", next_idx), 0)
            lose_idx = node_id_to_idx.get(n.get("lose_next", next_idx), 0)

            choices = n.get("choices", [])
            effs = n.get("effects", [])
            conds = n.get("conditions", [n.get("condition")] if "condition" in n else [])

            node_hdr = struct.pack(
                NODE_HEADER_FORMAT,
                nt_val,
                int(n.get("illustration", -1)),
                t_off, t_len, txt_off, txt_len,
                next_idx, pass_idx, fail_idx,
                mg_idx,
                win_idx, lose_idx,
                min(len(choices), 4), min(len(effs), 8), min(len(conds), 4),
            )

            # 4 Choices (62 bytes each)
            choices_bytes = bytearray()
            for c_i in range(4):
                if c_i < len(choices):
                    choices_bytes.extend(pack_choice(choices[c_i]))
                else:
                    choices_bytes.extend(b"\x00" * 62)

            # 8 Effects
            effs_bytes = bytearray()
            for e_i in range(8):
                if e_i < len(effs):
                    effs_bytes.extend(pack_effect(effs[e_i]))
                else:
                    effs_bytes.extend(b"\x00" * 4)

            # 4 Conditions
            conds_bytes = bytearray()
            for cd_i in range(4):
                if cd_i < len(conds):
                    conds_bytes.extend(pack_condition(conds[cd_i]))
                else:
                    conds_bytes.extend(b"\x00" * 4)

            nodes_bytes.extend(node_hdr + choices_bytes + effs_bytes + conds_bytes)

        # Assemble Chapter Block
        header_len = struct.calcsize(CHAPTER_HEADER_FORMAT)
        nodes_off = header_len
        mg_off = nodes_off + len(nodes_bytes)
        str_off = mg_off + len(mg_bytes_list)
        img_off = str_off + len(str_pool)
        total_bytes = img_off  # no images packed inline yet

        ch_header = struct.pack(
            CHAPTER_HEADER_FORMAT,
            len(ch_nodes_data),
            len(mg_list),
            len(str_pool),
            0,  # imageCount
            nodes_off,
            mg_off,
            str_off,
            img_off,
            total_bytes,
        )
        chapter_blocks.append(ch_header + nodes_bytes + mg_bytes_list + str_pool)

    # 4. Assemble Whole File
    # Calculate chapter table offsets
    table_offset = len(header_bytes) + len(meta_bytes)
    table_size = chapter_count * 4
    first_chapter_offset = table_offset + table_size

    chapter_offsets = []
    curr_off = first_chapter_offset
    for ch_b in chapter_blocks:
        chapter_offsets.append(curr_off)
        curr_off += len(ch_b)

    chapter_table_bytes = struct.pack(f"<{chapter_count}I", *chapter_offsets)

    return header_bytes + meta_bytes + chapter_table_bytes + b"".join(chapter_blocks)


def main():
    parser = argparse.ArgumentParser(description="Compile YAML story to .story binary")
    parser.add_argument("--input", "-i", required=True, help="Input story YAML file")
    parser.add_argument("--output", "-o", required=True, help="Output .story binary file")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        yaml_data = yaml.safe_load(f)

    story_binary = build_story(yaml_data)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(story_binary)

    print(f"Compiled '{args.input}' -> '{args.output}' ({len(story_binary):,} bytes)")


if __name__ == "__main__":
    main()
