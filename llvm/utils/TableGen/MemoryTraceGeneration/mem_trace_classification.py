def classify_instruction(mclk, instr_full, peripheral_mem, data_mem, program_mem):
    instr_length = str(len(mclk)/2)
    peripheral = ''.join(peripheral_mem)
    data = ''.join(data_mem)
    prgr = ''.join(program_mem)
    return instr_length + "." + peripheral + "." + data + "." + prgr
