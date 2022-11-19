#include "tiger/codegen/codegen.h"

#include <cassert>
#include <deque>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  auto *list = new assem::InstrList();

  std::deque<temp::Temp *> saved_callee_regs;
  auto callee_regs = reg_manager->CalleeSaves();
  for (int i = 0; i < callee_regs->GetList().size(); i++) {
    auto new_temp = temp::TempFactory::NewTemp();
    saved_callee_regs.push_back(new_temp);
    list->Append(
        new assem::MoveInstr("movq `s0,`d0", new temp::TempList(new_temp),
                             new temp::TempList(callee_regs->NthTemp(i))));
  }

  for (auto stm : traces_->GetStmList()->GetList())
    stm->Munch(*list, fs_);

  for (int i = 0; i < callee_regs->GetList().size(); i++)
    list->Append(new assem::MoveInstr(
        "movq `s0,`d0", new temp::TempList(callee_regs->NthTemp(i)),
        new temp::TempList(saved_callee_regs[i])));

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", new temp::TempList(),
                                         new temp::TempList(),
                                         new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(
      new assem::OperInstr("cmpq `s0,`s1", new temp::TempList(),
                           new temp::TempList{left_->Munch(instr_list, fs),
                                              right_->Munch(instr_list, fs)},
                           nullptr));
  std::string assem_str;
  switch (op_) {
  case EQ_OP:
    assem_str = "je j0";
    break;
  case NE_OP:
    assem_str = "jne j0";
    break;
  case LT_OP:
    assem_str = "jg j0";
    break;
  case GT_OP:
    assem_str = "jl j0";
    break;
  case LE_OP:
    assem_str = "jge j0";
    break;
  case GE_OP:
    assem_str = "jle j0";
    break;
  default:
    break;
  }
  instr_list.Append(new assem::OperInstr(
      assem_str, new temp::TempList(), new temp::TempList(),
      new assem::Targets(
          new std::vector<temp::Label *>{true_label_, false_label_})));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /**
   * TODO: Do maximal munch...
   * It's maybe to disgusting to do maximal munch, so temp try this easier one
   */
  instr_list.Append(new assem::MoveInstr(
      "movq `s0,`d0", new temp::TempList(dst_->Munch(instr_list, fs)),
      new temp::TempList(src_->Munch(instr_list, fs))));
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto left_temp = left_->Munch(instr_list, fs);
  auto right_temp = right_->Munch(instr_list, fs);
  auto result_temp = temp::TempFactory::NewTemp();
  switch (op_) {
  case PLUS_OP: {
    if (left_temp == reg_manager->FramePointer())
      instr_list.Append(new assem::OperInstr(
          "leaq " + std::string(fs) + "_framesize(%rsp),`d0",
          new temp::TempList(result_temp), new temp::TempList(), nullptr));
    else
      instr_list.Append(new assem::MoveInstr("movq `s0,`d0",
                                             new temp::TempList(result_temp),
                                             new temp::TempList(left_temp)));
    instr_list.Append(new assem::OperInstr(
        "addq `s0,`d0", new temp::TempList(result_temp),
        new temp::TempList{right_temp, result_temp}, nullptr));
    return result_temp;
  }
  case MINUS_OP: {
    instr_list.Append(new assem::MoveInstr("movq `s0,`d0",
                                           new temp::TempList(result_temp),
                                           new temp::TempList(left_temp)));
    instr_list.Append(new assem::OperInstr(
        "subq `s0,`d0", new temp::TempList(result_temp),
        new temp::TempList{right_temp, result_temp}, nullptr));
    return result_temp;
  }
  case MUL_OP: {
    auto rax = reg_manager->Rax();
    auto rdx = reg_manager->Rdx();
    instr_list.Append(new assem::MoveInstr("movq `s0,%rax",
                                           new temp::TempList(rax),
                                           new temp::TempList(left_temp)));
    instr_list.Append(
        new assem::OperInstr("imulq `s0", new temp::TempList{rax, rdx},
                             new temp::TempList{right_temp, rax}, nullptr));
    return rax;
  }
  case DIV_OP: {
    auto rax = reg_manager->Rax();
    auto rdx = reg_manager->Rdx();
    instr_list.Append(new assem::MoveInstr("movq `s0,%rax",
                                           new temp::TempList(rax),
                                           new temp::TempList(left_temp)));
    instr_list.Append(new assem::OperInstr("cqto", new temp::TempList(rdx),
                                           new temp::TempList(), nullptr));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0", new temp::TempList{rax, rdx},
        new temp::TempList{right_temp, rax, rdx}, nullptr));
    return rax;
  }
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /**
   * TODO: CHANGE THIS
   */
  auto return_temp = temp::TempFactory::NewTemp();
  if (typeid(*exp_) == typeid(BinopExp)) {
    auto exp2bin = static_cast<BinopExp *>(exp_);
    if (exp2bin->op_ == PLUS_OP) {
      if (typeid(*exp2bin->left_) == typeid(ConstExp)) {
        auto left2cst = static_cast<ConstExp *>(exp2bin->left_);
        auto right_temp = exp2bin->right_->Munch(instr_list, fs);
        // movq Imm(r),d
        std::ostringstream assem;
        if (right_temp == reg_manager->FramePointer()) {
          assem << "movq (" << fs << "_framesize" << left2cst->consti_
                << ")(`s0),`d0";
          right_temp = reg_manager->StackPointer();
        } else {
          assem << "movq " << left2cst->consti_ << "(`s0),`d0";
        }
        instr_list.Append(
            new assem::OperInstr(assem.str(), new temp::TempList(return_temp),
                                 new temp::TempList(right_temp), nullptr));
      } else if (typeid(*exp2bin->right_) == typeid(ConstExp)) {
        auto right2cst = static_cast<ConstExp *>(exp2bin->right_);
        // movq Imm(r),d
        auto left_temp = exp2bin->left_->Munch(instr_list, fs);
        std::ostringstream assem;
        if (left_temp == reg_manager->FramePointer()) {
          assem << "movq (" << fs << "_framesize" << right2cst->consti_
                << ")(`s0),`d0";
          left_temp = reg_manager->StackPointer();
        } else {
          assem << "movq " << right2cst->consti_ << "(`s0),`d0";
        }
        instr_list.Append(
            new assem::OperInstr(assem.str(), new temp::TempList(return_temp),
                                 new temp::TempList(left_temp), nullptr));
      } else {
        // movq (r), d
        instr_list.Append(new assem::OperInstr(
            "movq (`s0),`d0", new temp::TempList(return_temp),
            new temp::TempList(exp_->Munch(instr_list, fs)), nullptr));
      }
    }
  } else if (typeid(*exp_) == typeid(ConstExp)) {
    // movq Imm, d
    auto exp2cst = static_cast<ConstExp *>(exp_);
    std::ostringstream assem;
    assem << "movq " << exp2cst->consti_ << ",`d0";
    instr_list.Append(new assem::OperInstr(assem.str(),
                                           new temp::TempList(return_temp),
                                           new temp::TempList(), nullptr));
  } else {
    // movq (r), d
    instr_list.Append(new assem::OperInstr(
        "movq (`s0),`d0", new temp::TempList(return_temp),
        new temp::TempList(exp_->Munch(instr_list, fs)), nullptr));
  }
  return return_temp;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto addr_str = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr("leaq " + name_->Name() + "(%rip),`d0",
                                         new temp::TempList(addr_str),
                                         new temp::TempList(), nullptr));
  return addr_str;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto return_temp = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "movq $" + std::to_string(consti_) + ",`d0",
      new temp::TempList(return_temp), new temp::TempList(), nullptr));
  return return_temp;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /**
   * TODO: CHANGE THIS
   */
  size_t args_size = args_->GetList().size();
  if (args_size > 6) {
    std::ostringstream assem;
    assem << "subq $" << (args_size - 6) * reg_manager->WordSize() << ",%rsp";
    instr_list.Append(new assem::OperInstr(assem.str(), new temp::TempList(),
                                           new temp::TempList(), nullptr));
  }
  auto calldefs = reg_manager->CallerSaves();
  calldefs->Append(reg_manager->ReturnValue());
  auto temp_list = args_->MunchArgs(instr_list, fs);
  std::ostringstream assem;
  assem << "callq " << static_cast<NameExp *>(fun_)->name_->Name();
  instr_list.Append(
      new assem::OperInstr(assem.str(), calldefs, temp_list, nullptr));
  if (args_size > 6) {
    std::ostringstream assem;
    assem << "addq $" << (args_size - 6) * reg_manager->WordSize() << ",%rsp";
    instr_list.Append(new assem::OperInstr(assem.str(), new temp::TempList(),
                                           new temp::TempList(), nullptr));
  }
  return reg_manager->ReturnValue();
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /**
   * TODO: CHANGE THIS
   */
  auto temp_list = new temp::TempList();
  size_t exp_list_size = exp_list_.size();
  auto exp_list_it = exp_list_.begin();
  auto arg_temp_list = reg_manager->ArgRegs();
  for (int i = 0; i < exp_list_size; i++, exp_list_it++) {
    auto exp_temp = (*exp_list_it)->Munch(instr_list, fs);
    switch (i) {
    case 0:
      temp_list->Append(arg_temp_list->NthTemp(i));
      if (exp_temp == reg_manager->FramePointer()) {
        std::ostringstream assem;
        // we cannot use movq because we just want the address itself
        assem << "leaq " << fs << "_framesize(%rsp),`d0";
        instr_list.Append(new assem::OperInstr(
            assem.str(), new temp::TempList(arg_temp_list->NthTemp(i)),
            new temp::TempList(), nullptr));
      } else {
        instr_list.Append(new assem::MoveInstr(
            "movq `s0,`d0", new temp::TempList(arg_temp_list->NthTemp(i)),
            new temp::TempList(exp_temp)));
      }
      break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      temp_list->Append(arg_temp_list->NthTemp(i));
      instr_list.Append(new assem::MoveInstr(
          "movq `s0,`d0", new temp::TempList(arg_temp_list->NthTemp(i)),
          new temp::TempList(exp_temp)));
      break;
    default: {
      std::ostringstream assem;
      /* <del>We don't modify the %rsp here. We do it at the beginning of the
       * function In ProcEntryExit3</del> */
      // we modify %rsp here!!! You can use gcc to have a check.
      // Actually we don't modify it here, we modify it one-time before and
      // after the callexp. assem << "subq $" << reg_manager->WordSize() <<
      // ",`d0"; instr_list.Append(
      //   new assem::OperInstr(
      //     assem.str(),
      //     new temp::TempList(reg_manager->StackPointer()),
      //     new temp::TempList(reg_manager->StackPointer()),
      //     nullptr
      //   )
      // );
      assem << "movq `s0," << (i - 6) * reg_manager->WordSize() << "(%rsp)";
      instr_list.Append(new assem::OperInstr(assem.str(), new temp::TempList(),
                                             new temp::TempList(exp_temp),
                                             nullptr));
    } break;
    }
  }
  return temp_list;
}

} // namespace tree
