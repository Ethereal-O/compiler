#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
  return;
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing SimpleVar...\n");
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing FieldVar...\n");
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!(var_ty && typeid(*var_ty) == typeid(type::RecordTy))) {
    errormsg->Error(var_->pos_, "not a record type");
    return type::IntTy::Instance();
  }
  for (type::Field *a_field :
       static_cast<type::RecordTy *>(var_ty)->fields_->GetList()) {
    if (a_field->name_->Name().compare(sym_->Name()) == 0) {
      return a_field->ty_->ActualTy();
    }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing SubscriptVar...\n");
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!(var_ty && typeid(*var_ty) == typeid(type::ArrayTy))) {
    errormsg->Error(var_->pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *subscript_ty =
      subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!(subscript_ty && typeid(*subscript_ty) == typeid(type::IntTy))) {
    errormsg->Error(subscript_->pos_, "integer required");
    return type::IntTy::Instance();
  }
  return var_ty;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing VarExp...\n");
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  return var_ty;
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing NilExp...\n");
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing IntExp...\n");
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing StringExp...\n");
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing CallExp...\n");
  env::EnvEntry *entry = venv->Look(func_);
  if (!(entry && typeid(*entry) == typeid(env::FunEntry))) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::IntTy::Instance();
  }
  type::TyList *ty_list = static_cast<env::FunEntry *>(entry)->formals_;
  std::list<type::Ty *>::const_iterator iter_def = ty_list->GetList().begin();
  std::list<absyn::Exp *>::const_iterator iter_input = args_->GetList().begin();
  for (; iter_def != ty_list->GetList().end() &&
         iter_input != args_->GetList().end();
       iter_def++, iter_input++) {
    type::Ty *a_ty_input =
        (*iter_input)->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!a_ty_input->IsSameType((*iter_def)->ActualTy())) {
      errormsg->Error(pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
  }
  if (iter_input != args_->GetList().end()) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().c_str());
    return type::IntTy::Instance();
  }
  if (iter_input != args_->GetList().end()) {
    errormsg->Error(pos_, "too few params in function %s",
                    func_->Name().c_str());
    return type::IntTy::Instance();
  }

  return static_cast<env::FunEntry *>(entry)->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing OpExp...\n");
  type::Ty *left_ty =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP ||
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (!(left_ty && typeid(*left_ty) == typeid(type::IntTy))) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (!(right_ty && typeid(*right_ty) == typeid(type::IntTy))) {
      errormsg->Error(right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing RecordExp...\n");
  type::Ty *type_ty = tenv->Look(typ_);

  if (!(type_ty && typeid(*(type_ty->ActualTy())) == typeid(type::RecordTy))) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }
  type::FieldList *ty_list =
      static_cast<type::RecordTy *>(type_ty->ActualTy())->fields_;
  std::list<type::Field *>::const_iterator iter_def =
      ty_list->GetList().begin();
  std::list<absyn::EField *>::const_iterator iter_input =
      fields_->GetList().begin();
  for (; iter_def != ty_list->GetList().end() &&
         iter_input != fields_->GetList().end();
       iter_def++, iter_input++) {
    type::Ty *a_ty_input =
        (*iter_input)
            ->exp_->SemAnalyze(venv, tenv, labelcount, errormsg)
            ->ActualTy();
    if (!((*iter_def)->name_->Name().compare((*iter_input)->name_->Name()) ==
              0 &&
          a_ty_input->IsSameType((*iter_def)->ty_->ActualTy()))) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  printf("%s\n", typeid(type_ty->ActualTy()).name());
  return type_ty->ActualTy();
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing SeqExp...\n");
  type::Ty *seq_ty;
  for (absyn::Exp *a_exp : seq_->GetList()) {
    seq_ty = a_exp->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  }
  return seq_ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing AssignExp...\n");
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *exp_ty =
      exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!(var_ty && exp_ty && var_ty->IsSameType(exp_ty))) {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  if (labelcount > 0) {
    errormsg->Error(pos_, "loop variable can't be assigned");
  }
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing IfExp...\n");
  type::Ty *test_ty =
      test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!(test_ty->IsSameType(type::IntTy::Instance()))) {
    errormsg->Error(pos_, "integer type required");
  }
  type::Ty *then_ty =
      then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!then_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "if-then exp's body must produce no value");
  }
  if (elsee_) {
    type::Ty *elsee_ty =
        elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!elsee_ty->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(pos_, "procedure returns value");
    }
    if (!elsee_ty->IsSameType(then_ty)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
  }
  return type::VoidTy::Instance();
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing WhileExp...\n");
  type::Ty *test_ty =
      test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *body_ty =
      body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg)->ActualTy();
  if (!(test_ty->IsSameType(type::IntTy::Instance()))) {
    errormsg->Error(pos_, "integer type required");
  }
  if (!body_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "while body must produce no value");
  }
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing ForExp...\n");
  venv->BeginScope();
  tenv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance()));
  type::Ty *lo_ty =
      lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *hi_ty =
      hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!(lo_ty->IsSameType(type::IntTy::Instance()) &&
        hi_ty->IsSameType(type::IntTy::Instance()))) {
    errormsg->Error(pos_, "for exp's range type is not integer");
  }
  type::Ty *body_ty =
      body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg)->ActualTy();
  if (!body_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "while body must produce no value");
  }
  venv->EndScope();
  tenv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing BreakExp...\n");
  if (labelcount <= 0) {
    errormsg->Error(pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing LetExp...\n");
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList())
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);

  type::Ty *result;
  if (!body_)
    result = type::VoidTy::Instance();
  else
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing ArrayExp...\n");
  type::Ty *type_ty = tenv->Look(typ_);
  if (!(type_ty && typeid(*(type_ty->ActualTy())) == typeid(type::ArrayTy))) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }
  type::Ty *size_ty =
      size_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *init_ty =
      init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!size_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "array exp's size type is not integer");
  }
  if (!init_ty->IsSameType(
          static_cast<type::ArrayTy *>(type_ty->ActualTy())->ty_->ActualTy())) {
    printf("aaa\n");
    errormsg->Error(pos_, "type mismatch");
  }
  return type_ty->ActualTy();
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing VoidExp...\n");
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing FunctionDec...\n");
  for (FunDec *a_fun_dec : functions_->GetList()) {
    env::EnvEntry *entry = venv->Look(a_fun_dec->name_);
    if (entry) {
      errormsg->Error(pos_, "two functions have the same name");
      continue;
    }
    type::TyList *formals =
        a_fun_dec->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *result = type::VoidTy::Instance();
    if (a_fun_dec->result_ != nullptr) {
      type::Ty *result_ty = tenv->Look(a_fun_dec->result_);
      if (!result_ty) {
        errormsg->Error(pos_, "undefined type %s",
                        a_fun_dec->result_->Name().data());
        continue;
      }
      result = result_ty->ActualTy();
    }
    venv->Enter(a_fun_dec->name_, new env::FunEntry(formals, result));
  }
  for (FunDec *a_fun_dec : functions_->GetList()) {
    env::EnvEntry *entry = venv->Look(a_fun_dec->name_);
    if (!entry) {
      continue;
    }
    type::TyList *formals = static_cast<env::FunEntry *>(entry)->formals_;
    type::Ty *result = static_cast<env::FunEntry *>(entry)->result_;

    venv->BeginScope();
    auto formal_it = formals->GetList().begin();
    auto param_it = a_fun_dec->params_->GetList().begin();
    for (; param_it != a_fun_dec->params_->GetList().end();
         formal_it++, param_it++)
      venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));

    if (!a_fun_dec->body_->SemAnalyze(venv, tenv, labelcount, errormsg)
             ->ActualTy()
             ->IsSameType(result)) {
      if (a_fun_dec->result_ == nullptr) {
        errormsg->Error(pos_, "procedure returns value");
      } else {
        errormsg->Error(pos_, "mismatched type %s",
                        a_fun_dec->result_->Name().data());
      }
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing VarDec...\n");
  env::EnvEntry *entry = venv->Look(var_);
  if (entry) {
    errormsg->Error(pos_, "redefined variable %s", var_->Name().data());
  }
  type::Ty *typ_ty = nullptr;
  if (typ_) {
    typ_ty = tenv->Look(typ_);
    if (!typ_ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    }
  }
  type::Ty *init_ty =
      init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (init_ty->IsSameType(type::NilTy::Instance())) {
    errormsg->Error(pos_, "init should not be nil without type specified");
  }
  if (typ_ty && !init_ty->IsSameType(typ_ty->ActualTy())) {
    errormsg->Error(pos_, "type mismatch");
  }
  venv->Enter(var_, new env::VarEntry(init_ty));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing TypeDec...\n");
  std::list<NameAndTy *> type_succ;
  for (NameAndTy *a_name_and_ty : types_->GetList()) {
    type::Ty *name_ty = tenv->Look(a_name_and_ty->name_);
    if (name_ty) {
      errormsg->Error(pos_, "two types have the same name");
      continue;
    }
    tenv->Enter(a_name_and_ty->name_,
                new type::NameTy(a_name_and_ty->name_, NULL));
    type_succ.push_back(a_name_and_ty);
  }
  for (NameAndTy *a_name_and_ty : type_succ) {
    type::Ty *typ_ty = a_name_and_ty->ty_->SemAnalyze(tenv, errormsg);
    if (!typ_ty) {
      continue;
    }
    type::NameTy *new_ty =
        static_cast<type::NameTy *>(tenv->Look(a_name_and_ty->name_));
    if (typeid(*typ_ty) == typeid(type::NameTy)) {
      type::NameTy *root_ty = static_cast<type::NameTy *>(typ_ty);
      while (root_ty->ty_ && typeid(*(root_ty->ty_)) == typeid(type::NameTy)) {
        root_ty = static_cast<type::NameTy *>(root_ty->ty_);
      }
      if (root_ty == new_ty) {
        errormsg->Error(pos_, "illegal type cycle");
        continue;
      }
    }
    new_ty->ty_ = typ_ty;
    tenv->Set(a_name_and_ty->name_, new_ty);
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing NameTy...\n");
  type::Ty *name_ty = tenv->Look(name_);
  if (!name_ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return nullptr;
  }
  return name_ty;
  // return name_ty->ActualTy();
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing RecordTy...\n");
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("analyzing ArrayTy...\n");
  type::Ty *array_ty = tenv->Look(array_);
  if (!array_ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return nullptr;
  }
  return new type::ArrayTy(array_ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
