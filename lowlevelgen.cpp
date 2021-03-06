#include "lowlevelgen.h"
#include "cfg.h"
#include "highlevel.h"
#include "x86_64.h"
#include "codegen.h"
#include <cassert>
#include <iostream>
#include <iterator>
#include <map>
#include <ostream>
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
    int _mreg_count;

    // store stack pointer
    int rsp_offset;
    char flag = 'n';

  public:
    InstructionVisitor(InstructionSequence *iseq, int var_offset, int vreg_count, int mreg_count);
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
    void translate_mov(Instruction *ins);

    void translate_call(Instruction *ins);
    void translate_return(Instruction *ins);
    void translate_pass(Instruction *ins);

    void move_first(Instruction *ins, int operand_idx, struct Operand *reg_0, int *reg_0_constant = nullptr);
    void move_second(Instruction *ins, int operand_idx, struct Operand *reg_1, int reg_0_constant = 0);

    struct InstructionSequence *get_lowlevel();
    std::string translate_const_def(Instruction *ins);
    
    void set_flag(char flag);

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
                                                     {HINS_MOV, &InstructionVisitor::translate_mov},
                                                     {HINS_CALL, &InstructionVisitor::translate_call},
                                                     {HINS_RET, &InstructionVisitor::translate_return},
                                                     {HINS_PASS, &InstructionVisitor::translate_pass},
                                                     };
    
    // get the real memory reference of a vreg
    struct Operand vreg_ref(Operand vreg, int bias=0, int *flg=nullptr, int force=0);

    struct Operand rsp = Operand(OPERAND_MREG, MREG_RSP);
    struct Operand rdi = Operand(OPERAND_MREG, MREG_RDI);
    struct Operand rsi = Operand(OPERAND_MREG, MREG_RSI);
    struct Operand r10 = Operand(OPERAND_MREG, MREG_R10);
    struct Operand r11 = Operand(OPERAND_MREG, MREG_R11);
    struct Operand rax = Operand(OPERAND_MREG, MREG_RAX);
    
    struct Operand rdx = Operand(OPERAND_MREG, MREG_RDX);
    struct Operand eax = Operand(OPERAND_MREG, MREG_EAX);

    struct Operand r12 = Operand(OPERAND_MREG, MREG_R12);
    struct Operand r13 = Operand(OPERAND_MREG, MREG_R13);
    struct Operand r14 = Operand(OPERAND_MREG, MREG_R14);
    struct Operand r15 = Operand(OPERAND_MREG, MREG_R15);
    struct Operand rbx = Operand(OPERAND_MREG, MREG_RBX);

    struct Operand r8 = Operand(OPERAND_MREG, MREG_R8);
    struct Operand r9 = Operand(OPERAND_MREG, MREG_R9);
    struct Operand rcx = Operand(OPERAND_MREG, MREG_RCX);

    std::map<int, Operand> idx_to_register= {{7, r15}, {6, r14}, {5, r13}, {4, r12}, {3, rbx}, {2, r9}, {1, r8}, {0, rcx}};

    struct Operand inputfmt = Operand("s_readint_fmt", true);
    struct Operand readbuf_imm = Operand("s_readbuf", true);
    struct Operand readbuf = Operand("s_readbuf", false);
    struct Operand outputfmt = Operand("s_writeint_fmt", true);

    struct Operand write = Operand("printf");
    struct Operand read  = Operand("scanf");
    
    // bytes used in the stack memory
    struct Operand stack_size;

    // helper Operand for subtract one byte from rsp
    struct Operand one_byte = Operand(OPERAND_INT_LITERAL, 8);

    Instruction *cqto = new Instruction(MINS_CQTO);
};


InstructionVisitor::InstructionVisitor(InstructionSequence *iseq, int var_offset, int vreg_count, int mreg_count){
  high_level = iseq;
  _var_offset = var_offset;
  _vreg_count = vreg_count;
  _mreg_count = mreg_count;
  rsp_offset =  _var_offset + 8 * vreg_count;
  stack_size = Operand(OPERAND_INT_LITERAL, rsp_offset);
}

// set opt flag
void InstructionVisitor::set_flag(char flag){
  this->flag = flag;
}

// main translation function
void InstructionVisitor::translate(){
  // strating const declaretions
  std::string label = "\t.section .rodata\ns_readint_fmt:"
                          " .string \"%ld\"\ns_writeint_fmt: .string \"%ld\\n\"\n";
  if (flag == 'o') {
    label += "\t.section .bss\n"
	  "\t.align 8\ns_readbuf: .space 8\n";
  }
  // instruction to output an empty line
  struct Instruction *ins = new Instruction(MINS_EPTY);
  // iterate all high-level instructions
  std::vector<Instruction *>::iterator it;
  int op_code;
  
  // record all constants to .rodata if not opt
  for(it = high_level->begin(); it != high_level->end(); ++it){
    op_code = (*it)->get_opcode();
    if (op_code != HINS_CONS_DEF) {
      break;
    }
    if (flag != 'o'){
      label += translate_const_def(*it);
    }
  }
  
  // write headers
  label += "\t.section .text\n\t.globl main\nmain";
  low_level->define_label(label);
  
  // allocate spaces on stack
  if (flag == 'o'){
    for(int i = 7; i >= ((_mreg_count <= 5)?(8 - _mreg_count):3); i--){
      Instruction *pushq = new Instruction(MINS_PUSHQ, idx_to_register[i]);
      low_level->add_instruction(pushq);
      rsp_offset += 8;
    }
  }

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
  Instruction *addq = new Instruction(MINS_ADDQ, stack_size, rsp);
  low_level->add_instruction(addq);
  
  if (flag == 'o'){
    for(int i = ((_mreg_count <= 5)?(8 - _mreg_count):3); i <= 7; i++){
      Instruction *popq = new Instruction(MINS_POPQ, idx_to_register[i]);
      low_level->add_instruction(popq);
      rsp_offset -= 8;
    }
  }
  
  Instruction *ret0 = new Instruction(MINS_MOVL, Operand(OPERAND_INT_LITERAL, 0), eax);
  Instruction *ret = new Instruction(MINS_RET);
  
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
  Instruction *load, *move;

  if (flag == 'o'){
    int mreg_alloc = 0;
    Operand target = vreg_ref(ins->get_operand(0), 0, &mreg_alloc);
    if (mreg_alloc == 1) {
      load = new Instruction(MINS_LEAQ, address, target);
      low_level->add_instruction(load);
    } else {
      load = new Instruction(MINS_LEAQ, address, r10);
      move = new Instruction(MINS_MOVQ, r10, target);
      low_level->add_instruction(load);
      low_level->add_instruction(move);
    }
    
  } else {
    load = new Instruction(MINS_LEAQ, address, r10);
    move = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
    low_level->add_instruction(load);
    low_level->add_instruction(move);
  }

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
  Instruction *load;
  Operand target;

  if (flag == 'o'){
    // std::cout << 'o' << std::endl;
    int mreg_alloc = 0;
    target = vreg_ref(ins->get_operand(0), 8 * stack_push, &mreg_alloc);

    load = new Instruction(MINS_MOVQ, readbuf_imm, rsi);

  } else {
    load = new Instruction(MINS_LEAQ, vreg_ref(ins->get_operand(0), 8 * stack_push), rsi);
  }

  Instruction *call = new Instruction(MINS_CALL, read);
  low_level->add_instruction(move);
  low_level->add_instruction(load);
  low_level->add_instruction(call);

  if (flag == 'o'){
    Instruction *move_final = new Instruction(MINS_MOVQ, readbuf, target);
    low_level->add_instruction(move_final);
  }
  
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
  Instruction *load;

  if (flag == 'o'){
    int mreg_alloc = 0;
    Operand target = vreg_ref(ins->get_operand(0), 8 * stack_push, &mreg_alloc);
  
    load = new Instruction(MINS_MOVQ, target, rsi);

  } else {
    load = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0), 8 * stack_push), rsi);
  }

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
  Instruction *move_int, *move_var, *move_finl;

  if (flag == 'o'){
    int mreg_alloc_0 = 0;
    int mreg_alloc_1 = 0;
    Operand target_0 = vreg_ref(ins->get_operand(1), 0, &mreg_alloc_0);
    Operand target_1 = vreg_ref(ins->get_operand(0), 0, &mreg_alloc_1);

    Operand dest;
    if (mreg_alloc_1) {
      dest = Operand(OPERAND_MREG_MEMREF, target_1.get_base_reg());
    } else {
      move_var = new Instruction(MINS_MOVQ, target_1, r10);
      dest = Operand(OPERAND_MREG_MEMREF, r10.get_base_reg());
      low_level->add_instruction(move_var);
    }

    if (mreg_alloc_0) {
      move_finl = new Instruction(MINS_MOVQ, target_0, dest);
      low_level->add_instruction(move_finl);
    } else {
      move_int = new Instruction(MINS_MOVQ, target_0, r11);
      move_finl = new Instruction(MINS_MOVQ, r11, dest);
      low_level->add_instruction(move_int);
      low_level->add_instruction(move_finl);
    }
  
  } else {
    move_int = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
    move_var = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0), 0, 0, 1), r10);
    Operand dest = Operand(OPERAND_MREG_MEMREF, r10.get_base_reg());
    move_finl = new Instruction(MINS_MOVQ, r11, dest);  
    low_level->add_instruction(move_int);
    low_level->add_instruction(move_var);
    low_level->add_instruction(move_finl);
  }

}

// translate the mov instruction
void InstructionVisitor::translate_mov(Instruction *ins){
  Instruction *move_int, *move_finl;
  Operand reg, targe_reg;

  if(flag == 'o'){
      int mreg_alloc = 0;
      int mreg_alloc_target = 0;
      reg = vreg_ref(ins->get_operand(1), 0, &mreg_alloc);
      targe_reg = vreg_ref(ins->get_operand(0), 0, &mreg_alloc_target);
      if (mreg_alloc) {
        move_finl = new Instruction(MINS_MOVQ, reg, targe_reg);
        low_level->add_instruction(move_finl);
      } else {
        if (ins->get_operand(1).get_kind() == OPERAND_INT_LITERAL) {
          move_finl = new Instruction(MINS_MOVQ, reg, targe_reg); 
        } else {
          move_int = new Instruction(MINS_MOVQ, reg, r10);
          move_finl = new Instruction(MINS_MOVQ, r10, targe_reg); 
          low_level->add_instruction(move_int);
        }
        low_level->add_instruction(move_finl);
      }
    } else {
      move_int = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r10);
      move_finl = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
      low_level->add_instruction(move_int);
      low_level->add_instruction(move_finl);
    }

}

// translate the loadint instruction
void InstructionVisitor::translate_loadint(Instruction *ins){
  Instruction *move_var, *load_int, *store_int;
  Operand memr_ref;
  if (flag == 'o'){
    int mreg_alloc_0 = 0;
    int mreg_alloc_1 = 0;
    Operand target_0 = vreg_ref(ins->get_operand(1), 0, &mreg_alloc_0);
    Operand target_1 = vreg_ref(ins->get_operand(0), 0, &mreg_alloc_1);

    if (mreg_alloc_0) {
      memr_ref = Operand(OPERAND_MREG_MEMREF, target_0.get_base_reg());
    } else {
      move_var = new Instruction(MINS_MOVQ, target_0, r11);
      low_level->add_instruction(move_var);
      memr_ref = Operand(OPERAND_MREG_MEMREF, r11.get_base_reg());
    }
    load_int = new Instruction(MINS_MOVQ, memr_ref, r11);
    store_int = new Instruction(MINS_MOVQ, r11, target_1);
    low_level->add_instruction(load_int);
    low_level->add_instruction(store_int);
  } else {
    move_var = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
    memr_ref = Operand(OPERAND_MREG_MEMREF, r11.get_base_reg());
  
    load_int = new Instruction(MINS_MOVQ, memr_ref, r11);
    store_int = new Instruction(MINS_MOVQ, r11, vreg_ref(ins->get_operand(0)));
    
    low_level->add_instruction(move_var);
    low_level->add_instruction(load_int);
    low_level->add_instruction(store_int);
  }
}

// translate the loadconstint instruction
void InstructionVisitor::translate_loadcosntint(Instruction *ins){
  Instruction *move_const_int = new Instruction(MINS_MOVQ, ins->get_operand(1), vreg_ref(ins->get_operand(0)));
  low_level->add_instruction(move_const_int);
}

// translate the divide instruction
void InstructionVisitor::translate_div(Instruction *ins){
  Instruction *move_dividend;
  Instruction *move_divisor, *div, *move_result;

  // resolve memory reference
  if (flag == 'o') {
    Operand op1, op2, target;
    int op1_flg, op2_flg, target_flg;
    op1 = vreg_ref(ins->get_operand(1), 0, &op1_flg);
    op2 = vreg_ref(ins->get_operand(2), 0, &op2_flg);
    target = vreg_ref(ins->get_operand(0), 0, &target_flg);

    move_dividend = new Instruction(MINS_MOVQ, op1, rax);
    move_divisor = new Instruction(MINS_MOVQ, op2, r10);
    div = new Instruction(MINS_IDIVQ, r10);
    move_result = new Instruction(MINS_MOVQ, rax, target);

  } else {  
    if(ins->get_operand(1).has_base_reg()){
      move_dividend = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), rax);
    } else {
      move_dividend = new Instruction(MINS_MOVQ, ins->get_operand(1), rax);
    }

    if(ins->get_operand(2).has_base_reg()){
      move_divisor = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
    } else {
      move_divisor = new Instruction(MINS_MOVQ, ins->get_operand(2), r10);
    }

    div = new Instruction(MINS_IDIVQ, r10);
    move_result = new Instruction(MINS_MOVQ, rax, vreg_ref(ins->get_operand(0)));
    }

    low_level->add_instruction(move_dividend);
    low_level->add_instruction(cqto);
    low_level->add_instruction(move_divisor);
    low_level->add_instruction(div);
    low_level->add_instruction(move_result);
}

// translate the mod instruction
void InstructionVisitor::translate_mod(Instruction *ins){
  Instruction *move_dividend;
  Instruction *move_divisor, *div, *move_result;

  if (flag == 'o') {
    Operand op1, op2, target;
    int op1_flg, op2_flg, target_flg;
    op1 = vreg_ref(ins->get_operand(1), 0, &op1_flg);
    op2 = vreg_ref(ins->get_operand(2), 0, &op2_flg);
    target = vreg_ref(ins->get_operand(0), 0, &target_flg);

    move_dividend = new Instruction(MINS_MOVQ, op1, rax);
    move_divisor = new Instruction(MINS_MOVQ, op2, r10);
    div = new Instruction(MINS_IDIVQ, r10);
    move_result = new Instruction(MINS_MOVQ, rdx, target);

  } else {
    // resolve memory reference
    if(ins->get_operand(1).has_base_reg()){
      move_dividend = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), rax);
    } else {
      move_dividend = new Instruction(MINS_MOVQ, ins->get_operand(1), rax);
    }

    if(ins->get_operand(2).has_base_reg()){
      move_divisor = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
    } else {
      move_divisor = new Instruction(MINS_MOVQ, ins->get_operand(2), r10);
    }

    div = new Instruction(MINS_IDIVQ, r10);
    move_result = new Instruction(MINS_MOVQ, rdx, vreg_ref(ins->get_operand(0)));
  }
  
  low_level->add_instruction(move_dividend);
  low_level->add_instruction(cqto);
  low_level->add_instruction(move_divisor);
  low_level->add_instruction(div);
  low_level->add_instruction(move_result);
}

// translate the add instruction
void InstructionVisitor::translate_add(Instruction *ins){
  Operand reg_0;
  Operand reg_1;
  Instruction *add, *move_result;

  if (flag == 'o') {
    int target_flg = 0;
    Operand target = vreg_ref(ins->get_operand(0), 0, &target_flg);
    
    int op_0_flg = 0;
    int op_1_flg = 0;
    reg_0 = vreg_ref(ins->get_operand(1), 0, &op_0_flg);
    reg_1 = vreg_ref(ins->get_operand(2), 0, &op_1_flg);

    if(ins->get_operand(1).is_memref()) {
      reg_0 = reg_0.to_memref();
    }
    if(ins->get_operand(2).is_memref()) {
      reg_1 = reg_1.to_memref();
    }

    if (target_flg) {
      Instruction *move = new Instruction(MINS_MOVQ, reg_1, target);
      low_level->add_instruction(move);
      add = new Instruction(MINS_ADDQ, reg_0, target);
      low_level->add_instruction(add);
    } else {
      if (reg_0.has_base_reg()){
        add = new Instruction(MINS_ADDQ, reg_1, reg_0);
        move_result = new Instruction(MINS_MOVQ, reg_0, target);
      } else {
        add = new Instruction(MINS_ADDQ, reg_0, reg_1);
        move_result = new Instruction(MINS_MOVQ, reg_1, target);
      }
      low_level->add_instruction(add);
      low_level->add_instruction(move_result);
    }
  } else {
      int reg_0_constant = 0;
  // resolve memory reference
  move_first(ins, 1, &reg_0, &reg_0_constant);
  move_second(ins, 2, &reg_1, reg_0_constant);
  if(ins->get_operand(1).is_memref()) {
    reg_0 = reg_0.to_memref();
  }
  if(ins->get_operand(2).is_memref()) {
    reg_1 = reg_1.to_memref();
  }

  if(reg_0_constant == 0) {
    add = new Instruction(MINS_ADDQ, reg_1, reg_0);
    move_result = new Instruction(MINS_MOVQ, reg_0, vreg_ref(ins->get_operand(0)));
  } else {
    add = new Instruction(MINS_ADDQ, reg_0, reg_1);
    move_result = new Instruction(MINS_MOVQ, reg_1, vreg_ref(ins->get_operand(0)));
  }

  low_level->add_instruction(add);
  low_level->add_instruction(move_result);
  }

}

// translate the sub instruction
void InstructionVisitor::translate_sub(Instruction *ins){
  Operand reg_0;
  Operand reg_1;
  Instruction *add;
  Instruction *move_result;

  if (flag == 'o'){
    int target_flg = 0;
    Operand target = vreg_ref(ins->get_operand(0), 0, &target_flg);
    
    int op_0_flg = 0;
    int op_1_flg = 0;
    reg_0 = vreg_ref(ins->get_operand(1), 0, &op_0_flg);
    reg_1 = vreg_ref(ins->get_operand(2), 0, &op_1_flg);

    if(ins->get_operand(1).is_memref()) {
      reg_0 = reg_0.to_memref();
    }
    if(ins->get_operand(2).is_memref()) {
      reg_1 = reg_1.to_memref();
    }

    if (target_flg) {
      Instruction *move = new Instruction(MINS_MOVQ, reg_0, target);
      low_level->add_instruction(move);
      add = new Instruction(MINS_SUBQ, reg_1, target);
      low_level->add_instruction(add);
    } else {
      add = new Instruction(MINS_SUBQ, reg_1, reg_0);
      move_result = new Instruction(MINS_MOVQ, reg_0, target);

      low_level->add_instruction(add);
      low_level->add_instruction(move_result);
    }
  
  } else {
    // resolve memory reference
    move_first(ins, 2, &reg_1);
    move_second(ins, 1, &reg_0, 1);

    // resolve memory reference
    if(ins->get_operand(1).is_memref()) {
      reg_0 = reg_0.to_memref();
    }
    if(ins->get_operand(2).is_memref()) {
      reg_1 = reg_1.to_memref();
    }
    
    add = new Instruction(MINS_SUBQ, reg_1, reg_0);
    move_result = new Instruction(MINS_MOVQ, reg_0, vreg_ref(ins->get_operand(0)));

    low_level->add_instruction(add);
    low_level->add_instruction(move_result);
  }
}

// translate the mul instruction
void InstructionVisitor::translate_mul(Instruction *ins){

  Operand reg_0;
  Operand reg_1;

  Instruction *add;
  Instruction *move_result;

  if (flag == 'o') {
    int target_flg = 0;
    Operand target = vreg_ref(ins->get_operand(0), 0, &target_flg);
    
    int op_0_flg = 0;
    int op_1_flg = 0;
    reg_0 = vreg_ref(ins->get_operand(1), 0, &op_0_flg);
    reg_1 = vreg_ref(ins->get_operand(2), 0, &op_1_flg);

    if(ins->get_operand(1).is_memref()) {
      reg_0 = reg_0.to_memref();
    }
    if(ins->get_operand(2).is_memref()) {
      reg_1 = reg_1.to_memref();
    }

    if (target_flg) {
      Instruction *move = new Instruction(MINS_MOVQ, reg_1, target);
      low_level->add_instruction(move);
      add = new Instruction(MINS_IMULQ, reg_0, target);
      low_level->add_instruction(add);
    } else {
      if (reg_0.has_base_reg()){
        add = new Instruction(MINS_IMULQ, reg_1, reg_0);
        move_result = new Instruction(MINS_MOVQ, reg_0, target);
      } else {
        add = new Instruction(MINS_IMULQ, reg_0, reg_1);
        move_result = new Instruction(MINS_MOVQ, reg_1, target);
      }
      low_level->add_instruction(add);
      low_level->add_instruction(move_result);
    }
  } else {
    int reg_0_constant = 0;
    // resolve memory reference
    move_first(ins, 1, &reg_0, &reg_0_constant);
    move_second(ins, 2, &reg_1, reg_0_constant);

    // resolve memory reference
    if(ins->get_operand(1).is_memref()) {
      reg_0 = reg_0.to_memref();
    }
    if(ins->get_operand(2).is_memref()) {
      reg_1 = reg_1.to_memref();
    }

    if(reg_0_constant == 0) {
      add = new Instruction(MINS_IMULQ, reg_1, reg_0);
      move_result = new Instruction(MINS_MOVQ, reg_0, vreg_ref(ins->get_operand(0)));
    } else {
      add = new Instruction(MINS_IMULQ, reg_0, reg_1);
      move_result = new Instruction(MINS_MOVQ, reg_1, vreg_ref(ins->get_operand(0)));
    }
    
    low_level->add_instruction(add);
    low_level->add_instruction(move_result);
  }

}

// translate the cmp instruction
void InstructionVisitor::translate_cmp(Instruction *ins){
  Operand reg_0;
  Operand reg_1;
  if (flag == 'o') {
    int op_0_flg = 0;
    int op_1_flg = 0;
    reg_0 = vreg_ref(ins->get_operand(0), 0, &op_0_flg);
    reg_1 = vreg_ref(ins->get_operand(1), 0, &op_1_flg);

    if (op_0_flg == 0){
      Instruction *move = new Instruction(MINS_MOVQ, reg_0, r10);
      low_level->add_instruction(move);
      reg_0 = r10;
    }

    if (op_1_flg == 0){
      Instruction *move = new Instruction(MINS_MOVQ, reg_1, r11);
      low_level->add_instruction(move);
      reg_1 = r11;
    }
  } else {

    // resolve memory reference
    move_first(ins, 0, &reg_0);
    move_second(ins, 1, &reg_1);

  }
  Instruction *cmp = new Instruction(MINS_CMPQ, reg_1, reg_0);
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

void InstructionVisitor::translate_call(Instruction *ins){
  Instruction *move_int, *move_finl;
  Operand reg, targe_reg;

  /* if(flag == 'o'){
    int mreg_alloc = 0;
    int mreg_alloc_target = 0;
    reg = vreg_ref(ins->get_operand(1), 0, &mreg_alloc);
    targe_reg = vreg_ref(ins->get_operand(0), 0, &mreg_alloc_target);
    if (mreg_alloc) {
      move_finl = new Instruction(MINS_MOVQ, reg, targe_reg);
      low_level->add_instruction(move_finl);
    } else {
      if (ins->get_operand(1).get_kind() == OPERAND_INT_LITERAL) {
        move_finl = new Instruction(MINS_MOVQ, reg, targe_reg); 
      } else {
        move_int = new Instruction(MINS_MOVQ, reg, r10);
        move_finl = new Instruction(MINS_MOVQ, r10, targe_reg); 
        low_level->add_instruction(move_int);
      }
      low_level->add_instruction(move_finl);
    }
  } else  { */
  move_int = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r10);
  move_finl = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
  low_level->add_instruction(move_int);
  low_level->add_instruction(move_finl);
  /* } */
}

void InstructionVisitor::translate_return(Instruction *ins){

}


// get var reference to a vreg
struct Operand InstructionVisitor::vreg_ref(Operand vreg, int bias, int *flg, int force){
  if (this->flag == 'o') {
    assert(flg != nullptr);
    int mreg_id = vreg.get_m_reg_to_alloc();
    if (mreg_id >= 0){
      struct Operand mreg = idx_to_register[mreg_id];
      *flg = 1;
      return mreg;
    } 
  }

  // memory references
  if(force || vreg.has_base_reg()){
    struct Operand memr_address = Operand(OPERAND_MREG_MEMREF_OFFSET, MREG_RSP, _var_offset + 8 * vreg.get_base_reg() + bias);
    return memr_address;
  } else {
    return vreg;
  }
  
}

void InstructionVisitor::move_first(Instruction *ins, int operand_idx, struct Operand *reg_0, int *reg_0_constant){
  Instruction *move_first;
  if(ins->get_operand(operand_idx).has_base_reg()){
    move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(operand_idx)), r10);
    *reg_0 = r10;
    low_level->add_instruction(move_first);
  } else {
    if (flag == 'o'){
      *reg_0 = ins->get_operand(operand_idx);
      if (reg_0_constant != nullptr){
        *reg_0_constant = 1;
      }
    } else {
      move_first = new Instruction(MINS_MOVQ, ins->get_operand(operand_idx), r10);
      *reg_0 = r10;
      low_level->add_instruction(move_first);
    }
    // 
  }
}

void InstructionVisitor::move_second(Instruction *ins, int operand_idx, struct Operand *reg_1, int reg_0_constant){
  Instruction *move_second;
  if(ins->get_operand(operand_idx).has_base_reg()){
      move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(operand_idx)), r11);
      *reg_1 = r11;
      low_level->add_instruction(move_second);
    } else {
      if (flag == 'o' && reg_0_constant == 0){
        *reg_1 = ins->get_operand(operand_idx);
      } else {
        move_second = new Instruction(MINS_MOVQ, ins->get_operand(operand_idx), r11);
        *reg_1 = r11;
        low_level->add_instruction(move_second);
      }
    }
}


//////////////////////////////////////////////////////////////////////////////////
// API functions

struct InstructionSequence *InstructionVisitor::get_lowlevel(){
  return low_level;
}

struct InstructionVisitor *lowlevel_code_generator_create(InstructionSequence *iseq, int var_offset, int vreg_count, int mreg_count){
  return new InstructionVisitor(iseq, var_offset, vreg_count, mreg_count);
}

void generator_generate_lowlevel(struct InstructionVisitor *ivst){
  ivst->translate();
}

struct InstructionSequence *generate_lowlevel(struct InstructionVisitor *ivst){
  return ivst->get_lowlevel();
}

void lowlevel_generator_set_flag(struct InstructionVisitor *ivst, char flag){
  ivst->set_flag(flag);
}