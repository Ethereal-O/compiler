//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"
#include <map>

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  X64RegManager() {
    // pretreatment
    auto reg_strings = std::map<int, std::string *>{
        {0, new std::string("%rax")},  {1, new std::string("%rbx")},
        {2, new std::string("%rcx")},  {3, new std::string("%rdx")},
        {4, new std::string("%rsi")},  {5, new std::string("%rdi")},
        {6, new std::string("%rbp")},  {7, new std::string("%rsp")},
        {8, new std::string("%r8")},   {9, new std::string("%r9")},
        {10, new std::string("%r10")}, {11, new std::string("%r11")},
        {12, new std::string("%r12")}, {13, new std::string("%r13")},
        {14, new std::string("%r14")}, {15, new std::string("%r15")}};

    for (int i = 0; i < 16; i++) {
      // create register
      temp::Temp *a_temp = temp::TempFactory::NewTemp();
      // push them into vector
      regs_.push_back(a_temp);
      // push them into map
      temp_map_->Enter(a_temp, reg_strings[i]);
    }
  }

  temp::TempList *Registers() {
    temp::TempList *tempList = new temp::TempList(
        {regs_[0], regs_[1], regs_[2], regs_[3], regs_[5], regs_[6], regs_[7],
         regs_[8], regs_[9], regs_[10], regs_[11], regs_[12], regs_[13],
         regs_[14], regs_[15]});
    return tempList;
  }

  temp::TempList *ArgRegs() {
    temp::TempList *tempList = new temp::TempList(
        {regs_[5], regs_[4], regs_[3], regs_[2], regs_[8], regs_[0]});
    return tempList;
  }

  temp::TempList *CallerSaves() {
    temp::TempList *tempList = new temp::TempList({regs_[10], regs_[11]});
    return tempList;
  }

  temp::TempList *CalleeSaves() {
    temp::TempList *tempList = new temp::TempList(
        {regs_[1], regs_[6], regs_[12], regs_[13], regs_[14], regs_[15]});
    return tempList;
  }

  temp::TempList *ReturnSink() {
    temp::TempList *tempList = new temp::TempList({regs_[0]});
    return tempList;
  }

  int WordSize() { return 8; }

  temp::Temp *FramePointer() { return regs_[6]; }

  temp::Temp *StackPointer() { return regs_[7]; }

  temp::Temp *ReturnValue() { return regs_[0]; }
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
