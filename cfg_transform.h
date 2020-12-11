#ifndef CFG_TRANSFORM_H
#define CFG_TRANSFORM_H

#include "cfg.h"
#include <map>
#include <vector>
#include "live_vregs.h"
#include <set>

class ControlFlowGraph;

class ControlFlowGraphTransform {
private:
  ControlFlowGraph *m_cfg;

public:
  ControlFlowGraphTransform(ControlFlowGraph *cfg);
  virtual ~ControlFlowGraphTransform();

  ControlFlowGraph *get_orig_cfg();
  ControlFlowGraph *transform_cfg();

  virtual InstructionSequence *transform_basic_block(BasicBlock *bb) = 0;
};

class HighLevelControlFlowGraphTransform:public ControlFlowGraphTransform {
private:
  ControlFlowGraph *m_cfg;
  LiveVregs *lvreg;
  int mreg_aval = 8;
  int mreg_alloc = 0;

  int max_mreg_use = 0;

  int vreg_count = 0;
  std::vector<int> free_mreg = {0, 1, 2, 3, 4, 5, 6, 7};
  std::map<int, int> vreg_mreg;
  std::set<int> visited;

public:
  HighLevelControlFlowGraphTransform(ControlFlowGraph *cfg, LiveVregs *lvreg);
  virtual ~HighLevelControlFlowGraphTransform();

  virtual InstructionSequence *transform_basic_block(BasicBlock *bb);
  void reset_mreg_after_ins(BasicBlock *bb, Instruction *ins);

  void peephole(Operand m_reg, Operand v_reg, InstructionSequence::iterator it, InstructionSequence::iterator end, std::vector<int> *deleted);

  int get_vreg_count() {
    return vreg_count;
  }

  int get_max_mreg_use() {
    return max_mreg_use;
  }
};

#endif // CFG_TRANSFORM_H
