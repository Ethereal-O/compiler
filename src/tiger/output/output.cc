#include "tiger/output/output.h"

#include <cstdio>

#include "tiger/output/logger.h"

#define GC

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;
extern std::vector<gc::PointerMap> global_roots;

namespace output {
std::string GetOutputPointerMap(gc::PointerMap global_root) {
  std::string all_str = "";
  all_str += global_root.label + ":\n";
  all_str += ".quad " + global_root.return_address_label + "\n";
  all_str += ".quad " + global_root.next_label + "\n";
  all_str += ".quad " + global_root.is_main + "\n";
  all_str += ".quad " + global_root.frame_size + "\n";
  for (std::string offset : global_root.offsets)
    all_str += ".quad " + offset + "\n";
  all_str += ".quad " + global_root.end_label + "\n";
  return all_str;
}

std::vector<int> GetEscapePointers(frame::Frame *frame_) {
  std::vector<int> escape_pointers;
  for (auto access : frame_->formals_)
    if (typeid(*access) == typeid(frame::InFrameAccess) &&
        static_cast<frame::InFrameAccess *>(access)->isStorePointer)
      escape_pointers.push_back(
          static_cast<frame::InFrameAccess *>(access)->offset);
  return escape_pointers;
}

assem::InstrList *GenPointerMap(assem::InstrList *il, frame::Frame *frame_,
                                std::vector<int> escape_pointers,
                                temp::Map *color) {
  fg::FlowGraphFactory flow_graph_factory = fg::FlowGraphFactory(il);
  flow_graph_factory.AssemFlowGraph();
  gc::Roots roots = gc::Roots(il, frame_, flow_graph_factory.GetFlowGraph(),
                              escape_pointers, color);
  std::vector<gc::PointerMap> new_global_roots = roots.GetPointerMaps();
  il = roots.GetInstrList();

  if (global_roots.size() && new_global_roots.size())
    global_roots.back().next_label = new_global_roots.front().label;
  global_roots.insert(global_roots.end(), new_global_roots.begin(),
                      new_global_roots.end());
  return il;
}

void OutputPointerMap(FILE *out_) {
  if (global_roots.size())
    global_roots.back().next_label = "0";
  fprintf(out_, (".global " + gc::GC_ROOTS + "\n").c_str());
  fprintf(out_, ".data\n");
  fprintf(out_, (gc::GC_ROOTS + ":\n").c_str());
  for (auto global_root : global_roots)
    fprintf(out_, "%s", GetOutputPointerMap(global_root).c_str());
}

void AssemGen::GenAssem(bool need_ra) {
  frame::Frag::OutputPhase phase;

  // Output proc
  phase = frame::Frag::Proc;
  fprintf(out_, ".text\n");
  for (auto &&frag : frags->GetList())
    frag->OutputAssem(out_, phase, need_ra);

  // Output string
  phase = frame::Frag::String;
  fprintf(out_, ".section .rodata\n");
  for (auto &&frag : frags->GetList())
    frag->OutputAssem(out_, phase, need_ra);

#ifdef GC
  // output pointerMap
  OutputPointerMap(out_);
#endif
}

} // namespace output

namespace frame {

void ProcFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  std::unique_ptr<canon::Traces> traces;
  std::unique_ptr<cg::AssemInstr> assem_instr;
  std::unique_ptr<ra::Result> allocation;

  // When generating proc fragment, do not output string assembly
  if (phase != Proc)
    return;

  TigerLog("-------====IR tree=====-----\n");
  TigerLog(body_);

  {
    // Canonicalize
    TigerLog("-------====Canonicalize=====-----\n");
    canon::Canon canon(body_);

    // Linearize to generate canonical trees
    TigerLog("-------====Linearlize=====-----\n");
    tree::StmList *stm_linearized = canon.Linearize();
    TigerLog(stm_linearized);

    // Group list into basic blocks
    TigerLog("------====Basic block_=====-------\n");
    canon::StmListList *stm_lists = canon.BasicBlocks();
    TigerLog(stm_lists);

    // Order basic blocks into traces_
    TigerLog("-------====Trace=====-----\n");
    tree::StmList *stm_traces = canon.TraceSchedule();
    TigerLog(stm_traces);

    traces = canon.TransferTraces();
  }

  temp::Map *color =
      temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  {
    // Lab 5: code generation
    TigerLog("-------====Code generate=====-----\n");
    cg::CodeGen code_gen(frame_, std::move(traces));
    code_gen.Codegen();
    assem_instr = code_gen.TransferAssemInstr();
    TigerLog(assem_instr.get(), color);
  }

  assem::InstrList *il = assem_instr.get()->GetInstrList();
  std::vector<int> escape_pointers = output::GetEscapePointers(frame_);

  if (need_ra) {
    // Lab 6: register allocation
    TigerLog("----====Register allocate====-----\n");
    ra::RegAllocator reg_allocator(frame_, std::move(assem_instr));
    reg_allocator.RegAlloc();
    allocation = reg_allocator.TransferResult();
    il = allocation->il_;
    color = temp::Map::LayerMap(reg_manager->temp_map_, allocation->coloring_);
  }

#ifdef GC
  il = output::GenPointerMap(il, frame_, escape_pointers, color);
#endif

  TigerLog("-------====Output assembly for %s=====-----\n",
           frame_->name_->Name().data());

  assem::Proc *proc = frame::ProcEntryExit3(frame_, il);

  std::string proc_name = frame_->GetLabel();

  fprintf(out, ".globl %s\n", proc_name.data());
  fprintf(out, ".type %s, @function\n", proc_name.data());
  // prologue
  fprintf(out, "%s", proc->prolog_.data());
  // body
  proc->body_->Print(out, color);
  // epilog_
  fprintf(out, "%s", proc->epilog_.data());
  fprintf(out, ".size %s, .-%s\n", proc_name.data(), proc_name.data());
}

void StringFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  // When generating string fragment, do not output proc assembly
  if (phase != String)
    return;

  fprintf(out, "%s:\n", label_->Name().data());
  int length = static_cast<int>(str_.size());
  // It may contain zeros in the middle of string. To keep this work, we need
  // to print all the charactors instead of using fprintf(str)
  fprintf(out, ".long %d\n", length);
  fprintf(out, ".string \"");
  for (int i = 0; i < length; i++) {
    if (str_[i] == '\n') {
      fprintf(out, "\\n");
    } else if (str_[i] == '\t') {
      fprintf(out, "\\t");
    } else if (str_[i] == '\"') {
      fprintf(out, "\\\"");
    } else {
      fprintf(out, "%c", str_[i]);
    }
  }
  fprintf(out, "\"\n");
}
} // namespace frame
