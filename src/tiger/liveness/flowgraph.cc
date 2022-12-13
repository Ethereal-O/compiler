#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  flowgraph_ = new graph::Graph<assem::Instr>;

  std::list<std::pair<assem::OperInstr *, FNodePtr>> jump_list;

  FNodePtr from = nullptr;
  for (auto instr : instr_list_->GetList()) {
    FNodePtr to = flowgraph_->NewNode(instr);
    if (from)
      flowgraph_->AddEdge(from, to);
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      label_map_->Enter(static_cast<assem::LabelInstr *>(instr)->label_, to);
      from = to;
    } else if (typeid(*instr) == typeid(assem::OperInstr) &&
               static_cast<assem::OperInstr *>(instr)->jumps_) {
      from = nullptr;
      jump_list.emplace_back(
          std::make_pair(static_cast<assem::OperInstr *>(instr), to));
    } else {
      from = to;
    }
  }

  for (auto jump_pair : jump_list)
    for (auto label : *jump_pair.first->jumps_->labels_)
      flowgraph_->AddEdge(jump_pair.second, label_map_->Look(label));
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}
} // namespace assem
