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
private:
  uint32_t in_reg_num_;

public:
  explicit X64Frame(temp::Label *name, std::list<bool> formals) {
    name_ = name;
    size_ = reg_manager->WordSize();
    in_reg_num_ = 0;

    for (auto formal : formals)
      allocLocal(formal);
  }

  Access *allocLocal(bool escape) {
    static const uint32_t max_in_reg_num =
        reg_manager->ArgRegs()->GetList().size();
    Access *access;
    if (escape || in_reg_num_ >= max_in_reg_num) {
      access = new InFrameAccess(-size_);
      size_ += reg_manager->WordSize();
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
      in_reg_num_++;
    }
    formals_.push_back(access);
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

} // namespace frame