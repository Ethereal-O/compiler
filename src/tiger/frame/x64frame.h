//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  X64RegManager() {
    // create register
    temp::Temp *rax = temp::TempFactory::NewTemp();
    temp::Temp *rbx = temp::TempFactory::NewTemp();
    temp::Temp *rcx = temp::TempFactory::NewTemp();
    temp::Temp *rdx = temp::TempFactory::NewTemp();
    temp::Temp *rsi = temp::TempFactory::NewTemp();
    temp::Temp *rdi = temp::TempFactory::NewTemp();
    temp::Temp *rbp = temp::TempFactory::NewTemp();
    temp::Temp *rsp = temp::TempFactory::NewTemp();
    temp::Temp *r8 = temp::TempFactory::NewTemp();
    temp::Temp *r9 = temp::TempFactory::NewTemp();
    temp::Temp *r10 = temp::TempFactory::NewTemp();
    temp::Temp *r11 = temp::TempFactory::NewTemp();
    temp::Temp *r12 = temp::TempFactory::NewTemp();
    temp::Temp *r13 = temp::TempFactory::NewTemp();
    temp::Temp *r14 = temp::TempFactory::NewTemp();
    temp::Temp *r15 = temp::TempFactory::NewTemp();

    // push them into vector
    regs_.push_back(rax);
    regs_.push_back(rbx);
    regs_.push_back(rcx);
    regs_.push_back(rdx);
    regs_.push_back(rsi);
    regs_.push_back(rdi);
    regs_.push_back(rbp);
    regs_.push_back(rsp);
    regs_.push_back(r8);
    regs_.push_back(r9);
    regs_.push_back(r10);
    regs_.push_back(r11);
    regs_.push_back(r12);
    regs_.push_back(r13);
    regs_.push_back(r14);
    regs_.push_back(r15);

    // push them into map
    temp_map_->Enter(rax, new std::string("%rax"));
    temp_map_->Enter(rbx, new std::string("%rbx"));
    temp_map_->Enter(rcx, new std::string("%rcx"));
    temp_map_->Enter(rdx, new std::string("%rdx"));
    temp_map_->Enter(rsi, new std::string("%rsi"));
    temp_map_->Enter(rdi, new std::string("%rdi"));
    temp_map_->Enter(rbp, new std::string("%rbp"));
    temp_map_->Enter(rsp, new std::string("%rsp"));
    temp_map_->Enter(r8, new std::string("%r8"));
    temp_map_->Enter(r9, new std::string("%r9"));
    temp_map_->Enter(r10, new std::string("%r10"));
    temp_map_->Enter(r11, new std::string("%r11"));
    temp_map_->Enter(r12, new std::string("%r12"));
    temp_map_->Enter(r13, new std::string("%r13"));
    temp_map_->Enter(r14, new std::string("%r14"));
    temp_map_->Enter(r15, new std::string("%r15"));
  }

  temp::TempList *Registers() {
    temp::TempList *tempList = new temp::TempList();
    tempList->Append(regs_[0]);
    tempList->Append(regs_[1]);
    tempList->Append(regs_[2]);
    tempList->Append(regs_[3]);
    tempList->Append(regs_[5]);
    tempList->Append(regs_[6]);
    tempList->Append(regs_[7]);
    tempList->Append(regs_[8]);
    tempList->Append(regs_[9]);
    tempList->Append(regs_[10]);
    tempList->Append(regs_[11]);
    tempList->Append(regs_[12]);
    tempList->Append(regs_[13]);
    tempList->Append(regs_[14]);
    tempList->Append(regs_[15]);
    return tempList;
  }

  temp::TempList *ArgRegs() {
    temp::TempList *tempList = new temp::TempList();
    tempList->Append(regs_[5]);
    tempList->Append(regs_[4]);
    tempList->Append(regs_[3]);
    tempList->Append(regs_[2]);
    tempList->Append(regs_[8]);
    tempList->Append(regs_[9]);
  }

  temp::TempList *CallerSaves() {
    temp::TempList *tempList = new temp::TempList();
    tempList->Append(regs_[10]);
    tempList->Append(regs_[11]);
    return tempList;
  }

  temp::TempList *CalleeSaves() {
    temp::TempList *tempList = new temp::TempList();
    tempList->Append(regs_[1]);
    tempList->Append(regs_[6]);
    tempList->Append(regs_[12]);
    tempList->Append(regs_[13]);
    tempList->Append(regs_[14]);
    tempList->Append(regs_[15]);
    return tempList;
  }

  temp::TempList *ReturnSink() {
    temp::TempList *tempList = new temp::TempList();
    tempList->Append(regs_[0]);
    return tempList;
  }

  int WordSize() { return 8; }

  temp::Temp *FramePointer() { return regs_[6]; }

  temp::Temp *StackPointer() { return regs_[7]; }

  temp::Temp *ReturnValue() { return regs_[0]; }
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
