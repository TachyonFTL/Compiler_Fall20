#include <cassert>
#include <iostream>
#include <ostream>
#include "cfg.h"
#include "cfg_transform.h"
#include "live_vregs.h"
#include "x86_64.h"
#include <map>
#include <set>

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
  int xix = 0;
  for (auto i = m_cfg->bb_begin(); i != m_cfg->bb_end(); i++) {
    BasicBlock *orig = *i;
    // std::cout << xix << std::endl;
    xix ++;
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
}

HighLevelControlFlowGraphTransform::~HighLevelControlFlowGraphTransform(){

}

InstructionSequence *HighLevelControlFlowGraphTransform::transform_basic_block(BasicBlock *bb){
  std::cout << "enter basic block transform" << std::endl;
  auto it = bb->begin();

  Instruction* ins;
  InstructionSequence *new_iseq = new InstructionSequence();
  int num_operand;
  Operand reg_0, reg_1;

  std::vector<int> deleted;

  while(it != bb->end()){
    ins = *it;
    num_operand = ins->get_num_operands();
    
    Instruction* new_ins = ins->duplicate();

    for (int i = 0; i < num_operand; i++ ){
      if (mreg_alloc < mreg_aval) {
        Operand *op = &(*new_ins)[i];
        if (op->get_kind() == OPERAND_VREG || op->get_kind() == OPERAND_VREG_MEMREF_OFFSET) {
          if (vreg_mreg.find(op->get_base_reg()) == vreg_mreg.end()){
            op->set_m_reg_to_alloc(free_mreg.back());
            std::cout << "vreg: " << op->get_base_reg() << "mreg: " << free_mreg.back() << "op_reg: " << op->get_m_reg_to_alloc() << std::endl;
            
            vreg_mreg.insert({op->get_base_reg(), free_mreg.back()});
            free_mreg.pop_back();
            mreg_alloc++;
          } else {
            op->set_m_reg_to_alloc(vreg_mreg.find(op->get_base_reg())->second);
          }
        }
      } else {
        break;
      }
    }
    new_iseq->add_instruction(new_ins);
    reset_mreg_after_ins(bb, ins);
    it++;
  }

  return new_iseq;
}



void HighLevelControlFlowGraphTransform::peephole(Operand m_reg, Operand v_reg, InstructionSequence::iterator it, 
                                               InstructionSequence::iterator end, std::vector<int> *deleted)
{
  Instruction* ins;
  int m_opcode;
  Operand reg_0, reg_1;

  while (it != end){
    ins = *it;
    m_opcode = ins->get_opcode();

    if (m_opcode == MINS_MOVQ){ 
      reg_0 = ins->get_operand(0);  
      reg_1 = ins->get_operand(1);

      if (reg_1.get_kind() == OPERAND_MREG && reg_1.get_int_value() == m_reg.get_int_value()){
        break;
      } else if(reg_1.get_kind() == OPERAND_MREG_MEMREF_OFFSET && reg_1.get_base_reg() == v_reg.get_base_reg() 
                && reg_1.get_offset() == v_reg.get_offset()){
        break;
      }

      // a.insert(a.end(), b.begin(), b.end());
      

    } else if (m_opcode == MINS_ADDQ || m_opcode == MINS_IMULQ){
      reg_0 = ins->get_operand(0);  
      reg_1 = ins->get_operand(1);

      if (reg_1.get_kind() == OPERAND_MREG && reg_1.get_int_value() == m_reg.get_int_value()){
        break;
      } 
      
    } else if (m_opcode == MINS_SUBQ) {
      reg_0 = ins->get_operand(0);  
      reg_1 = ins->get_operand(1);

      if (reg_0.get_kind() == OPERAND_MREG && reg_0.get_int_value() == m_reg.get_int_value()){
        break;
      } else if(reg_0.get_kind() == OPERAND_MREG_MEMREF_OFFSET && reg_0.get_base_reg() == v_reg.get_base_reg() 
                && reg_0.get_offset() == v_reg.get_offset()){
        break;
      }
    }
    it++;
  }
}

void HighLevelControlFlowGraphTransform::reset_mreg_after_ins(BasicBlock *bb, Instruction *ins){
  LiveVregs::LiveSet live_set = lvreg->get_fact_after_instruction(bb, ins);
  std::set<int> live_vreg;

  for (unsigned i = 0; i < LiveVregs::MAX_VREGS; i++) {
    if (live_set.test(i)) {
      live_vreg.insert(i);
    }
  }

  std::map<int, int>::iterator it = vreg_mreg.begin();
  while (it != vreg_mreg.end()) {
    if (live_vreg.count(it->first) == 0) { 
        free_mreg.push_back(it->second);
        vreg_mreg.erase(it);
        mreg_alloc--;
    }
    it ++;
  }
}