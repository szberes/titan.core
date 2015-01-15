///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>

#include "../../common/memory.h"
#include "../datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* for generating output files */

  typedef struct output_struct_t {
    struct {
      char *includes;
      char *class_decls;
      char *typedefs;
      char *class_defs;
      char *function_prototypes;
      char *global_vars;
      char *testport_includes;
    } header;
    struct {
      char *includes;
      char *static_function_prototypes;
      char *static_conversion_function_prototypes;
      char *string_literals;
      char *class_defs;
      char *global_vars;
      char *methods;
      char *function_bodies;
      char *static_function_bodies;
      char *static_conversion_function_bodies;
    } source;
    struct {
      char *pre_init;  /**< Code for pre_init_module() */
      char *post_init; /**< Code for post_init_module() */
      char *set_param; /**< Code for set_module_param() */
      char *log_param; /**< Code for log_module_param() */
      char *init_comp; /**< Code for init_comp_type() */
      char *start;     /**< Code for start_ptc_function() */
      char *control;   /**< Code for module_control_part() */
    } functions;
  } output_struct;

  typedef struct expression_struct_t {
    char *preamble;
    char *expr;
    char *postamble;
  } expression_struct;

  /* for global and local constant definitions */

  typedef struct const_def_t {
    char *decl;
    char *def;
    /* char *cdef; */
    char *init;
  } const_def;

  /* Commonly used functions and variables */

  extern const char *infile;

  extern void write_output(output_struct *output, const char *module_name,
    const char *module_dispname, const char *filename_suffix, boolean is_ttcn,
    boolean has_circular_import, boolean is_module);
  extern void report_nof_updated_files(void);
  
  extern FILE *open_output_file(const char *file_name, boolean *is_temporary);
  extern void close_output_file(const char *file_name, FILE *fp,
    boolean is_temporary, size_t skip_lines);

#ifdef __cplusplus
}
#endif

extern int ttcn3_parse_file(const char* filename, boolean generate_code);

#endif
