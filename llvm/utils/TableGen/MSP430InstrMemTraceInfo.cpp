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
#include <boost/algorithm/string.hpp>

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
    case 'n':
      for (int j = 4; j < 16; j++){
        string instr = "mov #0x0202, r" + to_string(j) + ";nop;";
        instr += opcode +" @r" + to_string(j) + ", ";
        source_operands.push_back(instr);
      }
      break;
    case 'm':
      // Indexed
      for (int j = 4; j < 16; j++){
        string instr = "mov #0x0202, r" + to_string(j) + ";nop;";
        instr += opcode +" 2(r" + to_string(j) + "), ";
        source_operands.push_back(instr);
      }

      // Symbolic
      string inst = opcode + " 0x0202, ";
      source_operands.push_back(inst);

      // Absolute
      inst = opcode + " &0x0202, ";
      source_operands.push_back(inst);


  }
  return source_operands;
}

static vector<string> ComputeMemoryTrace(const CodeGenInstruction *II, raw_ostream &OS) {


  vector<string> gen_instr;
  string opcode = "";
  if (!II->AsmString.empty()) {
      string AsmString = CodeGenInstruction::FlattenAsmStringVariants(II->AsmString, 0);
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

    // For instructions that use register, these vectors are for generating the instructions
    // using all combinations of them
    vector<string> source_reg_instructions = generate_source_operand_instructions('r', opcode);
    vector<string> source_mem_instructions = generate_source_operand_instructions('m', opcode);
    vector<string> source_ind_instructions = generate_source_operand_instructions('n', opcode);



    switch (getValueFromBitsInit(As)) {
      case 0: // Register mode


        switch (getValueFromBitsInit(Ad)) {
          case 0:
            //INS#rr
            //R4 and R5 used
            gen_instr.push_back(opcode + " r4, r5");
            // Special & General purpose registers together
            /*for (int i = 0; i < 16; i++){
              for (int j = 0; j < 16; j++){
                string instr = source_reg_instructions[i] +"r" + to_string(j);
                gen_instr.push_back(instr);
              }
            }*/
            break;

          case 1:
            //INS#mr
            //R4 and R5 used
            // Indexed
            string instr = "mov #0x0206, r5;nop;";
            gen_instr.push_back(instr + opcode + " r4, 2(r5)");
            // Indexed mode, use R4 to R15
            /*for (int i = 0; i < 16; i++){
              for (int j = 4; j < 16; j++){

                string instr = "mov #0x0206, r" + to_string(j) + ";";
                instr += source_reg_instructions[i] +"2(r" + to_string(j) + ")";
                gen_instr.push_back(instr);
              }
            }*/

            // Symbolic mode
            // R4 used
            gen_instr.push_back(opcode + " r4, 0x0206");
            /*for (int i = 4; i < 16; i++){
                string instr = source_reg_instructions[i] +"0x0206";
                gen_instr.push_back(instr);

            }*/

            // Absolute mode
            // R4 used
            gen_instr.push_back(opcode + " r4, &0x0206");
            /*for (int i = 4; i < 16; i++){
                string instr = source_reg_instructions[i] +"&0x0206";
                gen_instr.push_back(instr);

            }*/
            break;

        }
        break;
      case 1: // Indexed, symbolic, absolute
        switch (getValueFromBitsInit(Ad)) {
          case 0:
            //INS#rm
            // TODO: wat is B?
            gen_instr.push_back("mov #0x0202, r4;nop;" + opcode +  " 2(r4), r5");
            gen_instr.push_back(opcode + " 0x0202, r4");
            gen_instr.push_back(opcode + " &0x0202, r4");
            /*for (int i = 0; i < 14; i++){ // Only twelve general purpose registers for indexed mode + 2 other addressing modes (Sym & Abs)
              for (int j = 0; j < 16; j++) {
                string instr = source_mem_instructions[i] +"r" + to_string(j);
                gen_instr.push_back(instr);
              }
            }*/
            break;
          case 1:
            //INS#mm
            std::vector<string> v;
            // Indexed source mode
            v.push_back("mov #0x0202, r4;nop;" + opcode +  " 2(r4), ");
            // Symbolic source mode
            v.push_back(opcode + " 0x0202, ");
            // Absolute mode
            v.push_back(opcode + " &0x0202, ");

            // Indexed dest mode
            for (int i = 0; i < 3; i++) {
              gen_instr.push_back("mov #0x0206, r5;nop;"+ v[i] + "2(r5)");
              gen_instr.push_back(v[i] + "0x0206");
              gen_instr.push_back(v[i] + "&0x0206");
            }

            // Indexed mode, use R4 to R15: on index i, register i+4 is used for indexed mode
            /*for (int i = 0; i < 12; i++){ // Only twelve general purpose registers for indexed mode
              for (int j = 4; j < 16; j++){
                if (i <12) {
                  if (i != j-4) {
                    string instr = "mov #0x0206, r" + to_string(j) + ";";
                    instr += source_mem_instructions[i] + "2(r" + to_string(j) + ")";
                    gen_instr.push_back(instr);
                  }

                } else {
                  string instr = "mov #0x0206, r" + to_string(j) + ";";
                  instr += source_mem_instructions[i] + "2(r" + to_string(j) + ")";
                  gen_instr.push_back(instr);
                }
              }
            }*/
            break;
          }
        break;
      case 2:
        switch (getValueFromBitsInit(Ad)) {
          case 0:
            gen_instr.push_back("mov #0x0202, r4;nop;" + opcode + " @r4, r5");
          //INS#rn
          /*for (int i = 0; i < 12; i++){ // Only twelve general purpose registers for indirect mode
            for (int j = 0; j < 16; j++) {
              string instr = source_ind_instructions[i] +"r" + to_string(j);
              gen_instr.push_back(instr);
            }
          }*/
          break;
          case 1:
            //INS#mn
            gen_instr.push_back("mov #0x0206, r5;nop;mov #0x0202, r4;nop;" + opcode + " @r4, 2(r5)");
            gen_instr.push_back("mov #0x0202, r4;nop;" + opcode + " @r4, 0x0206");
            gen_instr.push_back("mov #0x0202, r4;nop;" + opcode + " @r4, &0x0206");


            // Indexed mode, use R4 to R15
            /*for (int i = 0; i < 12; i++){
              for (int j = 4; j < 16; j++){
                if (i != j-4){
                  string instr = "mov #0x0206, r" + to_string(j) + ";";
                  instr += source_ind_instructions[i] +"2(r" + to_string(j) + ")";
                  gen_instr.push_back(instr);
                }
              }
            }
            // Symbolic mode
            for (int i = 0; i < 12; i++){
              string instr = source_ind_instructions[i] +"0x0206";
              gen_instr.push_back(instr);
            }

          // Absolute mode
          for (int i = 0; i < 12; i++){
              string instr = source_reg_instructions[i] +"&0x0206";
              gen_instr.push_back(instr);

          }*/
          break;

        }
        break;
      case 3:
        gen_instr.push_back("nothing yet");
        break;
    }

  }

  else if (   Inst->isSubClassOf("IIForm")
             || Inst->isSubClassOf("II16c")
             || Inst->isSubClassOf("II8c") ) {
    auto OpCode = getValueFromBitsInit(Inst->getValueAsBitsInit("Opcode"));
    if (OpCode == 6)  { // RETI
      gen_instr.push_back(AsmString);
    } else {
      uint16_t As = 0; // II16c and II8c : constant generators used, register mode
      if (Inst->isSubClassOf("IIForm")) {
        As = getValueFromBitsInit(Inst->getValueAsBitsInit("As"));
      }
      switch (As) {
        case 0:
          gen_instr.push_back(opcode + " r4");
          break;
          // 16 december: tot hier geraakt
        case 1:
          latency = (OpCode == 4 || OpCode == 5) ? 5 : 4;
          break;
        case 2:
          latency = (OpCode == 4 || OpCode == 5) ? 4 : 3;
          break;
        case 3:
          // Opcodes: PUSH=4, CALL=5
          switch (OpCode) {
            case 4:
              latency = Inst->getValue("imm") ? 4 : 5;
              break;
            case 5:
              latency = 5;
              break;
            default:
              latency = 4;
              break; // RRA, RRC, SWPB, SXT
          }
          break;
        default:
          llvm_unreachable("Invalid As value");
      }
    }
  }

  else {
    gen_instr.push_back("nothing yet")
  }

  return gen_instr;
}



void MSP430InstrMemTraceInfo::run(raw_ostream &OS) {
  emitSourceFileHeader("MSP430 Instruction Memory Traces", OS);

  CodeGenTarget &Target = CDP.getTargetInfo();
  StringRef Namespace = Target.getInstNamespace();

  OS << "namespace llvm {\n\n";
  OS << "namespace " << Namespace << " {\n";

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
