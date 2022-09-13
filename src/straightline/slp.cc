#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int stm1Args=stm1->MaxArgs();
  int stm2Args=stm2->MaxArgs();
  return stm1Args>stm2Args?stm1Args:stm2Args;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *newIntAndTable = exp->Interp(t);
  return new Table(id,newIntAndTable->i,newIntAndTable->t);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int expsArgs=exps->MaxArgs();
  int expsNum=exps->NumExps();
  return expsArgs>expsNum?expsArgs:expsNum;
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return exps->Interp(t)->t;
}

int A::IdExp::MaxArgs() const {
  // TODO: put your code here (lab1).
  return 0;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return new IntAndTable(t->Lookup(id),t);
}

int A::NumExp::MaxArgs() const {
  // TODO: put your code here (lab1).
  return 0;
}

IntAndTable *A::NumExp::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return new IntAndTable(num,t);
}

int A::OpExp::MaxArgs() const {
  // TODO: put your code here (lab1).
  int leftArgs=left->MaxArgs();
  int rightArgs=right->MaxArgs();
  return leftArgs>rightArgs?leftArgs:rightArgs;
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *leftIntAndTable=left->Interp(t);
  IntAndTable *rightIntAndTable=right->Interp(leftIntAndTable->t);
  switch(oper)
  {
    case PLUS:
    return new IntAndTable(leftIntAndTable->i+rightIntAndTable->i,rightIntAndTable->t);
    case MINUS:
    return new IntAndTable(leftIntAndTable->i-rightIntAndTable->i,rightIntAndTable->t);
    case TIMES:
    return new IntAndTable(leftIntAndTable->i*rightIntAndTable->i,rightIntAndTable->t);
    case DIV:
    return new IntAndTable(leftIntAndTable->i/rightIntAndTable->i,rightIntAndTable->t);
    default:
    break;
  }
  return nullptr;
}

int A::EseqExp::MaxArgs() const {
  // TODO: put your code here (lab1).
  int stmArgs=stm->MaxArgs();
  int expArgs=exp->MaxArgs();
  return stmArgs>expArgs?stmArgs:expArgs;
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return exp->Interp(stm->Interp(t));
}

int A::PairExpList::MaxArgs() const {
  // TODO: put your code here (lab1).
  int expArgs=exp->MaxArgs();
  int tailArgs=tail->MaxArgs();
  return expArgs>tailArgs?expArgs:tailArgs;
}

int A::PairExpList::NumExps() const {
  // TODO: put your code here (lab1).
  return 1+tail->NumExps();
}

IntAndTable *A::PairExpList::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *newIntAndTable=exp->Interp(t);
  printf("%d ",newIntAndTable->i);
  return tail->Interp(newIntAndTable->t);
}

int A::LastExpList::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

int A::LastExpList::NumExps() const {
  // TODO: put your code here (lab1).
  return 1;
}

IntAndTable *A::LastExpList::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *newIntAndTable=exp->Interp(t);
  printf("%d\n",newIntAndTable->i);
  return newIntAndTable;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
}  // namespace A
