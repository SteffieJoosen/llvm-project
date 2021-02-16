#!/usr/bin/env python3

import os
import re

# This program assumes that the script gen_mem_trace_py has been run and that in this directory,
# a 'Classes' folder with al resulting classification files resides

def source_addr_mode(source):
    if re.match('[0-9]+\(r[0-9]+\)',source) != None:
        return "m-indexed"
    if re.match('0x[0-9][0-9][0-9][0-9]',source) != None:
        return "m-symbolic"
    if re.match('&0x[0-9][0-9][0-9][0-9]', source) != None:
        return "m-absolute"


classes = list()
opcodes_for_IForm = ["mov", "add", "addc", "sub", "subc", "cmp", "dadd", "bit", "bic", "bis", "xor", "and"]
file_name = input("Class to find a dummy instruction for: ")
instr_class = file_name.split(".txt")[0]


instructions = [line.rstrip() for line in open("Classes1/" + instr_class + ".txt", "r")]
print(instructions)


dummy = "nothing found"

# R15 and address 0x0202 are used to hold dummy values, TODO = store values used in program that is being patched
for line in instructions:
    instruction = line.split(" || ")[0]
    addressing_modes = line.split(" || ")[1]
    print(instruction + "  ---->  " + addressing_modes)
    source = instruction.split(" ")[1]
    destination = instruction.split(", ")[1]
    if "mov " in instruction:
        dummy = "mov " + source + ", " + source
        break
    elif "add " in instruction:
        if addressing_modes[-1] == 'm':
            # specific_mode = source_addr_mode(source)
            # Emulated opcoder CLR is not used here, stick to basic opcodes
            dummy = "mov #0x000, " + source + "; " + instruction
        if addressing_modes[-1] == 'n' || addressing_modes[-1] == 'p':
            dummy = "mov #0x000, 0(" + source[1:] + "); " + instruction
        if addressing_modes[-1] == 'i':
            # don't use constant generator, add -3 and then 3 to the destination
            dummy = "add #0xFFFD, " + destination + "; add #0x0003, " + destination
