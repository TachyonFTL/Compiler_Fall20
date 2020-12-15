#ifndef SYMTAB_H
#define SYMTAB_H

#include "symbol.h"
#include <map>
#include <string>
#include <vector>

// main calss for symbol table
class SymbolTable;

#ifdef __cplusplus

class SymbolTable {
private:
  SymbolTable *parent;
  std::vector<SymbolTable*> kids;

  int symtab_level;
  int current_offset = 0;
  
  // store the names of symbols and their corresponding Symbol
  std::map<std::string, Symbol*> name_to_symbol;
  std::vector<std::string> names;

public:
  SymbolTable(int level = -1); // initialize with level -1 for builtin type symbol table
  ~SymbolTable();
  
  // insert a symbol
  int insert_symbol(std::string name, Symbol* symbol, struct Node *node);
  int insert_builtin_symbol(std::string name, Symbol* symbol);
  
  // get a symbol from symbol table 
  Symbol *get_symbol_in_scope(std::string name); //get a symbol only from the current scope (for field reference)
  Symbol *get_symbol(std::string name);

  // get all symbols in current scope
  std::vector<std::string> get_all_names();

  // add/get a kid in current symbol table 
  void add_kid(SymbolTable *kid);
  SymbolTable *get_kid(unsigned int index);
  
  // get parent symbol table
  SymbolTable *get_parent();

  void print();
  int get_level();
  int get_current_offset();

private:
  // base function for insert a symbol
  int insert(const char* name, int kind, std::string type);
};

// Derived class Record type
///////////////////////////////////
class Record_type: public Type {
private:
  SymbolTable *fields; // fields are implemented via symbol table
  Type_kind kind;

public:
  Record_type(std::string name, SymbolTable *fields);
  ~Record_type();

  // get this the string representation of this type name
  std::string get_type_name();

  // get the fields
  SymbolTable *get_field();

  // get the kind of this type (RECORD_TYPE)
  int get_kind();
  int get_size();
  
};

// Derived class Function type
///////////////////////////////////
class Function_type: public Type {
private:
  SymbolTable *args; // similar to Record type
  Type_kind kind;

public:
  Function_type(std::string name, SymbolTable *args);
  ~Function_type();

  // get this the string representation of this type name
  std::string get_type_name();

  // get the arguments
  SymbolTable *get_args();

  // get the kind of this type (FUNC_TYPE)
  int get_kind();
  
};
extern "C" {
#endif

int get_var_offset(SymbolTable *symtab);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif