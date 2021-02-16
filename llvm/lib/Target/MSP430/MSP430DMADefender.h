#ifndef LLVM_MSP430DMADEFENDER_H
#define LLVM_MSP430DMADEFENDER_H

#include "MSP430NemesisDefender.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace {
    class MSP430DMADefenderPass : public MSP430NemesisDefenderPass {
    public:
        MSP430DMADefenderPass() : MSP430NemesisDefenderPass() {}

        static char ID;
    private:
        void CheckAccessedMemoryRegions();
        void CompensateInstr(const MachineInstr &MI, MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI) override;

        MachineInstr *
        build_dummy_1_0_0_1(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, const TargetInstrInfo *TII);
    };
}
#endif // LLVM_MSP430DMADEFENDER_H