#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

#include <algorithm>
#include <set>

#define GC

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
void RegAllocator::RegAlloc() {
  allocation_ = std::make_unique<Result>();

  while (true) {
    printf("new term!\n");
    LivenessAnalysis();
    Build();
    MakeWorklist();
    while (!simplifyWorklist->GetList().empty() ||
           !worklistMoves->GetList().empty() ||
           !freezeWorklist->GetList().empty() ||
           !spillWorklist->GetList().empty()) {
      if (!simplifyWorklist->GetList().empty())
        Simplify();
      else if (!worklistMoves->GetList().empty())
        Coalesce();
      else if (!freezeWorklist->GetList().empty())
        Freeze();
      else if (!spillWorklist->GetList().empty())
        SelectSpill();
    }
    AssignColor();
    if (!spilledNodes->GetList().empty())
      RewriteProgram();
    else
      break;
  }

  allocation_->coloring_ = AssignRegisters();
  allocation_->il_ = assem_instr_.get()->GetInstrList();
}

void RegAllocator::LivenessAnalysis() {
  fg::FlowGraphFactory flow_graph_factory =
      fg::FlowGraphFactory(assem_instr_->GetInstrList());
  flow_graph_factory.AssemFlowGraph();
  live::LiveGraphFactory live_graph_factory =
      live::LiveGraphFactory(flow_graph_factory.GetFlowGraph());
  live_graph_factory.Liveness();
  live_graph_ = live_graph_factory.GetLiveGraph();
  adjGraph = live_graph_.interf_graph;
}

void RegAllocator::Build() {
  simplifyWorklist = new live::INodeList();
  freezeWorklist = new live::INodeList();
  spillWorklist = new live::INodeList();
  spilledNodes = new live::INodeList();
  worklistMoves = live_graph_.moves;

  movelist = new MoveListTab();
  degree = new DegreeTab();
  color = new ColorTab();
  alias = new AliasTab();

  selectStack = new live::INodeList();
  coalescedNodes = new live::INodeList();
  coloredNodes = new live::INodeList();
  frozenMoves = new live::MoveList();
  activeMoves = new live::MoveList();
  coalescedMoves = new live::MoveList();
  constrainedMoves = new live::MoveList();

  temp::TempList *reg_except_rsp = reg_manager->RegistersExceptRsp();
  K = reg_except_rsp->GetList().size();

  for (auto node : adjGraph->Nodes()->GetList()) {
    for (auto reg : reg_manager->RegistersExceptRsp()->GetList()) {
      if (node->NodeInfo() == reg) {
        coloredNodes->Append(node);
        color->Enter(node, reg);
      }
    }
    degree->Enter(node, new int(node->OutDegree()));
    alias->Enter(node, node);
    live::MoveList *moveList = new live::MoveList();
    for (auto move : worklistMoves->GetList())
      if (move.first == node || move.second == node)
        moveList->Append(move.first, move.second);
    movelist->Enter(node, moveList);
  }
}

void RegAllocator::MakeWorklist() {
  for (auto node : adjGraph->Nodes()->GetList()) {
    if (color->Look(node))
      continue;
    if (*degree->Look(node) >= K) {
      spillWorklist->Append(node);
    } else if (MoveRelated(node)) {
      freezeWorklist->Append(node);
    } else {
      simplifyWorklist->Append(node);
    }
  }
}

void RegAllocator::AssignColor() {
  for (auto node : coalescedNodes->GetList()) {
    color->Enter(node, color->Look(GetAlias(node)));
  }

  for (auto node : selectStack->GetList()) {
    std::set<temp::Temp *> okColors;
    for (auto reg : reg_manager->RegistersExceptRsp()->GetList())
      okColors.emplace(reg);
    for (auto node_adj : node->Succ()->GetList())
      if (color->Look(GetAlias(node_adj)))
        okColors.erase(color->Look(GetAlias(node_adj)));

    if (okColors.size() == 0) {
      spilledNodes->Append(node);
    } else {
      coloredNodes->Append(node);
      color->Enter(node, *okColors.begin());
    }
  }

  for (auto node : coalescedNodes->GetList())
    color->Enter(node, color->Look(GetAlias(node)));
}

void RegAllocator::RewriteProgram() {
  for (auto node : spilledNodes->GetList()) {
    frame::InFrameAccess *access =
        static_cast<frame::InFrameAccess *>(frame_->allocLocal(true));

#ifdef GC
    access->setIsStorePointer(node->NodeInfo()->isStorePointer);
#endif

    for (auto instr_it = assem_instr_->GetInstrList()->GetList().begin();
         instr_it != assem_instr_->GetInstrList()->GetList().end();
         instr_it++) {
      temp::TempList **src = nullptr, **dst = nullptr;
      if (typeid(*(*instr_it)) == typeid(assem::MoveInstr)) {
        src = &static_cast<assem::MoveInstr *>(*instr_it)->src_;
        dst = &static_cast<assem::MoveInstr *>(*instr_it)->dst_;
      } else if (typeid(*(*instr_it)) == typeid(assem::OperInstr)) {
        src = &static_cast<assem::OperInstr *>(*instr_it)->src_;
        dst = &static_cast<assem::OperInstr *>(*instr_it)->dst_;
      }

      if (src && std::find((*src)->GetList().begin(), (*src)->GetList().end(),
                           node->NodeInfo()) != (*src)->GetList().end()) {
        temp::Temp *new_temp = temp::TempFactory::NewTemp();

#ifdef GC
        new_temp->isStorePointer = node->NodeInfo()->isStorePointer;
#endif

        *src = replaceTempList(*src, node->NodeInfo(), new_temp);

        assem_instr_->GetInstrList()->Insert(
            instr_it,
            new assem::OperInstr(
                "movq (" + frame_->name_->Name() + "_framesize" +
                    std::to_string(access->offset) + ")(`s0),`d0",
                new temp::TempList(new_temp),
                new temp::TempList(reg_manager->StackPointer()), nullptr));
      }

      if (dst && std::find((*dst)->GetList().begin(), (*dst)->GetList().end(),
                           node->NodeInfo()) != (*dst)->GetList().end()) {
        temp::Temp *new_temp = temp::TempFactory::NewTemp();

#ifdef GC
        new_temp->isStorePointer = node->NodeInfo()->isStorePointer;
#endif

        *dst = replaceTempList(*dst, node->NodeInfo(), new_temp);

        assem_instr_->GetInstrList()->Insert(
            std::next(instr_it),
            new assem::OperInstr(
                "movq `s0,(" + frame_->name_->Name() + "_framesize" +
                    std::to_string(access->offset) + ")(`d0)",
                new temp::TempList(reg_manager->StackPointer()),
                new temp::TempList(new_temp), nullptr));
      }
    }
  }
}

temp::Map *RegAllocator::AssignRegisters() {
  temp::Map *reg_map = temp::Map::Empty();
  for (auto node : adjGraph->Nodes()->GetList())
    reg_map->Enter(node->NodeInfo(),
                   reg_manager->temp_map_->Look(color->Look(node)));
  return reg_map;
}

void RegAllocator::Simplify() {
  printf("Simplify\n");
  live::INodePtr node = simplifyWorklist->GetList().front();
  if (node) {
    simplifyWorklist->DeleteNode(node);
    selectStack->Prepend(node);
    for (auto node_succ : Adjacent(node)->GetList())
      Decrement(node);
  }
}

void RegAllocator::Coalesce() {
  printf("Coalesce\n");
  live::INodePtr x = worklistMoves->GetList().front().first;
  live::INodePtr y = worklistMoves->GetList().front().second;
  live::INodePtr u, v;
  if (color->Look(GetAlias(y))) {
    u = GetAlias(y);
    v = GetAlias(x);
  } else {
    u = GetAlias(x);
    v = GetAlias(y);
  }
  worklistMoves->Delete(x, y);

  if (u == v) {
    coalescedMoves->Append(x, y);
    AddWorkList(u);
  } else if (color->Look(v) || v->Succ()->Contain(u)) {
    constrainedMoves->Append(u, v);
    AddWorkList(u);
    AddWorkList(v);
  } else if ((color->Look(u) && AllOK(v, u)) ||
             (!color->Look(u) && Conservative(u->Succ()->Union(v->Succ())))) {
    coalescedMoves->Append(u, v);
    Combine(u, v);
    AddWorkList(u);
  } else {
    activeMoves->Append(x, y);
  }
}

void RegAllocator::Freeze() {
  printf("Freeze\n");
  live::INodePtr u = freezeWorklist->GetList().front();
  freezeWorklist->DeleteNode(u);
  simplifyWorklist->Append(u);
  FreezeMoves(u);
}

void RegAllocator::SelectSpill() {
  printf("SelectSpill\n");
  live::INodePtr m = spillWorklist->GetList().front();
  spillWorklist->DeleteNode(m);
  simplifyWorklist->Append(m);
  FreezeMoves(m);
}

void RegAllocator::AddEdge(live::INodePtr node_A, live::INodePtr node_B) {
  if (!node_A->Succ()->Contain(node_B) && node_A != node_B) {
    adjGraph->AddEdge(node_A, node_B);
    (*degree->Look(node_A))++;
    adjGraph->AddEdge(node_B, node_A);
    (*degree->Look(node_B))++;
  }
}

live::INodeListPtr RegAllocator::Adjacent(live::INodePtr node) {
  return node->Succ()->Diff(selectStack->Union(coalescedNodes));
}

live::MoveList *RegAllocator::NodeMoves(live::INodePtr node) {
  return movelist->Look(node)->Intersect(activeMoves->Union(worklistMoves));
}

bool RegAllocator::MoveRelated(live::INodePtr node) {
  return NodeMoves(node)->GetList().size() != 0;
}

void RegAllocator::Decrement(live::INodePtr node) {
  int oldDegree = (*degree->Look(node))--;
  if (oldDegree == K && !color->Look(node)) {
    live::INodeListPtr adj_and_self = Adjacent(node);
    adj_and_self->Append(node);
    EnableMoves(adj_and_self);
    spillWorklist->DeleteNode(node);
    if (MoveRelated(node))
      freezeWorklist->Append(node);
    else
      simplifyWorklist->Append(node);
  }
}

void RegAllocator::EnableMoves(live::INodeListPtr nodes) {
  for (auto node : nodes->GetList())
    for (auto move : NodeMoves(node)->GetList())
      if (activeMoves->Contain(move.first, move.second))
        activeMoves->Delete(move.first, move.second);
}

void RegAllocator::AddWorkList(live::INodePtr node) {
  if (!color->Look(node) && !MoveRelated(node) && (*degree->Look(node)) < K) {
    freezeWorklist->DeleteNode(node);
    simplifyWorklist->Append(node);
  }
}

bool RegAllocator::AllOK(live::INodePtr node_A, live::INodePtr node_B) {
  for (auto node_A_adj : node_A->Succ()->GetList())
    if (!OK(node_A_adj, node_B))
      return false;
  return true;
}

bool RegAllocator::OK(live::INodePtr node_A, live::INodePtr node_B) {
  return *degree->Look(node_A) < K || color->Look(node_A) ||
         node_A->Succ()->Contain(node_B);
}

bool RegAllocator::Conservative(live::INodeListPtr nodes) {
  int k = 0;
  for (auto node : nodes->GetList())
    if (*degree->Look(node) >= K)
      k++;
  return k < K;
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr node) {
  if (coalescedNodes->Contain(node))
    return GetAlias(alias->Look(node));
  else
    return node;
}

void RegAllocator::Combine(live::INodePtr node_A, live::INodePtr node_B) {
  if (freezeWorklist->Contain(node_B))
    freezeWorklist->DeleteNode(node_B);
  else
    spillWorklist->DeleteNode(node_B);
  coalescedNodes->Prepend(node_B);
  alias->Enter(node_B, node_A);

  movelist->Enter(node_A,
                  movelist->Look(node_A)->Union(movelist->Look(node_B)));

  for (auto node_B_adj : node_B->Succ()->GetList()) {
    AddEdge(node_B_adj, node_A);
    Decrement(node_B_adj);
  }
  if (*degree->Look(node_A) >= K && freezeWorklist->Contain(node_A)) {
    freezeWorklist->DeleteNode(node_A);
    spillWorklist->Prepend(node_A);
  }
}

void RegAllocator::FreezeMoves(live::INodePtr node) {
  for (auto m : NodeMoves(node)->GetList()) {
    live::INodePtr x = m.first;
    live::INodePtr y = m.second;
    live::INodePtr v;
    if (GetAlias(y) == GetAlias(node))
      v = GetAlias(x);
    else
      v = GetAlias(y);
    activeMoves->Delete(x, y);
    frozenMoves->Append(x, y);
    if (!MoveRelated(v) && *degree->Look(v) < K) {
      freezeWorklist->DeleteNode(v);
      simplifyWorklist->Append(v);
    }
  }
}

temp::TempList *RegAllocator::replaceTempList(temp::TempList *temp_list,
                                              temp::Temp *old_temp,
                                              temp::Temp *new_temp) {
  temp::TempList *new_temp_list = new temp::TempList();
  for (auto temp : temp_list->GetList())
    if (temp == old_temp)
      new_temp_list->Append(new_temp);
    else
      new_temp_list->Append(temp);
  return new_temp_list;
}

} // namespace ra