#include "tiger/liveness/liveness.h"
#include <algorithm>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

temp::TempList *Union(temp::TempList *list_A, temp::TempList *list_B) {
  temp::TempList *res = new temp::TempList();

  if (!list_A && !list_B)
    return res;

  if (!list_A) {
    for (auto item_B : list_B->GetList())
      res->Append(item_B);
    return res;
  }

  if (!list_B) {
    for (auto item_A : list_A->GetList())
      res->Append(item_A);
    return res;
  }

  for (auto item_A : list_A->GetList())
    res->Append(item_A);

  for (auto item_B : list_B->GetList())
    if (std::find(res->GetList().begin(), res->GetList().end(), item_B) ==
        res->GetList().end())
      res->Append(item_B);
  return res;
}

temp::TempList *Subtract(temp::TempList *list_A, temp::TempList *list_B) {
  temp::TempList *res = new temp::TempList();

  if (!list_A)
    return res;

  for (auto item_A : list_A->GetList())
    if (!list_B || std::find(list_B->GetList().begin(), list_B->GetList().end(),
                             item_A) == list_B->GetList().end())
      res->Append(item_A);

  return res;
}

bool IsSame(temp::TempList *list_A, temp::TempList *list_B) {
  if (list_A == nullptr || list_B == nullptr)
    return list_A == list_B;

  for (auto item_A : list_A->GetList())
    if (std::find(list_B->GetList().begin(), list_B->GetList().end(), item_A) ==
        list_B->GetList().end())
      return false;
  return true;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  for (auto flow_node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(flow_node, new temp::TempList());
    out_->Enter(flow_node, new temp::TempList());
  }

  while (true) {
    bool isAllSame = true;
    for (auto flow_node_it = flowgraph_->Nodes()->GetList().rbegin();
         flow_node_it != flowgraph_->Nodes()->GetList().rend();
         flow_node_it++) {
      temp::TempList *pre_out = out_->Look(*flow_node_it);

      temp::TempList *new_out = nullptr;
      for (auto flow_node_succ : (*flow_node_it)->Succ()->GetList())
        new_out = Union(new_out, in_->Look(flow_node_succ));

      temp::TempList *pre_in = in_->Look(*flow_node_it);
      temp::TempList *new_in =
          Union((*flow_node_it)->NodeInfo()->Use(),
                Subtract(new_out, (*flow_node_it)->NodeInfo()->Def()));

      if (!IsSame(new_in, pre_in) || !IsSame(new_out, pre_out)) {
        isAllSame = false;
        in_->Enter(*flow_node_it, new_in);
        out_->Enter(*flow_node_it, new_out);
      }
    }
    if (isAllSame)
      break;
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  live_graph_.moves = new MoveList();

  temp::TempList *reg_except_rsp = reg_manager->RegistersExceptRsp();

  for (auto reg : reg_except_rsp->GetList())
    temp_node_map_->Enter(reg, live_graph_.interf_graph->NewNode(reg));

  for (auto reg_it_A = reg_except_rsp->GetList().begin();
       reg_it_A != reg_except_rsp->GetList().end(); reg_it_A++)
    for (auto reg_it_B = std::next(reg_it_A);
         reg_it_B != reg_except_rsp->GetList().end(); reg_it_B++) {
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(*reg_it_A),
                                        temp_node_map_->Look(*reg_it_B));
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(*reg_it_B),
                                        temp_node_map_->Look(*reg_it_A));
    }

  for (auto node : flowgraph_->Nodes()->GetList())
    for (temp::Temp *reg :
         Union(out_->Look(node), node->NodeInfo()->Def())->GetList()) {
      if (reg == reg_manager->StackPointer())
        continue;
      if (temp_node_map_->Look(reg) == nullptr)
        temp_node_map_->Enter(reg, live_graph_.interf_graph->NewNode(reg));
    }

  for (auto node : flowgraph_->Nodes()->GetList()) {
    if (typeid(*(node->NodeInfo())) == typeid(assem::MoveInstr)) {
      for (auto def : node->NodeInfo()->Def()->GetList()) {
        for (auto out :
             Subtract(out_->Look(node), node->NodeInfo()->Use())->GetList()) {
          if (def == reg_manager->StackPointer() ||
              out == reg_manager->StackPointer())
            continue;
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(def),
                                            temp_node_map_->Look(out));
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(out),
                                            temp_node_map_->Look(def));
        }
        for (auto use : node->NodeInfo()->Use()->GetList()) {
          if (def == reg_manager->StackPointer() ||
              use == reg_manager->StackPointer())
            continue;
          if (!live_graph_.moves->Contain(temp_node_map_->Look(def),
                                          temp_node_map_->Look(use)))
            live_graph_.moves->Append(temp_node_map_->Look(def),
                                      temp_node_map_->Look(use));
        }
      }
    } else {
      for (auto def : node->NodeInfo()->Def()->GetList())
        for (auto out : out_->Look(node)->GetList()) {
          if (def == reg_manager->StackPointer() ||
              out == reg_manager->StackPointer())
            continue;
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(def),
                                            temp_node_map_->Look(out));
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(out),
                                            temp_node_map_->Look(def));
        }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
