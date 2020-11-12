#ifndef CODEGEN_H
#define CODEGEN_H

#ifdef __cplusplus

#include "node.h"
#include "astvisitor.h"
#include "symtab.h"
#include "cfg.h"

class CodeGenerator;
extern "C" {
#endif

struct CodeGenerator *highlevel_code_generator_create(struct Node *ast, SymbolTable *symtab);

void generator_generate_highlevel(struct CodeGenerator *cgt);
struct InstructionSequence *generator_get_highlevel(struct CodeGenerator *cgt);

#ifdef __cplusplus
}
#endif



#endif // CODEGEN_H
