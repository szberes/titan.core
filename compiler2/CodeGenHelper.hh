///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef _Common_CodeGenHelper_HH
#define _Common_CodeGenHelper_HH

#include "ttcn3/compiler.h"
#include "map.hh"
#include "vector.hh"
#include "string.hh"
#include "ttcn3/AST_ttcn3.hh"
#include "Type.hh"

namespace Ttcn {
class Definition;
}

namespace Common {

class CodeGenHelper {
public:
  enum split_type {
    SPLIT_NONE,         ///< original code generation
    SPLIT_BY_KIND,      ///< place different kind of types in their own file
    SPLIT_BY_NAME,      ///< place all definitions/assignments in their own file
    SPLIT_BY_HEURISTICS ///< heuristic function will decide the structure
  };

private:
  struct generated_output_t {

    generated_output_t();
    ~generated_output_t();

    output_struct os;
    string modulename;
    string module_dispname;
    string filename;
    bool is_module;
    bool is_ttcn;
    bool has_circular_import;
  };

  static const char* const typetypemap[];

  typedef map<string, generated_output_t> output_map_t;

  output_map_t generated_code;
  split_type split_mode;

  struct module_names_t {
    string name;
    string dispname;
  };

  vector<module_names_t> modules;
  string current_module;

  static CodeGenHelper* instance;

public:
  CodeGenHelper();

  static CodeGenHelper& GetInstance();

  void set_split_mode(split_type st);
  bool set_split_mode(const char* type);
  split_type get_split_mode() const;

  void add_module(const string& name, const string& dispname, bool is_ttcn,
    bool has_circular_import);
  output_struct* get_outputstruct(const string& name);
  output_struct* get_outputstruct(Ttcn::Definition* def);
  output_struct* get_outputstruct(Type* type);
  output_struct* get_current_outputstruct();
  void finalize_generation(Type* type);

  void set_current_module(const string& name);

  void write_output();

  ~CodeGenHelper();

private:
  string get_key(Ttcn::Definition& def) const;
  string get_key(Type& type) const;
  static void transfer_value(char* &dst, char* &src);
};

}

#endif /* _Common_CodeGenHelper_HH */
