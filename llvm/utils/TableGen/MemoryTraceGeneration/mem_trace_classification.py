def classify_instruction(last_instr, mclk, instr_full, peripheral_mem, data_mem, program_mem):
    instr_length = str(int(len(mclk)/2))
    peripheral1 = [peripheral_mem[i] if i%2 == 0 else '' for i in range(len(peripheral_mem))]
    peripheral = ''.join(peripheral1)
    data1 = [data_mem[i] if i%2 == 0 else '' for i in range(len(data_mem))]
    data = ''.join(data1)
    progr1 = [program_mem[i] if i%2 == 0 else '' for i in range(len(program_mem))]
    prgr = ''.join(progr1)
    return instr_length + "_|_" + peripheral + "_|_" + data + "_|_" + prgr
