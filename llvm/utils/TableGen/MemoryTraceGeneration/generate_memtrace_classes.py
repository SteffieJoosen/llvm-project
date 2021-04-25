#!/usr/bin/env python3

import os
import sys
import re
import binascii
from mem_trace_classification import classify_instruction

NO_CLOCK_TICKS_BEFORE_TARGET_INSTR = 38659
LENGTH_CLOCK_EDGE = 25
INDEX_TIME = 0
INDEX_CLK = 1
INDEX_INST = 2
INDEX_eu_pmem_en = 3
INDEX_fe_pmem_en = 4
INDEX_fe_pmem_en_dly = 5
INDEX_eu_dmem_en = 6
INDEX_eu_dmem_en_ok = 7
INDEX_per_en = 8

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

def remove_glitches(list_of_lines):
    new_list_of_lines = []
    previous_line = ""
    previous_was_bad_line = False
    for line in list_of_lines:
        if not (int(split(line)[INDEX_TIME])%LENGTH_CLOCK_EDGE == 0):
            previous_data = split(previous_line)
            current_data = split(line)
            previous_line = previous_data[INDEX_TIME] + ' ' * 2 + previous_data[INDEX_CLK] + ' ' * 35 + previous_data[INDEX_INST] + ' ' + current_data[INDEX_eu_pmem_en] + ' ' + current_data[INDEX_fe_pmem_en] + ' ' + current_data[INDEX_fe_pmem_en_dly] + ' ' + current_data[INDEX_eu_dmem_en] + ' ' + current_data[INDEX_eu_dmem_en_ok] + ' ' + current_data[INDEX_per_en]
            new_list_of_lines.append(previous_line)
            previous_was_bad_line = True
        else:
            if not previous_was_bad_line: new_list_of_lines.append(previous_line)
            previous_was_bad_line = False
            previous_line = line


    new_list_of_lines.append(previous_line)
    return new_list_of_lines

def find_instruction_lines_vcd(new_lines, start_instruction, no_instr_to_analyse):
    instr_lines = []
    instr_to_analyse = ""
    lines_skipped = 0
    no_instr_found = 0
    start_last_instr = 0
    for i in range(len(new_lines)):
        if not new_lines[i] == "":
            current_instruction = split(new_lines[i])[INDEX_INST]
            if i - lines_skipped == start_instruction:
                no_instr_found = 1
                instr_to_analyse = current_instruction
            if 1 <= no_instr_found <= no_instr_to_analyse:
                if current_instruction == instr_to_analyse:
                    instr_lines.append(new_lines[i])
                else:
                    no_instr_found +=1
                    instr_to_analyse = current_instruction
                    if no_instr_found <= no_instr_to_analyse:
                        instr_lines.append(new_lines[i])
                        start_last_instr = len(instr_lines)-1
        else:
            lines_skipped +=1

    return instr_lines, start_last_instr

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
    for asm in instr:
        f.write("    \"" + asm + "\\n\\t\"\r\n")
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


def generate_instruction(all_instr):
    if all_instr == "": return

    no_instr_to_analyse = len(all_instr)


    os.system('cd ~/sancus-main/sancus-examples && mkdir mem_trace')

    create_c_file(all_instr)
    create_makefile()

    # run the simulation
    os.system('cd ~/sancus-main/sancus-examples/mem_trace && make sim')

    os.system('echo \"--------------------------------------\"')
    os.system('echo \"----- VCD FILE IS BEING ANALYSED -----\"')
    os.system('echo \"--------------------------------------\"\n')

    # run the vcdvis script to generate the tex file containing the traces
    os.system('cp /home/steffie/sancus-main/sancus-examples/mem_trace/mem_trace.vcd /home/steffie/vcdvcd/mem_trace.vcd')
    os.system('cp /home/steffie/sancus-main/sancus-examples/mem_trace/mem_trace.vcd /home/steffie/sllvm/sllvm/llvm/utils/TableGen/MemoryTraceGeneration/mem_trace.vcd')
    os.system('cd ~/vcdvcd && '
              './vcdcat -x mem_trace.vcd TOP.tb_openMSP430.mclk TOP.tb_openMSP430.inst_full TOP.tb_openMSP430.dut.mem_backbone_0.eu_pmem_en TOP.tb_openMSP430.dut.mem_backbone_0.fe_pmem_en TOP.tb_openMSP430.dut.mem_backbone_0.eu_dmem_en TOP.tb_openMSP430.dut.mem_backbone_0.per_en'
              ' > ' + sys.path[0] + '/Trace.txt')
              # output sigals in orde: program memory, data memory, peripheral memory

    length = file_len("Trace.txt")

    start_line = search_line("Trace.txt", "===")
    length_of_instruction= 0
    f = open("Trace.txt")
    file_lines = f.readlines()
    new_lines = remove_glitches(file_lines[start_line:len(file_lines)])

    # find a new way to Instantiate this variable
    start_instruction = NO_CLOCK_TICKS_BEFORE_TARGET_INSTR*2 + 1

    result = find_instruction_lines_vcd(new_lines, start_instruction, no_instr_to_analyse)
    instr_lines = result[0]
    start_last_instr = result[1]



    mclk = []
    instr_full = []
    peripheral_mem = []
    data_mem = []
    program_mem = []
    for i in range(start_last_instr, len(instr_lines)):
        data = split(instr_lines[i])
        mclk.append(data[INDEX_CLK])
        instr_full.append(data[INDEX_INST])
        if (bool(data[INDEX_eu_pmem_en] == '1') | bool(data[INDEX_fe_pmem_en] == '1')) == True:
            program_mem.append('1')
        else:
            program_mem.append('0')
        data_mem.append(data[INDEX_eu_dmem_en])
        peripheral_mem.append(data[INDEX_per_en])


    # clean up
    if os.path.isdir("/home/steffie/sancus-main/sancus-examples/mem_trace"):
        os.system("rm -r /home/steffie/sancus-main/sancus-examples/mem_trace")
    if os.path.exists("/home/steffie/vcdvcd/mem_trace.vcd"):
        os.system("rm /home/steffie/vcdvcd/mem_trace.vcd")
    if os.path.exists("mem_trace.c"):
        os.system("rm mem_trace.c")
    if os.path.exists("Traces.tex"):
        os.remove("Traces.tex")


    instr_class = classify_instruction(all_instr[len(all_instr)-1], mclk, instr_full, peripheral_mem, data_mem, program_mem)

    return all_instr[len(all_instr)-1], instr_class





# create directory file to dump the class files in
# ------------------------------------------------
if os.path.isdir("/home/steffie/sllvm/sllvm/llvm/utils/TableGen/MemoryTraceGeneration/Classes"):
    os.system("rm -r /home/steffie/sllvm/sllvm/llvm/utils/TableGen/MemoryTraceGeneration/Classes")

os.system("mkdir Classes")

mem_region = input("The combination of memory regions to be accessed: ")

# Read instruction to generate memory tace for
# This assumes the sllvm github repository is cloned into your home directory
inc_file = open('/home/steffie/sllvm/build/sllvm/lib/Target/MSP430/MSP430GenInstrMemTraceInfo.inc')
found_table = False
all_instructions = []
for pos, line in enumerate(inc_file):
    if "Instruction_classes_progr_data" in line:
        if mem_region == "prd":
            found_table = True
        elif mem_region == "dd":
            found_table = False
        elif mem_region == "ped":
            found_table = False
        elif mem_region == "dpe":
            found_table = False
        elif mem_region == "prpe":
            found_table = False
        elif mem_region == "pepe":
            found_table = False
    if "Instruction_classes_data_data" in line:
        if mem_region == "prd":
            found_table = False
        elif mem_region == "dd":
            found_table = True
        elif mem_region == "ped":
            found_table = False
        elif mem_region == "dpe":
            found_table = False
        elif mem_region == "prpe":
            found_table = False
        elif mem_region == "pepe":
            found_table = False
    if "Instruction_classes_per_data" in line:
        if mem_region == "prd":
            found_table = False
        elif mem_region == "dd":
            found_table = False
        elif mem_region == "ped":
            found_table = True
        elif mem_region == "dpe":
            found_table = False
        elif mem_region == "prpe":
            found_table = False
        elif mem_region == "pepe":
            found_table = False
    if "Instruction_classes_data_per" in line:
        if mem_region == "prd":
            found_table = False
        elif mem_region == "dd":
            found_table = False
        elif mem_region == "ped":
            found_table = False
        elif mem_region == "dpe":
            found_table = True
        elif mem_region == "prpe":
            found_table = False
        elif mem_region == "pepe":
            found_table = False
    if "Instruction_classes_progr_per" in line:
        if mem_region == "prd":
            found_table = False
        elif mem_region == "dd":
            found_table = False
        elif mem_region == "ped":
            found_table = False
        elif mem_region == "dpe":
            found_table = False
        elif mem_region == "prpe":
            found_table = True
        elif mem_region == "pepe":
            found_table = False
    if "Instruction_classes_per_per" in line:
        if mem_region == "prd":
            found_table = False
        elif mem_region == "dd":
            found_table = False
        elif mem_region == "ped":
            found_table = False
        elif mem_region == "dpe":
            found_table = False
        elif mem_region == "prpe":
            found_table = False
        elif mem_region == "pepe":
            found_table = True
    if found_table:
        if "{" in line and "}," in line and not "nothing yet" in line:
            all_instructions.append(line)

#with open('/home/steffie/sllvm/build/sllvm/lib/Target/MSP430/MSP430GenInstrMemTraceInfo.inc') as f:
#    all_instructions = [line.rstrip('\n') if "{" in line and "}," in line and not "nothing yet" in line else "" for line in f]



print("len(all_instructions) = " + str(len(all_instructions)))
start_instr_number = input("Start number in .inc file: ")
end_instr_number = input("End number in .inc file: ")
i = 0
simulated_instr = []
add_instr = False
for line in all_instructions:
    if "/* " + start_instr_number + "*/" in line:
        add_instr = True
    if add_instr:
        simulated_instr.append(line)
    if "/* " + end_instr_number + "*/" in line:
        add_instr = False

print("The instructions that are about to be simulated")
for i in simulated_instr:
    print(i)

found_classes = list()
result_class = ""

total = len(simulated_instr)
done = 1
for ins in simulated_instr:
    all_instr = ""
    if ins != "":
        this_line = re.sub('\"', '', ', '.join(ins.split('{')[1].split('}')[0].split(", ")[0:-1])).split(' --- ')
        for asm_code in this_line:
            all_instr = asm_code.split(";")
            addressing_modes = ins[-2:]
            print("Instruction simulated: " + str(all_instr))
            res = generate_instruction(all_instr)
            assembly_string = res[0]
            result_class = res[1]
            if result_class!= "" :
                file_name = "Classes/" + result_class + ".txt"
                if result_class in found_classes:
                    class_file = open(file_name, "a+")
                else:
                    class_file = open(file_name, "w+")
                    found_classes.append(result_class)


                class_file.write(assembly_string + " || "+ addressing_modes +  "\r\n")
                class_file.close()

            os.system('echo \"--------------------------------------\"')
            os.system('echo \"------- | '+ str(int(done/total *100)) +'% | -------\"')
            os.system('echo \"--------------------------------------\"\n')
    done += 1
