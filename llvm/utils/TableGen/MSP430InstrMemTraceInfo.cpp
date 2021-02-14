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
  }

  Record *Inst = II->TheDef;
  StringRef instruction_name = Inst->getName();

  if (Inst-> isSubClassOf("IForm"))  {


    BitsInit* As = (Inst->getValueAsBitsInit("As"));
    BitsInit* Ad = Inst->getValueAsBitsInit("Ad");

    // For instructions that use register, these vectors are for generating the instructions
    // using all combinations of them
    std::vector<std::string> source_reg_instructions = generate_source_operand_instructions('r', opcode);
    std::vector<std::string> source_mem_instructions = generate_source_operand_instructions('m', opcode);
    std::vector<std::string> source_ind_instructions = generate_source_operand_instructions('n', opcode);

    char my_chars[] = {'S','t','e', '\0'};


    switch (getValueFromBitsInit(As)) {
      case 0: // Register mode


        switch (getValueFromBitsInit(Ad)) {
          case 0:
            //INS#rr
            //R4 and R5 used
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, r5","1 | 0 | 0 | 1"));
            break;

          case 1:
            //INS#mr
            //R4 and R5 used
            // Indexed
            std::string instr = "mov #0x0406, r5;nop;";
            gen_instr_class_pairs.push_back(std::make_pair(instr + opcode + " r4, 2(r5)","class"));
            // Indexed mode, use R4 to R15
            /*for (int i = 0; i < 16; i++){
              for (int j = 4; j < 16; j++){

                std::string instr = "mov #0x0406, r" + std::to_string(j) + ";";
                instr += source_reg_instructions[i] +"2(r" + std::to_string(j) + ")";
                gen_instr.push_back(instr);
              }
            }*/

            // Symbolic mode
            // R4 used
            // This instruction gave rise to an unexpected clock cycle extra
            //gen_instr.push_back(opcode + " r4, 0x0406");
            /*for (int i = 4; i < 16; i++){
                std::string instr = source_reg_instructions[i] +"0x0406";
                gen_instr.push_back(instr);

            }*/

            // Absolute mode
            // R4 used
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;" + opcode + " r4, &0x0406","class"));
            /*for (int i = 4; i < 16; i++){
                std::string instr = source_reg_instructions[i] +"&0x0406";
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
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4), r5", "class"));
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0x0402, r5","class"));
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, 0x0402;nop;" + opcode + " &0x0402, r5","class"));
            /*for (int i = 0; i < 14; i++){ // Only twelve general purpose registers for indexed mode + 2 other addressing modes (Sym & Abs)
              for (int j = 0; j < 16; j++) {
                std::string instr = source_mem_instructions[i] +"r" + std::to_string(j);
                gen_instr.push_back(instr);
              }
            }*/
            break;
          case 1:
            //INS#mm
            std::vector<std::string> v;
            // Indexed source mode
            v.push_back("mov #0x0402, r4;nop;" + opcode +  " 2(r4), ");
            // Symbolic source mode
            v.push_back(opcode + " 0x0402, ");
            // Absolute mode
            v.push_back("mov #0x0402, 0x0402;nop;" + opcode + " &0x0402, ");

            // Indexed dest mode
            for (int i = 0; i < 3; i++) {
              gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;"+ v[i] + "2(r5)","cl"));
              // This instruction gave rise to an unexpected clock cycle extra
              //gen_instr.push_back(v[i] + "0x0406");
              gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;"+ v[i] + "&0x0406","cl"));
            }

            // Indexed mode, use R4 to R15: on index i, register i+4 is used for indexed mode
            /*for (int i = 0; i < 12; i++){ // Only twelve general purpose registers for indexed mode
              for (int j = 4; j < 16; j++){
                if (i <12) {
                  if (i != j-4) {
                    std::string instr = "mov #0x0406, r" + std::to_string(j) + ";";
                    instr += source_mem_instructions[i] + "2(r" + std::to_string(j) + ")";
                    gen_instr.push_back(instr);
                  }

                } else {
                  std::string instr = "mov #0x0406, r" + std::to_string(j) + ";";
                  instr += source_mem_instructions[i] + "2(r" + std::to_string(j) + ")";
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
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4, r5","cl"));
          //INS#rn
          /*for (int i = 0; i < 12; i++){ // Only twelve general purpose registers for indirect mode
            for (int j = 0; j < 16; j++) {
              std::string instr = source_ind_instructions[i] +"r" + std::to_string(j);
              gen_instr.push_back(instr);
            }
          }*/
          break;
          case 1:
            //INS#mn
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;mov #0x0402, r4;nop;" + opcode + " @r4, 2(r5)","cl"));
            // This instruction gave rise to an unexpected extra clock cycle
            //gen_instr.push_back("mov #0x0402, r4;nop;" + opcode + " @r4, 0x0406");
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;mov #0x0406, 0x0406;nop;" + opcode + " @r4, &0x0406","cl"));


            // Indexed mode, use R4 to R15
            /*for (int i = 0; i < 12; i++){
              for (int j = 4; j < 16; j++){
                if (i != j-4){
                  std::string instr = "mov #0x0406, r" + std::to_string(j) + ";";
                  instr += source_ind_instructions[i] +"2(r" + std::to_string(j) + ")";
                  gen_instr.push_back(instr);
                }
              }
            }
            // Symbolic mode
            for (int i = 0; i < 12; i++){
              std::string instr = source_ind_instructions[i] +"0x0406";
              gen_instr.push_back(instr);
            }

          // Absolute mode
          for (int i = 0; i < 12; i++){
              std::string instr = source_reg_instructions[i] +"&0x0406";
              gen_instr.push_back(instr);

          }*/
          break;

        }
        break;
      case 3:

        switch (getValueFromBitsInit(Ad)) {
          case 0:
            if (instruction_name.back() == 'i') { // INS#ri, don't use constant generator
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0045, r5","cl"));
            }
            else { // INS#rp
              if (opcode == "ret"){
                gen_instr_class_pairs.push_back(std::make_pair(opcode,"cl"));
              }  else {
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4+, r5","cl"));
              }

            }
            break;
          case 1:
          if (instruction_name.back() == 'i') { // INS#mi, don't use constant generator
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " #0x0045, 2(r5)","cl"));
            // This instruction gave rise to an unexpected extra clock cycle
            //gen_instr.push_back(opcode + " #0x0045, 0x0406");
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;" + opcode + " #0x0045, &0x0406","cl"));
          }
          else { // INS#mp
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " @r4+, 2(r5)","cl"));
            // This instruction gave rise to an unexpected extra clock cycle
            //gen_instr.push_back(opcode + " @r4+, 0x0406");
            gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;" + opcode + " @r4+, &0x0406","cl"));
          }

          break;
        }
      break;
    }

  }

  else if (Inst->isSubClassOf("IIForm")
             || Inst->isSubClassOf("II16c")
             || Inst->isSubClassOf("II8c") ) {
    auto OpCodeValue = getValueFromBitsInit(Inst->getValueAsBitsInit("Opcode"));
    if (OpCodeValue == 6)  { // RETI
      gen_instr_class_pairs.push_back(std::make_pair(opcode,"cl"));
    } else {
      uint16_t As = 0; // II16c and II8c : constant generators used, register mode
      if (Inst->isSubClassOf("IIForm")) {
        As = getValueFromBitsInit(Inst->getValueAsBitsInit("As"));
      }
      switch (As) {
        case 0:
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","cl"));
          break;
        case 1:
        gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4)","cl"));
        gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0x0402","cl"));
        gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, 0x0402;nop;" + opcode + " &0x0402","cl"));
          break;
        case 2:
          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4","cl"));
          break;
        case 3:
          if (instruction_name.back() == 'i') { // INS#i, use constant generator or not
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x00045","cl"));
          }
          else { // INS#p
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " @r4+","cl"));
          }
          break;
        default:
          llvm_unreachable("Invalid As value");
      }
    }
  }

  else if (Inst->isSubClassOf("CJForm")) {
    gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode +  " 2(r4)","cl"));
    gen_instr_class_pairs.push_back(std::make_pair(opcode + " 0x0402","cl"));
    gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, 0x0402;nop;" + opcode + " &0x0402","cl"));
  }

  // Constant generators
  else if (Inst -> isSubClassOf("I8rc") || Inst->isSubClassOf("I8mc") || Inst->isSubClassOf("I16rc") || Inst-> isSubClassOf("I16mc")) {
    BitsInit* Ad = Inst->getValueAsBitsInit("Ad");
    switch (getValueFromBitsInit(Ad)) {
      case 0:
        //INS#rc
        gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008, r5","cl"));
        break;

      case 1:
        //INS#mc
        gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, r5;nop;" + opcode + " #0x0008, 2(r5)","cl"));
        // This instruction gave rise to an unexpected extra clock cycle
        //gen_instr.push_back(opcode + " #0x0008, 0x0406");
        gen_instr_class_pairs.push_back(std::make_pair("mov #0x0406, 0x0406;nop;" + opcode + " #0x0008, &0x0406","cl"));
        break;

    }
  }

  else if (Inst-> isSubClassOf("II8c") || Inst->isSubClassOf("II16c")) {
      //INS#c
      gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0008","cl"));

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

  OS << "static const string Generated_Instructions[][1] = {\n";


  unsigned Num = 0;
  for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

    Record *Inst = II->TheDef;
    auto generated_instructions = ComputeMemoryTraceClass(II, OS);

    for (unsigned i = 0; i < generated_instructions.size(); i++){
      OS << "/* " << Num << "*/ "
          << "{\"" << generated_instructions[i].first << "\" , \"" << generated_instructions[i].second << "\"}, "
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
