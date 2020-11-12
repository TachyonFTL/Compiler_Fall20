#include "symtab.h"
#include "grammar_symbols.h"
#include "iostream"
#include "node.h"
#include "symbol.h"
#include "type.h"

SymbolTable::SymbolTable(int level) {
  this->symtab_level = level;
}

int SymbolTable::insert(const char* name, int kind, std::string type) {
  Symbol *target_type = get_symbol(type);

  // insert a symbol to symbol table if the type is defined and is a valid type
  if(target_type != nullptr && target_type->get_kind() == KIND_TYPE){
    // insert a symbol to symbol table if it is not in the table
    if(name_to_symbol.find(name) == name_to_symbol.end()){
      this->names.push_back(name);
      Symbol new_symbol = Symbol(name, kind, target_type->get_type());
      name_to_symbol.insert({name, &new_symbol});
      return 1;
    } 
  }

  return 0;
}

int SymbolTable::insert_symbol(std::string name, Symbol* symbol, struct Node *node) {
  // insert a symbol to symbol table if it is not in the table
  if(name_to_symbol.find(name) == name_to_symbol.end()){
    this->names.push_back(name);

    if(symbol->get_kind() == KIND_VAR){
      symbol->set_offset(this->current_offset);
      this->current_offset += symbol->get_type()->get_size();
    }

    name_to_symbol.insert({name, symbol});
    return 1;
  }
  error_at_node(node, "Redeclaration of a same name");
  return 0;
}

int SymbolTable::insert_builtin_symbol(std::string name, Symbol* symbol) {
  return insert_symbol(name, symbol, nullptr);
}

Symbol *SymbolTable::get_symbol_in_scope(std::string name) {
  // search a given symbol in current scope (table)
  std::map<std::string, Symbol*>::iterator it = name_to_symbol.find(name);
  if (it != name_to_symbol.end()){
    return it->second;
  } else {
    return nullptr;
  }
}

Symbol *SymbolTable::get_symbol(std::string name) {
  // search a given symbol in current scope (table)
  Symbol *sym = get_symbol_in_scope(name); 

  // search a given symbol in outer scope (table) if outer scope existed
  if (sym != nullptr){
    return sym;
  } else if (parent != nullptr){
    return parent->get_symbol(name);
  }
  return nullptr;
}

// print symboltable (UNUSED)
void SymbolTable::print(){

  std::vector<std::string >::iterator it;
  for(it = this->names.begin(); it != this->names.end(); ++it){
    std::cout <<  *it << name_to_symbol[*it]->get_kind() << ' ' << name_to_symbol[*it]->get_type()->get_kind() << std::endl;
    if (name_to_symbol[*it]->get_kind() == KIND_TYPE && name_to_symbol[*it]->get_type()->get_kind() == RECORD_TYPE){
      
      name_to_symbol[*it]->get_type()->get_field()->print();
    }
    std::cout << symtab_level << ',' << name_to_symbol[*it]->get_kind_name() << ',' \
    << *it << ',' << name_to_symbol[*it]->get_type_name() << std::endl; 
  }
}

void SymbolTable::add_kid(SymbolTable *kid) {
  this->kids.push_back(kid);
  kid->parent = this;
}

SymbolTable *SymbolTable::get_kid(unsigned int index) {
  return this->kids.at(unsigned(index));
}

std::vector<std::string> SymbolTable::get_all_names() {
  return this->names;
}

int SymbolTable::get_level() {
  return this->symtab_level;
}

int SymbolTable::get_current_offset() {
  return this->current_offset;
}

SymbolTable *SymbolTable::get_parent() {
  return this->parent;
}

// Derived class Record type
///////////////////////////////////
Record_type::Record_type(std::string name, SymbolTable * fields): Type(name) {
  this->fields = fields;
  this->kind = RECORD_TYPE;
}

Record_type::~Record_type() {

}

std::string Record_type::get_type_name(){
  std::string ret_string = std::string("RECORD (");
  std::vector<std::string> names = this->fields->get_all_names();
  std::vector<std::string>::iterator it;
  
  // iterate fields to get its string representation
  int i = 0;
  for(it = names.begin(); it != names.end(); it++, i++){
    if(i != 0){
      ret_string.push_back(' ');
      ret_string.push_back('x');
      ret_string.push_back(' ');
    }
    ret_string += this->fields->get_symbol(*it)->get_type_name();
  }

  ret_string.push_back(')');
  return ret_string;
}

SymbolTable *Record_type::get_field() { 
  return this->fields;
}

int Record_type::get_kind() {
  return this->kind;
}

int Record_type::get_size() {
  return this->fields->get_current_offset();
}

