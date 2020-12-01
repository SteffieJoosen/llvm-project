def classify_instruction(last_instr, mclk, instr_full, peripheral_mem, data_mem, program_mem):
    instr_length = str(int(len(mclk)/2))
    peripheral = ''.join(peripheral_mem)
    data = ''.join(data_mem)
    prgr = ''.join(program_mem)
    return instr_length + " | " + peripheral + " | " + data + " | " + prgr
