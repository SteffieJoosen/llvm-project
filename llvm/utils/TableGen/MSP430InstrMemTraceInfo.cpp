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

#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include <iostream>

#define DEBUG_TYPE "skeleton-emitter"

using namespace llvm;
using namespace std;

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

static vector<string> generate_source_operand_instructions(char addressing_mode, string opcode) {

  vector<string> source_operands;
  switch (addressing_mode) {
    case 'r':
      for (int i = 0; i < 16; i++){
          string instr = opcode + " r" + to_string(i) + ", ";
          source_operands.push_back(instr);
      }
      break;
    case 'm':
      // Indexed
      for (int j = 4; j < 16; j++){
        string instr = "mov r" + to_string(j) + ", #3999; ";
        instr += opcode +" 1(r" + to_string(j) + "), ";
        source_operands.push_back(instr);
      }

      // Symbolic
      string inst = opcode + " 4000, ";
      source_operands.push_back(inst);

      // Absolute
      inst = opcode + " &4000, ";
      source_operands.push_back(inst);


  }
  return source_operands;
}

static vector<string> ComputeMemoryTrace(const CodeGenInstruction *II, raw_ostream &OS) {


  vector<string> gen_instr;
  string opcode = "";
  if (!II->AsmString.empty()) {
      string AsmString =CodeGenInstruction::FlattenAsmStringVariants(II->AsmString, 0);
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

   vector<string> source_reg_instructions = generate_source_operand_instructions('r', opcode);
   vector<string> source_mem_instructions = generate_source_operand_instructions('m', opcode);



    switch (getValueFromBitsInit(As)) {
      case 0: // Register mode


        switch (getValueFromBitsInit(Ad)) {
          case 0:
            //INS#rr
            // Special & General purpose registers together
            for (int i = 0; i < 16; i++){
              for (int j = 0; j < 16; j++){
                string instr = source_reg_instructions[i] +"r" + to_string(j);
                gen_instr.push_back(instr);
              }
            }
            break;

          case 1:
            //INS#mr
            // Indexed mode, use R4 to R15
            for (int i = 0; i < 16; i++){
              for (int j = 4; j < 16; j++){
                string instr = "mov r" + to_string(j) + ", #2999; ";
                instr += source_reg_instructions[i] +"1(r" + to_string(j) + ")";
                gen_instr.push_back(instr);
              }
            }

            // Symbolic mode
            for (int i = 4; i < 16; i++){
                string instr = source_reg_instructions[i] +"3000";
                gen_instr.push_back(instr);

            }

            // Absolute mode
            for (int i = 4; i < 16; i++){
                string instr = source_reg_instructions[i] +"&3000";
                gen_instr.push_back(instr);

            }
            break;

        }
        break;
      case 1: // Indexed, symbolic, absolute
        switch (getValueFromBitsInit(Ad)) {
          case 0:
            //INS#rm
            // TODO: wat is B?
            // Only twelve general purpose registers for indexed mode + 2 other addressing modes (Sym & Abs)
            for (int i = 0; i < 14; i++){
              for (int j = 0; j < 16; j++) {
                string instr = source_mem_instructions[i] +"r" + to_string(j);
                gen_instr.push_back(instr);
              }
            }
            break;

        }
        break;
      case 2:
        switch (getValueFromBitsInit(Ad)) {
          case 0:
            gen_instr.push_back("Indirect");
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
  StringRef Namespace = Target.getInstNamespace();

  OS << "namespace llvm {\n\n";
  OS << "namespace " << Namespace << " {\n";

  OS << Target.getInstructionsByEnumValue()[1]->TheDef->getName() << "\n\n";
  OS << "static const string Generated_Instructions[][1] = {\n";


  unsigned Num = 0;
  for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

    Record *Inst = II->TheDef;
    auto generated_instructions = ComputeMemoryTrace(II, OS);

    for (unsigned i = 0; i < generated_instructions.size(); i++){
      OS << "/* " << Num << "*/ "
          << "{" << generated_instructions[i] << "}, "
          << "// " << Namespace << "::" << Inst->getName() << "\n";
    }
    Num++;



  }

  OS << "};\n";
  OS << "} // end namespace " << Namespace << "\n";
  OS << "} // end namespace llvm\n";
}

namespace llvm {

// The only thing that should be in the llvm namespace is the
// emitter entry point function.

void EmitMSP430InstrMemTraceInfo(RecordKeeper &RK, raw_ostream &OS) {
  // Instantiate the emitter class and invoke run().
  MSP430InstrMemTraceInfo(RK).run(OS);


}

} // namespace llvm
