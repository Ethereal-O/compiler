#include "tiger/codegen/codegen.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <sstream>

#define GC

extern frame::RegManager *reg_manager;
extern std::vector<int> frame_pointers;
extern std::vector<int> reg_pointers;
extern std::vector<std::string> functions_ret_pointers;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void GetAllPointers(frame::Frame *frame) {
  std::list<frame::Access *> frame_formals = frame->formals_;

  int index = 0;
  for (auto access_iter = frame_formals.begin();
       access_iter != frame_formals.end(); access_iter++, index++) {
    if (typeid(*(*access_iter)) == typeid(frame::InFrameAccess) &&
        static_cast<frame::InFrameAccess *>(*access_iter)->isStorePointer)
      frame_pointers.push_back(
          static_cast<frame::InFrameAccess *>(*access_iter)->offset);

    if (typeid(*(*access_iter)) == typeid(frame::InRegAccess) &&
        static_cast<frame::InRegAccess *>(*access_iter)->reg->isStorePointer)
      reg_pointers.push_back(index);
  }
}

bool RegIsPointer(int index) {
  return std::find(reg_pointers.begin(), reg_pointers.end(), index) !=
         reg_pointers.end();
}

bool ReturnIsPointer(std::string function_name) {
  return function_name == "init_array" || function_name == "alloc_record" ||
         std::find(functions_ret_pointers.begin(), functions_ret_pointers.end(),
                   function_name) != functions_ret_pointers.end();
}

bool FrameIsPointer(int offset) {
  return std::find(frame_pointers.begin(), frame_pointers.end(), offset) !=
         frame_pointers.end();
}

void TransferPointer(assem::Instr *instr) {
  if (typeid(*instr) == typeid(assem::MoveInstr)) {
    assem::MoveInstr *move_instr = static_cast<assem::MoveInstr *>(instr);
    if (move_instr->dst_ && move_instr->src_)
      move_instr->dst_->GetList().front()->isStorePointer =
          move_instr->src_->GetList().front()->isStorePointer;
  }
  if (typeid(*instr) == typeid(assem::OperInstr)) {
    assem::OperInstr *oper_instr = static_cast<assem::OperInstr *>(instr);
    if ((oper_instr->assem_.find("add") != oper_instr->assem_.npos ||
         oper_instr->assem_.find("sub") != oper_instr->assem_.npos) &&
        oper_instr->dst_ && oper_instr->src_ &&
        oper_instr->dst_->GetList().size() > 0 &&
        oper_instr->src_->GetList().size() > 0)
      oper_instr->dst_->GetList().front()->isStorePointer =
          oper_instr->src_->GetList().front()->isStorePointer;
  }
}

void AppendList(assem::InstrList &instr_list_, assem::Instr *instr) {
#ifdef GC
  TransferPointer(instr);
#endif
  instr_list_.Append(instr);
}

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  assem::InstrList *instr_list = new assem::InstrList();

#ifdef GC
  GetAllPointers(frame_);
#endif

  for (auto stm : traces_->GetStmList()->GetList())
    stm->Munch(*instr_list, fs_);

  assem_instr_ = std::make_unique<AssemInstr>(
      frame::ProcEntryExit2(frame::ProcEntryExit1_Refactor(instr_list)));
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
  cg::AppendList(instr_list, new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  cg::AppendList(instr_list,
                 new assem::OperInstr("jmp `j0", new temp::TempList(),
                                      new temp::TempList(),
                                      new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  cg::AppendList(
      instr_list,
      new assem::OperInstr("cmpq `s0,`s1", new temp::TempList(),
                           new temp::TempList{left_->Munch(instr_list, fs),
                                              right_->Munch(instr_list, fs)},
                           nullptr));
  std::string assem_str;
  switch (op_) {
  case EQ_OP:
    assem_str = "je `j0";
    break;
  case NE_OP:
    assem_str = "jne `j0";
    break;
  case LT_OP:
    assem_str = "jg `j0";
    break;
  case GT_OP:
    assem_str = "jl `j0";
    break;
  case LE_OP:
    assem_str = "jge `j0";
    break;
  case GE_OP:
    assem_str = "jle `j0";
    break;
  default:
    break;
  }
  cg::AppendList(instr_list,
                 new assem::OperInstr(
                     assem_str, new temp::TempList(), new temp::TempList(),
                     new assem::Targets(new std::vector<temp::Label *>{
                         true_label_, false_label_})));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // movq s, Mem
  if (typeid(*dst_) == typeid(MemExp)) {
    MemExp *dst_of_mem = static_cast<MemExp *>(dst_);
    if (typeid(*dst_of_mem->exp_) == typeid(BinopExp)) {
      BinopExp *mem_of_bin = static_cast<BinopExp *>(dst_of_mem->exp_);
      if (mem_of_bin->op_ == PLUS_OP) {
        if (typeid(*mem_of_bin->left_) == typeid(ConstExp) ||
            typeid(*mem_of_bin->right_) == typeid(ConstExp)) {
          // movq Imm(r),d
          bool is_left = typeid(*mem_of_bin->left_) == typeid(ConstExp);
          ConstExp *cst_exp = is_left
                                  ? static_cast<ConstExp *>(mem_of_bin->left_)
                                  : static_cast<ConstExp *>(mem_of_bin->right_);
          tree::Exp *temp_exp =
              is_left ? mem_of_bin->right_ : mem_of_bin->left_;
          temp::Temp *temp_tmp = temp_exp->Munch(instr_list, fs);

          std::string instr_str = "";
          if (typeid(*src_) == typeid(ConstExp)) {
            // movq $Ism, Imm(r)
            ConstExp *src_of_cst = static_cast<ConstExp *>(src_);
            if (temp_tmp == reg_manager->FramePointer()) {
              instr_str += "movq $" + std::to_string(src_of_cst->consti_) +
                           ",(" + std::string(fs) + "_framesize" +
                           std::to_string(cst_exp->consti_) + ")(`s0)";
              temp_tmp = reg_manager->StackPointer();
            } else {
              instr_str += "movq $" + std::to_string(src_of_cst->consti_) +
                           "," + std::to_string(cst_exp->consti_) + "(`s0)";
            }
            cg::AppendList(instr_list,
                           new assem::OperInstr(instr_str, new temp::TempList(),
                                                new temp::TempList(temp_tmp),
                                                nullptr));
          } else {
            // movq s, Imm(r)
            if (temp_tmp == reg_manager->FramePointer()) {
              instr_str += "movq `s0,(" + std::string(fs) + "_framesize" +
                           std::to_string(cst_exp->consti_) + ")(`s1)";
              temp_tmp = reg_manager->StackPointer();
            } else {
              instr_str +=
                  "movq `s0," + std::to_string(cst_exp->consti_) + "(`s1)";
            }
            cg::AppendList(
                instr_list,
                new assem::OperInstr(
                    instr_str, new temp::TempList(),
                    new temp::TempList{src_->Munch(instr_list, fs), temp_tmp},
                    nullptr));
          }
        } else {
          // movq (r), d
          cg::AppendList(
              instr_list,
              new assem::OperInstr(
                  "movq `s0,(`s1)", new temp::TempList(),
                  new temp::TempList{src_->Munch(instr_list, fs),
                                     dst_of_mem->exp_->Munch(instr_list, fs)},
                  nullptr));
        }
      }
    } else if (typeid(*dst_of_mem->exp_) == typeid(ConstExp)) {
      // movq s, Ism
      ConstExp *dst_of_cst = static_cast<ConstExp *>(dst_of_mem->exp_);
      if (typeid(*src_) == typeid(ConstExp))
        // movq $Ism, Ism
        cg::AppendList(
            instr_list,
            new assem::OperInstr(
                "movq $" +
                    std::to_string(static_cast<ConstExp *>(src_)->consti_) +
                    "," + std::to_string(dst_of_cst->consti_),
                new temp::TempList(), new temp::TempList(), nullptr));
      else
        // movq s, Ism
        cg::AppendList(instr_list,
                       new assem::OperInstr(
                           "movq `s0," + std::to_string(dst_of_cst->consti_),
                           new temp::TempList(),
                           new temp::TempList(src_->Munch(instr_list, fs)),
                           nullptr));
    } else {
      // movq s, (r)
      if (typeid(*src_) == typeid(ConstExp))
        // movq $Ism, (r)
        cg::AppendList(
            instr_list,
            new assem::OperInstr(
                "movq $" +
                    std::to_string(static_cast<ConstExp *>(src_)->consti_) +
                    "(`s0)",
                new temp::TempList(),
                new temp::TempList(dst_of_mem->Munch(instr_list, fs)),
                nullptr));
      else
        // movq s,(r)
        cg::AppendList(
            instr_list,
            new assem::OperInstr(
                "movq `s0,(`s1)", new temp::TempList(),
                new temp::TempList{src_->Munch(instr_list, fs),
                                   dst_of_mem->exp_->Munch(instr_list, fs)},
                nullptr));
    }
  } else if (typeid(*src_) == typeid(MemExp)) {
    MemExp *src_of_mem = static_cast<MemExp *>(src_);
    if (typeid(*src_of_mem->exp_) == typeid(BinopExp)) {
      BinopExp *mem_of_bin = static_cast<BinopExp *>(src_of_mem->exp_);
      if (mem_of_bin->op_ == PLUS_OP) {
        if (typeid(*mem_of_bin->left_) == typeid(ConstExp) ||
            typeid(*mem_of_bin->right_) == typeid(ConstExp)) {
          // movq Imm(r), d
          bool is_left = typeid(*mem_of_bin->left_) == typeid(ConstExp);
          ConstExp *cst_exp = is_left
                                  ? static_cast<ConstExp *>(mem_of_bin->left_)
                                  : static_cast<ConstExp *>(mem_of_bin->right_);
          tree::Exp *temp_exp =
              is_left ? mem_of_bin->right_ : mem_of_bin->left_;
          temp::Temp *temp_tmp = temp_exp->Munch(instr_list, fs);

          temp::Temp *dst_temp = dst_->Munch(instr_list, fs);

          std::string instr_str = "";
          if (temp_tmp == reg_manager->FramePointer()) {
            if (cst_exp->consti_ < 0)
              instr_str += "movq (" + std::string(fs) + "_framesize" +
                           std::to_string(cst_exp->consti_) + ")(`s0)," + "`d0";
            else
              instr_str += "movq (" + std::string(fs) + "_framesize+" +
                           std::to_string(cst_exp->consti_) + ")(`s0)," + "`d0";
            temp_tmp = reg_manager->StackPointer();
#ifdef GC
            dst_temp->isStorePointer = cg::FrameIsPointer(cst_exp->consti_);
#endif
          } else {
            instr_str +=
                "movq " + std::to_string(cst_exp->consti_) + "(`s0)," + "`d0";
          }
          cg::AppendList(
              instr_list,
              new assem::OperInstr(instr_str, new temp::TempList(dst_temp),
                                   new temp::TempList(temp_tmp), nullptr));
        } else {
          // movq (r), d
          cg::AppendList(
              instr_list,
              new assem::OperInstr(
                  "movq (`s0),`d0",
                  new temp::TempList(dst_->Munch(instr_list, fs)),
                  new temp::TempList(src_of_mem->exp_->Munch(instr_list, fs)),
                  nullptr));
        }
      }
    } else if (typeid(*src_of_mem->exp_) == typeid(ConstExp)) {
      // movq Ism, d
      cg::AppendList(
          instr_list,
          new assem::OperInstr(
              "movq " +
                  std::to_string(
                      static_cast<ConstExp *>(src_of_mem->exp_)->consti_) +
                  ",`d0",
              new temp::TempList(dst_->Munch(instr_list, fs)),
              new temp::TempList(), nullptr));
    } else {
      // movq (r), d
      cg::AppendList(
          instr_list,
          new assem::OperInstr(
              "movq (`s0),`d0", new temp::TempList(dst_->Munch(instr_list, fs)),
              new temp::TempList(src_of_mem->exp_->Munch(instr_list, fs)),
              nullptr));
    }

  } else if (typeid(*src_) == typeid(ConstExp)) {
    // movq $Ism, d
    cg::AppendList(
        instr_list,
        new assem::OperInstr(
            "movq $" + std::to_string(static_cast<ConstExp *>(src_)->consti_) +
                ",`d0",
            new temp::TempList(dst_->Munch(instr_list, fs)),
            new temp::TempList(), nullptr));
  } else {
    // movq s, d
    cg::AppendList(
        instr_list,
        new assem::MoveInstr("movq `s0,`d0",
                             new temp::TempList(dst_->Munch(instr_list, fs)),
                             new temp::TempList(src_->Munch(instr_list, fs))));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);
  temp::Temp *result_temp = temp::TempFactory::NewTemp();
  switch (op_) {
  case PLUS_OP: {
    if (left_temp == reg_manager->FramePointer())
      cg::AppendList(instr_list,
                     new assem::OperInstr("leaq " + std::string(fs) +
                                              "_framesize(%rsp),`d0",
                                          new temp::TempList(result_temp),
                                          new temp::TempList(), nullptr));
    else
      cg::AppendList(instr_list,
                     new assem::MoveInstr("movq `s0,`d0",
                                          new temp::TempList(result_temp),
                                          new temp::TempList(left_temp)));
    cg::AppendList(instr_list,
                   new assem::OperInstr(
                       "addq `s0,`d0", new temp::TempList(result_temp),
                       new temp::TempList{right_temp, result_temp}, nullptr));
    return result_temp;
  }
  case MINUS_OP: {
    cg::AppendList(instr_list,
                   new assem::MoveInstr("movq `s0,`d0",
                                        new temp::TempList(result_temp),
                                        new temp::TempList(left_temp)));
    cg::AppendList(instr_list,
                   new assem::OperInstr(
                       "subq `s0,`d0", new temp::TempList(result_temp),
                       new temp::TempList{right_temp, result_temp}, nullptr));
    return result_temp;
  }
  case MUL_OP: {
    auto rax = reg_manager->Rax();
    auto rdx = reg_manager->Rdx();
    cg::AppendList(instr_list, new assem::MoveInstr(
                                   "movq `s0,%rax", new temp::TempList(rax),
                                   new temp::TempList(left_temp)));
    cg::AppendList(
        instr_list,
        new assem::OperInstr("imulq `s0", new temp::TempList{rax, rdx},
                             new temp::TempList{right_temp, rax}, nullptr));
    return rax;
  }
  case DIV_OP: {
    auto rax = reg_manager->Rax();
    auto rdx = reg_manager->Rdx();
    cg::AppendList(instr_list, new assem::MoveInstr(
                                   "movq `s0,%rax", new temp::TempList(rax),
                                   new temp::TempList(left_temp)));
    cg::AppendList(instr_list,
                   new assem::OperInstr("cqto", new temp::TempList(rdx),
                                        new temp::TempList(), nullptr));
    cg::AppendList(instr_list,
                   new assem::OperInstr(
                       "idivq `s0", new temp::TempList{rax, rdx},
                       new temp::TempList{right_temp, rax, rdx}, nullptr));
    return rax;
  }
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *return_temp = temp::TempFactory::NewTemp();
  if (typeid(*exp_) == typeid(BinopExp)) {
    BinopExp *exp_of_bin = static_cast<BinopExp *>(exp_);
    if (exp_of_bin->op_ == PLUS_OP) {
      if (typeid(*exp_of_bin->left_) == typeid(ConstExp) ||
          typeid(*exp_of_bin->right_) == typeid(ConstExp)) {
        // movq Imm(r),d
        bool is_left = typeid(*exp_of_bin->left_) == typeid(ConstExp);
        ConstExp *cst_exp = is_left
                                ? static_cast<ConstExp *>(exp_of_bin->left_)
                                : static_cast<ConstExp *>(exp_of_bin->right_);
        tree::Exp *temp_exp = is_left ? exp_of_bin->right_ : exp_of_bin->left_;
        temp::Temp *temp_tmp = temp_exp->Munch(instr_list, fs);

        std::string instr_str = "";
        if (temp_tmp == reg_manager->FramePointer()) {
          instr_str += "movq (" + std::string(fs) + "_framesize" +
                       std::to_string(cst_exp->consti_) + ")(`s0),`d0";
          temp_tmp = reg_manager->StackPointer();
#ifdef GC
          return_temp->isStorePointer = cg::FrameIsPointer(cst_exp->consti_);
#endif
        } else {
          instr_str += "movq " + std::to_string(cst_exp->consti_) + "(`s0),`d0";
        }
        cg::AppendList(
            instr_list,
            new assem::OperInstr(instr_str, new temp::TempList(return_temp),
                                 new temp::TempList(temp_tmp), nullptr));
      } else {
        // movq (r), d
        cg::AppendList(instr_list,
                       new assem::OperInstr(
                           "movq (`s0),`d0", new temp::TempList(return_temp),
                           new temp::TempList(exp_->Munch(instr_list, fs)),
                           nullptr));
      }
    }
  } else if (typeid(*exp_) == typeid(ConstExp)) {
    // movq Imm, d
    ConstExp *exp_of_cst = static_cast<ConstExp *>(exp_);
    cg::AppendList(instr_list,
                   new assem::OperInstr(
                       "movq " + std::to_string(exp_of_cst->consti_) + ",`d0",
                       new temp::TempList(return_temp), new temp::TempList(),
                       nullptr));
  } else {
    // movq (r), d
    cg::AppendList(
        instr_list,
        new assem::OperInstr("movq (`s0),`d0", new temp::TempList(return_temp),
                             new temp::TempList(exp_->Munch(instr_list, fs)),
                             nullptr));
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
  temp::Temp *addr_str = temp::TempFactory::NewTemp();
  cg::AppendList(instr_list,
                 new assem::OperInstr("leaq " + name_->Name() + "(%rip),`d0",
                                      new temp::TempList(addr_str),
                                      new temp::TempList(), nullptr));
  return addr_str;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *return_temp = temp::TempFactory::NewTemp();
  cg::AppendList(instr_list, new assem::OperInstr(
                                 "movq $" + std::to_string(consti_) + ",`d0",
                                 new temp::TempList(return_temp),
                                 new temp::TempList(), nullptr));
  return return_temp;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  size_t args_size = args_->GetList().size();
  size_t args_max_size = reg_manager->ArgRegs()->GetList().size();

  if (args_size > args_max_size)
    cg::AppendList(instr_list,
                   new assem::OperInstr(
                       "subq $" +
                           std::to_string((args_size - args_max_size) *
                                          reg_manager->WordSize()) +
                           ",%rsp",
                       new temp::TempList(), new temp::TempList(), nullptr));

  temp::TempList *caller_saves_regs = reg_manager->CallerSaves();
  caller_saves_regs->Append(reg_manager->ReturnValue());
  cg::AppendList(instr_list,
                 new assem::OperInstr(
                     "callq " + static_cast<NameExp *>(fun_)->name_->Name(),
                     caller_saves_regs, args_->MunchArgs(instr_list, fs),
                     nullptr));

#ifdef GC
  temp::Label *return_label = temp::LabelFactory::NewLabel();
  cg::AppendList(instr_list, new assem::LabelInstr(
                                 temp::LabelFactory::LabelString(return_label),
                                 return_label));

  reg_manager->ReturnValue()->isStorePointer =
      cg::ReturnIsPointer(static_cast<NameExp *>(fun_)->name_->Name());
#endif

  if (args_size > args_max_size)
    cg::AppendList(instr_list,
                   new assem::OperInstr(
                       "addq $" +
                           std::to_string((args_size - args_max_size) *
                                          reg_manager->WordSize()) +
                           ",%rsp",
                       new temp::TempList(), new temp::TempList(), nullptr));

  return reg_manager->ReturnValue();
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  size_t args_max_size = reg_manager->ArgRegs()->GetList().size();
  temp::TempList *arg_temp_list = reg_manager->ArgRegs();
  temp::TempList *temp_list = new temp::TempList();

  size_t exp_list_size = exp_list_.size();
  size_t i = 0;
  for (auto exp : exp_list_) {
    temp::Temp *exp_temp = exp->Munch(instr_list, fs);
    switch (i) {
    case 0:
#ifdef GC
      reg_manager->ArgRegs()->NthTemp(i)->isStorePointer = cg::RegIsPointer(i);
#endif
      temp_list->Append(arg_temp_list->NthTemp(i));
      if (exp_temp == reg_manager->FramePointer())
        cg::AppendList(instr_list,
                       new assem::OperInstr(
                           "leaq " + std::string(fs) + "_framesize(%rsp),`d0",
                           new temp::TempList(arg_temp_list->NthTemp(i)),
                           new temp::TempList(), nullptr));
      else
        cg::AppendList(
            instr_list,
            new assem::MoveInstr("movq `s0,`d0",
                                 new temp::TempList(arg_temp_list->NthTemp(i)),
                                 new temp::TempList(exp_temp)));
      break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
#ifdef GC
      reg_manager->ArgRegs()->NthTemp(i)->isStorePointer = cg::RegIsPointer(i);
#endif
      temp_list->Append(arg_temp_list->NthTemp(i));
      cg::AppendList(
          instr_list,
          new assem::MoveInstr("movq `s0,`d0",
                               new temp::TempList(arg_temp_list->NthTemp(i)),
                               new temp::TempList(exp_temp)));
      break;
    default:
      cg::AppendList(
          instr_list,
          new assem::OperInstr("movq `s0," +
                                   std::to_string((i - args_max_size) *
                                                  reg_manager->WordSize()) +
                                   "(%rsp)",
                               new temp::TempList(),
                               new temp::TempList(exp_temp), nullptr));
      break;
    }
    i++;
  }
  return temp_list;
}

} // namespace tree
