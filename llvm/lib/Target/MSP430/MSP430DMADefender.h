//
// Created by steffie on 08.02.21.
//

#ifndef LLVM_MSP430DMADEFENDER_H
#define LLVM_MSP430DMADEFENDER_H

#include "MSP430NemesisDefender.h"

class MSP430DMADefender : public MSP430NemesisDefenderPass {
public:
private:
  void CheckAccessedMemoryRegions();
  void CompensateInstr(const MachineInstr &MI, MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI) override;
};

#endif // LLVM_MSP430DMADEFENDER_H
