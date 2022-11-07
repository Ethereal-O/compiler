#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new Access(level, level->frame_->allocLocal(escape));
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *stm_ = new tree::CjumpStm(
        tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    PatchList trues = PatchList({&stm_->true_label_});
    PatchList falses = PatchList({&stm_->false_label_});
    return Cx(trues, falses, stm_);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(0, "Should never expect to see a tr::NxExp kind!");
    return Cx(PatchList(), PatchList(), nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    temp::Label *label = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(label);
    cx_.falses_.DoPatch(label);
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(label));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

tree::Exp *GetStaticLink(Level *current, Level *target) {
  tree::Exp *frame_ptr = new tree::TempExp(reg_manager->FramePointer());
  while (current != target) {
    frame_ptr = current->frame_->formals_.front()->ToExp(frame_ptr);
    current = current->parent_;
  }
  return frame_ptr;
}

tree::Stm *GetRecord(temp::Temp *record_temp,
                     std::vector<tr::ExpAndTy *> *tr_ExpAndTys, int index) {
  auto newStm = new tree::MoveStm(
      new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, new tree::TempExp(record_temp),
          new tree::ConstExp(index * reg_manager->WordSize()))),
      tr_ExpAndTys->at(index)->exp_->UnEx());
  if (index < tr_ExpAndTys->size() - 1)
    return new tree::SeqStm(newStm,
                            GetRecord(record_temp, tr_ExpAndTys, ++index));
  else
    return newStm;
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                         temp::LabelFactory::NewLabel(), errormsg_.get());
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::VarEntry *var_entry = static_cast<env::VarEntry *>(venv->Look(sym_));
  return new tr::ExpAndTy(
      new tr::ExExp(var_entry->access_->access_->ToExp(
          tr::GetStaticLink(level, var_entry->access_->level_))),
      var_entry->ty_->ActualTy());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  uint32_t offset = 0;
  type::Ty *type = nullptr;
  tr::ExpAndTy *var_ExpAndTy =
      var_->Translate(venv, tenv, level, label, errormsg);
  for (type::Field *a_field :
       static_cast<type::RecordTy *>(var_ExpAndTy->ty_)->fields_->GetList()) {
    if (a_field->name_->Name().compare(sym_->Name()) == 0) {
      type = a_field->ty_->ActualTy();
      break;
    }
    offset += reg_manager->WordSize();
  }
  return new tr::ExpAndTy(new tr::ExExp(new tree::MemExp(new tree::BinopExp(
                              tree::BinOp::PLUS_OP, var_ExpAndTy->exp_->UnEx(),
                              new tree::ConstExp(offset)))),
                          type->ActualTy());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_ExpAndTy =
      var_->Translate(venv, tenv, level, label, errormsg);

  tr::ExpAndTy *exp_ExpAndTy =
      subscript_->Translate(venv, tenv, level, label, errormsg);

  return new tr::ExpAndTy(
      new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, var_ExpAndTy->exp_->UnEx(),
          new tree::BinopExp(tree::BinOp::MUL_OP,
                             new tree::ConstExp(reg_manager->WordSize()),
                             exp_ExpAndTy->exp_->UnEx())))),
      static_cast<type::ArrayTy *>(var_ExpAndTy->ty_)->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::NameExp(temp::LabelFactory::NewLabel())),
      type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::FunEntry *fun_entry = static_cast<env::FunEntry *>(venv->Look(func_));
  tree::ExpList *args_ExpList = new tree::ExpList();
  for (auto arg : args_->GetList())
    args_ExpList->Append(
        arg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx());

  tree::Exp *call_Exp;
  if (!fun_entry->level_) {
    call_Exp = frame::ExternalCall(fun_entry->label_->Name(), args_ExpList);
  } else {
    args_ExpList->Insert(tr::GetStaticLink(level, fun_entry->level_));
    call_Exp =
        new tree::CallExp(new tree::NameExp(fun_entry->label_), args_ExpList);
  }
  return new tr::ExpAndTy(new tr::ExExp(call_Exp),
                          fun_entry->result_->ActualTy());
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *left_ExpAndTy =
      left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_ExpAndTy =
      right_->Translate(venv, tenv, level, label, errormsg);

  tree::BinOp tree_binop;
  tree::RelOp tree_relop;
  switch (oper_) {
    // +-*/
  case Oper::PLUS_OP:
    tree_binop = tree::PLUS_OP;
    goto return_exexp;
  case Oper::MINUS_OP:
    tree_binop = tree::MINUS_OP;
    goto return_exexp;
  case Oper::TIMES_OP:
    tree_binop = tree::MUL_OP;
    goto return_exexp;
  case Oper::DIVIDE_OP:
    tree_binop = tree::DIV_OP;
    goto return_exexp;
    // ><
  case Oper::LT_OP:
    tree_relop = tree::LT_OP;
    goto return_glexp;
  case Oper::LE_OP:
    tree_relop = tree::LE_OP;
    goto return_glexp;
  case Oper::GT_OP:
    tree_relop = tree::GT_OP;
    goto return_glexp;
  case Oper::GE_OP:
    tree_relop = tree::GE_OP;
    goto return_glexp;
  // =&!=
  case EQ_OP:
    tree_relop = tree::EQ_OP;
    goto return_eqexp;
  case NEQ_OP:
    tree_relop = tree::NE_OP;
    goto return_eqexp;
  }

return_exexp : {
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::BinopExp(tree_binop, left_ExpAndTy->exp_->UnEx(),
                                       right_ExpAndTy->exp_->UnEx())),
      type::IntTy::Instance());
}

return_glexp : {
  tree::CjumpStm *stm =
      new tree::CjumpStm(tree_relop, left_ExpAndTy->exp_->UnEx(),
                         right_ExpAndTy->exp_->UnEx(), nullptr, nullptr);
  return new tr::ExpAndTy(new tr::CxExp(tr::PatchList({&stm->true_label_}),
                                        tr::PatchList({&stm->false_label_}),
                                        stm),
                          type::IntTy::Instance());
}

return_eqexp : {
#define EXTERNALCALL_STRING_EQUAL "string_equal"
#define EXTERNALCALL_STRING_EQUAL_RETURN 1
  tree::CjumpStm *stm;
  if (left_ExpAndTy->ty_->ActualTy()->IsSameType(type::StringTy::Instance()))
    stm = new tree::CjumpStm(
        tree_relop,
        frame::ExternalCall(EXTERNALCALL_STRING_EQUAL,
                            new tree::ExpList({left_ExpAndTy->exp_->UnEx(),
                                               right_ExpAndTy->exp_->UnEx()})),
        new tree::ConstExp(EXTERNALCALL_STRING_EQUAL_RETURN), nullptr, nullptr);
  else
    stm = new tree::CjumpStm(tree_relop, left_ExpAndTy->exp_->UnEx(),
                             right_ExpAndTy->exp_->UnEx(), nullptr, nullptr);

  return new tr::ExpAndTy(new tr::CxExp(tr::PatchList({&stm->true_label_}),
                                        tr::PatchList({&stm->false_label_}),
                                        stm),
                          type::IntTy::Instance());
}
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lafieldb5 code here */
  auto record_temp = temp::TempFactory::NewTemp();
  tree::MoveStm *alloc_stm = new tree::MoveStm(
      new tree::TempExp(record_temp),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          new tree::ExpList{new tree::ConstExp(fields_->GetList().size() *
                                               reg_manager->WordSize())}));

  // transform list into vector
  std::vector<tr::ExpAndTy *> tr_ExpAndTys;
  for (auto field : fields_->GetList())
    tr_ExpAndTys.push_back(
        field->exp_->Translate(venv, tenv, level, label, errormsg));

  // record must have at least one value, so there is no error
  tree::Stm *stm = tr::GetRecord(record_temp, &tr_ExpAndTys, 0);

  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(new tree::SeqStm(alloc_stm, stm),
                                      new tree::TempExp(record_temp))),
      tenv->Look(typ_)->ActualTy());
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto exps = seq_->GetList();
  if (exps.size() == 0)
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  tr::ExpAndTy *first_exp =
      exps.front()->Translate(venv, tenv, level, label, errormsg);
  if (exps.size() == 1)
    return first_exp;

  auto stm = first_exp->exp_->UnNx();
  for (auto iter = ++exps.begin(); iter != (--exps.end()); iter++)
    stm = new tree::SeqStm(
        stm,
        (*iter)->Translate(venv, tenv, level, label, errormsg)->exp_->UnNx());

  tr::ExpAndTy *last_ExpAndTy =
      exps.back()->Translate(venv, tenv, level, label, errormsg);

  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(stm, last_ExpAndTy->exp_->UnEx())),
      last_ExpAndTy->ty_->ActualTy());
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_ExpAndTy =
      var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_ExpAndTy =
      exp_->Translate(venv, tenv, level, label, errormsg);
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::MoveStm(var_ExpAndTy->exp_->UnEx(),
                                      exp_ExpAndTy->exp_->UnEx())),
      type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::NameTy(name_, tenv->Look(name_));
  // return new type::NameTy(name_, tenv->Look(name_)->ActualTy());
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::ArrayTy(tenv->Look(array_)->ActualTy());
}

} // namespace absyn
