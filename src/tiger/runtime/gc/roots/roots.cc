#include "roots.h"

namespace gc {
template <typename T>
std::vector<T> *Union(std::vector<T> *vector_A, std::vector<T> *vector_B) {
  std::vector<T> *res = new std::vector<T>();

  if (!vector_A && !vector_B)
    return res;

  if (!vector_A) {
    for (auto item_B : *vector_B)
      res->push_back(item_B);
    return res;
  }

  if (!vector_B) {
    for (auto item_A : *vector_A)
      res->push_back(item_A);
    return res;
  }

  for (auto item_A : *vector_A)
    res->push_back(item_A);

  for (auto item_B : *vector_B)
    if (std::find(res->begin(), res->end(), item_B) == res->end())
      res->push_back(item_B);
  return res;
}

template <typename T>
std::vector<T> *Subtract(std::vector<T> *vector_A, std::vector<T> *vector_B) {
  std::vector<T> *res = new std::vector<T>();

  if (!vector_A)
    return res;

  for (auto item_A : *vector_A)
    if (!vector_B || std::find(vector_B->begin(), vector_B->end(), item_A) ==
                         vector_B->end())
      res->push_back(item_A);

  return res;
}

template <typename T>
bool IsSame(std::vector<T> *vector_A, std::vector<T> *vector_B) {
  if (vector_A == nullptr || vector_B == nullptr)
    return vector_A == vector_B;

  for (auto item_A : *vector_A)
    if (std::find(vector_B->begin(), vector_B->end(), item_A) ==
        vector_B->end())
      return false;
  return true;
}

/**
 * IMPORTANT:
 * Because x86 is not supported copy directly from memory to memory, so there is
 * no need to divide source and destination. And we set the address in codegen
 * and regalloc.
 */

std::vector<int> Address(assem::Instr *ins) {
  if (typeid(*ins) == typeid(assem::OperInstr)) {
    assem::OperInstr *oper_ins = static_cast<assem::OperInstr *>(ins);
    if (oper_ins->assem_.find("movq") != oper_ins->assem_.npos &&
        oper_ins->assem_.find("_framesize") != oper_ins->assem_.npos)
      return std::vector<int>(1, oper_ins->offset_);
  }
  return std::vector<int>();
}

std::vector<std::string> GetCalleeSaves() {
  temp::Map *temp_map = reg_manager->temp_map_;
  temp::TempList *calleeSaves = reg_manager->CalleeSaves();
  std::vector<std::string> calleeSavesStr;
  for (auto calleeSave : calleeSaves->GetList())
    calleeSavesStr.push_back(*temp_map->Look(calleeSave));
  return calleeSavesStr;
}

std::vector<PointerMap> Roots::GetPointerMaps() {
  AddressLiveMap();
  TempLiveMap();
  ValidPointerMap();
  RewriteProgram();
  return MakePointerMaps();
}

void Roots::AddressLiveMap() {
  for (auto flow_node : flowgraph_->Nodes()->GetList()) {
    address_in_->Enter(flow_node, new std::vector<int>());
    address_out_->Enter(flow_node, new std::vector<int>());
  }

  while (true) {
    bool isAllSame = true;
    for (auto flow_node_it = flowgraph_->Nodes()->GetList().rbegin();
         flow_node_it != flowgraph_->Nodes()->GetList().rend();
         flow_node_it++) {
      std::vector<int> *pre_out = address_out_->Look(*flow_node_it);

      std::vector<int> *new_out = nullptr;
      for (auto flow_node_succ : (*flow_node_it)->Succ()->GetList())
        new_out = Union(new_out, address_in_->Look(flow_node_succ));

      std::vector<int> *pre_in = address_in_->Look(*flow_node_it);
      std::vector<int> *new_in = Union(
          new std::vector<int>(Address((*flow_node_it)->NodeInfo())),
          Subtract(new_out,
                   new std::vector<int>(Address((*flow_node_it)->NodeInfo()))));

      if (!IsSame(new_in, pre_in) || !IsSame(new_out, pre_out)) {
        isAllSame = false;
        address_in_->Enter(*flow_node_it, new_in);
        address_out_->Enter(*flow_node_it, new_out);
      }
    }
    if (isAllSame)
      break;
  }
}

void Roots::TempLiveMap() {
  for (auto flow_node : flowgraph_->Nodes()->GetList()) {
    temp_in_->Enter(flow_node, new temp::TempList());
    temp_out_->Enter(flow_node, new temp::TempList());
  }

  while (true) {
    bool isAllSame = true;
    for (auto flow_node_it = flowgraph_->Nodes()->GetList().rbegin();
         flow_node_it != flowgraph_->Nodes()->GetList().rend();
         flow_node_it++) {
      temp::TempList *pre_out = temp_out_->Look(*flow_node_it);

      temp::TempList *new_out = nullptr;
      for (auto flow_node_succ : (*flow_node_it)->Succ()->GetList())
        new_out = live::Union(new_out, temp_in_->Look(flow_node_succ));

      temp::TempList *pre_in = temp_in_->Look(*flow_node_it);
      temp::TempList *new_in = live::Union(
          (*flow_node_it)->NodeInfo()->Use(),
          live::Subtract(new_out, (*flow_node_it)->NodeInfo()->Def()));

      if (!live::IsSame(new_in, pre_in) || !live::IsSame(new_out, pre_out)) {
        isAllSame = false;
        temp_in_->Enter(*flow_node_it, new_in);
        temp_out_->Enter(*flow_node_it, new_out);
      }
    }
    if (isAllSame)
      break;
  }
}

void Roots::ValidPointerMap() {
  std::vector<std::string> calleeSaves = GetCalleeSaves();
  for (auto flow_node_it = flowgraph_->Nodes()->GetList().begin();
       flow_node_it != flowgraph_->Nodes()->GetList().end(); flow_node_it++) {
    assem::Instr *ins = (*flow_node_it)->NodeInfo();
    if (typeid(*ins) == typeid(assem::OperInstr) &&
        static_cast<assem::OperInstr *>(ins)->assem_.find("call") !=
            static_cast<assem::OperInstr *>(ins)->assem_.npos &&
        ++flow_node_it != flowgraph_->Nodes()->GetList().end()) {
      ins = (*flow_node_it)->NodeInfo();
      valid_address_->Enter(ins, address_in_->Look(*flow_node_it));
      valid_temp_->Enter(ins, new std::vector<std::string>());

      for (auto temp : temp_in_->Look(*flow_node_it)->GetList())
        if (temp->isStorePointer) {
          std::string regName = *color_->Look(temp);
          if (std::find(calleeSaves.begin(), calleeSaves.end(), regName) !=
                  calleeSaves.end() &&
              std::find(valid_temp_->Look(ins)->begin(),
                        valid_temp_->Look(ins)->end(),
                        regName) == valid_temp_->Look(ins)->end())
            valid_temp_->Look(ins)->push_back(regName);
        }
    }
  }
}

void Roots::RewriteProgram() {
  std::list<assem::Instr *> ins_list = il_->GetList();
  for (auto ins_it = ins_list.begin(); ins_it != ins_list.end(); ins_it++)
    if (typeid(**ins_it) == typeid(assem::OperInstr) &&
        static_cast<assem::OperInstr *>(*ins_it)->assem_.find("call") !=
            static_cast<assem::OperInstr *>(*ins_it)->assem_.npos) {
      auto labelnode = std::next(ins_it);
      for (std::string reg : *valid_temp_->Look(*labelnode)) {
        int offset = -frame_->size_;
        frame::Access *access = frame_->allocLocal(true);
        access->setIsStorePointer(true);
        valid_address_->Look(*labelnode)->push_back(offset);
        ins_it = ins_list.insert(
            ins_it, new assem::OperInstr(
                        "movq " + reg + ", (" + frame_->name_->Name() +
                            "_framesize" + std::to_string(offset) + ")(%rsp)",
                        nullptr, nullptr, nullptr, offset));
        ins_it++;
      }
    }

  il_ = new assem::InstrList();
  for (auto ins : ins_list)
    il_->Append(ins);
}

std::vector<PointerMap> Roots::MakePointerMaps() {
  std::vector<PointerMap> pointerMaps;
  bool is_main = (frame_->name_->Name().compare("tigermain") == 0);

  valid_address_->Dump([&](assem::Instr *instr, std::vector<int> *vec) {
    PointerMap new_map;
    new_map.label =
        "L" + static_cast<assem::LabelInstr *>(instr)->label_->Name();
    new_map.return_address_label =
        static_cast<assem::LabelInstr *>(instr)->label_->Name();
    new_map.next_label = "0";
    new_map.is_main = is_main ? "1" : "0";
    new_map.frame_size = frame_->name_->Name() + "_framesize";
    new_map.offsets = std::vector<std::string>();
    for (int offset : *vec)
      new_map.offsets.push_back(std::to_string(offset));
    pointerMaps.push_back(new_map);
  });

  for (int i = 0; i < (int)pointerMaps.size() - 1; i++)
    pointerMaps[i].next_label = pointerMaps[i + 1].label;

  return pointerMaps;
}

} // namespace gc