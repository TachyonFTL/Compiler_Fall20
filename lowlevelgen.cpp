#include "lowlevelgen.h"
#include "cfg.h"
#include "highlevel.h"
#include "x86_64.h"
#include "codegen.h"
#include <iostream>
#include <iterator>
#include <map>

class InstructionVisitor{
  private:
    struct InstructionSequence *high_level;
    struct InstructionSequence *low_level = new InstructionSequence();
		int _var_offset;
		int _vreg_count;
		int rsp_offset;

    //SymbolTable *symtable = nullptr;
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

		void translate_div(Instruction *ins);
		void translate_add(Instruction *ins);
		void translate_sub(Instruction *ins);
		void translate_mul(Instruction *ins);
		void translate_mod(Instruction *ins);


		struct InstructionSequence *get_lowlevel();

	private:
		typedef void (InstructionVisitor::*func_ptr)(Instruction*);
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
																										 {HINS_JUMP, &InstructionVisitor::translate_jump}};
		struct Operand vreg_ref(Operand vreg);

		//struct Operand localvar = Operand(OPERAND_MREG_MEMREF_OFFSET, MREG_RSP, 0);
		struct Operand rsp = Operand(OPERAND_MREG, MREG_RSP);
		struct Operand rdi = Operand(OPERAND_MREG, MREG_RDI);
		struct Operand rsi = Operand(OPERAND_MREG, MREG_RSI);
		struct Operand r10 = Operand(OPERAND_MREG, MREG_R10);
		struct Operand r11 = Operand(OPERAND_MREG, MREG_R11);
		struct Operand rax = Operand(OPERAND_MREG, MREG_RAX);
		struct Operand rdx = Operand(OPERAND_MREG, MREG_RDX);
		struct Operand eax = Operand(OPERAND_MREG, MREG_EAX);

		struct Operand inputfmt = Operand("s_readint_fmt", true);
		struct Operand outputfmt = Operand("s_writeint_fmt", true);

		struct Operand write = Operand("printf");
    struct Operand read  = Operand("scanf");

		struct Operand stack_size;
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

void InstructionVisitor::translate(){
	low_level->define_label("\t.section .rodata\ns_readint_fmt:"
	                        " .string \"%ld\"\ns_writeint_fmt: .string \"%ld\\n\"\n\t.section .text\n\t.globl main\nmain");
	//struct Instruction *ins = new Instruction(MINS_EPTY);
	//low_level->add_instruction(ins);
	
  Instruction *pushq = new Instruction(MINS_SUBQ, stack_size, rsp);
	low_level->add_instruction(pushq);

  std::vector<Instruction *>::iterator it;
	for(it = high_level->begin(); it != high_level->end(); ++it){
    int op_code = (*it)->get_opcode();
		std::map<int, func_ptr>::iterator iter;
		if (high_level->has_label(it - high_level->begin())) {
			low_level->define_label(high_level->get_label(it - high_level->begin()));
		}
    iter = translate_high_to_low.find(op_code);
		if(iter != translate_high_to_low.end()){
			(this->*(iter->second))(*it);
		} 
	}

	Instruction *popq = new Instruction(MINS_ADDQ, stack_size, rsp);
	Instruction *ret0 = new Instruction(MINS_MOVL, Operand(OPERAND_INT_LITERAL, 0), eax);
	Instruction *ret = new Instruction(MINS_RET);
	low_level->add_instruction(popq);
	low_level->add_instruction(ret0);
	low_level->add_instruction(ret);
}

void InstructionVisitor::translate_localaddr(Instruction *ins){
	Operand address = Operand(OPERAND_MREG_MEMREF_OFFSET, MREG_RSP, ins->get_operand(1).get_int_value());
	Instruction *load = new Instruction(MINS_LEAQ, address, r10);
	Instruction *move = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
	low_level->add_instruction(load);
	low_level->add_instruction(move);
}

void InstructionVisitor::translate_readint(Instruction *ins){
	int stack_push = 0;
	if(rsp_offset % 16 != 8){
		stack_push = 1;
		Instruction *pushq = new Instruction(MINS_SUBQ, one_byte, rsp);
		low_level->add_instruction(pushq);
		rsp_offset += 8;
	}

	Instruction *move = new Instruction(MINS_MOVQ, inputfmt, rdi);
	Instruction *load = new Instruction(MINS_LEAQ, vreg_ref(ins->get_operand(0)), rsi);
	Instruction *call = new Instruction(MINS_CALL, read);
	low_level->add_instruction(move);
	low_level->add_instruction(load);
	low_level->add_instruction(call);
	
	if(stack_push == 1){
		Instruction *popq = new Instruction(MINS_ADDQ, one_byte, rsp);
		low_level->add_instruction(popq);
		rsp_offset -= 8;
	}
}

void InstructionVisitor::translate_writeint(Instruction *ins){
	int stack_push = 0;
	if(rsp_offset % 16 != 8){
		stack_push = 1;
		Instruction *pushq = new Instruction(MINS_SUBQ, one_byte, rsp);
		low_level->add_instruction(pushq);
		rsp_offset += 8;
	}

	Instruction *move = new Instruction(MINS_MOVQ, outputfmt, rdi);
	Instruction *load = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0)), rsi);
	Instruction *call = new Instruction(MINS_CALL, write);
	low_level->add_instruction(move);
	low_level->add_instruction(load);
	low_level->add_instruction(call);

	if(stack_push == 1){
		Instruction *popq = new Instruction(MINS_ADDQ, one_byte, rsp);
		low_level->add_instruction(popq);
		rsp_offset -= 8;
	}
}

void InstructionVisitor::translate_storeint(Instruction *ins){
	Instruction *move_int = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
	Instruction *move_var = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0)), r10);
	Operand dest = Operand(OPERAND_MREG_MEMREF, r10.get_base_reg());
	Instruction *move_finl = new Instruction(MINS_MOVQ, r11, dest);
	low_level->add_instruction(move_int);
	low_level->add_instruction(move_var);
	low_level->add_instruction(move_finl);
}

void InstructionVisitor::translate_loadint(Instruction *ins){
	Instruction *move_var = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);

	Operand memr_ref = Operand(OPERAND_MREG_MEMREF, r11.get_base_reg());
	Instruction *load_int = new Instruction(MINS_MOVQ, memr_ref, r11);
	Instruction *store_int = new Instruction(MINS_MOVQ, r11, vreg_ref(ins->get_operand(0)));
	low_level->add_instruction(move_var);
	low_level->add_instruction(load_int);
	low_level->add_instruction(store_int);

}

void InstructionVisitor::translate_loadcosntint(Instruction *ins){
	Instruction *move_const_int = new Instruction(MINS_MOVQ, ins->get_operand(1), vreg_ref(ins->get_operand(0)));
  low_level->add_instruction(move_const_int);
}

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

void InstructionVisitor::translate_add(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  Instruction *add = new Instruction(MINS_ADDQ, r11, r10);
	//std::cout << _var_offset + 8 * ins->get_operand(0).get_base_reg() << std::endl;
  Instruction *move_result = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
  
	low_level->add_instruction(move_first);
	low_level->add_instruction(move_second);
	low_level->add_instruction(add);
	low_level->add_instruction(move_result);
}

void InstructionVisitor::translate_sub(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  Instruction *add = new Instruction(MINS_SUBQ, r10, r11);
	//std::cout << _var_offset + 8 * ins->get_operand(0).get_base_reg() << std::endl;
  Instruction *move_result = new Instruction(MINS_MOVQ, r11, vreg_ref(ins->get_operand(0)));
  
	low_level->add_instruction(move_first);
	low_level->add_instruction(move_second);
	low_level->add_instruction(add);
	low_level->add_instruction(move_result);
}

void InstructionVisitor::translate_mul(Instruction *ins){
  Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(2)), r10);
  Instruction *add = new Instruction(MINS_IMULQ, r11, r10);
	//std::cout << _var_offset + 8 * ins->get_operand(0).get_base_reg() << std::endl;
  Instruction *move_result = new Instruction(MINS_MOVQ, r10, vreg_ref(ins->get_operand(0)));
  
	low_level->add_instruction(move_first);
	low_level->add_instruction(move_second);
	low_level->add_instruction(add);
	low_level->add_instruction(move_result);
}


void InstructionVisitor::translate_cmp(Instruction *ins){
	Instruction *move_first = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(0)), r10);
  Instruction *move_second = new Instruction(MINS_MOVQ, vreg_ref(ins->get_operand(1)), r11);
  Instruction *cmp = new Instruction(MINS_CMPQ, r11, r10);
	low_level->add_instruction(move_first);
	low_level->add_instruction(move_second);
	low_level->add_instruction(cmp);
}

void InstructionVisitor::translate_jump(Instruction *ins){
	Instruction *jmp = new Instruction(MINS_JMP, ins->get_operand(0));
	low_level->add_instruction(jmp);
}

void InstructionVisitor::translate_jlte(Instruction *ins){
	Instruction *jmp = new Instruction(MINS_JLE, ins->get_operand(0));
	low_level->add_instruction(jmp);
}

struct Operand InstructionVisitor::vreg_ref(Operand vreg){
  struct Operand memr_address = Operand(OPERAND_MREG_MEMREF_OFFSET, MREG_RSP, _var_offset + 8 * vreg.get_base_reg());
	return memr_address;
}

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
