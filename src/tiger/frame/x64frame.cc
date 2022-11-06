#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr,
                                               new tree::ConstExp(offset)));
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
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  explicit X64Frame(temp::Label *name, std::list<bool> formals) {
    const uint32_t wordSize = 8;
    const uint32_t maxInRegNum = 6;
    name_ = name;
    uint32_t offset = wordSize;

    formals_.push_back(new InFrameAccess(offset));
    offset += wordSize;

    uint32_t inRegNum = 0;
    for (auto formal : formals) {
      if (formal) {
        assert(inRegNum < maxInRegNum);
        formals_.push_back(new InRegAccess(temp::TempFactory::NewTemp()));
      } else {
        size += wordSize;
        Access *access = new InFrameAccess(-size);
        formals_.push_back(access);
        locals_.push_back(access);
      }
    }
  }
};
/* TODO: Put your lab5 code here */

frame::Frame *Frame::NewFrame(temp::Label *name, std::list<bool> formals) {
  return new X64Frame(name, formals);
}

Access *Frame::AllocLocal(bool escape) {
  const uint32_t wordSize = 8;
  Access *access;
  if (escape) {
    size += wordSize;
    access = new InFrameAccess(-size);
    formals_.push_back(access);
    locals_.push_back(access);
  } else {
    access = new InRegAccess(temp::TempFactory::NewTemp());
  }
  return access;
}

} // namespace frame