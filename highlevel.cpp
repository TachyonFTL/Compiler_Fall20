#include <cassert>
#include <string>
#include "cfg.h"
#include "highlevel.h"

PrintHighLevelInstructionSequence::PrintHighLevelInstructionSequence(InstructionSequence *ins)
  : PrintInstructionSequence(ins) {
}

std::string PrintHighLevelInstructionSequence::get_opcode_name(int opcode) {
  switch (opcode) {
  case HINS_NOP:         return "nop";
  case HINS_LOAD_ICONST: return "ldci";
  case HINS_INT_ADD:     return "addi";
  case HINS_INT_SUB:     return "subi";
  case HINS_INT_MUL:     return "muli";
  case HINS_INT_DIV:     return "divi";
  case HINS_INT_MOD:     return "modi";
  case HINS_INT_NEGATE:  return "negi";
  case HINS_LOCALADDR:   return "localaddr";
  case HINS_LOAD_INT:    return "ldi";
  case HINS_STORE_INT:   return "sti";
  case HINS_READ_INT:    return "readi";
  case HINS_WRITE_INT:   return "writei";
  case HINS_JUMP:        return "jmp";
  case HINS_JE:          return "je";
  case HINS_JNE:         return "jne";
  case HINS_JLT:         return "jlt";
  case HINS_JLTE:        return "jlte";
  case HINS_JGT:         return "jgt";
  case HINS_JGTE:        return "jgte";
  case HINS_INT_COMPARE: return "cmpi";
  case HINS_CONS_DEF:    return "const";
  case HINS_EMPTY:       return "";
  case HINS_MOV:         return "mov";
  default:
    assert(false);
    return "<invalid>";
  }
}

std::string PrintHighLevelInstructionSequence::get_mreg_name(int regnum) {
  // high level instructions should not use machine registers
  assert(false);
  return "<invalid>";
}

HighLevelControlFlowGraphBuilder::HighLevelControlFlowGraphBuilder(InstructionSequence *iseq)
  : ControlFlowGraphBuilder(iseq) {
}

HighLevelControlFlowGraphBuilder::~HighLevelControlFlowGraphBuilder() {
}

bool HighLevelControlFlowGraphBuilder::falls_through(Instruction *ins) {
  // only unconditional jump instructions don't fall through
  return ins->get_opcode() != HINS_JUMP;
}

HighLevelControlFlowGraphPrinter::HighLevelControlFlowGraphPrinter(ControlFlowGraph *cfg)
  : ControlFlowGraphPrinter(cfg) {
}

HighLevelControlFlowGraphPrinter::~HighLevelControlFlowGraphPrinter() {
}

void HighLevelControlFlowGraphPrinter::print_basic_block(BasicBlock *bb) {
  for (auto i = bb->cbegin(); i != bb->cend(); i++) {
      Instruction *ins = *i;
      std::string s = format_instruction(bb, ins);
      for(int i = 0; i < ins->get_num_operands(); i++) {
        s += " " + std::to_string(ins->get_operand(i).get_m_reg_to_alloc());
      }
      printf("\t%s\n", s.c_str());
  }
}

std::string HighLevelControlFlowGraphPrinter::format_instruction(BasicBlock *bb,
                                                                 Instruction *ins) {
  PrintHighLevelInstructionSequence p(bb);
  return p.format_instruction(ins);
}

int is_def(Instruction *ins){
  int m_opcode;
  m_opcode = ins->get_opcode();
  if (m_opcode ==  HINS_INT_ADD ||
      m_opcode ==  HINS_INT_SUB ||
      m_opcode ==  HINS_INT_MUL ||
      m_opcode ==  HINS_INT_DIV ||
      m_opcode ==  HINS_INT_MOD ||
      m_opcode ==  HINS_MOV ||
      m_opcode ==  HINS_STORE_INT ||
      m_opcode ==  HINS_LOAD_ICONST ||
      m_opcode ==  HINS_LOCALADDR ||
      m_opcode == HINS_READ_INT ||
      m_opcode == HINS_LOAD_INT) {
    return 1;
  } else {
    return 0;
  }

}

int is_use(Instruction *ins, int idx){
  int m_opcode;
  m_opcode = ins->get_opcode();
  if (m_opcode ==  HINS_INT_ADD ||
      m_opcode ==  HINS_INT_SUB ||
      m_opcode ==  HINS_INT_MUL ||
      m_opcode ==  HINS_INT_DIV ||
      m_opcode ==  HINS_INT_MOD ||
      
      
      m_opcode ==  HINS_LOAD_ICONST ||
      m_opcode ==  HINS_LOCALADDR ||
      m_opcode == HINS_READ_INT ||
      m_opcode == HINS_LOAD_INT) {
    if (ins->get_operand(idx).get_kind() == OPERAND_VREG_MEMREF || ins->get_operand(idx).get_kind() == OPERAND_VREG){
      if(idx == 0) {
      return 0;
      } else {
        return 1;
      }
    } else {
      return 0;
    }

    
  } else {
    if (ins->get_operand(idx).get_kind() == OPERAND_VREG_MEMREF || ins->get_operand(idx).get_kind() == OPERAND_VREG){
      return 1;
    } else {
      return 0;
    }
    
  }
}
