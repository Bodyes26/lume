#!/usr/bin/env python3
"""
Lume Trail Bytecode Assembler.

Compiles textual assembly into binary bytecode for the Trail micro-VM.
Supports labels, comments, and all 40+ VM opcodes.

See docs/lume/15-trail-game-design.md §7.3.
"""

import struct
from typing import Dict, List, Tuple

# Opcode mapping
OPCODES = {
    # Stack
    "PUSH": (0x01, "imm16"),
    "POP": (0x02, None),
    "DUP": (0x03, None),
    "SWAP": (0x04, None),
    "OVER": (0x05, None),

    # Arithmetic
    "ADD": (0x10, None),
    "SUB": (0x11, None),
    "MUL": (0x12, None),
    "DIV": (0x13, None),
    "MOD": (0x14, None),
    "NEG": (0x15, None),
    "ABS": (0x16, None),
    "MIN": (0x17, None),
    "MAX": (0x18, None),
    "CLAMP": (0x19, None),

    # Comparison
    "EQ": (0x20, None),
    "NE": (0x21, None),
    "LT": (0x22, None),
    "GT": (0x23, None),
    "LE": (0x24, None),
    "GE": (0x25, None),

    # Logic
    "AND": (0x28, None),
    "OR": (0x29, None),
    "NOT": (0x2A, None),

    # Flow Control (relative 16-bit offset)
    "JMP": (0x30, "rel16"),
    "JZ": (0x31, "rel16"),
    "JNZ": (0x32, "rel16"),
    "CALL": (0x33, "rel16"),
    "RET": (0x34, None),
    "HALT": (0x35, None),

    # Variables
    "LOAD": (0x40, "u8"),
    "STORE": (0x41, "u8"),
    "INC": (0x42, "u8"),
    "DEC": (0x43, "u8"),

    # Grid
    "GINIT": (0x50, "u8_u8"),
    "GSET": (0x51, None),
    "GGET": (0x52, None),
    "GFILL": (0x53, "u8"),
    "GCOUNT": (0x54, "u8"),
    "GADJ": (0x55, None),
    "GADJ4": (0x56, None),
    "GFLOOD": (0x57, None),
    "GSWAP": (0x58, None),
    "GROW": (0x59, None),

    # Cursor
    "CURX": (0x60, None),
    "CURY": (0x61, None),
    "CMOV": (0x62, None),
    "CREL": (0x63, None),

    # Random
    "RAND": (0x68, None),

    # Rendering
    "CMAP": (0x70, "u8_ch"),
    "CSTYLE": (0x71, "u8_u8"),
    "TEXT": (0x72, "u8"),
    "TITLE": (0x73, "u8"),
    "BAR": (0x74, "u8"),
    "SCORE": (0x75, None),
    "FLASH": (0x76, None),

    # Result
    "WIN": (0x80, None),
    "LOSE": (0x81, None),
    "YIELD": (0x82, None),
}


def assemble(source: str) -> bytes:
    """Assembles assembly source text into binary bytecode."""
    lines = source.strip().splitlines()
    labels: Dict[str, int] = {}
    instructions: List[Tuple[str, List[str], int]] = []

    # Pass 1: Collect labels and determine instruction offsets
    current_offset = 0
    for line in lines:
        # Strip comments and whitespace
        line = line.split("#")[0].strip()
        if not line:
            continue

        # Check for label definition: "@label:" or "label:"
        if line.endswith(":"):
            label_name = line[:-1].strip().lstrip("@")
            labels[label_name] = current_offset
            continue

        tokens = line.split()
        op = tokens[0].upper()
        args = tokens[1:]

        if op not in OPCODES:
            raise ValueError(f"Unknown opcode: {op} in line: '{line}'")

        opcode_val, arg_type = OPCODES[op]
        inst_size = 1  # opcode byte

        if arg_type == "imm16":
            inst_size += 2
        elif arg_type == "rel16":
            inst_size += 2
        elif arg_type == "u8":
            inst_size += 1
        elif arg_type == "u8_u8":
            inst_size += 2
        elif arg_type == "u8_ch":
            inst_size += 2

        instructions.append((op, args, current_offset))
        current_offset += inst_size

    # Pass 2: Emit binary bytes with resolved labels
    output = bytearray()
    for op, args, inst_offset in instructions:
        opcode_val, arg_type = OPCODES[op]
        output.append(opcode_val)
        next_pc = inst_offset + 1

        if arg_type == "imm16":
            val = int(args[0], 0)
            output.extend(struct.pack("<h", val))
        elif arg_type == "rel16":
            target = args[0].lstrip("@")
            if target in labels:
                target_offset = labels[target]
                # Offset is relative to the PC immediately after the 2-byte operand
                next_pc = inst_offset + 3
                rel_offset = target_offset - next_pc
            else:
                rel_offset = int(target, 0)
            output.extend(struct.pack("<h", rel_offset))
        elif arg_type == "u8":
            val = int(args[0], 0) & 0xFF
            output.append(val)
        elif arg_type == "u8_u8":
            v1 = int(args[0], 0) & 0xFF
            v2 = int(args[1], 0) & 0xFF
            output.extend([v1, v2])
        elif arg_type == "u8_ch":
            v1 = int(args[0], 0) & 0xFF
            ch = ord(args[1][0]) if isinstance(args[1], str) else int(args[1], 0)
            output.extend([v1, ch & 0xFF])

    return bytes(output)


if __name__ == "__main__":
    sample = """
    GINIT 4 4
    PUSH 3
    STORE 0
    loop:
      LOAD 0
      PUSH 1
      SUB
      STORE 0
      LOAD 0
      JNZ @loop
    WIN
    """
    bc = assemble(sample)
    print(f"Assembled {len(bc)} bytes: {bc.hex()}")
