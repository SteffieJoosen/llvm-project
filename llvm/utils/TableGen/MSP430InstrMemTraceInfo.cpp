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
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

#include "CodeGenTarget.h"
#include "CodeGenDAGPatterns.h"


#include <string>
#include <vector>



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

const unsigned int DATA_MEM = 0;
const unsigned int PROGR_MEM = 1;
const unsigned int PER_MEM = 2;

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

static std::vector<std::string> INSmr_asms(std::string opcode, std::string absolute, std::string relative = "\0") {
  std::vector<std::string> asms;
    asms.push_back("mov #" + absolute + ", r5;nop;" + opcode + " r4, 2(r5)");
    if (relative != "\0") asms.push_back(opcode + " r4, " + relative);
    asms.push_back(opcode + " r4, &"+ absolute);
  return asms;
}

static std::vector<std::string> INSrm_asms(std::string opcode, std::string absolute, std::string relative = "\0") {
  std::vector<std::string> asms;
    asms.push_back("mov #" + absolute + ", r4;nop;" + opcode + " 2(r4), r5");
    if (relative != "\0") asms.push_back(opcode + " " + relative + ", r5");
    asms.push_back(opcode + " &"+ absolute + ", r5");
  return asms;
}

static std::vector<std::string> INSmm_asms(std::string opcode, std::string src_absolute, std::string dst_absolute, std::string src_relative = "\0", std::string dst_relative = "\0") {
  std::vector<std::string> asms;
  std::vector<std::string> source_asms;
  // Indexed source mode
  source_asms.push_back("mov #" + src_absolute + ", r4;nop;" + opcode +  " 2(r4), ");
  // Absolute source mode
  source_asms.push_back(opcode + " &" + src_absolute + ", ");
  // Symbolic source mode
  if (src_relative != "\0") source_asms.push_back(opcode + " " + src_relative + ", ");

  for (auto asmstr : source_asms){
    asms.push_back("mov #" + dst_absolute + ", r5;nop;" + asmstr + "2(r5)");
    asms.push_back(asmstr + "&" + dst_absolute);
    if (dst_relative != "\0") asms.push_back(asmstr + dst_relative);
  }
  return asms;
}

static std::vector<std::string> INSmn_asms(std::string opcode, std::string src_absolute, std::string dst_absolute, std::string dst_relative = "\0") {
    std::vector<std::string> asms;
    std::string source_asm;
    source_asm = "mov #" + src_absolute + ", r4;nop;" + opcode +  " @r4, ";

    asms.push_back("mov #" + dst_absolute + ", r5;nop;" + source_asm + "2(r5)");
    asms.push_back(source_asm + "&" + dst_absolute);
    if (dst_relative != "\0") asms.push_back(source_asm + " " + dst_relative);

    return asms;
}

static std::vector<std::string> INSmi_asms(std::string opcode, std::string absolute, std::string relative = "\0") {
    std::vector<std::string> asms;
    asms.push_back("mov #" + absolute + ", r5;nop;" + opcode +  " #0x0045, 2(r5)");
    if (relative != "\0") asms.push_back(opcode + " #0x0045, " + relative);
    asms.push_back(opcode + " #0x0045, &" + absolute);

    return asms;
}

static std::vector<std::string> INSmp_asms(std::string opcode, std::string src_absolute, std::string dst_absolute, std::string dst_relative = "\0") {
    std::vector<std::string> asms;
    asms.push_back("mov #" + src_absolute + ", r4;nop;" + "mov #" + dst_absolute + ", r5;nop;" + opcode +  " @r4+, 2(r5)");
    if (dst_relative != "\0") asms.push_back("mov #" + src_absolute + ", r4;nop;" + opcode + " @r4+, " + dst_relative);
    asms.push_back("mov #" + src_absolute + ", r4;nop;" + opcode + " @r4+, &" + dst_absolute);

    return asms;
}

static std::vector<std::string> INSmc_asms(std::string opcode, std::string absolute, std::string relative = "\0") {
    std::vector<std::string> asms;
    asms.push_back("mov #" + absolute + ", r5;nop;" + opcode +  " #0x0008, 2(r5)");
    if (relative != "\0") asms.push_back(opcode + " #0x0008, " + relative);
    asms.push_back(opcode + " #0x0008, &" + absolute);

    return asms;
}

static std::vector<std::string> INSm_asms(std::string opcode, std::string absolute, std::string relative = "\0") {
  std::vector<std::string> asms;
    asms.push_back("mov #" + absolute + ", r4;nop;" + opcode + " 2(r4)");
    if (relative != "\0") asms.push_back(opcode + " " + relative);
    asms.push_back(opcode + " &"+ absolute);
  return asms;
}


static std::vector<std::pair<std::string,std::string>> ComputeMemoryTraceClass(const CodeGenInstruction *II, raw_ostream &OS,
   unsigned int source_mem_region, unsigned int dest_mem_region) {


  std::vector<std::pair<std::string,std::string>> gen_instr_class_pairs;
  std::string instr_class;
  std::string instr_to_simulate;

  std::string opcode = "";
  std::string AsmString = "";

  std::vector<std::string> v;

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


    switch (getValueFromBitsInit(As)) {
      case 0: // Register mode
        if (opcode.rfind("br", 0) == 0) { // Br
          gen_instr_class_pairs.push_back(std::make_pair("mov #0xFFDC, r4;nop;" + opcode + " r4","2 | 00 | 00 | 11"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
              //INS#rr
              //R4 and R5 used
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4, r5","1 | 0 | 0 | 1"));
              break;

            case 1:
              //INS#mr
              //R4 and R5 used
              // Indexed and symbolic
              if (dest_mem_region == DATA_MEM) {
                v = INSmr_asms(opcode, "0x0406", "0xDFDE");
                if (opcode.rfind("mov", 0) == 0) { // MOV
                  for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"4 | 0000 | 0001 | 1001")); }
                } else {
                  for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"4 | 0000 | 0101 | 1001")); }
                }
              } else if (dest_mem_region == PER_MEM){
                  // Destination operand in data memory
                v = INSmr_asms(opcode, "0x0010");
                if (opcode.rfind("mov", 0) == 0) { // MOV
                  for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"4 | 0001 | 0000 | 1001")); }
                } else {
                  for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"4 | 0101 | 0000 | 1001")); }
                }
              }
              break;
          }
        }
        break;
      case 1: // Indexed, symbolic, absolute
        if (opcode.rfind("br", 0) == 0) { // Bm
            if (dest_mem_region == DATA_MEM) {
                v = INSm_asms(opcode, "0x0402", "0xDFDE");
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"test")); }
            } else if (dest_mem_region == PER_MEM) {
                v = INSm_asms(opcode, "0x0010");
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"test")); }
            }

        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
              //INS#rm
              if (source_mem_region == DATA_MEM) {
                v = INSrm_asms(opcode, "0x0402", "0xDFDE");
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"3 | 000 | 010 | 101")); }
              } else if (source_mem_region == PROGR_MEM) {
                v = INSrm_asms(opcode, "0xFFDC", "0x0010");
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"3 | 000 | 000 | 111")); }
              } else if (source_mem_region == PER_MEM) {
                  v = INSrm_asms(opcode, "0x0010");
                  for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"3 | 010 | 000 | 101")); }
              }

              break;
            case 1:
              //INS#mm

              if (source_mem_region == DATA_MEM && dest_mem_region == DATA_MEM) {
                v = INSmm_asms(opcode, "0x0402", "0x0402", "0xDFDE", "0xDFDE");
                if (opcode.rfind("mov", 0) == 0) { // MOV
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000000 | 010001 | 110001")); }
                } else {
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000000 | 010101 | 110001")); }
                }
              } else if (source_mem_region == PROGR_MEM && dest_mem_region == DATA_MEM){
                  // Destination in data memory
                    v = INSmm_asms(opcode, "0xFFDC","0x0402","0x0010", "0xDFDE");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000000 | 000001 | 111001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000000 | 000101 | 111001")); }
                    }
              } else if (source_mem_region == PER_MEM && dest_mem_region == DATA_MEM) {
                  v = INSmm_asms(opcode, "0x0010","0x0402", "\0", "0xDFDE");
                  if (opcode.rfind("mov", 0) == 0) { // MOV
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 010000 | 000001 | 110001")); }
                  } else {
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 010000 | 000101 | 110001")); }
                  }
              } else if (source_mem_region == DATA_MEM && dest_mem_region == PER_MEM) {
                  v = INSmm_asms(opcode, "0x0402","0x0010", "0xDFDE");
                  if (opcode.rfind("mov", 0) == 0) { // MOV
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000001 | 010000 | 110001")); }
                  } else {
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000101 | 010000 | 110001")); }
                  }
              } else if (source_mem_region == PROGR_MEM && dest_mem_region == PER_MEM) {
                  v = INSmm_asms(opcode, "0xFFDC","0x0010", "0x0010");
                  if (opcode.rfind("mov", 0) == 0) { // MOV
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000001 | 000000 | 111001")); }
                  } else {
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 000101 | 000000 | 111001")); }
                  }
              } else if (source_mem_region == PER_MEM && dest_mem_region == PER_MEM) {
                  v = INSmm_asms(opcode, "0x0010","0x0010");
                  if (opcode.rfind("mov", 0) == 0) { // MOV
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 010001 | 000000 | 110001")); }
                  } else {
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"6 | 010101 | 000000 | 110001")); }
                  }
              }

              break;
            }
        }
        break;

      case 2:
        if (opcode.rfind("br", 0) == 0) { // Bm : always branch to program memory
                gen_instr_class_pairs.push_back(std::make_pair("mov #0xFFDC, r4;nop;" + opcode + " @r4","3 | 000 | 000 | 100"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0:
                if (source_mem_region == DATA_MEM) {
                    gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4, r5","2 | 00 | 10 | 01"));
                } else if (source_mem_region == PROGR_MEM) {
                    gen_instr_class_pairs.push_back(std::make_pair("mov #0xFFDC, r4;nop;" + opcode + " @r4, r5","2 | 00 | 00 | 11"));
                } else if (source_mem_region == PER_MEM) {
                    gen_instr_class_pairs.push_back(std::make_pair("mov #0x0010, r4;nop;" + opcode + " @r4, r5","2 | 10 | 00 | 01"));
                }
                break;
            case 1:
                //INS#mn
                if (source_mem_region == DATA_MEM && dest_mem_region == DATA_MEM) {
                    v = INSmn_asms(opcode, "0x0402", "0x0402", "0xDFDE");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 10001 | 10001")); }
                    } else {
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 10101 | 10001")); }
                    }
                } else if (source_mem_region == PROGR_MEM && dest_mem_region == DATA_MEM) {
                  v = INSmn_asms(opcode, "0xFFDC","0x0402", "0xDFDE");
                  if (opcode.rfind("mov", 0) == 0) { // MOV
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 00001 | 11001")); }
                  } else {
                      for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 00101 | 11001")); }
                  }
              } else if (source_mem_region == PER_MEM && dest_mem_region == DATA_MEM) {
                    v = INSmn_asms(opcode, "0x0010","0x0402", "0xDFDE");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 10000 | 00001 | 10001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 10000 | 00101 | 10001")); }
                    }
                } else if (source_mem_region == DATA_MEM && dest_mem_region == PER_MEM) {
                    v = INSmn_asms(opcode, "0x0402","0x0010");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00001 | 10000 | 10001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00101 | 10000 | 10001")); }
                    }
                } else if (source_mem_region == PROGR_MEM && dest_mem_region == PER_MEM) {
                    v = INSmn_asms(opcode, "0xFFDC","0x0010");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00001 | 00000 | 11001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00101 | 00000 | 11001")); }
                    }
                } else if (source_mem_region == PER_MEM && dest_mem_region == PER_MEM) {
                    v = INSmn_asms(opcode, "0x0010","0x0010");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 10001 | 00000 | 10001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 10101 | 00000 | 10001")); }
                    }
                }
            break;
          }
        }
        break;
      case 3:
        if (opcode.rfind("br", 0) == 0) { // BR
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0046","3 | 000 | 000 | 100"));
        } else {
          switch (getValueFromBitsInit(Ad)) {
            case 0: // INS#ri, don't use constant generator
              if (instruction_name.back() == 'i') {
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x0045, r5","2 | 00 | 00 | 11"));
              }
              else { // INS#rp
                if (opcode == "ret"){
                  gen_instr_class_pairs.push_back(std::make_pair(opcode,"3 | 000 | 100 | 000"));
                }  else {
                  if (opcode.rfind("pop", 0) == 0) { // POP
                    gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","2 | 00 | 10 | 01"));
                      if (dest_mem_region == DATA_MEM) {
                          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4","5 | 00000 | 10001 | 10001"));
                      } else if (dest_mem_region == PER_MEM) {
                          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0010, r4;nop;" + opcode + " @r4","per_mem"));
                      }

                  } else {
                      if (source_mem_region == DATA_MEM) {
                          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4+, r5","2 | 00 | 10 | 01"));
                      } else if (source_mem_region == PROGR_MEM) {
                          gen_instr_class_pairs.push_back(std::make_pair("mov #0xFFDC, r4;nop;" + opcode + " @r4+, r5","2 | 00 | 00 | 11"));
                      } else if (source_mem_region == PER_MEM) {
                          gen_instr_class_pairs.push_back(std::make_pair("mov #0x0010, r4;nop;" + opcode + " @r4+, r5","2 | 10 | 00 | 01"));
                      }
                  }

                }

              }
              break;
            case 1:
            if (instruction_name.back() == 'i') { // INS#mi, don't use constant generator
                if (dest_mem_region == DATA_MEM) {
                    v = INSmi_asms(opcode, "0x0402", "0xDFDE");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 00001 | 11001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 00101 | 11001")); }
                    }
                } else if (dest_mem_region == PER_MEM){
                    // Destination operand in data memory
                    v = INSmi_asms(opcode, "0x0010");
                    if (opcode.rfind("mov", 0) == 0) { // MOV
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00001 | 00000 | 11001")); }
                    } else {
                        for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00101 | 00000 | 11001")); }
                    }
                }
            }
            else { // INS#mp (no MOV instructions here)
                if (source_mem_region == DATA_MEM && dest_mem_region == DATA_MEM) {
                    v = INSmp_asms(opcode, "0x0402",  "0x0402", "0xDFDE");
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 10101 | 10001")); }
                } else if (source_mem_region == PROGR_MEM && dest_mem_region == DATA_MEM) {
                    v = INSmp_asms(opcode, "0xFFDC", "0x0402", "0xDFDE");
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00000 | 00101 | 11001")); }
                } else if (source_mem_region == PER_MEM && dest_mem_region == DATA_MEM) {
                    v = INSmp_asms(opcode, "0x0010", "0x0402", "0xDFDE");
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 10000 | 00101 | 10001")); }
                } else if (source_mem_region == DATA_MEM && dest_mem_region == PER_MEM) {
                    v = INSmp_asms(opcode, "0x0402", "0x0010");
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00101 | 10000 | 10001")); }
                } else if (source_mem_region == PROGR_MEM && dest_mem_region == PER_MEM) {
                    v = INSmp_asms(opcode, "0xFFDC", "0x0010");
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 00101 | 00000 | 11001")); }
                } else if (source_mem_region == PER_MEM && dest_mem_region == PER_MEM) {
                    v = INSmp_asms(opcode, "0x0010", "0x0010");
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr,"5 | 10101 | 00000 | 10001")); }
                }
            }
            break;
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
      gen_instr_class_pairs.push_back(std::make_pair(opcode,"5 | 00000 | 10000 | 00000"));
    } else {
      uint16_t As = 0; // II16c and II8c : constant generators used, register mode
      if (Inst->isSubClassOf("IIForm")) {
        As = getValueFromBitsInit(Inst->getValueAsBitsInit("As"));
      }
      switch (As) {
        case 0:
          if (opcode.rfind("call",0) == 0) {
            gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","simulation fails"));
          } else {
              if (opcode.rfind("br",0) == 0) { // Br
                  // Branch to program memory
                  gen_instr_class_pairs.push_back(std::make_pair("mov #0xFFDC, r4;nop;" + opcode + " r4","BRCALL not yet"));
              } else if (opcode.rfind("push",0) == 0) { // PUSH
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","3 | 000 | 001 | 001"));
            } else {
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " r4","1 | 0 | 0 | 1"));
            }

          }
          break;
        case 1:
            if (dest_mem_region == DATA_MEM) {
                v = INSm_asms(opcode, "0x0402", "0xDFDE");
                if (opcode.rfind("br", 0) == 0 || opcode.rfind("call", 0) == 0) { // BRCALLm
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "BRCALL not yet")); }
                } else {
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "4 | 0000 | 0101 | 1001")); }
                }
            } else if (dest_mem_region == PER_MEM) {
                v = INSm_asms(opcode, "0x0010");
                if (opcode.rfind("br", 0) == 0 || opcode.rfind("call", 0) == 0) { // BRCALLm
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "BRCALL not yet")); }
                } else {
                    for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "4 | 0101 | 0000 | 1001")); }
                }
            }
            break;
        case 2:
            if (dest_mem_region == DATA_MEM) {
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4","3 | 000 | 101 | 001"));
            } else if (dest_mem_region == PER_MEM) {
                gen_instr_class_pairs.push_back(std::make_pair("mov #0x0010, r4;nop;" + opcode + " @r4","3 | 101 | 000 | 001"));
            }
            break;
        case 3:
          if (instruction_name.back() == 'i') { // INS#i, use constant generator or not
            if (opcode.rfind("br", 0) == 0 || opcode.rfind("call",0) == 0) { // BRCALLi
                // Branch to program memory
              gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0xFFDC","BRCALL not yet"));
            } else if (opcode.rfind("push",0) == 0) { // PUSH
                gen_instr_class_pairs.push_back(std::make_pair(opcode + " #0x00046","4 | 0000 | 0001 | 1001"));
            } else { /*no cases yet*/ }
          } else { // INS#p
              if (dest_mem_region == DATA_MEM) {
                  gen_instr_class_pairs.push_back(std::make_pair("mov #0x0402, r4;nop;" + opcode + " @r4+","3 | 000 | 101 | 001"));
              } else if (dest_mem_region == PER_MEM) {
                  gen_instr_class_pairs.push_back(std::make_pair("mov #0x0010, r4;nop;" + opcode + " @r4+","3 | 101 | 000 | 001"));
              }

          }
          break;

      default:
          llvm_unreachable("Invalid As value");
      }
    }
  }

  else if (Inst->isSubClassOf("CJForm")) {
      if (opcode == "j$cond") {
          gen_instr_class_pairs.push_back(std::make_pair("jne LABEL", "2 | 00 | 00 | 11"));
          gen_instr_class_pairs.push_back(std::make_pair("jeq LABEL", "2 | 00 | 00 | 11"));
          gen_instr_class_pairs.push_back(std::make_pair("jnc LABEL", "2 | 00 | 00 | 11"));
          gen_instr_class_pairs.push_back(std::make_pair("jc LABEL", "2 | 00 | 00 | 11"));
          gen_instr_class_pairs.push_back(std::make_pair("jn LABEL", "2 | 00 | 00 | 11"));
          gen_instr_class_pairs.push_back(std::make_pair("jge LABEL", "2 | 00 | 00 | 11"));
          gen_instr_class_pairs.push_back(std::make_pair("jl LABEL", "2 | 00 | 00 | 11"));
      } else {
          gen_instr_class_pairs.push_back(std::make_pair(opcode + " LABEL", "2 | 00 | 00 | 11"));
      }

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
        if (dest_mem_region == DATA_MEM) {
            v = INSmc_asms(opcode, "0x0402", "0xDFDE");
            if (opcode.rfind("mov", 0) == 0) { // MOV
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "4 | 0000 | 0001 | 1001")); }
            } else {
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "4 | 0000 | 0101 | 1001")); }
            }
        } else if (dest_mem_region == PER_MEM) {
            v = INSmc_asms(opcode, "0x0010");
            if (opcode.rfind("mov", 0) == 0) { // MOV
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "4 | 0001 | 0000 | 1001")); }
            } else {
                for (auto asmstr : v) { gen_instr_class_pairs.push_back(std::make_pair(asmstr, "4 | 0101 | 0000 | 1001")); }
            }
        }
        break;
    }
  }
  else if (Inst->isSubClassOf("Pseudo")) {
      //! TODO
    gen_instr_class_pairs.push_back(std::make_pair("nothing yet","no class"));
  }
  else {
      //! TODO
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

  OS << "static const StringRef Instruction_classes_data_data[][2] = {\n";


  unsigned Num = 0;
  for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

    Record *Inst = II->TheDef;
    auto generated_instructions = ComputeMemoryTraceClass(II, OS, DATA_MEM, DATA_MEM);
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

  OS << "};\n\n";

  OS << "static const StringRef Instruction_classes_progr_data[][2] = {\n";


  Num = 0;
  for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

    Record *Inst = II->TheDef;
    auto generated_instructions = ComputeMemoryTraceClass(II, OS, PROGR_MEM, DATA_MEM);
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

  OS << "};\n\n";

    OS << "static const StringRef Instruction_classes_per_data[][2] = {\n";


    Num = 0;
    for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

        Record *Inst = II->TheDef;
        auto generated_instructions = ComputeMemoryTraceClass(II, OS, PER_MEM, DATA_MEM);
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

    OS << "};\n\n";

    OS << "static const StringRef Instruction_classes_data_per[][2] = {\n";


    Num = 0;
    for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

        Record *Inst = II->TheDef;
        auto generated_instructions = ComputeMemoryTraceClass(II, OS, DATA_MEM, PER_MEM);
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

    OS << "};\n\n";

    OS << "static const StringRef Instruction_classes_progr_per[][2] = {\n";


    Num = 0;
    for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

        Record *Inst = II->TheDef;
        auto generated_instructions = ComputeMemoryTraceClass(II, OS, PROGR_MEM, PER_MEM);
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

    OS << "};\n\n";

    OS << "static const StringRef Instruction_classes_per_per[][2] = {\n";


    Num = 0;
    for (const CodeGenInstruction *II : Target.getInstructionsByEnumValue()) {

        Record *Inst = II->TheDef;
        auto generated_instructions = ComputeMemoryTraceClass(II, OS, PER_MEM, PER_MEM);
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

    OS << "};\n\n";

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
