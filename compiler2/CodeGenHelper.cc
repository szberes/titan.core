///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "CodeGenHelper.hh"
#include "Code.hh"
#include "error.h"
#include "main.hh"
#include <cstdio>
#include <cstring>

namespace Common {

CodeGenHelper* CodeGenHelper::instance = 0;

CodeGenHelper::generated_output_t::generated_output_t() :
  is_module(false),
  is_ttcn(true),
  has_circular_import(false)
{
  Code::init_output(&os);
}

CodeGenHelper::generated_output_t::~generated_output_t() {
  Code::free_output(&os);
}

// from Type.cc
const char* const CodeGenHelper::typetypemap[] = {
  "", /**< undefined */
  "", /**< erroneous (e.g. nonexistent reference) */
  "", /**< null (ASN.1) */
  "", /**< boolean */
  "", /**< integer */
  "", /**< integer / ASN */
  "", /**< real/float */
  "", /**< enumerated / ASN */
  "", /**< enumerated / TTCN */
  "", /**< bitstring */
  "", /**< bitstring */
  "", /**< hexstring (TTCN-3) */
  "", /**< octetstring */
  "", /**< charstring (TTCN-3) */
  "", /**< universal charstring (TTCN-3) */
  "", /**< UTF8String (ASN.1) */
  "", /**< NumericString (ASN.1) */
  "", /**< PrintableString (ASN.1) */
  "", /**< TeletexString (ASN.1) */
  "", /**< VideotexString (ASN.1) */
  "", /**< IA5String (ASN.1) */
  "", /**< GraphicString (ASN.1) */
  "", /**< VisibleString (ASN.1) */
  "", /**< GeneralString (ASN.1) */
  "",  /**< UniversalString (ASN.1) */
  "", /**< BMPString (ASN.1) */
  "", /**< UnrestrictedCharacterString (ASN.1) */
  "", /**< UTCTime (ASN.1) */
  "", /**< GeneralizedTime (ASN.1) */
  "", /** Object descriptor, a kind of string (ASN.1) */
  "", /**< object identifier */
  "", /**< relative OID (ASN.1) */
  "_union", /**< choice /ASN, uses u.secho */
  "_union", /**< union /TTCN, uses u.secho */
  "_seqof", /**< sequence (record) of */
  "_setof", /**< set of */
  "_seq", /**< sequence /ASN, uses u.secho */
  "_seq", /**< record /TTCN, uses u.secho */
  "_set", /**< set /ASN, uses u.secho */
  "_set", /**< set /TTCN, uses u.secho */
  "", /**< ObjectClassFieldType (ASN.1) */
  "", /**< open type (ASN.1) */
  "", /**< ANY (deprecated ASN.1) */
  "", /**< %EXTERNAL (ASN.1) */
  "", /**< EMBEDDED PDV (ASN.1) */
  "", /**< referenced */
  "", /**< special referenced (by pointer, not by name) */
  "", /**< selection type (ASN.1) */
  "", /**< verdict type (TTCN-3) */
  "", /**< port type (TTCN-3) */
  "", /**< component type (TTCN-3) */
  "", /**< address type (TTCN-3) */
  "", /**< default type (TTCN-3) */
  "", /**< array (TTCN-3), uses u.array */
  "", /**< signature (TTCN-3) */
  "", /**< function reference (TTCN-3) */
  "", /**< altstep reference (TTCN-3) */
  "", /**< testcase reference (TTCN-3) */
  "", /**< anytype (TTCN-3) */
  0
};

CodeGenHelper::CodeGenHelper() :
  split_mode(SPLIT_NONE)
{
  if (instance != 0)
    FATAL_ERROR("Attempted to create a second code generator.");
  instance = this;
}

CodeGenHelper& CodeGenHelper::GetInstance() {
  if (instance == 0)
    FATAL_ERROR("Trying to access to the already destroyed code generator.");
  return *instance;
}

void CodeGenHelper::set_split_mode(split_type st) {
  split_mode = st;
}

bool CodeGenHelper::set_split_mode(const char* type) {
  if (strcmp(type, "none") == 0)
    split_mode = SPLIT_NONE;
  else if (strcmp(type, "type") == 0)
    split_mode = SPLIT_BY_KIND;
  else
    return false;
  return true;
}

CodeGenHelper::split_type CodeGenHelper::get_split_mode() const {
  return split_mode;
}

void CodeGenHelper::add_module(const string& name, const string& dispname,
  bool is_ttcn, bool has_circular_import) {
  generated_output_t* go = new generated_output_t;
  go->filename.clear();
  go->modulename = name;
  go->module_dispname = dispname;
  go->is_module = true;
  go->is_ttcn = is_ttcn;
  go->has_circular_import = has_circular_import;
  generated_code.add(dispname, go);
  module_names_t* mod_names = new module_names_t;
  mod_names->name = name;
  mod_names->dispname = dispname;
  modules.add(mod_names);
}

output_struct* CodeGenHelper::get_outputstruct(const string& name) {
  return &generated_code[name]->os;
}

void CodeGenHelper::set_current_module(const string& name) {
  current_module = name;
}

output_struct* CodeGenHelper::get_outputstruct(Ttcn::Definition* def) {
  string key = get_key(*def);
  const string& new_name = current_module + key;
  if (!generated_code.has_key(new_name)) {
    generated_output_t* go = new generated_output_t;
    go->filename = key;
    go->modulename = generated_code[current_module]->modulename;
    go->module_dispname = generated_code[current_module]->module_dispname;
    generated_code.add(new_name, go);
    go->os.source.includes = mprintf("\n#include \"%s.hh\"\n"
      , current_module.c_str());
  }
  return &generated_code[new_name]->os;
}

output_struct* CodeGenHelper::get_outputstruct(Type* type) {
  string key = get_key(*type);
  const string& new_name = current_module + key;
  if (!generated_code.has_key(new_name)) {
    generated_output_t* go = new generated_output_t;
    go->filename = key;
    go->modulename = generated_code[current_module]->modulename;
    go->module_dispname = generated_code[current_module]->module_dispname;
    generated_code.add(new_name, go);
    go->os.source.includes = mprintf("\n#include \"%s.hh\"\n"
      , current_module.c_str());
  }
  return &generated_code[new_name]->os;
}

output_struct* CodeGenHelper::get_current_outputstruct() {
  return &generated_code[current_module]->os;
}

void CodeGenHelper::transfer_value(char* &dst, char* &src) {
  dst = mputstr(dst, src);
  Free(src);
  src = 0;
}

void CodeGenHelper::finalize_generation(Type* type) {
  string key = get_key(*type);
  if (key.empty()) return;

  output_struct& dst = *get_current_outputstruct();
  output_struct& src = *get_outputstruct(current_module + key);
  // key is not empty so these can never be the same

  transfer_value(dst.header.includes,     src.header.includes);
  transfer_value(dst.header.class_decls,  src.header.class_decls);
  transfer_value(dst.header.typedefs,     src.header.typedefs);
  transfer_value(dst.header.class_defs,   src.header.class_defs);
  transfer_value(dst.header.function_prototypes, src.header.function_prototypes);
  transfer_value(dst.header.global_vars,  src.header.global_vars);
  transfer_value(dst.header.testport_includes, src.header.testport_includes);

  transfer_value(dst.source.global_vars,  src.source.global_vars);

  transfer_value(dst.functions.pre_init,  src.functions.pre_init);
  transfer_value(dst.functions.post_init, src.functions.post_init);

  transfer_value(dst.functions.set_param, src.functions.set_param);
  transfer_value(dst.functions.log_param, src.functions.log_param);
  transfer_value(dst.functions.init_comp, src.functions.init_comp);
  transfer_value(dst.functions.start,     src.functions.start);
  transfer_value(dst.functions.control,   src.functions.control);
}

string CodeGenHelper::get_key(Ttcn::Definition& def) const {
  string retval;
  switch (split_mode) {
  case SPLIT_NONE:
    // returns the current module
    break;
  case SPLIT_BY_KIND:
    break;
  case SPLIT_BY_NAME:
    retval += "_" + def.get_id().get_name();
    break;
  case SPLIT_BY_HEURISTICS:
    break;
  }
  return retval;
}

string CodeGenHelper::get_key(Type& type) const {
  string retval;
  switch (split_mode) {
  case SPLIT_NONE:
    break;
  case SPLIT_BY_KIND: {
    Type::typetype_t tt = type.get_typetype();
    switch(tt) {
    case Type::T_CHOICE_A:
    case Type::T_CHOICE_T:
    case Type::T_SEQOF:
    case Type::T_SETOF:
    case Type::T_SEQ_A:
    case Type::T_SEQ_T:
    case Type::T_SET_A:
    case Type::T_SET_T:
      retval += typetypemap[(int)tt];
      break;
    default:
      // put it into the module (no suffix)
      break;
    }
    break; }
  case SPLIT_BY_NAME:
    break;
  case SPLIT_BY_HEURISTICS:
    break;
  }
  return retval;
}

void CodeGenHelper::write_output() {
  size_t i, j;
  if (split_mode == SPLIT_BY_KIND) {
    // Create empty files to have a fix set of files to compile
    string fname;
    for (j = 0; j < modules.size(); j++) {
      for (i = 0; typetypemap[i]; i++) {
        fname = modules[j]->dispname + typetypemap[i];
        if (!generated_code.has_key(fname)) {
          generated_output_t* go = new generated_output_t;
          go->filename = typetypemap[i];
          go->modulename = modules[j]->name;
          go->module_dispname = modules[j]->dispname;
          go->os.source.includes = mcopystr(
            "\n//This file intentionally empty."
            "\n#include <version.h>\n");
          generated_code.add(fname, go);
        }
      }
    }
  }
  generated_output_t* go;
  for (i = 0; i < generated_code.size(); i++) {
    go = generated_code.get_nth_elem(i);
    ::write_output(&go->os, go->modulename.c_str(), go->module_dispname.c_str(),
      go->filename.c_str(), go->is_ttcn, go->has_circular_import, go->is_module);
  }
}

CodeGenHelper::~CodeGenHelper() {
  size_t i;
  for (i = 0; i < generated_code.size(); i++)
    delete generated_code.get_nth_elem(i);
  generated_code.clear();
  for (i = 0; i < modules.size(); i++)
    delete modules[i];
  modules.clear();
  instance = 0;
}

}
