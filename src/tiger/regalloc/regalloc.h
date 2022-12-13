#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {
using MoveListTab = tab::Table<live::INode, live::MoveList>;
using DegreeTab = tab::Table<live::INode, int>;
using ColorTab = tab::Table<live::INode, temp::Temp>;
using AliasTab = tab::Table<live::INode, live::INode>;
using MoveListTabPtr = MoveListTab *;
using DegreeTabPtr = DegreeTab *;
using ColorTabPtr = ColorTab *;
using AliasTabPtr = AliasTab *;

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result(){};
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr)
      : frame_(frame), assem_instr_(std::move(assem_instr)) {}

  void RegAlloc();
  std::unique_ptr<Result> TransferResult() { return std::move(allocation_); }

private:
  int K;
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  std::unique_ptr<Result> allocation_;
  // additional vars
  live::INodeListPtr simplifyWorklist;
  live::INodeListPtr freezeWorklist;
  live::INodeListPtr spillWorklist;
  live::INodeListPtr spilledNodes;
  live::MoveList *worklistMoves;
  // additional vars for inner functions
  MoveListTabPtr movelist;
  DegreeTabPtr degree;
  ColorTabPtr color;
  AliasTabPtr alias;
  live::IGraphPtr adjGraph;
  live::INodeListPtr selectStack;
  live::INodeListPtr coalescedNodes;
  live::INodeListPtr coloredNodes;
  live::MoveList *frozenMoves;
  live::MoveList *activeMoves;
  live::MoveList *coalescedMoves;
  live::MoveList *constrainedMoves;
  // additional vars for temporaily store data
  live::LiveGraph live_graph_;
  // additional functions
  void LivenessAnalysis();
  void Build();
  void MakeWorklist();
  void AssignColor();
  void RewriteProgram();
  temp::Map *AssignRegisters();
  // additional functions for loop
  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
  // additional functions for inner functions
  void AddEdge(live::INodePtr node_A, live::INodePtr node_B);
  live::INodeListPtr Adjacent(live::INodePtr node);
  live::MoveList *NodeMoves(live::INodePtr node);
  bool MoveRelated(live::INodePtr node);
  void Decrement(live::INodePtr node);
  void EnableMoves(live::INodeListPtr nodes);
  void AddWorkList(live::INodePtr node);
  bool AllOK(live::INodePtr node_A, live::INodePtr node_B);
  bool OK(live::INodePtr node_A, live::INodePtr node_B);
  bool Conservative(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr node);
  void Combine(live::INodePtr node_A, live::INodePtr node_B);
  void FreezeMoves(live::INodePtr node);
  temp::TempList *replaceTempList(temp::TempList *temp_list,
                                  temp::Temp *old_temp, temp::Temp *new_temp);
};

} // namespace ra

#endif