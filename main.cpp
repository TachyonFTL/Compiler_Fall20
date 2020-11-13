#include <cstdio>
#include <cstdlib>
#include <unistd.h> // for getopt
#include "node.h"
#include "symtab.h"
#include "util.h"
#include "grammar_symbols.h"
#include "ast.h"
#include "treeprint.h"
#include "context.h"
#include "codegen.h"
#include "highlevel.h"
#include "lowlevelgen.h"
#include "x86_64.h"

extern "C" {
int yyparse(void);
void lexer_set_source_file(const char *filename);
}

void print_usage(void) {
  err_fatal(
    "Usage: compiler [options] <filename>\n"
    "Options:\n"
    "   -p    print AST\n"
    "   -g    print AST as graph (DOT/graphviz)\n"
    "   -s    print symbol table information\n"
  );
}

enum Mode {
  PRINT_AST,
  PRINT_AST_GRAPH,
  PRINT_SYMBOL_TABLE,
  PRINT_HIGHLEVEL,
  COMPILE,
};

int main(int argc, char **argv) {
  extern FILE *yyin;
  extern struct Node *g_program;

  int mode = COMPILE;
  int opt;

  while ((opt = getopt(argc, argv, "pgsh")) != -1) {
    switch (opt) {
    case 'p':
      mode = PRINT_AST;
      break;

    case 'g':
      mode = PRINT_AST_GRAPH;
      break;

    case 's':
      mode = PRINT_SYMBOL_TABLE;
      break;

    case 'h':
      mode = PRINT_HIGHLEVEL;
      break;

    case '?':
      print_usage();
    }
  }

  if (optind >= argc) {
    print_usage();
  }

  const char *filename = argv[optind];

  yyin = fopen(filename, "r");
  if (!yyin) {
    err_fatal("Could not open input file \"%s\"\n", filename);
  }
  lexer_set_source_file(filename);

  yyparse();

  if (mode == PRINT_AST) {
    treeprint(g_program, ast_get_tag_name);
  } else if (mode == PRINT_AST_GRAPH) {
    ast_print_graph(g_program);
  } else {
    struct Context *ctx = context_create(g_program);
    if (mode == PRINT_SYMBOL_TABLE) {
      context_set_flag(ctx, 's'); // tell Context to print symbol table info
    }
    context_build_symtab(ctx);

    struct CodeGenerator *cgt = highlevel_code_generator_create(g_program, get_sym_tab(ctx));
    generator_generate_highlevel(cgt);
    struct InstructionSequence *code = generator_get_highlevel(cgt);
    
    if (mode == PRINT_HIGHLEVEL) {
      PrintHighLevelInstructionSequence print_ins(code);
      print_ins.print();
    } else {
      struct InstructionVisitor *lowlevel_generator = lowlevel_code_generator_create(code, get_var_offset(get_sym_tab(ctx)), get_vreg_offset(cgt));

      generator_generate_lowlevel(lowlevel_generator);
      struct InstructionSequence *lowlevel = generate_lowlevel(lowlevel_generator);
    
      PrintX86_64InstructionSequence print_ins(lowlevel);
      print_ins.print();
    }


  }

  return 0;
}
