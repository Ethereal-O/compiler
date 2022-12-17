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
    temp::TempList *tempList =
        new temp::TempList({regs_[0], regs_[10], regs_[11], regs_[5], regs_[4],
                            regs_[3], regs_[2], regs_[8], regs_[9]});
    return tempList;
  }

  temp::TempList *CalleeSaves() {
    temp::TempList *tempList = new temp::TempList(
        {regs_[1], regs_[6], regs_[12], regs_[13], regs_[14], regs_[15]});
    return tempList;
  }

  temp::TempList *ReturnSink() {
    temp::TempList *temp_list = CalleeSaves();
    temp_list->Append(StackPointer());
    temp_list->Append(ReturnValue());
    return temp_list;
  }

  int WordSize() { return 8; }

  temp::Temp *FramePointer() { return regs_[6]; }

  temp::Temp *StackPointer() { return regs_[7]; }

  temp::Temp *ReturnValue() { return regs_[0]; }

  temp::Temp *Rax() { return regs_[0]; }

  temp::Temp *Rdx() { return regs_[3]; }

  temp::TempList *RegistersExceptRsp() {
    temp::TempList *tempList = new temp::TempList(
        {regs_[0], regs_[1], regs_[2], regs_[3], regs_[4], regs_[5], regs_[6],
         regs_[8], regs_[9], regs_[10], regs_[11], regs_[12], regs_[13],
         regs_[14], regs_[15]});
    return tempList;
  }
};

class InFrameAccess : public Access {
public:
  int offset;
  bool isStorePointer;

  explicit InFrameAccess(int offset) : offset(offset), isStorePointer(false) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr,
                                               new tree::ConstExp(offset)));
  }

  void setIsStorePointer(bool willIsStorePointer) {
    isStorePointer = willIsStorePointer;
  }
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }

  void setIsStorePointer(bool willIsStorePointer) {
    reg->isStorePointer = willIsStorePointer;
  }
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
