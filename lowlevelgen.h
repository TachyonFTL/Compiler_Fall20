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

struct InstructionVisitor *lowlevel_code_generator_create(InstructionSequence *iseq, int var_offset, int vreg_count);

void generator_generate_lowlevel(struct InstructionVisitor *ivst);
struct InstructionSequence *generate_lowlevel(struct InstructionVisitor *ivst);

#ifdef __cplusplus
}
#endif

#endif // LOWLEVELGEN_H
