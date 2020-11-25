#include "lowlevelgen.h"
#include "cfg.h"
#include "highlevel.h"
#include "x86_64.h"
#include "codegen.h"
#include <iostream>
#include <iterator>
#include <map>
#include <string>


//////////////////////////////////////////////////////////
// main class to translate high-level code to x86_64 code
class InstructionVisitor{
  private:
    struct InstructionSequence *high_level;
    struct InstructionSequence *low_level = new InstructionSequence();
    // record num of var and vreg used
    int _var_offset;
    int _vreg_count;

    // store stack pointer
    int rsp_offset;

  public:
    InstructionVisitor(InstructionSequence *iseq, int var_offset, int vreg_count);
    ~InstructionVisitor() = default;
    
    void translate();
    
    void translate_localaddr(Instruction *ins);
    void translate_readint(Instruction *ins);
    void translate_writeint(Instruction *ins);
    void translate_storeint(Instruction *ins);
    void translate_loadint(Instruction *ins);
    void translate_loadcosntint(Instruction *ins);
    
    void translate_cmp(Instruction *ins);

    void translate_jump(Instruction *ins);
    void translate_jlte(Instruction *ins);
    void translate_jgte(Instruction *ins);
    void translate_jlt(Instruction *ins);
    void translate_jgt(Instruction *ins);
    void translate_jeq(Instruction *ins);
    void translate_jne(Instruction *ins);

    void translate_div(Instruction *ins);
    void translate_add(Instruction *ins);
    void translate_sub(Instruction *ins);
    void translate_mul(Instruction *ins);
    void translate_mod(Instruction *ins);


    struct InstructionSequence *get_lowlevel();
    std::string translate_const_def(Instruction *ins);
    
  private:
    typedef void (InstructionVisitor::*func_ptr)(Instruction*);
    // function map based on high-level operator
    std::map<int, func_ptr> translate_high_to_low = {{HINS_LOCALADDR, &InstructionVisitor::translate_localaddr}, 
                                                     {HINS_READ_INT, &InstructionVisitor::translate_readint},
                                                     {HINS_WRITE_INT, &InstructionVisitor::translate_writeint},
                                                     {HINS_STORE_INT, &InstructionVisitor::translate_storeint},
                                                     {HINS_LOAD_INT, &InstructionVisitor::translate_loadint},
                                                     {HINS_LOAD_ICONST, &InstructionVisitor::translate_loadcosntint},
                                                     {HINS_INT_DIV, &InstructionVisitor::translate_div},
                                                     {HINS_INT_ADD, &InstructionVisitor::translate_add},
                                                     {HINS_INT_SUB, &InstructionVisitor::translate_sub},
                                                     {HINS_INT_MUL, &InstructionVisitor::translate_mul},
                                                     {HINS_INT_MOD, &InstructionVisitor::translate_mod},
                                                     {HINS_INT_COMPARE, &InstructionVisitor::translate_cmp},
                                                     {HINS_JLTE, &InstructionVisitor::translate_jlte},
                                                     {HINS_JGTE, &InstructionVisitor::translate_jgte},
                                                     {HINS_JLT, &InstructionVisitor::translate_jlt},
                                                     {HINS_JGT, &InstructionVisitor::translate_jgt},
                                                     {HINS_JE, &InstructionVisitor::translate_jeq},
                                                     {HINS_JNE, &InstructionVisitor::translate_jne},
                                                     {HINS_JUMP, &InstructionVisitor::translate_jump},
                                                     };
    
    // get the real memory reference of a vreg
    struct Operand vreg_ref(Operand vreg, int bias=0);

    struct Operand rsp = Operand(OPERAND_MREG, MREG_RSP);
    struct Operand rdi = Operand(OPERAND_MREG, MREG_RDI);
    struct Operand rsi = Operand(OPERAND_MREG, MREG_RSI);
    struct Operand r10 = Operand(OPERAND_MREG, MREG_R10);
    struct Operand r11 = Operand(OPERAND_MREG, MREG_R11);
    struct Operand rax = Operand(OPERAND_MREG, MREG_RAX);
    struct Operand rbx = Operand(OPERAND_MREG, MREG_RBX);

    struct Operand rdx = Operand(OPERAND_MREG, MREG_RDX);
    struct Operand eax = Operand(OPERAND_MREG, MREG_EAX);

    struct Operand inputfmt = Operand("s_readint_fmt", true);
    struct Operand outputfmt = Operand("s_writeint_fmt", true);

    struct Operand write = Operand("printf");
    struct Operand read  = Operand("scanf");
    
    // bytes used in the stack memory
    struct Operand stack_size;

    // helper Operand for subtract one byte from rsp
    struct Operand one_byte = Operand(OPERAND_INT_LITERAL, 8);

    Instruction *cqto = new Instruction(MINS_CQTO);
};


InstructionVisitor::InstructionVisitor(InstructionSequence *iseq, int var_offset, int vreg_count){
  high_level = iseq;
  _var_offset = var_offset;
  _vreg_count = vreg_count;
  rsp_offset =  _var_offset + 8 * vreg_count;
  stack_size = Operand(OPERAND_INT_LITERAL, rsp_offset);
}

// main translation function
void InstructionVisitor::translate(){
  // strating const declaretions
  std::string label = "\t.section .rodata\ns_readint_fmt:"
                          " .string \"%ld\"\ns_writeint_fmt: .string \"%ld\\n\"\n";
  
  // instruction to output an empty line
  struct Instruction *ins = new Instruction(MINS_EPTY);
  // iterate all high-level instructions
  std::vector<Instruction *>::iterator it;
  int op_code;
  
  // record all constants to .rodata
  for(it = high_level->begin(); it != high_level->end(); ++it){
    op_code = (*it)->get_opcode();
    if (op_code != HINS_CONS_DEF) {
      break;
    }
    label += translate_const_def(*it);
  }
  // write headers
  label += "\t.section .text\n\t.globl main\nmain";
  low_level->define_label(label);
  
  // allocate spaces on stack
  Instruction *pushq = new Instruction(MINS_SUBQ, stack_size, rsp);
  low_level->add_instruction(pushq);
  
  // iterate high-level instructions
  for(; it != high_level->end(); ++it){
    op_code = (*it)->get_opcode();
    
    // map high-level instructions to correspoding translation functions
    std::map<int, func_ptr>::iterator iter;

    // if a label is encountered, write the label to low-level code
    if (high_level->has_label(it - high_level->begin())) {
      low_level->define_label(high_level->get_label(it - high_level->begin()));
      // write an empty line
      low_level->add_instruction(ins);
    }

    // run correspoding translation function
    iter = translate_high_to_low.find(op_code);
    if(iter != translate_high_to_low.end()){
      (this->*(iter->second))(*it);
    } 
  }
  
  // free stack allocation and return 0
  Instruction *popq = new Instruction(MINS_ADDQ, stack_size, rsp);
  Instruction *ret0 = new Instruction(MINS_MOVL, Operand(OPERAND_INT_LITERAL, 0), eax);
  Instruction *ret = new Instruction(MINS_RET);
  low_level->add_instruction(popq);
  low_level->add_instruction(ret0);
  low_level->add_instruction(ret);
}

// translate a const definition to rodata
std::string InstructionVisitor::translate_const_def(Instruction *ins){
  std::string name = ins->get_operand(0).get_target_label();
  std::string val = std::to_string(ins->get_operand(1).get_int_value());
  return name + ": .quad " + val + "\n";
}

// translate the localaddr instruction
void InstructionVisitor::translate_localaddr(Instruction *ins){
  Operand address = Operand(OPERAND_MREG_MEMREF_OFFSET, MREG_RSP, ins->get_operand(1).get_int_value());
  Instruction *load = new Instruction(MINS_LEAQ, address, r10);
  Instruction *move = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
  low_level->add_instruction(load);
  low_level->add_instruction(move);
}

// translate the readint instruction
void InstructionVisitor::translate_readint(Instruction *ins){
  // push rsp by 8 in case rep offset is not a multiple of 16
  int stack_push = 0;
  if(rsp_offset % 16 != 8){
    stack_push = 1;
    Instruction *pushq = new Instruction(MINS_SUBQ, one_byte, rsp);
    low_level->add_instruction(pushq);
    rsp_offset += 8;
  }

  Instruction *move = new Instruction(MINS_MOVQ, inputfmt, rdi);
  Instruction *load = new Instruction(MINS_LEAQ, vreg_ref(ins->get_operand(0), 8 * stack_push), rsi);
  Instruction *call = new Instruction(MINS_CALL, read);
  low_level->add_instruction(move);
  low_level->add_instruction(load);
  low_level->add_instruction(call);
  
  // pop rsp by 8 in case rep offset is not a multiple of 16
  if(stack_push == 1){
    Instruction *popq = new Instruction(MINS_ADDQ, one_byte, rsp);
    low_level->add_instruction(popq);
    rsp_offset -= 8;
  }
}

// translate the writeint instruction
void InstructionVisitor::translate_writeint(Instruction *ins){
  // push rsp by 8 in case rep offset is not a multiple of 16
  int stack_push = 0;
  if(rsp_offset % 16 != 8){
    stack_push = 1;
    Instruction *pushq = new Instruction(MINS_SUBQ, one_byte, rsp);
    low_level->add_instruction(pushq);
    rsp_offset += 8;
  }

  Instruction *move = new Instruction(MINS_MOVQ, outputfmt, rdi);
  Instruction *load = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0), 8 * stack_push), rsi);
  Instruction *call = new Instruction(MINS_CALL, write);
  low_level->add_instruction(move);
  low_level->add_instruction(load);
  low_level->add_instruction(call);
  
  // pop rsp by 8 in case rep offset is not a multiple of 16
  if(stack_push == 1){
    Instruction *popq = new Instruction(MINS_ADDQ, one_byte, rsp);
    low_level->add_instruction(popq);
    rsp_offset -= 8;
  }
}

// translate the storeint instruction
void InstructionVisitor::translate_storeint(Instruction *ins){
  Instruction *move_int = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_var = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0)), r10);

  Operand dest = Operand(OPERAND_MREG_MEMREF, r10.get_base_reg());
  
  Instruction *move_finl = new Instruction(MINS_MOVQ, r11, dest);
  
  low_level->add_instruction(move_int);
  low_level->add_instruction(move_var);
  low_level->add_instruction(move_finl);
}

// translate the loadint instruction
void InstructionVisitor::translate_loadint(Instruction *ins){
  Instruction *move_var = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);

  Operand memr_ref = Operand(OPERAND_MREG_MEMREF, r11.get_base_reg());
  
  Instruction *load_int = new Instruction(MINS_MOVQ, memr_ref, r11);
  Instruction *store_int = new Instruction(MINS_MOVQ, r11, vreg_ref(ins->get_operand(0)));
  
  low_level->add_instruction(move_var);
  low_level->add_instruction(load_int);
  low_level->add_instruction(store_int);
}

// translate the loadconstint instruction
void InstructionVisitor::translate_loadcosntint(Instruction *ins){
  Instruction *move_const_int = new Instruction(MINS_MOVQ, ins->get_operand(1), vreg_ref(ins->get_operand(0)));
  low_level->add_instruction(move_const_int);
}

// translate the divide instruction
void InstructionVisitor::translate_div(Instruction *ins){
  Instruction *move_dividend = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), rax);
  Instruction *move_divisor = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  Instruction *div = new Instruction(MINS_IDIVQ, r10);
  Instruction *move_result = new Instruction(MINS_MOVQ, rax, vreg_ref(ins->get_operand(0)));
  
  low_level->add_instruction(move_dividend);
  low_level->add_instruction(cqto);
  low_level->add_instruction(move_divisor);
  low_level->add_instruction(div);
  low_level->add_instruction(move_result);
}

// translate the mod instruction
void InstructionVisitor::translate_mod(Instruction *ins){
  Instruction *move_dividend = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), rax);
  Instruction *move_divisor = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  Instruction *div = new Instruction(MINS_IDIVQ, r10);
  Instruction *move_result = new Instruction(MINS_MOVQ, rdx, vreg_ref(ins->get_operand(0)));
  
  low_level->add_instruction(move_dividend);
  low_level->add_instruction(cqto);
  low_level->add_instruction(move_divisor);
  low_level->add_instruction(div);
  low_level->add_instruction(move_result);
}

// translate the add instruction
void InstructionVisitor::translate_add(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_second;

  // resolve memory reference
  if(ins->get_operand(2).has_base_reg()){
    move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  } else {
    move_second = new Instruction(MINS_MOVQ, ins->get_operand(2), r10);
  }

  Instruction *add = new Instruction(MINS_ADDQ, r11, r10);

  Instruction *move_result = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
  
  low_level->add_instruction(move_first);
  low_level->add_instruction(move_second);
  low_level->add_instruction(add);
  low_level->add_instruction(move_result);
}

// translate the sub instruction
void InstructionVisitor::translate_sub(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_second;

  // resolve memory reference
  if(ins->get_operand(2).has_base_reg()){
    move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  } else {
    move_second = new Instruction(MINS_MOVQ, ins->get_operand(2), r10);
  }
  
  Instruction *add = new Instruction(MINS_SUBQ, r10, r11);
  Instruction *move_result = new Instruction(MINS_MOVQ, r11, vreg_ref(ins->get_operand(0)));
  
  low_level->add_instruction(move_first);
  low_level->add_instruction(move_second);
  low_level->add_instruction(add);
  low_level->add_instruction(move_result);
}

// translate the mul instruction
void InstructionVisitor::translate_mul(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_second;

  // resolve memory reference
  if(ins->get_operand(2).has_base_reg()){
    move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  } else {
    move_second = new Instruction(MINS_MOVQ, ins->get_operand(2), r10);
  }

  Operand reg_0 = r11;
  Operand reg_1 = r10;

  // resolve memory reference
  if(ins->get_operand(1).is_memref()) {
    reg_0 = reg_0.to_memref();
  }
  if(ins->get_operand(2).is_memref()) {
    reg_1 = reg_1.to_memref();
  }

  Instruction *add = new Instruction(MINS_IMULQ, reg_0, reg_1);
  Instruction *move_result = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
  
  low_level->add_instruction(move_first);
  low_level->add_instruction(move_second);
  low_level->add_instruction(add);
  low_level->add_instruction(move_result);
}

// translate the cmp instruction
void InstructionVisitor::translate_cmp(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0)), r10);
  Instruction *move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *cmp = new Instruction(MINS_CMPQ, r11, r10);
  low_level->add_instruction(move_first);
  low_level->add_instruction(move_second);
  low_level->add_instruction(cmp);
}

// translate the jump instruction
void InstructionVisitor::translate_jump(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JMP, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// translate the jlte instruction
void InstructionVisitor::translate_jlte(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JLE, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// translate the jgte instruction
void InstructionVisitor::translate_jgte(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JGE, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// translate the jlt instruction
void InstructionVisitor::translate_jlt(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JL, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// translate the jgt instruction
void InstructionVisitor::translate_jgt(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JG, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// translate the jeq instruction
void InstructionVisitor::translate_jeq(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JE, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// translate the jne instruction
void InstructionVisitor::translate_jne(Instruction *ins){
  Instruction *jmp = new Instruction(MINS_JNE, ins->get_operand(0));
  low_level->add_instruction(jmp);
}

// get var reference to a vreg
struct Operand InstructionVisitor::vreg_ref(Operand vreg, int bias){
  struct Operand memr_address = Operand(OPERAND_MREG_MEMREF_OFFSET, MREG_RSP, _var_offset + 8 * vreg.get_base_reg() + bias);
  return memr_address;
}


//////////////////////////////////////////////////////////////////////////////////
// API functions

struct InstructionSequence *InstructionVisitor::get_lowlevel(){
  return low_level;
}

struct InstructionVisitor *lowlevel_code_generator_create(InstructionSequence *iseq, int var_offset, int vreg_count){
  return new InstructionVisitor(iseq, var_offset, vreg_count);
}

void generator_generate_lowlevel(struct InstructionVisitor *ivst){
  ivst->translate();
}

struct InstructionSequence *generate_lowlevel(struct InstructionVisitor *ivst){
  return ivst->get_lowlevel();
}
