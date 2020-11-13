//===- SkeletonEmitter.cpp - Skeleton TableGen backend          -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This Tablegen backend emits ...
//
//===----------------------------------------------------------------------===//



#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

#include "CodeGenTarget.h"
#include "CodeGenDAGPatterns.h"
#include "MSP430InstrLatencyInfo.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include <iostream>

#define DEBUG_TYPE "skeleton-emitter"

using namespace llvm;

namespace {

// Any helper data structures can be defined here. Some backends use
// structs to collect information from the records.

class MSP430InstrMemTraceInfo {
private:
  RecordKeeper &Records;
  CodeGenDAGPatterns CDP;

public:
  MSP430InstrMemTraceInfo(RecordKeeper &RK) : Records(RK), CDP(RK) {}

  void run(raw_ostream &OS);
}; // emitter class

} // anonymous namespace



// Calculates the integer value representing the BitsInit object
static inline uint64_t getValueFromBitsInit(const BitsInit *B) {
  assert(B->getNumBits() <= sizeof(uint64_t) * 8 && "BitInits' too long!");

  uint64_t Value = 0;
  for (unsigned i = 0, e = B->getNumBits(); i != e; ++i) {
    BitInit *Bit = cast<BitInit>(B->getBit(i));
    Value |= uint64_t(Bit->getValue()) << i;
  }
  return Value;
}

static std::vector<std::string> ComputeMemoryTrace(const CodeGenInstruction *II, raw_ostream &OS) {

  //TODO: make this an enum?
  std::vector<std::string> special_purpose_registers = {"r0", "r1", "r2", "r3" };

  std::vector<std::string> gen_instr;
  std::string opcode = "";
  if (!II->AsmString.empty()) {
      std::string AsmString =CodeGenInstruction::FlattenAsmStringVariants(II->AsmString, 0);
      unsigned i = 0;
      while (i < AsmString.length() && AsmString.at(i) != '\t') {
        opcode += AsmString.at(i);
        i++;
      }
  }

  Record *Inst = II->TheDef;

  if (Inst-> isSubClassOf("IForm"))  { // TODO: e.g. Ixmc instructions


    BitsInit* As = (Inst->getValueAsBitsInit("As"));
    BitsInit* Ad = Inst->getValueAsBitsInit("Ad");

    switch (getValueFromBitsInit(As)) {
      case 0: // Register mode
        switch (getValueFromBitsInit(Ad)) {
          case 0:
            //INS#rr

            // Special purpose registers
            for (int i = 0; i < special_purpose_registers.size(); i++){
              for (int j = 0; j < special_purpose_registers.size(); j++){
                std::string instr = opcode + " " + special_purpose_registers[i] + ", " + special_purpose_registers[j];
                gen_instr.push_back(instr);
              }

            }

            break;

          case 1:
            //INS#mr
            gen_instr.push_back("");
        }

        break;
      case 1: // Indexed, symbolic, absolute
      switch (getValueFromBitsInit(Ad)) {
        case 0:
          gen_instr.push_back("");
      }
        gen_instr.push_back("");
        break;
      case 2:
      switch (getValueFromBitsInit(Ad)) {
        case 0:
          gen_instr.push_back("");
      }
        break;
      case 3:
        gen_instr.push_back("");
        break;
    }

  }

  else gen_instr.push_back("nothing yet");

  return gen_instr;
}



void MSP430InstrMemTraceInfo::run(raw_ostream &OS) {
  emitSourceFileHeader("MSP430 Instruction Memory Traces", OS);

  CodeGenTarget &Target = CDP.getTargetInfo();
  //const std::string &TargetName = Target.getName();
  StringRef Namespace = Target.getInstNamespace();
  //Record *InstrInfo = Target.getInstructionSet();

  //OS << "#ifdef GET_INSTRINFO_LATENCY_DESC\n";
  //OS << "#undef GET_INSTRINFO_LATENCY_DESC\n";

  OS << "namespace llvm {\n\n";
  OS << "namespace " << Namespace << " {\n";

  // TODO: Generate documentation containing the following information
  //
  // Table of (latency, PC-correction) entries
  // For format-I MSP430 instructions, the instruction latency can differ
  // with one cycle when
  //    1) the destination addressing mode (Ad) is register mode
  //    2) and the program counter is the destination register.
  // This behavior is represented by the second element of a latency table entry.


  unsigned VariantCount = Target.getAsmParserVariantCount();

  OS << "static const string Generated_Instructions[][1] = {\n";


  unsigned Num = 0;
  for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {


    Record *Inst = II->TheDef;
    auto generated_instructions = ComputeMemoryTrace(II, OS);

    for (int i = 0; i < generated_instructions.size(); i++){
      OS << "/* " << Num << "*/ "
          << "{" << generated_instructions[i] << "}, "
          << "// " << Namespace << "::" << Inst->getName() << "\n";
    }
    Num++;



  }

  OS << "};\n";
  OS << "} // end namespace " << Namespace << "\n";
  OS << "} // end namespace llvm\n";
  OS << "#endif // GET_INSTRINFO_LATENCY_DESC\n";
}

namespace llvm {

// The only thing that should be in the llvm namespace is the
// emitter entry point function.

void EmitMSP430InstrMemTraceInfo(RecordKeeper &RK, raw_ostream &OS) {
  // Instantiate the emitter class and invoke run().
  MSP430InstrMemTraceInfo(RK).run(OS);


}

} // namespace llvm
