#ifndef CODEGEN_H
#define CODEGEN_H

class CodeGenerator;

#ifdef __cplusplus

#include "node.h"
#include "astvisitor.h"
#include "symtab.h"
#include "cfg.h"


extern "C" {
#endif

// API functions
// create generator obj
struct CodeGenerator *highlevel_code_generator_create(struct Node *ast, SymbolTable *symtab);

// generate high-level code
void generator_generate_highlevel(struct CodeGenerator *cgt);

// retrive high-level code
struct InstructionSequence *generator_get_highlevel(struct CodeGenerator *cgt);

// get number of vreg allcated (for low-level code generator)
int get_vreg_offset(struct CodeGenerator *cgt);

void generator_set_flag(struct CodeGenerator *cgt, char flag);
#ifdef __cplusplus
}
#endif



#endif // CODEGEN_H
