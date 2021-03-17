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
#include <fstream>
#include <boost/algorithm/string.hpp>



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

static std::vector<std::string> generate_source_operand_instructions(char addressing_mode, std::string opcode) {

  std::vector<std::string> source_operands;
  switch (addressing_mode) {
    case 'r':
      for (int i = 0; i < 16; i++){
          std::string instr = opcode + " r" + std::to_string(i) + ", ";
          source_operands.push_back(instr);
      }
      break;
    case 'n':
      for (int j = 4; j < 16; j++){
        std::string instr = "mov #0x0402, r" + std::to_string(j) + ";nop;";
        instr += opcode +" @r" + std::to_string(j) + ", ";
        source_operands.push_back(instr);
      }
      break;
    case 'm':
      // Indexed
      for (int j = 4; j < 16; j++){
        std::string instr = "mov #0x0402, r" + std::to_string(j) + ";nop;";
        instr += opcode +" 2(r" + std::to_string(j) + "), ";
        source_operands.push_back(instr);
      }

      // Symbolic

      std::string inst = opcode + " 0x0402, ";
      source_operands.push_back(inst);

      // Absolute
      inst = "mov #0x0402, 0x0402;nop;";
      inst = opcode + " &0x0402, ";
      source_operands.push_back(inst);


  }
  return source_operands;
}

static std::vector<std::pair<std::string,std::string>> ComputeMemoryTraceClass(const CodeGenInstruction *II, raw_ostream &OS) {


  std::vector<std::pair<std::string,std::string>> gen_instr_class_pairs;
  std::string instr_class;
  std::string instr_to_simulate;

  std::ifstream fs;

  std::string opcode = "";
  std::string AsmString = "";
  if (!II->AsmString.empty()) {
      AsmString += CodeGenInstruction::FlattenAsmStringVariants(II->AsmString, 0);
      unsigned i = 0;
      while (i < AsmString.length() && AsmString.at(i) != '\t') {
        opcode += AsmString.at(i);
        i++;
      }
      if (opcode == "j$cond") {
        opcode = "jc";
      }
  }

  Record *Inst = II->TheDef;
  StringRef instruction_name = Inst->getName();

  if (Inst-> isSubClassOf("IForm"))  {


    BitsInit* As = (Inst->getValueAsBitsInit("As"));
    BitsInit* Ad = Inst->getValueAsBitsInit("Ad");


    switch (getValueFromBitsInit(As)) {
      case 0: // Register mode
        if (opcode.rfind("br", 0) == 0) { // Br
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","simulation fails"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
              //INS#rrtouch ../../sllvm/llvm/utils/TableGen/MSP430InstrMemTraceInfo.cpp
              //R4 and R5 used
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, r5","1 | 0 | 0 | 1"));
              break;

            case 1:
              //INS#mr
              //R4 and R5 used
              // Indexed and symbolic
              if (opcode.rfind("mov", 0) == 0) { // MOV
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " r4, 2(r5)","4 | 0000 | 0001 | 1001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, 0xDFDE","4 | 0000 | 0001 | 1001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, &0x0406","4 | 0000 | 0001 | 1001"));
              } else {
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " r4, 2(r5)","4 | 0000 | 0101 | 1001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, 0xDFDE","4 | 0000 | 0101 | 1001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, &0x0406","4 | 0000 | 0101 | 1001"));
              }
              break;

          }

        }
        break;

      case 1: // Indexed, symbolic, absolute
        if (opcode.rfind("br", 0) == 0) { // Bm
          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4)", "simulation fails"));
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0xDFDE","simulation fails"));
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " &0x0402","simulation fails"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
              //INS#rm
              // TODO: wat is B?
              gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4), r5", "3 | 000 | 010 | 101"));
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0xDFDE, r5","3 | 000 | 010 | 101"));
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " &0x0402, r5","3 | 000 | 010 | 101"));

              break;
            case 1:
              //INS#mm
              std::vector<std::string> v;
              // Indexed source mode
              v.push_back("mov #0x0402, r4;nop;" + opcode +  " 2(r4), ");
              // Absolute source mode
              v.push_back(opcode + " &0x0402, ");
              // Symbolic source mode
              v.push_back(opcode + " 0xDFDE, ");

              // Indexed dest mode
              if (opcode.rfind("mov", 0) == 0) { // MOV
                for (int i = 0; i < 3; i++) {
                  gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;"+ v[i] + "2(r5)","6 | 000000 | 010001 | 110001"));
                  gen_instr_class_pairs.push_back(std::make_pair(v[i] + "0xDFDE","6 | 000000 | 010001 | 110001"));
                  gen_instr_class_pairs.push_back(std::make_pair(v[i] + "&0x0406","6 | 000000 | 010001 | 110001"));
                }
              } else {
                for (int i = 0; i < 3; i++) {
                  gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;"+ v[i] + "2(r5)","6 | 000000 | 010101 | 110001"));
                  gen_instr_class_pairs.push_back(std::make_pair(v[i] + "0xDFDE","6 | 000000 | 010101 | 110001"));
                  gen_instr_class_pairs.push_back(std::make_pair(v[i] + "&0x0406","6 | 000000 | 010101 | 110001"));
                }
              }
              break;
            }

        }
        break;

      case 2:
        if (opcode.rfind("br", 0) == 0) { // Bm
        gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4","simulation fails"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
              gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4, r5","2 | 00 | 10 | 01"));
            break;
            case 1:
              //INS#mn
              if (opcode.rfind("mov", 0) == 0) { // MOV
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;mov #0x0402, r4;nop;" + opcode + " @r4, 2(r5)","5 | 00000 | 10001 | 10001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4, 0xDFDE","5 | 00000 | 10001 | 10001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4, &0x0406","5 | 00000 | 10001 | 10001"));
              } else {
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;mov #0x0402, r4;nop;" + opcode + " @r4, 2(r5)","5 | 00000 | 10101 | 10001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4, 0xDFDE","5 | 00000 | 10101 | 10001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4, &0x0406","5 | 00000 | 10101 | 10001"));
              }
            break;
          }
        break;
      }

      case 3:
        if (opcode.rfind("br", 0) == 0) { // BRCALLi
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0046","simulation fails"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
              if (instruction_name.back() == 'i') { // INS#ri, don't use constant generator
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0045, r5","2 | 00 | 00 | 11"));
              }
              else { // INS#rp
                if (opcode == "ret"){
                  gen_instr_class_pairs.push_back(std::make_pair(opcode,"3 | 000 | 100 | 000"));
                }  else {
                  if (opcode.rfind("pop", 0) == 0) { // POP
                    gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","2 | 00 | 10 | 01"));
                    gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4","5 | 00000 | 10001 | 10001"));
                  } else {
                    gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4+, r5","2 | 00 | 10 | 01"));

                  }

                }

              }
              break;
            case 1:
            if (instruction_name.back() == 'i') { // INS#mi, don't use constant generator
              if (opcode.rfind("mov", 0) == 0) { // MOV
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " #0x0045, 2(r5)","5 | 00000 | 00001 | 11001"));
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;" + opcode + " #0x0045, 0xDFDE","5 | 00000 | 00001 | 11001"));
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;" + opcode + " #0x0045, &0x0406","5 | 00000 | 00001 | 11001"));
              } else {
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " #0x0045, 2(r5)","5 | 00000 | 00101 | 11001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0045, 0xDFDE","5 | 00000 | 00101 | 11001"));
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0045, &0x0406","5 | 00000 | 00101 | 11001"));

              }


            }
            else { // INS#mp (no MOV instructions here)
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;mov #0x0406, r5;nop;" + opcode + " @r4+, 2(r5)","5 | 00000 | 10101 | 10001"));
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4+, 0xDFDE","5 | 00000 | 10101 | 10001"));
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4+, &0x0406","5 | 00000 | 10101 | 10001"));
            }
            break;
          }
          break;
        }

    }

  }

  else if (Inst->isSubClassOf("IIForm")
             || Inst->isSubClassOf("II16c")
             || Inst->isSubClassOf("II8c") ) {
    auto OpCodeValue = getValueFromBitsInit(Inst->getValueAsBitsInit("Opcode"));
    if (OpCodeValue == 6)  { // RETI
      gen_instr_class_pairs.push_back(std::make_pair(opcode,"5 | 00000 | 10000 | 00000"));
    } else {
      uint16_t As = 0; // II16c and II8c : constant generators used, register mode
      if (Inst->isSubClassOf("IIForm")) {
        As = getValueFromBitsInit(Inst->getValueAsBitsInit("As"));
      }
      switch (As) {
        case 0:
          if (opcode.rfind("br", 0) == 0 || opcode.rfind("call",0) == 0) { // BRCALLr
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","simulation fails"));
          } else {
            if (opcode.rfind("push",0) == 0) { // PUSH
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","3 | 000 | 001 | 001"));
            } else {
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","1 | 0 | 0 | 1"));
            }

          }
          break;
        case 1:
          if (opcode.rfind("br", 0) == 0 || opcode.rfind("call",0) == 0) { // BRCALLm
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4)","simulation fails"));
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0xDFDE","simulation fails"));
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " &0x0402","simulation fails"));
          } else {
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4)","4 | 0000 | 0101 | 1001"));
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0xDFDE","4 | 0000 | 0101 | 1001"));
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " &0x0402","4 | 0000 | 0101 | 1001"));
          }

          break;
        case 2:
          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4","3 | 000 | 101 | 001"));
          break;
        case 3:
          if (instruction_name.back() == 'i') { // INS#i, use constant generator or not
            if (opcode.rfind("br", 0) == 0 || opcode.rfind("call",0) == 0) { // BRCALLi
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x00046","simulation fails"));
            } else {
              if (opcode.rfind("push",0) == 0) { // PUSH
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x00046","4 | 0000 | 0001 | 1001"));
              } else {
                // no cases yet
              }

            }

          }
          else { // INS#p
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4+","3 | 000 | 101 | 001"));
          }
          break;
        default:
          llvm_unreachable("Invalid As value");
      }
    }
  }

  else if (Inst->isSubClassOf("CJForm")) {
    gen_instr_class_pairs.push_back(std::make_pair("nothing yet","no class"));
  }

  // Constant generators
  else if (Inst -> isSubClassOf("I8rc") || Inst->isSubClassOf("I8mc") || Inst->isSubClassOf("I16rc") || Inst-> isSubClassOf("I16mc")) {
    BitsInit* Ad = Inst->getValueAsBitsInit("Ad");
    switch (getValueFromBitsInit(Ad)) {
      case 0:
        //INS#rc
        gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008, r5","1 | 0 | 0 | 1"));
        break;

      case 1:
        //INS#mc
        if (opcode.rfind("mov", 0) == 0) { // MOV
          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " #0x0008, 2(r5)","4 | 0000 | 0001 | 1001"));
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008, 0xDFDE","4 | 0000 | 0001 | 1001"));
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008, &0x0406","4 | 0000 | 0001 | 1001"));
        } else {
          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " #0x0008, 2(r5)","4 | 0000 | 0101 | 1001"));
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008, 0xDFDE","4 | 0000 | 0101 | 1001"));
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008, &0x0406","4 | 0000 | 0101 | 1001"));
        }

        break;

    }
  }
  else if (Inst->isSubClassOf("Pseudo")) {
    gen_instr_class_pairs.push_back(std::make_pair("nothing yet","no class"));
  }
  else {
    gen_instr_class_pairs.push_back(std::make_pair("nothing yet","no class"));
  }

  return gen_instr_class_pairs;
}



void MSP430InstrMemTraceInfo::run(raw_ostream &OS) {
  emitSourceFileHeader("MSP430 Instruction Memory Traces", OS);

  CodeGenTarget &Target = CDP.getTargetInfo();
  StringRef Namespace = Target.getInstNamespace();

  OS << "namespace llvm {\n\n";
  OS << "namespace " << Namespace << " {\n";

  OS << "static const StringRef Instruction_classes[][2] = {\n";


  unsigned Num = 0;
  for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

    Record *Inst = II->TheDef;
    auto generated_instructions = ComputeMemoryTraceClass(II, OS);
    OS << "/* " << Num << "*/ "
       << "{"
       << "\"" << generated_instructions[0].first;
    for (unsigned i = 1; i < generated_instructions.size(); i++){
        OS  << " --- " << generated_instructions[i].first;

    }
    OS  << "\", \""
        << generated_instructions[0].second << "\"}, "
        << "// " << Namespace << "::" << Inst->getName() << "\n";
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
