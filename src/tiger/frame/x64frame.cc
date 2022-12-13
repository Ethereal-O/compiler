#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
// class InFrameAccess : public Access {
// public:
//   int offset;

//   explicit InFrameAccess(int offset) : offset(offset) {}
//   /* TODO: Put your lab5 code here */
//   tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
//     return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr,
//                                                new tree::ConstExp(offset)));
//   }
// };

// class InRegAccess : public Access {
// public:
//   temp::Temp *reg;

//   explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
//   /* TODO: Put your lab5 code here */
//   tree::Exp *ToExp(tree::Exp *framePtr) const override {
//     return new tree::TempExp(reg);
//   }
// };

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */

public:
  explicit X64Frame(temp::Label *name, std::list<bool> formals) {
    name_ = name;
    size_ = 0;

    for (auto formal : formals)
      formals_.push_back(allocLocal(formal));

    static const uint32_t max_in_reg_num =
        reg_manager->ArgRegs()->GetList().size();
  }

  Access *allocLocal(bool escape) {
    Access *access;
    if (escape) {
      access = new InFrameAccess(-size_ - reg_manager->WordSize());
      size_ += reg_manager->WordSize();
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    return access;
  }
};
/* TODO: Put your lab5 code here */

frame::Frame *Frame::NewFrame(temp::Label *name, std::list<bool> formals) {
  return new X64Frame(name, formals);
}

tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

/**
 * IMPORTANT:
 * The ProcEntryExit1 function is resolved into two function, which are
 * tr::GetProcFrag and frame::ProcEntryExit1_Refactor. tr::GetProcFrag is to
 * generate the main routine of the function, and frame::ProcEntryExit1_Refactor
 * is to save and recover the callee-saved-registers.
 * This is because add objects in the end of the seqStm is time-cosuming.
 */

assem::InstrList *ProcEntryExit1_Refactor(assem::InstrList *instr_list) {
  temp::TempList *callee_regs = reg_manager->CalleeSaves();
  for (int i = 0; i < callee_regs->GetList().size(); i++) {
    temp::Temp *new_temp = temp::TempFactory::NewTemp();

    instr_list->Insert(
        instr_list->GetList().begin(),
        new assem::MoveInstr("movq `s0,`d0", new temp::TempList(new_temp),
                             new temp::TempList(callee_regs->NthTemp(i))));

    instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0", new temp::TempList(callee_regs->NthTemp(i)),
        new temp::TempList(new_temp)));
  }
  return instr_list;
}

assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  std::string prologue = "";
  prologue += ".set " + frame->name_->Name() + std::string("_framesize, ") +
              std::to_string(frame->size_) + "\n";
  prologue += frame->name_->Name() + ":\n";
  prologue += "subq $" + std::to_string(frame->size_) + ",%rsp\n";

  std::string epilogue = "";
  epilogue += "addq $" + std::to_string(frame->size_) + ",%rsp\n";
  epilogue += "retq\n";
  return new assem::Proc(prologue, body, epilogue);
}

} // namespace frame