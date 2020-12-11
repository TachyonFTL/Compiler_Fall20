#ifndef LOWLEVELGEN_H
#define LOWLEVELGEN_H

#ifdef __cplusplus

#include "node.h"
#include "astvisitor.h"
#include "symtab.h"
#include "cfg.h"

class InstructionVisitor;

extern "C" {
#endif

// API functions
// create a low-level code translator obj
struct InstructionVisitor *lowlevel_code_generator_create(InstructionSequence *iseq, int var_offset, int vreg_count);

// generate low-level code
void generator_generate_lowlevel(struct InstructionVisitor *ivst);

// get the generated low-level code 
struct InstructionSequence *generate_lowlevel(struct InstructionVisitor *ivst);

void lowlevel_generator_set_flag(struct InstructionVisitor *ivst, char flag);
#ifdef __cplusplus
}
#endif

#endif // LOWLEVELGEN_H
