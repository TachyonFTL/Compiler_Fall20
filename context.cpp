#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <ostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "grammar_symbols.h"
#include "util.h"
#include "cpputil.h"
#include "node.h"
#include "type.h"
#include "symbol.h"
#include "symtab.h"
#include "ast.h"
#include "astvisitor.h"
#include "context.h"
#include "tuple"

////////////////////////////////////////////////////////////////////////
// Classes
////////////////////////////////////////////////////////////////////////

struct Context {
private:
  struct Node *root = nullptr;
  SymbolTable *symtable = nullptr;
  SymbolTableBuilder *symtab_builder;

  char flag = 'n';  // print flag

public:
  Context(struct Node *ast);
  ~Context();

  void set_flag(char flag);
  void build_symtab();

  struct SymbolTable *get_sym_tab();

};

typedef std::vector<std::tuple<struct Node*, std::string>> tuple_node_string;

////////////////////////////////////////////////////////////////////////
// Helper class SymbolTableBuilder to visit the AST
class SymbolTableBuilder: public ASTVisitor  {
private:
  struct Node *root = nullptr;
  SymbolTable *symtable = nullptr;
  SymbolTable *current_table = nullptr;

  char flag = 'n';  // print flag

public:
  SymbolTableBuilder(struct Node *ast_declarations, SymbolTable *symtab);
  ~SymbolTableBuilder();

  void build_symtab();
  void set_flag(char flag);

private:
  void visit_constant_def(struct Node *ast);
  void visit_type_def(struct Node *ast);
  void visit_var_def(struct Node *ast);
  void visit_write(struct Node *ast);
  void visit_read(struct Node *ast);
  void visit_assign(struct Node *ast);
  void visit_field_ref(struct Node *ast);
  void visit_array_element_ref(struct Node *ast);

  // get the string representations of identifiers
  tuple_node_string get_identifier_list(struct Node *ast);
  
  // evaluate type refernces
  Type *eval_expr_type(struct Node *ast);
  std::vector<Type*> eval_expr_list_type(struct Node *ast);
  Type *eval_var_ref_type(struct Node *ast);
  Type *eval_array_ref_type(struct Node *ast);
  Type *eval_named_type(struct Node *ast);
  Type *eval_array_type(struct Node *ast, std::string name);
  Type *eval_record_type(struct Node *ast, std::string name);

  // evaluate constant expressions
  int eval_const_expr(struct Node *ast);
  int eval_const_int_literal(struct Node *ast);
  int eval_const_var_ref(struct Node *ast, int *p);
  
};

// print sysmbo if flag is set to 's'
inline void print_symbol(char flag, int level, Symbol *sym) {
  if (flag == 's') {
    std::cout << level << ',' << sym->get_kind_name() << ',' \
    << sym->get_name() << ',' << sym->get_type_name() << ',' << sym->get_offset() << std::endl; 
  }
}

////////////////////////////////////////////////////////////////////////
// Context class implementation
////////////////////////////////////////////////////////////////////////

Context::Context(struct Node *ast) {
  this->root = ast;
  SymbolTable *builtin_table = new SymbolTable(); // symbol table to store built-in types (CHAR and INTEGER)
  SymbolTable *table = new SymbolTable(0); // program symbol table
  builtin_table->add_kid(table);

  // built-in types (CHAR and INTEGER)
  Type *BASIC_TYPE_INTEGER = new Type("INTEGER", nullptr, 1);
  Type *BASIC_TYPE_CHAR = new Type("CHAR", nullptr, 1);
  Symbol *SYM_TYPE_INTEGER = new Symbol("INTEGER", KIND_TYPE, BASIC_TYPE_INTEGER);
  Symbol *SYM_TYPE_CHAR = new Symbol("CHAR", KIND_TYPE, BASIC_TYPE_CHAR);

  builtin_table->insert_builtin_symbol("INTEGER", SYM_TYPE_INTEGER);
  builtin_table->insert_builtin_symbol("CHAR", SYM_TYPE_CHAR);

  this->symtable = table;
  this->symtab_builder = new SymbolTableBuilder(this->root, this->symtable);
}

Context::~Context() {
}

void Context::set_flag(char flag) {
  this->flag = flag;
  this->symtab_builder->set_flag(flag);
}

void Context::build_symtab() {
  this->symtab_builder->build_symtab();
}

struct SymbolTable *Context::get_sym_tab(){
  return this->symtable;
}

////////////////////////////////////////////////////////////////////////
// SymbolTableBuilder class implementation
//////////////////////////////////////////////////////////////////////// 

SymbolTableBuilder::SymbolTableBuilder(struct Node *ast_program, SymbolTable *symtab) {
  this->root = ast_program;
  this->symtable = symtab;
  this->current_table = this->symtable;
}

SymbolTableBuilder::~SymbolTableBuilder() {

}


void SymbolTableBuilder::build_symtab() {
  return visit_program(this->root);
}

// evaluate constant def
void SymbolTableBuilder::visit_constant_def(struct Node *ast) {
  std::string name = node_get_str(node_get_kid(ast, 0));
  Type *type = eval_expr_type(node_get_kid(ast, 1));
  Symbol *constant = new Symbol(name, KIND_CONST, type);

  ast->set_type(type);
  // evaluate constant reference and store in symtable
  constant->set_const_val(eval_const_expr(node_get_kid(ast, 1)));
  this->current_table->insert_symbol(name, constant, node_get_kid(ast, 0));

  print_symbol(this->flag, this->current_table->get_level(), constant);
}

// evaluate type def
void SymbolTableBuilder::visit_type_def(struct Node *ast) {
  struct Node *type = node_get_kid(ast, 1);
  Symbol *sym;
  std::string name = node_get_str(node_get_kid(ast, 0));

  switch (node_get_tag(type)) {
    case AST_NAMED_TYPE: 
        sym = new Symbol(node_get_str(node_get_kid(ast, 0)), KIND_TYPE, eval_named_type(type));
        break;

    case AST_ARRAY_TYPE: 
        sym = new Symbol(name, KIND_TYPE, eval_array_type(type, name));
        break;

    case AST_RECORD_TYPE: 
        sym = new Symbol(name, KIND_TYPE, eval_record_type(type, name));
        break;
  }

  this->current_table->insert_symbol(name, sym, node_get_kid(ast, 0));
  print_symbol(this->flag, this->current_table->get_level(), sym);
}

void SymbolTableBuilder::visit_field_ref(struct Node *ast){
  Type *record = eval_expr_type(node_get_kid(ast, 0));

  // check if the left part is a valid record type var
  if (record != nullptr){
    if (record->get_kind() == RECORD_TYPE) {
      // check if the field refernce is valid             
      Symbol *a_field = record->get_field()->get_symbol_in_scope(node_get_str(node_get_kid(ast, 1)));
      if (a_field != nullptr) {
      } else {
        error_at_node(node_get_kid(ast, 1), "Undefined field");
      }
    }
  } else {
    error_at_node(node_get_kid(ast, 0), "Not a record type");
  }
  ast->set_type(record); 
}

void SymbolTableBuilder::visit_array_element_ref(struct Node *ast){
  int size = eval_const_expr(node_get_kid(ast, 0));
  struct Node *base_type = node_get_kid(ast, 1);
  Type *named_type;

  // check if it is a valid array refernece and get base type
  if (node_get_tag(base_type) == AST_ARRAY_TYPE){
    // base type is a multi-dim array
    named_type = eval_array_type(base_type, "NONE");
  } else {
    named_type = eval_named_type(base_type);
  }
}

// evaluate named type def
Type *SymbolTableBuilder::eval_named_type(struct Node *ast) {
  // get a named type from symtable
  Symbol *named_type = this->current_table->get_symbol(node_get_str(node_get_kid(ast, 0)));

  // check if it is a valid refernece
  if(named_type != nullptr){
    if(named_type->get_kind() != KIND_TYPE){
      error_at_node(ast, "Not a Type");
    }
    return named_type->get_type();
  } else {
    error_at_node(ast, "Undefined Named Type");
  }
  return nullptr;
}

// evaluate array type def
Type *SymbolTableBuilder::eval_array_type(struct Node *ast, std::string name) {
  int size = eval_const_expr(node_get_kid(ast, 0));
  struct Node *base_type = node_get_kid(ast, 1);
  Type *named_type;

  // check if it is a valid array refernece and get base type
  if (node_get_tag(base_type) == AST_ARRAY_TYPE){
    // base type is a multi-dim array
    named_type = eval_array_type(base_type, "NONE");
  } else {
    named_type = eval_named_type(base_type);
  }

  // check if base type is valid
  if(named_type != nullptr){
    Type *new_type = new Array_type(name, named_type, size);
    return new_type;
  } else {
    error_at_node(ast, "Undefined Named Type");
  }
  return nullptr;
}

// evaluate record type def
Type *SymbolTableBuilder::eval_record_type(struct Node *ast, std::string name) {
  // build a new symtable for record fields
  SymbolTable *fields = new SymbolTable(this->current_table->get_level() + 1);
  this->current_table->add_kid(fields);
  this->current_table = fields;

  visit_var_declarations(node_get_kid(ast, 0));

  // set symtable back to the parent level
  this->current_table = this->current_table->get_parent();

  Type *new_type = new Record_type(name, fields);
  
  return new_type;
}

// evaluate a constant expression reference
int SymbolTableBuilder::eval_const_expr(struct Node *ast) {
  switch (node_get_tag(ast)) {
    case AST_INT_LITERAL:
      return std::stoi(node_get_str(ast));

    case AST_VAR_REF:
      int p;
      eval_const_var_ref(ast, &p);
      return p;

    case AST_NEGATE:
      return -eval_const_expr(node_get_kid(ast, 0));
    
    case AST_ADD:
      return eval_const_expr(node_get_kid(ast, 0)) + eval_const_expr(node_get_kid(ast, 1));

    case AST_SUBTRACT:
      return eval_const_expr(node_get_kid(ast, 0)) - eval_const_expr(node_get_kid(ast, 1));

    case AST_MULTIPLY:
      return eval_const_expr(node_get_kid(ast, 0)) * eval_const_expr(node_get_kid(ast, 1));

    case AST_DIVIDE:
      return eval_const_expr(node_get_kid(ast, 0)) / eval_const_expr(node_get_kid(ast, 1));

    case AST_MODULUS:
      return eval_const_expr(node_get_kid(ast, 0)) % eval_const_expr(node_get_kid(ast, 1));

    default:
      error_at_node(ast, "Unrecognized expr");
      return 0;
  }
}

// evaluate a constant identifier reference
int SymbolTableBuilder::eval_const_var_ref(struct Node *ast, int *p) {
  std::string name = node_get_str(ast);
  Symbol *sym = this->current_table->get_symbol(name);

  if (sym == nullptr) {
    error_at_node(ast, "Undefined var reference");
    return -1;
  }
  if (sym->get_kind() == KIND_CONST) {
    *p = sym->get_const_val();
    return 1;
  } else {
    error_at_node(ast, "Non Const val encountered during const eval");
  }
  return 0;
}

// evaluate the type of a given expr
Type *SymbolTableBuilder::eval_expr_type(struct Node *ast) {
  Symbol *sym;
  switch (node_get_tag(ast)) {
    case AST_INT_LITERAL:
      sym = this->current_table->get_symbol("INTEGER");
      if (this->flag == 'o') {
        ast->set_const();
      }
      return sym->get_type();

    case AST_NEGATE:
      return eval_expr_type(node_get_kid(ast, 0));

    case AST_VAR_REF:
      return eval_var_ref_type(ast);

    case AST_ARRAY_ELEMENT_REF:
      return eval_array_ref_type(ast);

    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MODULUS:
    case AST_DIVIDE:
    case AST_MULTIPLY:
      {
        Type *left = eval_expr_type(node_get_kid(ast, 0));
        Type *right = eval_expr_type(node_get_kid(ast, 1));

        // left/right var must be integral in an arithmatica expr 
        if (left != nullptr && right != nullptr && left->is_integral() == 1 && right->is_integral()){
          return this->current_table->get_symbol("INTEGER")->get_type();;
        } else {
          error_at_node(ast, "Invalid arithmatic types");
        }
      }
    case AST_FIELD_REF:
      {
        Type *record = eval_expr_type(node_get_kid(ast, 0));
        ast->set_type(record); 
        // std::cout << ast->get_type() << std::endl;
        // check if the left part is a valid record type var
        if (record != nullptr){
          if (record->get_kind() == RECORD_TYPE) {
            // check if the field refernce is valid             
            Symbol *a_field = record->get_field()->get_symbol_in_scope(node_get_str(node_get_kid(ast, 1)));

            if (a_field != nullptr) {
              return a_field->get_type();
            } else {
              error_at_node(node_get_kid(ast, 1), "Undefined field");
            }
          }
        } else {
          error_at_node(node_get_kid(ast, 0), "Not a record type");
        }

      }
    default:
      error_at_node(ast, "Unrecognized expr");
      return nullptr;
  }
}

// evaluate assignemnt statement
void SymbolTableBuilder::visit_assign(struct Node *ast) {
  Type *left = eval_expr_type(node_get_kid(ast, 0));
  Type *right = eval_expr_type(node_get_kid(ast, 1));

  // check if var and expr are valid 
  if (left != nullptr && right != nullptr){
    if (left->get_type_name() == right->get_type_name()) {
    } else if (left->get_kind() == ARRAY_TYPE && left->get_base_type()->get_type_name() == right->get_type_name()) {
    } else {
      error_at_node(ast, "Type mismatch");
    }     
  } else {
    error_at_node(ast, "Invalid types");
  }
}

// evaluate the type of a var refernece
Type *SymbolTableBuilder::eval_var_ref_type(struct Node *ast) {
  std::string name = node_get_str(ast);
  Symbol *sym = this->current_table->get_symbol(name);
  if (sym == nullptr) {
    error_at_node(ast, "Undefined var reference");
    return nullptr;
  } else if (sym->get_kind() == KIND_TYPE) {
    error_at_node(ast, "Cannot reference a type");
    return nullptr;
  }

  if (this->flag == 'o') {
    if (sym->get_kind() == KIND_CONST){
      // std::cout << name << std::endl;
      ast->set_const();
    }
  }

  return sym->get_type();
}

// evaluate the type of a array var refernece
Type *SymbolTableBuilder::eval_array_ref_type(struct Node *ast) {
  Type *array = eval_expr_type(node_get_kid(ast, 0));

  // check if the referenced array is defined 
  if (array == nullptr) {
    error_at_node(ast, "Undefined array reference");
    return nullptr;
  } else if (array->get_kind() != ARRAY_TYPE) {
    error_at_node(ast, "Not an array type");
    return nullptr;
  }

  // Annotate Type for Nodes
  ast->set_type(array); 

  // get array subscripts
  std::vector<Type*> arg_list = eval_expr_list_type(node_get_kid(ast, 1));
  std::vector<Type*>::iterator it;

  struct Node *current = ast;
  // evaluate the array reference type; error if the number of arguments is greater than array dim
  for(it = arg_list.begin(); it != arg_list.end(); ++it){
    if (array->get_base_type() != nullptr) {
      array = array->get_base_type();
    } else {
      error_at_node(node_get_kid(ast, 1), "Too many arguments for array");
    }
  }
  return array;
}

// evaluate the types of an array subscript reference
std::vector<Type*> SymbolTableBuilder::eval_expr_list_type(struct Node *ast) {
  std::vector<Type*> arg_list;
  Type *list_elem_type = eval_expr_type(node_get_kid(ast, 0));

  // check if it is a valid subscript
  if (list_elem_type != nullptr && list_elem_type->is_integral() == 1) {
    arg_list.push_back(list_elem_type);

    // if more than one argument, recursive call to add them to the list
    if (node_get_num_kids(ast) > 1) {
      std::vector<Type*> rem_arg_list = eval_expr_list_type(node_get_kid(ast, 1));
      arg_list.insert(arg_list.end(), rem_arg_list.begin(), rem_arg_list.end() );
    } 
  } else {
    error_at_node(node_get_kid(ast, 0), "Invalid arrary reference");
  }
  return arg_list;

}

// evaluate the names of an identifier list; return a list of tuples of {node in ast, identifier name}
// node in ast is for tracing the error source
tuple_node_string SymbolTableBuilder::get_identifier_list(struct Node *ast) {
  tuple_node_string identifiers;
  identifiers.push_back({node_get_kid(ast, 0), node_get_str(node_get_kid(ast, 0))});

  // if more than one argument, recursive call to add them to the list
  if (node_get_num_kids(ast) > 1){
    tuple_node_string rem_idents = get_identifier_list(node_get_kid(ast, 1));
    identifiers.insert(identifiers.end(), rem_idents.begin(), rem_idents.end() );
  }
  return identifiers;
}

// evaluate var def
void SymbolTableBuilder::visit_var_def(struct Node *ast){
  tuple_node_string identifiers = get_identifier_list(node_get_kid(ast, 0));
  struct Node *type = node_get_kid(ast, 1);
  Type *ret_type;

  // get var type
  switch (node_get_tag(type)) {
    case AST_NAMED_TYPE:
        ret_type = eval_named_type(type);
        break;
    case AST_ARRAY_TYPE:
        ret_type = eval_array_type(type, "NONE");
        break;
    case AST_RECORD_TYPE:
        ret_type = eval_record_type(type, "NONE");
        break;
  }

  // iterate the identifier list and add identifiers to symtable
  tuple_node_string::iterator it;
  for(it = identifiers.begin(); it != identifiers.end(); it++){
    auto [node, name] = *it;
    Symbol *sym = new Symbol(name, KIND_VAR, ret_type);
    this->current_table->insert_symbol(name, sym, node);

    print_symbol(this->flag, this->current_table->get_level(), sym);
  }
}

void SymbolTableBuilder::set_flag(char flag) {
  this->flag = flag;
}

// evalute write statement
void SymbolTableBuilder::visit_write(struct Node *ast) {
  Type *type = eval_expr_type(node_get_kid(ast, 0));
  
  if (type != nullptr) {
    if (type->is_integral() == 1) {
    } else if (type->get_kind() == ARRAY_TYPE && type->get_base_type()->get_type_name() == "CHAR") {
    } else {
      error_at_node(ast, "Write statement encounters non-writable expr");
    }
  }
}

// evalute read statement
void SymbolTableBuilder::visit_read(struct Node *ast) {
  Type *type = eval_expr_type(node_get_kid(ast, 0));
  
  if (type != nullptr) {
    if (type->is_integral() == 1) {
    } else if (type->get_kind() == ARRAY_TYPE && type->get_base_type()->get_type_name() == "CHAR") {
    } else {
      error_at_node(ast, "Read statement encounters non-writable expr");
    }
  }
}

////////////////////////////////////////////////////////////////////////
// Context API functions
////////////////////////////////////////////////////////////////////////

struct Context *context_create(struct Node *ast) {
  return new Context(ast);
}

void context_destroy(struct Context *ctx) {
  delete ctx;
}

void context_set_flag(struct Context *ctx, char flag) {
  ctx->set_flag(flag);
}

void context_build_symtab(struct Context *ctx) {
  ctx->build_symtab();
}

void context_check_types(struct Context *ctx) {
}

struct SymbolTable *get_sym_tab(struct Context *ctx){
  return ctx->get_sym_tab();
}