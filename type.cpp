#include "type.h"
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

// Base class for TYPE
///////////////////////////////////
Type::Type(std::string name, Type *base, int is_integral) {
  this->type_name = name;
  this->type_base = base;
  this->type_is_integral = is_integral;
  this->kind = BASE_TYPE;
}

Type::~Type() {

}

Type *Type::get_base_type() {
  return this->type_base;
}

std::string Type::get_type_name() {
  return this->type_name;
}

int Type::get_kind(){
  return this->kind;
}

int Type::is_integral(){
  return this->type_is_integral;
}

SymbolTable *Type::get_field(){
  return nullptr;
}

SymbolTable *Type::get_args(){
  return nullptr;
}

void Type::set_size(int size){
  this->size = size;
}

int Type::get_size(){
  return this->size;
}

Type *Type::get_return_type(){
  return nullptr;
}

// Derived class Array type
///////////////////////////////////
Array_type::Array_type(std::string name, Type *base, int size): Type(name, base) {
  this->type_size = size;
  this->kind = ARRAY_TYPE;
  this->size = this->type_size * base->get_size();
}

Array_type::~Array_type() {

}

int Array_type::get_kind(){
  return this->kind;
}

int Array_type::get_size(){
  return this->size;
}

std::string Array_type::get_type_name(){
  return std::string("ARRAY ") + std::to_string(this->type_size) + std::string(" OF ") + this->get_base_type()->get_type_name();
}
