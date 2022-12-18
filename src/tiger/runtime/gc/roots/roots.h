#ifndef TIGER_RUNTIME_GC_ROOTS_H
#define TIGER_RUNTIME_GC_ROOTS_H

#include <iostream>
#include <vector>

#include "tiger/codegen/assem.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/liveness/liveness.h"
#include "tiger/translate/tree.h"
#include <algorithm>
#include <map>

extern frame::RegManager *reg_manager;

namespace gc {

const std::string GC_ROOTS = "GLOBAL_GC_ROOTS";

struct PointerMap {
  std::string label;
  std::string return_address_label;
  std::string next_label;
  std::string is_main = "0";
  std::string frame_size;
  std::vector<std::string> offsets;
  std::string end_label = "-1";
};

class Roots {
  // Todo(lab7): define some member and methods here to keep track of gc roots;
public:
  Roots(assem::InstrList *il, frame::Frame *frame, fg::FGraphPtr flowgraph,
        std::vector<int> escapes, temp::Map *color)
      : escapes_(escapes), flowgraph_(flowgraph), color_(color), il_(il),
        frame_(frame),
        address_in_(
            std::make_unique<graph::Table<assem::Instr, std::vector<int>>>()),
        address_out_(
            std::make_unique<graph::Table<assem::Instr, std::vector<int>>>()),
        temp_in_(
            std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        temp_out_(
            std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        valid_address_(
            std::make_unique<tab::Table<assem::Instr, std::vector<int>>>()),
        valid_temp_(std::make_unique<
                    tab::Table<assem::Instr, std::vector<std::string>>>()) {}
  ~Roots() = default;

  std::vector<PointerMap> GetPointerMaps();

  assem::InstrList *GetInstrList() { return il_; }

private:
  assem::InstrList *il_;
  frame::Frame *frame_;
  fg::FGraphPtr flowgraph_;
  std::vector<int> escapes_;
  temp::Map *color_;

  std::unique_ptr<graph::Table<assem::Instr, std::vector<int>>> address_in_;
  std::unique_ptr<graph::Table<assem::Instr, std::vector<int>>> address_out_;

  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> temp_in_;
  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> temp_out_;

  std::unique_ptr<tab::Table<assem::Instr, std::vector<int>>> valid_address_;
  std::unique_ptr<tab::Table<assem::Instr, std::vector<std::string>>>
      valid_temp_;

  void AddressLiveMap();

  void TempLiveMap();

  void ValidPointerMap();

  void RewriteProgram();

  std::vector<PointerMap> MakePointerMaps();
};

} // namespace gc

#endif // TIGER_RUNTIME_GC_ROOTS_H