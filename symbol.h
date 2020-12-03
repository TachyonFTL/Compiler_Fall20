#ifndef SYMBOL_H
#define SYMBOL_H

#include "cfg.h"
#include "node.h"
#include "type.h"

enum Symkind {
  KIND_VAR = 0,
  KIND_TYPE,
  KIND_CONST,
};

const std::vector<std::string> kind_names = {"VAR", "TYPE", "CONST"}; 

// main class for Symbol
class Symbol {
private:
  std::string sym_name;
  int sym_kind;
  Type *sym_type;
  int const_val; // for constant type only
  int offset = -1;
  struct Operand* operand = nullptr;

public:
  Symbol(std::string name, int kind, Type *type);
  ~Symbol();
  
  std::string get_name();
  int get_kind();
  Type *get_type();

  std::string get_kind_name();
  std::string get_type_name();

  // set/get val of the symbol if it is a const type
  void set_const_val(int val);
  int get_const_val();

  void set_offset(int offset);
  int get_offset();

  void set_operand(struct Operand* oprd);
  struct Operand* get_operand();

};

#endif