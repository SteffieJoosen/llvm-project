#!/usr/bin/env python3

import os
import sys

NO_CLOCK_TICKS_BEFORE_TARGET_INSTR = 38659

def file_len(fname):
    with open(fname) as f:
        i = 0
        for l in enumerate(f):
            i+=1
    return i + 1

def split(string):
    list_of_words = []
    space_passed = False
    word = ""
    for i in string:
        if i == ' ':
            space_passed = True
        elif space_passed and i != ' ':
            list_of_words.append(word)
            word = str(i)
            space_passed = False
        elif  not space_passed and i != ' ':
            word += str(i)
    list_of_words.append(word)
    return list_of_words


def create_makefile():
    # create makefile for simulation
    # -----------------------------


    os.system('cp /home/steffie/sancus-main/sancus-examples/dma/Makefile '
              '/home/steffie/sancus-main/sancus-examples/mem_trace/Makefile')
    f = open("/home/steffie/sancus-main/sancus-examples/mem_trace/Makefile", "r")
    list_of_lines = f.readlines()
    list_of_lines[20] = "\t$(SANCUS_SIM) -d mem_trace.vcd $(SIMFLAGS) $<"

    f = open("/home/steffie/sancus-main/sancus-examples/mem_trace/Makefile", "w")
    f.writelines(list_of_lines)
    f.close()


def create_c_file(instr):
    # create .c file for simulation
    # -------------------------------
    f = open("mem_trace.c", "w+")

    f.write("#include <sancus_support/sm_io.h>\r\n")
    f.write("#include <sancus_support/sancus_step.h>\r\n")
    f.write("\r\n")
    f.write("DECLARE_SM(foo, 0x1234);\r\n")
    f.write("\r\n")
    f.write("void SM_ENTRY(foo) test(char key) {}\r\n")
    f.write("\r\n")
    f.write("void irqHandler(void) {}\r\n")
    f.write("\r\n")
    f.write("int main()\r\n")
    f.write("{\r\n")
    f.write("    msp430_io_init();\r\n")
    f.write("sancus_enable(&foo);\r\n")
    f.write("__asm__ __volatile__(\r\n")
    f.write("    \"add r5, r6\"\r\n")
    f.write(");\r\n")
    f.write("\r\n")
    f.write("}\r\n")
    f.write("\r\n")
    f.write("/* ======== TIMER A ISR ======== */\r\n")
    f.write("SANCUS_STEP_ISR_ENTRY2(irqHandler, __ss_end)\r\n")
    f.close()

    os.system('cp mem_trace.c ~/sancus-main/sancus-examples/mem_trace/mem_trace.c')

def search_line(file_name, leading_chars):
    f = open(file_name, "r")
    line_number = 0
    for line in f:
        if line.startswith(leading_chars):
            return line_number+1
        line_number+=1


def generate_instruction(instr_number):
    # Read instruction to generate memory tace for
    # This assumes that the sllvm github repository is cloned into your home directory
    with open('/home/steffie/sllvm/build/sllvm/lib/Target/MSP430/MSP430GenInstrMemTraceInfo.s') as f:
        all_instructions = [line.rstrip('\n') for line in f]

    instr = all_instructions[instr_number].split('{')[1].split('}')[0].split("; ")

    os.system('cd ~/sancus-main/sancus-examples && mkdir mem_trace')

    create_c_file(instr)
    create_makefile()

    # run the simulation
    os.system('cd ~/sancus-main/sancus-examples/mem_trace && make sim')

    os.system('echo \"--------------------------------------\"')
    os.system('echo \"---- TEX FILE IS BEING GENERATED -----\"')
    os.system('echo \"--------------------------------------\"')

    # run the vcdvis script to generate the tex file containing the traces
    os.system('cp mem_trace.vcd /home/steffie/vcdvcd/mem_trace.vcd')
    os.system('cd ~/vcdvcd && '
              './vcdcat -x mem_trace.vcd TOP.tb_openMSP430.mclk TOP.tb_openMSP430.inst_full TOP.tb_openMSP430.dut.mem_backbone_0.eu_pmem_en TOP.tb_openMSP430.dut.mem_backbone_0.eu_dmem_en TOP.tb_openMSP430.dut.mem_backbone_0.fe_pmem_en'
              ' > ' + sys.path[0] + '/Trace.txt')

    length = file_len("Trace.txt")

    start_line = search_line("Trace.txt", "===")
    length_of_instruction= 0
    f = open("Trace.txt")
    file_lines = f.readlines()
    start_instruction = start_line+NO_CLOCK_TICKS_BEFORE_TARGET_INSTR*2 + 1

    instr_lines = []
    instr_full = []
    mclk = []
    instr_to_analyse = ""

    for i in range(start_line,len(file_lines)):
        current_instruction = split(file_lines[i])[1]
        if i == start_instruction:
            instr_to_analyse = current_instruction
        if current_instruction == instr_to_analyse:
            instr_lines.append(file_lines[i])

    instr_full = []
    mclk = []
    peripheral_mem = []
    data_mem = []
    program_mem = []
    for instr_string in instr_lines:
        data = split(instr_string)
        mclk.append(data[1])
        instr_full.append(data[2])
        peripheral_mem.append(data[3])
        data_mem.append(data[4])
        program_mem.append(data[5])

    

    # clean up
    if os.path.exists("/home/steffie/sancus-main/sancus-examples/mem_trace"):
        os.system("rm -r /home/steffie/sancus-main/sancus-examples/mem_trace")


    return


if os.path.exists("traces.tex"):
    os.remove("traces.tex")
if os.path.exists("Traces.tex"):
    os.remove("Traces.tex")

if os.path.exists("mem_trace.c"):
    os.remove("mem_trace.c")

# create tex file to dump the traces in
# -------------------------------------

traces_file = open("traces.tex", "w+")

traces_file.write("\documentclass{article}\r\n")
traces_file.write("\\usepackage{tikz}\r\n")
traces_file.write("\r\n")
traces_file.write("\\begin{document}\r\n")

traces_file.close()

generate_instruction(258)

traces_file = open("traces.tex", "a")

traces_file.write("\\end{document}\r\n")
traces_file.close()
