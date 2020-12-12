#include <cassert>
#include <iostream>
#include <ostream>
#include "cfg.h"
#include "cfg_transform.h"
#include "live_vregs.h"
#include "x86_64.h"
#include <map>


ControlFlowGraphTransform::ControlFlowGraphTransform(ControlFlowGraph *cfg)
  : m_cfg(cfg) {
}

ControlFlowGraphTransform::~ControlFlowGraphTransform() {
}

ControlFlowGraph *ControlFlowGraphTransform::get_orig_cfg() {
  return m_cfg;
}

ControlFlowGraph *ControlFlowGraphTransform::transform_cfg() {
  ControlFlowGraph *result = new ControlFlowGraph();

  // map of basic blocks of original CFG to basic blocks in transformed CFG
  std::map<BasicBlock *, BasicBlock *> block_map;

  // iterate over all basic blocks, transforming each one
  for (auto i = m_cfg->bb_begin(); i != m_cfg->bb_end(); i++) {
    BasicBlock *orig = *i;
    // transform the instructions
    InstructionSequence *result_iseq = transform_basic_block(orig);

    // create result basic block
    BasicBlock *result_bb = result->create_basic_block(orig->get_kind(), orig->get_label());
    block_map[orig] = result_bb;

    // copy instructions into result basic block
    for (auto j = result_iseq->cbegin(); j != result_iseq->cend(); j++) {
      result_bb->add_instruction((*j)->duplicate());
    }

    delete result_iseq;
  }

  // add edges to transformed CFG
  for (auto i = m_cfg->bb_begin(); i != m_cfg->bb_end(); i++) {
    BasicBlock *orig = *i;
    const ControlFlowGraph::EdgeList &outgoing_edges = m_cfg->get_outgoing_edges(orig);
    for (auto j = outgoing_edges.cbegin(); j != outgoing_edges.cend(); j++) {
      Edge *orig_edge = *j;

      BasicBlock *transformed_source = block_map[orig_edge->get_source()];
      BasicBlock *transformed_target = block_map[orig_edge->get_target()];

      result->create_edge(transformed_source, transformed_target, orig_edge->get_kind());
    }
  }

  return result;
}

HighLevelControlFlowGraphTransform::HighLevelControlFlowGraphTransform(ControlFlowGraph *cfg, LiveVregs *lvreg)
: ControlFlowGraphTransform(cfg){
  this->lvreg = lvreg;
  m_cfg = cfg;
}

HighLevelControlFlowGraphTransform::~HighLevelControlFlowGraphTransform(){

}

// alloc mreg to vreg
InstructionSequence *HighLevelControlFlowGraphTransform::transform_basic_block(BasicBlock *bb){
  //std::cout << "enter basic block transform" << std::endl;
  auto it = bb->begin();

  Instruction* ins;
  InstructionSequence *new_iseq = new InstructionSequence();
  int num_operand;
  Operand reg_0, reg_1;

  while(it != bb->end()){
    ins = *it;
    num_operand = ins->get_num_operands();
    
    Instruction* new_ins = ins->duplicate();

    for (int i = 0; i < num_operand; i++ ){
      
        Operand *op = &(*new_ins)[i];

        // check if op is a vreg
        if (op->get_kind() == OPERAND_VREG || op->get_kind() == OPERAND_VREG_MEMREF || op->get_kind() == OPERAND_VREG_MEMREF_OFFSET) {

            // vreg not alloc a mreg
            if (vreg_mreg.find(op->get_base_reg()) == vreg_mreg.end()){
              if (mreg_alloc < mreg_aval) {
                op->set_m_reg_to_alloc(free_mreg.back());

                vreg_mreg.insert({op->get_base_reg(), free_mreg.back()});
                free_mreg.pop_back();
                mreg_alloc++;

                max_mreg_use = (max_mreg_use>mreg_alloc)?max_mreg_use:mreg_alloc;

                // already alloc
              } else {
                // op with the same id, alloc a same mreg
                if (visited.find(op->get_base_reg()) == visited.end()) {
                  visited.insert(op->get_base_reg());
                  vreg_count++;
                }
              }
            } else {
              op->set_m_reg_to_alloc(vreg_mreg.find(op->get_base_reg())->second);
            }
        }
      } 
    new_iseq->add_instruction(new_ins);
    
    // check if any mreg can be reused
    reset_mreg_after_ins(bb, ins);
    it++;
  }

  return new_iseq;
}

// check dead vreg 
void HighLevelControlFlowGraphTransform::reset_mreg_after_ins(BasicBlock *bb, Instruction *ins){
  LiveVregs::LiveSet live_set = lvreg->get_fact_after_instruction(bb, ins);
  LiveVregs::LiveSet live_set_global;
  std::set<int> live_vreg;
  std::set<int> live_vreg_global;

  // live vreg after a ins
  for (unsigned i = 0; i < LiveVregs::MAX_VREGS; i++) {
    if (live_set.test(i)) {
      live_vreg.insert(i);
    }
  }

  std::map<int, int>::iterator it = vreg_mreg.begin();

  // iterate all alive vreg
  while (it != vreg_mreg.end()) {

    if (live_vreg.count(it->first) == 0) { 
      int flag, start;
      // check if another basic block needs the vreg
      for (auto i = m_cfg->bb_begin(); i != m_cfg->bb_end(); i++) {
        if (*i == bb) {
          start = 1; // mark current bb
        } else if (start == 1) {
          live_set_global = lvreg->get_fact_at_beginning_of_block(*i);
          if (live_set_global.test(it->first)){
            flag = 1;
          }
        }
      }
      // vreg is dead, realloc its mreg
      if (flag != 1){
        free_mreg.push_back(it->second);
        vreg_mreg.erase(it);
        mreg_alloc--;
      }

    }
    it ++;
  }
}