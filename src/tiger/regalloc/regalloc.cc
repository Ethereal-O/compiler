#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

#include <algorithm>

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
void RegAllocator::RegAlloc() {
  allocation_ = std::make_unique<Result>();

  while (true) {
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
  frozenMoves = new live::MoveList();
  activeMoves = new live::MoveList();

  K = reg_manager->RegistersExceptRsp()->GetList().size();

  for (auto node : adjGraph->Nodes()->GetList()) {
    degree->Enter(node, new int(node->OutDegree()));
    if (std::find(reg_manager->RegistersExceptRsp()->GetList().begin(),
                  reg_manager->RegistersExceptRsp()->GetList().end(),
                  node->NodeInfo()) !=
        reg_manager->RegistersExceptRsp()->GetList().end())
      color->Enter(node, node->NodeInfo());
    alias->Enter(node, node);
    auto moveList = new live::MoveList();
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

void RegAllocator::AssignColor() {}

void RegAllocator::RewriteProgram() {}

void RegAllocator::Simplify() {
  live::INodePtr node = simplifyWorklist->GetList().back();
  if (node) {
    simplifyWorklist->DeleteNode(node);
    selectStack->Prepend(node);
    for (auto node_succ : Adjacent(node)->GetList())
      Decrement(node);
  }
}

void RegAllocator::Coalesce() {}

void RegAllocator::Freeze() {}

void RegAllocator::SelectSpill() {}

void RegAllocator::AddEdge(live::INodePtr node_A, live::INodePtr node_B) {
  if (!node_A->Succ()->Contain(node_B) && node_A != node_B) {
    if (!color->Look(node_A)) {
      adjGraph->AddEdge(node_A, node_B);
      (*degree->Look(node_A))++;
    }
    if (!color->Look(node_B)) {
      adjGraph->AddEdge(node_B, node_A);
      (*degree->Look(node_B))++;
    }
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
  for (auto node : nodes->GetList()) {
    for (auto move : NodeMoves(node)->GetList()) {
      if (activeMoves->Contain(move.first, move.second)) {
        activeMoves->Delete(move.first, move.second);
        worklistMoves->Delete(move.first, move.second);
      }
    }
  }
}

void RegAllocator::AddWorkList(live::INodePtr node) {}

bool RegAllocator::OK(live::INodePtr node_A, live::INodePtr node_B) {}

bool RegAllocator::Conservative(live::INodeListPtr nodes) {}

live::INodePtr RegAllocator::GetAlias(live::INodePtr node) {}

void RegAllocator::Combine(live::INodePtr node_A, live::INodePtr node_B) {}

void RegAllocator::FreezeMoves(live::INodePtr node) {}

void RegAllocator::SelectSpill() {}

} // namespace ra