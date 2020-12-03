#include "symbol.h"


Symbol::Symbol(std::string name, int kind, Type *type){
  sym_name = name;
  sym_kind = kind;
  sym_type = type;
}

Symbol::~Symbol(){

}

std::string Symbol::get_name(){
  return sym_name;
}

int Symbol::get_kind(){
  return sym_kind;
}

Type *Symbol::get_type(){
  return sym_type;
}

void Symbol::set_offset(int offset){
  this->offset = offset;
}

int Symbol::get_offset(){
  return this->offset;
}

std::string Symbol::get_kind_name(){
  return kind_names[sym_kind];
}

std::string Symbol::get_type_name(){
  return get_type()->get_type_name();
}

void Symbol::set_const_val(int val){
  this->const_val = val;
}

int Symbol::get_const_val(){
  return this->const_val;
}

void Symbol::set_operand(struct Operand* oprd){
  this->operand = oprd;
}

struct Operand* Symbol::get_operand(){
  return this->operand;
}
