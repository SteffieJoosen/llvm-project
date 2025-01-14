//===-- MSP430InstrInfo.cpp - MSP430 Instruction Information --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the MSP430 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "MSP430InstrInfo.h"
#include "MSP430.h"
#include "MSP430MachineFunctionInfo.h"
#include "MSP430TargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "MSP430GenInstrInfo.inc"

#define GET_INSTRINFO_LATENCY_DESC
#include "MSP430GenInstrLatencyInfo.inc"

#define GET_INSTRINFO_MEMTRACE_DESC
#include "MSP430GenInstrMemTraceInfo.inc"

// Pin the vtable to this file.
void MSP430InstrInfo::anchor() {}

MSP430InstrInfo::MSP430InstrInfo(MSP430Subtarget &STI)
  : MSP430GenInstrInfo(MSP430::ADJCALLSTACKDOWN, MSP430::ADJCALLSTACKUP),
    RI() {}

void MSP430InstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                    Register SrcReg, bool isKill, int FrameIdx,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (MI != MBB.end()) DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &MSP430::GR16RegClass)
    BuildMI(MBB, MI, DL, get(MSP430::MOV16mr))
      .addFrameIndex(FrameIdx).addImm(0)
      .addReg(SrcReg, getKillRegState(isKill)).addMemOperand(MMO);
  else if (RC == &MSP430::GR8RegClass)
    BuildMI(MBB, MI, DL, get(MSP430::MOV8mr))
      .addFrameIndex(FrameIdx).addImm(0)
      .addReg(SrcReg, getKillRegState(isKill)).addMemOperand(MMO);
  else
    llvm_unreachable("Cannot store this register to stack slot!");
}

void MSP430InstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator MI,
                                           Register DestReg, int FrameIdx,
                                           const TargetRegisterClass *RC,
                                           const TargetRegisterInfo *TRI) const{
  DebugLoc DL;
  if (MI != MBB.end()) DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &MSP430::GR16RegClass)
    BuildMI(MBB, MI, DL, get(MSP430::MOV16rm))
      .addReg(DestReg, getDefRegState(true)).addFrameIndex(FrameIdx)
      .addImm(0).addMemOperand(MMO);
  else if (RC == &MSP430::GR8RegClass)
    BuildMI(MBB, MI, DL, get(MSP430::MOV8rm))
      .addReg(DestReg, getDefRegState(true)).addFrameIndex(FrameIdx)
      .addImm(0).addMemOperand(MMO);
  else
    llvm_unreachable("Cannot store this register to stack slot!");
}

void MSP430InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const DebugLoc &DL, MCRegister DestReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  unsigned Opc;
  if (MSP430::GR16RegClass.contains(DestReg, SrcReg))
    Opc = MSP430::MOV16rr;
  else if (MSP430::GR8RegClass.contains(DestReg, SrcReg))
    Opc = MSP430::MOV8rr;
  else
    llvm_unreachable("Impossible reg-to-reg copy");

  BuildMI(MBB, I, DL, get(Opc), DestReg)
    .addReg(SrcReg, getKillRegState(KillSrc));
}

unsigned MSP430InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");

  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;

  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != MSP430::JMP &&
        I->getOpcode() != MSP430::JCC &&
        I->getOpcode() != MSP430::Br &&
        I->getOpcode() != MSP430::Bm)
      break;
    // Remove the branch.
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return Count;
}

bool MSP430InstrInfo::
reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Xbranch condition!");

  MSP430CC::CondCodes CC = static_cast<MSP430CC::CondCodes>(Cond[0].getImm());

  switch (CC) {
  default: llvm_unreachable("Invalid branch condition!");
  case MSP430CC::COND_E:
    CC = MSP430CC::COND_NE;
    break;
  case MSP430CC::COND_NE:
    CC = MSP430CC::COND_E;
    break;
  case MSP430CC::COND_L:
    CC = MSP430CC::COND_GE;
    break;
  case MSP430CC::COND_GE:
    CC = MSP430CC::COND_L;
    break;
  case MSP430CC::COND_HS:
    CC = MSP430CC::COND_LO;
    break;
  case MSP430CC::COND_LO:
    CC = MSP430CC::COND_HS;
    break;
  }

  Cond[0].setImm(CC);
  return false;
}

bool MSP430InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  // Start from the bottom of the block and work up, examining the
  // terminator instructions.
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;

    // Working from the bottom, when we see a non-terminator
    // instruction, we're done.
    if (!isUnpredicatedTerminator(*I))
      break;

    // A terminator that isn't a branch can't easily be handled
    // by this analysis.
    if (!I->isBranch())
      return true;

    // Cannot handle indirect branches.
    if (I->getOpcode() == MSP430::Br ||
        I->getOpcode() == MSP430::Bm)
      return true;

    // Handle unconditional branches.
    if (I->getOpcode() == MSP430::JMP) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }

      // If the block has any instructions after a JMP, delete them.
      while (std::next(I) != MBB.end())
        std::next(I)->eraseFromParent();
      Cond.clear();
      FBB = nullptr;

      // Delete the JMP if it's equivalent to a fall-through.
      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }

      // TBB is used to indicate the unconditinal destination.
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    // Handle conditional branches.
    assert(I->getOpcode() == MSP430::JCC && "Invalid conditional branch");
    MSP430CC::CondCodes BranchCode =
      static_cast<MSP430CC::CondCodes>(I->getOperand(1).getImm());
    if (BranchCode == MSP430CC::COND_INVALID)
      return true;  // Can't handle weird stuff.

    // Working from the bottom, handle the first conditional branch.
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(BranchCode));
      continue;
    }

    // Handle subsequent conditional branches. Only handle the case where all
    // conditional branches branch to the same destination.
    assert(Cond.size() == 1);
    assert(TBB);

    // Only handle the case where all conditional branches branch to
    // the same destination.
    if (TBB != I->getOperand(0).getMBB())
      return true;

    MSP430CC::CondCodes OldBranchCode = (MSP430CC::CondCodes)Cond[0].getImm();
    // If the conditions are the same, we can leave them alone.
    if (OldBranchCode == BranchCode)
      continue;

    return true;
  }

  return false;
}

unsigned MSP430InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                       MachineBasicBlock *TBB,
                                       MachineBasicBlock *FBB,
                                       ArrayRef<MachineOperand> Cond,
                                       const DebugLoc &DL,
                                       int *BytesAdded) const {
  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.size() == 0) &&
         "MSP430 branch conditions have one component!");
  assert(!BytesAdded && "code size not handled");

  if (Cond.empty()) {
    // Unconditional branch?
    assert(!FBB && "Unconditional branch with multiple successors!");
    BuildMI(&MBB, DL, get(MSP430::JMP)).addMBB(TBB);
    return 1;
  }

  // Conditional branch.
  unsigned Count = 0;
  BuildMI(&MBB, DL, get(MSP430::JCC)).addMBB(TBB).addImm(Cond[0].getImm());
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(&MBB, DL, get(MSP430::JMP)).addMBB(FBB);
    ++Count;
  }
  return Count;
}

/// GetInstSize - Return the number of bytes of code the specified
/// instruction may be.  This returns the maximum number of bytes.
///
unsigned MSP430InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  const MCInstrDesc &Desc = MI.getDesc();

  switch (Desc.getOpcode()) {
  case TargetOpcode::CFI_INSTRUCTION:
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction *MF = MI.getParent()->getParent();
    const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
    return TII.getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                                  *MF->getTarget().getMCAsmInfo());
  }
  case MSP430::eexit:
  case MSP430::eenter:
  case MSP430::ereturn:
  case MSP430::rete:
  case MSP430::attest:
    // TODO: Not sure what ... (but code asserts when case is left out)
    return 12;
  }

  return Desc.getSize();
}

/// Compute the instruction latency of a given instruction.
/// If the instruction has higher cost when predicated, it's returned via
/// PredCost.
unsigned MSP430InstrInfo::getInstrLatency(const InstrItineraryData *ItinData,
                                          const MachineInstr &MI,
                                          unsigned *PredCost) const {
  unsigned L;

  // The LatencyTable below is generated by tablegen
  //   (see MSP430InstrLatencyInfo.cpp)
  auto &E = MSP430::LatencyTable[MI.getDesc().getOpcode()];
  L = E[0];

  // TODO: Figure out if it is the next instruction that should receive the
  //        PC-correction...
  if (MI.findRegisterDefOperandIdx(MSP430::PC) >= 0) {
    L += E[1];
  }

  return L;
}
unsigned int MSP430InstrInfo::getInstrMemTraceClass(const InstrItineraryData *ItinData, const MachineInstr &MI, StringRef AccessedMemregions,
                                                  unsigned int *PredCost) const {
    StringRef instr_class = "no class";
    if (AccessedMemregions == "dd"){
        auto &instr = MSP430::Instruction_classes_data_data[MI.getDesc().getOpcode()];
        instr_class = instr[1];
    }
    else if (AccessedMemregions == "pd") {
        auto &instr = MSP430::Instruction_classes_progr_data[MI.getDesc().getOpcode()];
        instr_class = instr[1];
    }
    else if (AccessedMemregions == "ped") {
        auto &instr = MSP430::Instruction_classes_per_data[MI.getDesc().getOpcode()];
        instr_class = instr[1];
    }
    else if (AccessedMemregions == "dpe") {
        auto &instr = MSP430::Instruction_classes_data_per[MI.getDesc().getOpcode()];
        instr_class = instr[1];
    }
    else if (AccessedMemregions == "ppe") {
        auto &instr = MSP430::Instruction_classes_progr_per[MI.getDesc().getOpcode()];
        instr_class = instr[1];
    }
    else if (AccessedMemregions == "pepe") {
        auto &instr = MSP430::Instruction_classes_per_per[MI.getDesc().getOpcode()];
        instr_class = instr[1];
    }

    if (instr_class.str().compare("1 | 0 | 0 | 1") == 0) {
        return 10;
    } else if (instr_class.str().compare("2 | 00 | 00 | 11") == 0) {
        return 20;
    } else if (instr_class.str().compare("2 | 00 | 10 | 01") == 0) {
        return 21;
    }  else if (instr_class.str().compare("3 | 000 | 010 | 101") == 0) {
        return 30;
    } else if (instr_class.str().compare("3 | 000 | 101 | 001") == 0) { // data mem
        return 31;
    } else if (instr_class.str().compare("3 | 000 | 001 | 001") == 0) {
        return 32;
    } else if (instr_class.str().compare("3 | 000 | 100 | 000") == 0) { // RET-instruction
        return 33;
    } else if (instr_class.str().compare("3 | 000 | 000 | 001") == 0) { // BR
        return 34;
    } else if (instr_class.str().compare("3 | 000 | 000 | 111") == 0) { // program mem
        return 35;
    } else if (instr_class.str().compare("4 | 0000 | 0101 | 1001") == 0) {
        return 40;
    } else if (instr_class.str().compare("4 | 0000 | 0001 | 1001") == 0) {
        return 41;
    } else if (instr_class.str().compare("5 | 00000 | 00101 | 11001") == 0) {
        return 50;
    } else if (instr_class.str().compare("5 | 00000 | 10101 | 10001") == 0) {
        return 51;
    } else if (instr_class.str().compare("5 | 00000 | 00001 | 11001") == 0) {
        return 52;
    } else if (instr_class.str().compare("5 | 00000 | 10001 | 10001") == 0) {
        return 53;
    } else if (instr_class.str().compare("5 | 00000 | 10000 | 00000") == 0) { // RETI-instruction
        return 54;
    } else if (instr_class.str().compare("6 | 000000 | 010101 | 110001") == 0) {
        return 60;
    } else if (instr_class.str().compare("6 | 000000 | 010001 | 110001") == 0) {
        return 61;
    } else if (instr_class.str().compare("6 | 000000 | 000101 | 111001") == 0) { // New for program memory
        return 660;
    } else if (instr_class.str().compare("6 | 000000 | 000001 | 111001") == 0) {
        return 661;
    } else if (instr_class.str().compare("2 | 10 | 00 | 01") == 0) { // New for peripheral memory
        return 2221;
    } else if (instr_class.str().compare("3 | 010 | 000 | 101") == 0) {
        return 3330;
    } else if (instr_class.str().compare("3 | 101 | 000 | 001") == 0) {
        return 3331;
    } else if (instr_class.str().compare("4 | 0001 | 0000 | 1001") == 0) {
        return 4440;
    } else if (instr_class.str().compare("4 | 0101 | 0000 | 1001") == 0) {
        return 4441;
    } else if (instr_class.str().compare("5 | 10000 | 00001 | 10001") == 0) {
        return 5550;
    } else if (instr_class.str().compare("5 | 10000 | 00101 | 10001") == 0) {
        return 5551;
    } else if (instr_class.str().compare("5 | 00001 | 10000 | 10001") == 0) {
        return 5552;
    } else if (instr_class.str().compare("5 | 00101 | 10000 | 10001") == 0) {
        return 5553;
    } else if (instr_class.str().compare("5 | 00001 | 00000 | 11001") == 0) {
        return 5554;
    } else if (instr_class.str().compare("5 | 00101 | 00000 | 11001") == 0) {
        return 5555;
    } else if (instr_class.str().compare("5 | 10001 | 00000 | 10001") == 0) {
        return 5556;
    } else if (instr_class.str().compare("5 | 10101 | 00000 | 10001") == 0) {
        return 5557;
    } else if (instr_class.str().compare("6 | 010000 | 000001 | 110001") == 0) {
        return 6660;
    } else if (instr_class.str().compare("6 | 010000 | 000101 | 110001") == 0) {
        return 6661;
    } else if (instr_class.str().compare("6 | 000001 | 010000 | 110001") == 0) {
        return 6662;
    } else if (instr_class.str().compare("6 | 000101 | 010000 | 110001") == 0) {
        return 6663;
    } else if (instr_class.str().compare("6 | 000001 | 000000 | 111001") == 0) {
        return 6664;
    } else if (instr_class.str().compare("6 | 000101 | 000000 | 111001") == 0) {
        return 6665;
    } else if (instr_class.str().compare("6 | 010001 | 000000 | 110001") == 0) {
        return 6666;
    } else if (instr_class.str().compare("6 | 010101 | 000000 | 110001") == 0) {
        return 6667;
    } else {
        return 0;
    }

}



/// Insert a noop into the instruction stream at the specified point.
void MSP430InstrInfo::insertNoop(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI) const {
  DebugLoc DL; // TODO: Is this correct?
  BuildMI(MBB, MI, DL, get(MSP430::MOV16ri), MSP430::CG).addImm(0);
}
