#include "MSP430DMADefender.h"
#include "MSP430.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "MSP430NemesisDefender.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

void MSP430DMADefenderPass::CheckAccessedMemoryRegions() {
}

MachineInstr *MSP430DMADefenderPass::build_dummy_1_0_0_1(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, const TargetInstrInfo *TII) {
    DebugLoc DL;
    // MOV  #0, R3       ; 1 cycle , 1 word
    return BuildMI(MBB, MBBI, DL, TII->get(MSP430::MOV16rc), MSP430::CG).addImm(0);
}

void MSP430DMADefenderPass::CompensateInstr(const MachineInstr &MI, MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {

    auto instruction_class = getTII()->getInstrMemTraceClass(nullptr, MI);



    /*if (MI.isAnnotationLabel()) {
        // don't know yet
    }*/
    switch (instruction_class) {
        case "no class": return;
        case "1 | 0 | 0 | 1": build_dummy_1_0_0_1(MBB, MBBI, getTII()); break; // TODO: this is a placeholder
        default:
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
            MI.dump();
#endif
            llvm_unreachable("Unexpected instruction latency");
    };
    //getDummyForClass(instruction_class,MBB)


}



FunctionPass *llvm::createMSP430DMADefenderPass() {
    return new MSP430DMADefenderPass();
}