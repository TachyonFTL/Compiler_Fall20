#include "codegen.h"
#include "ast.h"
#include "node.h"
#include "symbol.h"
#include "symtab.h"
#include "cfg.h"
#include "highlevel.h"
#include "stdio.h"
#include "type.h"
#include <iostream>
#include <ostream>
#include <string>
#include <map>

////////////////////////////////////////////////////////////////////////
// Main class CodeGenerator as a ASTvisitor
class CodeGenerator: public ASTVisitor  {
private:
  // ast root
  struct Node *root = nullptr;

  char flag = 'n';

  // symbol table from static analysis
  SymbolTable *symtable = nullptr;
  
  InstructionSequence *code = new InstructionSequence();
  int vreg_count = 0;
  int max_vreg_count = 0;
  int label_count = 0;

public:
  CodeGenerator(struct Node *ast, SymbolTable *symtab);
  ~CodeGenerator();
  void generate_code();
  InstructionSequence *get_code();
  int get_vreg_count();
  void set_flag(char flg);
  
private:
  // two maps to store corresponding jump commands for normal and inverted jump
  std::map<int, int> compare_to_jump_inverted = {{AST_COMPARE_LT, HINS_JGTE}, {AST_COMPARE_GT, HINS_JLTE},
                                                 {AST_COMPARE_LTE, HINS_JGT}, {AST_COMPARE_GTE, HINS_JLT},
                                                 {AST_COMPARE_EQ,  HINS_JNE}, {AST_COMPARE_NEQ, HINS_JE}};

  std::map<int, int> compare_to_jump = {{AST_COMPARE_LT, HINS_JLT}, {AST_COMPARE_GT, HINS_JGT},
                                        {AST_COMPARE_LTE, HINS_JLTE}, {AST_COMPARE_GTE, HINS_JGTE},
                                        {AST_COMPARE_EQ,  HINS_JE}, {AST_COMPARE_NEQ, HINS_JNE}};

  void visit_instructions(struct Node *ast);
  void visit_constant_def(struct Node *ast);
  void visit_read(struct Node *ast);
  void visit_write(struct Node *ast);
  void visit_var_ref(struct Node *ast);
  void visit_array_element_ref(struct Node *ast);
  void visit_field_ref(struct Node *ast);
  void visit_expression_list(struct Node *ast);

  void visit_assign(struct Node *ast);
  void visit_designator(struct Node *ast);
  void visit_expression(struct Node *ast);
  void visit_modulus(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right);
  void visit_add(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right);
  void visit_subtract(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right);
  void visit_multiply(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right);
  void visit_divide(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right);
  void visit_negate(struct Node *ast);
  void visit_int_literal(struct Node *ast);

  void visit_compare(struct Node *ast, struct Operand *left, struct Operand *right);

  void visit_if(struct Node *ast);
  void visit_if_else(struct Node *ast);
  void visit_repeat(struct Node *ast);
  void visit_while(struct Node *ast);

  int get_jmp_ins(struct Node *ast, bool invert = 1);

  // alloc a new vreg
  int alloc_vreg();

  // reset allocated vreg after instructions
  void rest_vreg();

  // alloc a new label for code blocks
  std::string alloc_label();


};

CodeGenerator::CodeGenerator(struct Node *ast, SymbolTable *symtab){
  this->root = ast;
  this->symtable = symtab;
}

CodeGenerator::~CodeGenerator(){
}

void CodeGenerator::set_flag(char flg){
  this->flag = flg;
}

void CodeGenerator::generate_code(){
  visit(node_get_kid(this->root, 1));
  return visit_instructions(node_get_kid(this->root, 2));
}

InstructionSequence * CodeGenerator::get_code(){
  return this->code;
}

// visit constant definitions
void CodeGenerator::visit_constant_def(struct Node *ast){
  std::string name = node_get_str(node_get_kid(ast, 0));
  Symbol *constant = this->symtable->get_symbol_in_scope(name);
  std::string type_name = constant->get_type_name();

  struct Operand* oprand = new Operand(name, true);
  struct Operand* immval;

  // store int constant with CONS_DEF operation
  if (type_name == "INTEGER"){
    immval = new Operand(OPERAND_INT_LITERAL, constant->get_const_val());
  }  else {
    error_at_node(ast, "Unspported constant type");
  }

  ast->set_oprand(oprand);
  Instruction *ins = new Instruction(HINS_CONS_DEF, *oprand, *immval);
  this->code->add_instruction(ins);
}

// visit a given instruction based on its kind
void CodeGenerator::visit_instructions(struct Node *ast){
  int num_kids = node_get_num_kids(ast);
  struct Node *instruction = node_get_kid(ast, 0);
  switch (node_get_tag(instruction)) {
    case AST_READ:
      visit_read(instruction);
      break;
    case AST_WRITE:
      visit_write(instruction);
      break;
    case AST_VAR_REF:
      visit_var_ref(instruction);
      break;
    case AST_FIELD_REF:
      visit_field_ref(instruction);
      break;
    case AST_ASSIGN:
      visit_assign(instruction);
      break;
    case AST_IF:
      visit_if(instruction);
      break;
    case AST_IF_ELSE:
      visit_if_else(instruction);
      break;
    case AST_WHILE:
      visit_while(instruction);
      break;    
    case AST_REPEAT:
      visit_repeat(instruction);
      break;
    default:
      error_at_node(instruction, "visit_instructions: Unknown Instruction");
  }
  
  // rest allocated vreg
  // this->rest_vreg();

  // recursively visit subsequent instructions
  if(num_kids == 2){
    return visit_instructions(node_get_kid(ast, 1));
  }

}

// visit readint instruction
void CodeGenerator::visit_read(struct Node *ast){
  struct Node* designator = node_get_kid(ast, 0);

  visit_expression(designator);
  
  // read int
  struct Operand* oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
  this->code->add_instruction(new Instruction(HINS_READ_INT, *oprand));
  
  // assign int to var
  struct Operand* vreg_ref;

  int tag = node_get_tag(designator);
  if (tag == AST_VAR_REF) {
    vreg_ref = designator->get_oprand();
    this->code->add_instruction(new Instruction(HINS_MOV, *vreg_ref, *oprand));
  } else {
    vreg_ref = new Operand(OPERAND_VREG_MEMREF, designator->get_oprand()->get_base_reg());
    this->code->add_instruction(new Instruction(HINS_STORE_INT, *vreg_ref, *oprand));
  }
  
}

// visit writeint instruction
void CodeGenerator::visit_write(struct Node *ast){
  struct Node* expression = node_get_kid(ast, 0);
  
  Operand *exp_oprand;

  visit_expression(expression);
  
  if (expression->is_const() == 1){
    exp_oprand = expression->get_oprand();
  } else {
    int tag = node_get_tag(expression);
    // check if it is a memory reference
    if (tag == AST_VAR_REF) {
      exp_oprand = expression->get_oprand();
    } else if (tag == AST_ARRAY_ELEMENT_REF || tag == AST_FIELD_REF){
      Operand *exp_result = new Operand(OPERAND_VREG_MEMREF, expression->get_oprand()->get_base_reg());
      exp_oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
      this->code->add_instruction(new Instruction(HINS_LOAD_INT, *exp_oprand, *exp_result));
    } else {
      exp_oprand = new Operand(OPERAND_VREG, expression->get_oprand()->get_base_reg());
    }
  }

  this->code->add_instruction(new Instruction(HINS_WRITE_INT, *exp_oprand));
}

// visit var reference 
void CodeGenerator::visit_var_ref(struct Node *ast){
  struct Operand* oprand;
  struct Operand* immval;
  
  // check if the var is a constant or a real variable
  if (this->symtable->get_symbol(ast->get_str())->get_kind() == KIND_CONST){
    if (this->flag == 'o'){
      immval = new Operand(OPERAND_INT_LITERAL, this->symtable->get_symbol(ast->get_str())->get_const_val());
      ast->set_oprand(immval);
      ast->set_const();
    } else {
      immval = new Operand(ast->get_str(), true);
      oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
      this->code->add_instruction(new Instruction(HINS_LOAD_ICONST, *oprand, *immval));
      ast->set_oprand(oprand);
    }
    
    // this->code->add_instruction(new Instruction(HINS_LOAD_ICONST, *oprand, *immval));
  } else {
    if (this->symtable->get_symbol(ast->get_str())->get_type()->get_kind() == BASE_TYPE){
      struct Operand* op = this->symtable->get_symbol(ast->get_str())->get_operand();
      if (op == nullptr){
        oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
        this->symtable->get_symbol(ast->get_str())->set_operand(oprand);
      } else {
        oprand = op;
      }
    } else {
      oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
      immval = new Operand(OPERAND_INT_LITERAL, this->symtable->get_symbol(ast->get_str())->get_offset());
      this->code->add_instruction(new Instruction(HINS_LOCALADDR, *oprand, *immval));
    }
    

    ast->set_oprand(oprand);
  }
}

// visit var reference 
void CodeGenerator::visit_array_element_ref(struct Node *ast){
  // left is the array var and right is an expression
  struct Node *left = node_get_kid(ast, 0);
  struct Node *right = node_get_kid(ast, 1);

  visit_expression(left);
  visit_expression_list(right);
  
  // get the type of the array
  struct Type *current_type = ast->get_type();

  // resolve multidim array reference
  while(right != nullptr){
    // allocate vreg for expression and get the element data size of current array
    struct Operand* oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
    struct Operand* arr_data_size = new Operand(OPERAND_INT_LITERAL, current_type->get_base_type()->get_size());

    this->code->add_instruction(new Instruction(HINS_INT_MUL, *oprand, *right->get_oprand(), *arr_data_size));

    struct Operand* arr_element_ref = new Operand(OPERAND_VREG, this->alloc_vreg());
    
    ast->set_oprand(arr_element_ref);
    this->code->add_instruction(new Instruction(HINS_INT_ADD, *arr_element_ref, *left->get_oprand(), *oprand));
    
    // continue if there are more dims
    if (node_get_num_kids(right) == 1){
      break;
    }
    right->set_oprand(arr_element_ref);
    
    left = right;
    right = node_get_kid(right, 1);
    current_type = current_type->get_base_type();
  }
}

// visit references to a field of a record type
void CodeGenerator::visit_field_ref(struct Node *ast){
  struct Node *record = node_get_kid(ast, 0);
  struct Node *field = node_get_kid(ast, 1);

  visit_expression(record);

  struct Operand* oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
  
  // get field memory offset from symbol table
  struct Operand* immval = new Operand(OPERAND_INT_LITERAL, ast->get_type()->get_field()->get_symbol_in_scope(node_get_str(field))->get_offset());

  this->code->add_instruction(new Instruction(HINS_INT_ADD, *oprand, *record->get_oprand(), *immval));
  ast->set_oprand(oprand);
  
}

// visit a list of expressions encountered on multidim array
void CodeGenerator::visit_expression_list(struct Node *ast){
  struct Node *kid = node_get_kid(ast, 0);
  visit_expression(kid);

  if (kid->is_const() == 1){
    struct Operand *left_exp_result = kid->get_oprand();
    ast->set_oprand(left_exp_result);
  } else {
    int tag = node_get_tag(kid);
    
    // check if the current expression is a memory reference
    if (tag == AST_VAR_REF) {
      struct Operand *left_exp_result = kid->get_oprand();
      ast->set_oprand(left_exp_result);
    } else if (tag == AST_ARRAY_ELEMENT_REF || tag == AST_FIELD_REF){
      struct Operand *left_exp_result = new Operand(OPERAND_VREG_MEMREF, kid->get_oprand()->get_base_reg());
      ast->set_oprand(left_exp_result);
    } else {
      ast->set_oprand(node_get_kid(ast, 0)->get_oprand());
    }
  }  

  // recursion of remaining expressions
  if(node_get_num_kids(ast) != 1) {
    return visit_expression_list(node_get_kid(ast, 1));
  }
  
}

// visit var assignment
void CodeGenerator::visit_assign(struct Node *ast){
  struct Node* designator = node_get_kid(ast, 0);
  struct Node* expression = node_get_kid(ast, 1);

  visit_expression(designator);
  visit_expression(expression);
  
  struct Operand *exp_oprand;

  // resolve memory refernce
  struct Operand* vreg_ref;

  if (expression->is_const() == 1){
    exp_oprand = expression->get_oprand();
  } else {
    int tag = node_get_tag(expression);
    struct Operand *exp_result;
    if (tag == AST_VAR_REF) {
      exp_oprand = expression->get_oprand();

    } else if (tag == AST_ARRAY_ELEMENT_REF || tag == AST_FIELD_REF){
      exp_result = new Operand(OPERAND_VREG_MEMREF, expression->get_oprand()->get_base_reg());
      exp_oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
      this->code->add_instruction(new Instruction(HINS_LOAD_INT, *exp_oprand, *exp_result));
    } else {
      exp_oprand = new Operand(OPERAND_VREG, expression->get_oprand()->get_base_reg());
    }
  }

  int designator_tag = node_get_tag(designator);
  if (designator_tag == AST_VAR_REF) {
      vreg_ref = designator->get_oprand();
      this->code->add_instruction(new Instruction(HINS_MOV, *vreg_ref, *exp_oprand));
    } else {
      vreg_ref = new Operand(OPERAND_VREG_MEMREF, designator->get_oprand()->get_base_reg());
      this->code->add_instruction(new Instruction(HINS_STORE_INT, *vreg_ref, *exp_oprand));
    }
  
  
}

// visit a designator (Deprecated)
void CodeGenerator::visit_designator(struct Node *ast){
    switch (node_get_tag(ast)) {
    case AST_VAR_REF:
      return visit_var_ref(ast);
    default:
      error_at_node(ast, "visit_designator: Designator Type Not Implemented");
  }
}

// visit an expression
void CodeGenerator::visit_expression(struct Node *ast){
  // uniary expression
  switch (node_get_tag(ast)) {
    case AST_INT_LITERAL:
      return visit_int_literal(ast);
    case AST_VAR_REF:
      return visit_var_ref(ast);        
    case AST_NEGATE:
      return visit_negate(ast);
    case AST_ARRAY_ELEMENT_REF:
      return visit_array_element_ref(ast);
    case AST_FIELD_REF:
      return visit_field_ref(ast);
  }

  // biary expression
  struct Node *left = node_get_kid(ast, 0);
  struct Node *right = node_get_kid(ast, 1);
  struct Operand* left_oprand;
  struct Operand* right_oprand;

  visit_expression(left);
  visit_expression(right);

 // resolve memory reference
  if (left->is_const() == 1){
    left_oprand = left->get_oprand();
  } else {
    int l_tag = node_get_tag(node_get_kid(ast, 0));
    if (l_tag == AST_VAR_REF) {
      left_oprand = left->get_oprand();
    } else if (l_tag == AST_ARRAY_ELEMENT_REF || l_tag == AST_FIELD_REF){
      struct Operand *left_exp_result; 
      left_exp_result = new Operand(OPERAND_VREG_MEMREF, left->get_oprand()->get_base_reg());
      left_oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
      this->code->add_instruction(new Instruction(HINS_LOAD_INT, *left_oprand, *left_exp_result));
    } else {
      left_oprand = new Operand(OPERAND_VREG, left->get_oprand()->get_base_reg());
    }
  }
  // std::cout << right->get_str() << right->is_const() << std::endl;
  if (right->is_const() == 1){
    right_oprand = right->get_oprand();
  } else {
    int r_tag = node_get_tag(node_get_kid(ast, 1));
    if (r_tag == AST_VAR_REF) {
      right_oprand = right->get_oprand();
    } else if (r_tag == AST_ARRAY_ELEMENT_REF || r_tag == AST_FIELD_REF){
      struct Operand *right_exp_result;
      right_exp_result = new Operand(OPERAND_VREG_MEMREF, right->get_oprand()->get_base_reg());
      right_oprand = new Operand(OPERAND_VREG, this->alloc_vreg());
      this->code->add_instruction(new Instruction(HINS_LOAD_INT, *right_oprand, *right_exp_result));
    } else {
      right_oprand = new Operand(OPERAND_VREG, right->get_oprand()->get_base_reg());
    }
  }

  // relational expression
  switch (node_get_tag(ast)) {
    case AST_COMPARE_LT:
    case AST_COMPARE_GT:
    case AST_COMPARE_LTE:
    case AST_COMPARE_GTE:
    case AST_COMPARE_EQ:
    case AST_COMPARE_NEQ:
      return visit_compare(ast, left_oprand, right_oprand);
  }

  // arithmatic operation
  struct Operand* op = new Operand(OPERAND_VREG, this->alloc_vreg());
  ast->set_oprand(op);

  switch (node_get_tag(ast)) {
    case AST_ADD:
      return visit_add(ast, op, left_oprand, right_oprand);
    case AST_MODULUS:
      return visit_modulus(ast, op, left_oprand, right_oprand);
    case AST_SUBTRACT:
      return visit_subtract(ast, op, left_oprand, right_oprand);
    case AST_MULTIPLY:
      return visit_multiply(ast, op, left_oprand, right_oprand);
    case AST_DIVIDE:
      return visit_divide(ast, op, left_oprand, right_oprand);
    default:
      error_at_node(ast, "visit_expression: Expression Type Not Implemented");
  }
}

void CodeGenerator::visit_modulus(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right){
  this->code->add_instruction(new Instruction(HINS_INT_MOD, *op, *left, *right));
}

void CodeGenerator::visit_add(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right){
  this->code->add_instruction(new Instruction(HINS_INT_ADD, *op, *left, *right));
}

void CodeGenerator::visit_subtract(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right){
  this->code->add_instruction(new Instruction(HINS_INT_SUB, *op, *left, *right));
}

void CodeGenerator::visit_multiply(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right){
  this->code->add_instruction(new Instruction(HINS_INT_MUL, *op, *left, *right));
}

void CodeGenerator::visit_divide(struct Node *ast, struct Operand *op, struct Operand *left, struct Operand *right){
  this->code->add_instruction(new Instruction(HINS_INT_DIV, *op, *left, *right));
}

// visit negation of an expression
void CodeGenerator::visit_negate(struct Node *ast){
  struct Node *expression = node_get_kid(ast, 0);
  visit_expression(expression);

  struct Operand *exp_result = new Operand(OPERAND_VREG_MEMREF, expression->get_oprand()->get_base_reg());
  struct Operand* exp_reg = new Operand(OPERAND_VREG, this->alloc_vreg());

  struct Operand* op = new Operand(OPERAND_VREG, this->alloc_vreg());
  ast->set_oprand(op);

  this->code->add_instruction(new Instruction(HINS_LOAD_INT, *exp_reg, *exp_result));
  this->code->add_instruction(new Instruction(HINS_INT_NEGATE, *op, *exp_reg));
}

void CodeGenerator::visit_int_literal(struct Node *ast){
  if (this->flag == 'o'){
    struct Operand* immval = new Operand(OPERAND_INT_LITERAL, ast->get_ival());
    ast->set_oprand(immval);
    ast->set_const();
  } else {
    struct Operand* reg = new Operand(OPERAND_VREG, this->alloc_vreg());
    struct Operand* immval = new Operand(OPERAND_INT_LITERAL, ast->get_ival());
    ast->set_oprand(reg);
    Instruction *ins = new Instruction(HINS_LOAD_ICONST, *reg, *immval);
    this->code->add_instruction(ins);
  }
  // struct Operand* reg = new Operand(OPERAND_VREG, this->alloc_vreg());
 
  
  //Instruction *ins = new Instruction(HINS_LOAD_ICONST, *reg, immval);
  //this->code->add_instruction(ins);
}

// visit if statement
void CodeGenerator::visit_if(struct Node *ast) {
  struct Node *cond = ast->get_kid(0);
  struct Node *iftrue = ast->get_kid(1);

  // allocate a new code block label
  std::string label = alloc_label();
  struct Operand *out_label = new Operand(label);
  cond->set_oprand(out_label);

  visit_expression(cond);
  
  // jump if cond is not true
  Instruction *ins = new Instruction(get_jmp_ins(cond), *out_label);

  this->code->add_instruction(ins);
  visit_instructions(iftrue);
  
  this->code->define_label(label);
  
  // output an empty line
  Instruction *ins_empty = new Instruction(HINS_EMPTY);
  this->code->add_instruction(ins_empty);
}

// visit if-else statement
void CodeGenerator::visit_if_else(struct Node *ast){
  struct Node *cond = ast->get_kid(0);
  struct Node *iftrue = ast->get_kid(1);
  
  // needs two code block labels for if and else
  std::string label_0 = alloc_label();
  std::string label_1 = alloc_label();
  struct Operand *else_label = new Operand(label_0);
  struct Operand *out_label = new Operand(label_1);

  cond->set_oprand(else_label);

  visit_expression(cond);
  
  // if cond is not true
  Instruction *ins = new Instruction(get_jmp_ins(cond), *else_label);
  this->code->add_instruction(ins);
  
  visit_instructions(iftrue);
  
  cond->set_oprand(out_label);
  
  // jump after excution of current code block
  Instruction *ins_out = new Instruction(HINS_JUMP, *out_label);
  this->code->add_instruction(ins_out);
  this->code->define_label(label_0);

  struct Node *else_exp = ast->get_kid(2);
  visit_instructions(else_exp);
  
  this->code->define_label(label_1);
  
  // output an empty line
  Instruction *ins_empty = new Instruction(HINS_EMPTY);
  this->code->add_instruction(ins_empty);
}

// visit repeat statement
void CodeGenerator::visit_repeat(struct Node *ast){
  struct Node *cond = ast->get_kid(1);
  struct Node *iftrue = ast->get_kid(0);

  std::string label_0 = alloc_label();
  std::string label_1 = alloc_label();

  struct Operand *iftrue_label = new Operand(label_0);
  struct Operand *out_label = new Operand(label_1);
  cond->set_oprand(iftrue_label);
  
  // iteration body
  this->code->define_label(label_0);
  visit_instructions(iftrue);

  this->code->define_label(label_1);
  visit_expression(cond);
  
  // iterate util cond is false
  Instruction *ins;
  ins = new Instruction(get_jmp_ins(cond, 1), *iftrue_label);
  this->code->add_instruction(ins);
}

// visit while statement
void CodeGenerator::visit_while(struct Node *ast){
  struct Node *cond = ast->get_kid(0);
  struct Node *iftrue = ast->get_kid(1);

  std::string label_0 = alloc_label();
  std::string label_1 = alloc_label();

  struct Operand *iftrue_label = new Operand(label_0);
  struct Operand *out_label = new Operand(label_1);
  
  cond->set_oprand(iftrue_label);
  
  // jump to cond
  Instruction *ins = new Instruction(HINS_JUMP, *out_label);
  this->code->add_instruction(ins);

  this->code->define_label(label_0);

  visit_instructions(iftrue);

  this->code->define_label(label_1);

  visit_expression(cond);

  // jump to loop body if cond is true
  ins = new Instruction(get_jmp_ins(cond, 0), *iftrue_label);
  this->code->add_instruction(ins);

}

// visit a compare statement
void CodeGenerator::visit_compare(struct Node *ast, struct Operand *left, struct Operand *right){
  this->code->add_instruction(new Instruction(HINS_INT_COMPARE, *left, *right));
}


//////////////////////
// helper functions //

int CodeGenerator::alloc_vreg(){
  return this->vreg_count++;
}

int CodeGenerator::get_vreg_count(){
  return this->vreg_count;
}

std::string CodeGenerator::alloc_label(){
  std::string label = ".L";
  return label + std::to_string(this->label_count++);
}

// reset vreg allocation and record maximum vreg allocated
void CodeGenerator::rest_vreg(){
  this->max_vreg_count = (this->vreg_count > this->max_vreg_count) ? this->vreg_count : this->max_vreg_count;
  this->vreg_count = 0;
}

// get correct jump code
int CodeGenerator::get_jmp_ins(struct Node *ast, bool invert){
  int tag = node_get_tag(ast);
  
  // find correspoding jump command from map
  std::map<int, int>::iterator iter;  
  
  // return correspoding jump command 
  if(invert){
    iter = compare_to_jump_inverted.find(tag);
    if(iter != compare_to_jump_inverted.end())
      return iter->second;  
    else
      error_at_node(ast, "get_jmp_ins: Unknown relational operator");
  } else {
    iter = compare_to_jump.find(tag);
    if(iter != compare_to_jump.end())
      return iter->second;  
    else
      error_at_node(ast, "get_jmp_ins: Unknown relational operator");
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////
//                          API FUNCTIONS                             //
////////////////////////////////////////////////////////////////////////

struct CodeGenerator *highlevel_code_generator_create(struct Node *ast, SymbolTable *symtab){
  return new CodeGenerator(ast, symtab);
}

void generator_generate_highlevel(struct CodeGenerator *cgt){
  cgt->generate_code();
}

void generator_set_flag(struct CodeGenerator *cgt, char flag){
  cgt->set_flag(flag);
}

struct InstructionSequence *generator_get_highlevel(struct CodeGenerator *cgt){
  return cgt->get_code();
}

int get_vreg_offset(struct CodeGenerator *cgt){
  return cgt->get_vreg_count();
}