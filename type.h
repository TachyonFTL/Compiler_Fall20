#ifndef TYPE_H
#define TYPE_H

#include "node.h"
#include <string>
#include <vector>

enum Type_kind {
  BASE_TYPE = 0,
  ARRAY_TYPE,
  RECORD_TYPE,
  FUNC_TYPE
};

// Base class for TYPE
///////////////////////////////////
struct Type {
private:
  std::string type_name;
  Type *type_base; // base type (reserved for array type)
  Type_kind kind;
  int type_is_integral = 0; 
  int size = 8;

public:

  Type(std::string name, Type *base = nullptr, int is_integral = 0);
  virtual ~Type();

  // get base type (reserved for array type)
  virtual Type *get_base_type();
  
  // get the string representation of this type
  virtual std::string get_type_name();

  // get fields (reserved for array type)
  virtual SymbolTable *get_field();

  virtual int get_kind();
  virtual int is_integral();

  virtual void set_size(int size);
  virtual int get_size();

};

// Derived class Array type
///////////////////////////////////
class Array_type: public Type {
private:
  int type_size; // record array length
  int size;  // actual size
  Type_kind kind;

public:
  Array_type(std::string name, Type *base, int size);
  ~Array_type();
  
  int get_kind();
  int get_size();
  
  std::string get_type_name();
  
};



#endif