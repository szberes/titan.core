///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "Ttcnstuff.hh"
#include "../Int.hh"
#include "../CompilerError.hh"
#include "AST_ttcn3.hh"
#include "../main.hh"
#include "Attributes.hh"
#include <errno.h>

// implemented in coding_attrib_p.y
extern Ttcn::ExtensionAttributes * parse_extattributes(
  Ttcn::WithAttribPath *w_attrib_path);

namespace Ttcn {

  // =================================
  // ===== ErrorBehaviorSetting
  // =================================

  ErrorBehaviorSetting *ErrorBehaviorSetting::clone() const
  {
    FATAL_ERROR("ErrorBehaviorSetting::clone");
  }

  void ErrorBehaviorSetting::dump(unsigned level) const
  {
    DEBUG(level, "%s : %s", error_type.c_str(), error_handling.c_str());
  }

  // =================================
  // ===== ErrorBehaviorList
  // =================================

  ErrorBehaviorList::~ErrorBehaviorList()
  {
    for (size_t i = 0; i < ebs_v.size(); i++)
      delete ebs_v[i];
    ebs_v.clear();
    ebs_m.clear();
  }

  ErrorBehaviorList *ErrorBehaviorList::clone() const
  {
    FATAL_ERROR("ErrorBehaviorList::clone");
  }

  void ErrorBehaviorList::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for (size_t i = 0; i < ebs_v.size(); i++)
      ebs_v[i]->set_fullname(p_fullname
        + ".<setting" + Common::Int2string(i + 1) + ">");
  }

  void ErrorBehaviorList::add_ebs(ErrorBehaviorSetting *p_ebs)
  {
    if (!p_ebs || checked) FATAL_ERROR("ErrorBehaviorList::add_ebs()");
    ebs_v.add(p_ebs);
  }

  void ErrorBehaviorList::steal_ebs(ErrorBehaviorList *p_eblist)
  {
    if (!p_eblist || checked || p_eblist->checked)
      FATAL_ERROR("ErrorBehaviorList::steal_ebs()");
    for (size_t i = 0; i < p_eblist->ebs_v.size(); i++)
      ebs_v.add(p_eblist->ebs_v[i]);
    p_eblist->ebs_v.clear();
    join_location(*p_eblist);
  }

  bool ErrorBehaviorList::has_setting(const string& p_error_type)
  {
    if (!checked) chk();
    return ebs_all != 0 || ebs_m.has_key(p_error_type);
  }

  string ErrorBehaviorList::get_handling(const string& p_error_type)
  {
    if (!checked) chk();
    if (ebs_m.has_key(p_error_type))
      return ebs_m[p_error_type]->get_error_handling();
    else if (ebs_all) return ebs_all->get_error_handling();
    else return string("DEFAULT");
  }

  void ErrorBehaviorList::chk()
  {
    if (checked) return;
    const string all_str("ALL");
    Common::Error_Context cntxt(this, "In error behavior list");
    for (size_t i = 0; i < ebs_v.size(); i++) {
      ErrorBehaviorSetting *ebs = ebs_v[i];
      const string& error_type = ebs->get_error_type();
      if (error_type == all_str) {
        if (ebs_all) {
          ebs->warning("Duplicate setting for error type `ALL'");
          ebs_all->warning("The previous setting is ignored");
        }
        ebs_all = ebs;
        if (!ebs_m.empty()) {
          ebs->warning("All settings before `ALL' are ignored");
          ebs_m.clear();
        }
      } else {
        if (ebs_m.has_key(error_type)) {
          ebs->warning("Duplicate setting for error type `%s'",
            error_type.c_str());
          ErrorBehaviorSetting*& ebs_ref = ebs_m[error_type];
          ebs_ref->warning("The previous setting is ignored");
          // replace the previous setting in the map
          ebs_ref = ebs;
        } else ebs_m.add(error_type, ebs);
        static const char * const valid_types[] = {
          "UNBOUND", "INCOMPL_ANY", "ENC_ENUM", "INCOMPL_MSG", "LEN_FORM",
          "INVAL_MSG", "REPR", "CONSTRAINT", "TAG", "SUPERFL", "EXTENSION",
          "DEC_ENUM", "DEC_DUPFLD", "DEC_MISSFLD", "DEC_OPENTYPE", "DEC_UCSTR",
          "LEN_ERR", "SIGN_ERR", "INCOMP_ORDER", "TOKEN_ERR", "LOG_MATCHING",
          "FLOAT_TR", "FLOAT_NAN", "OMITTED_TAG", "NEGTEST_CONFL", NULL };
        bool type_found = false;
        for (const char * const *str = valid_types; *str; str++) {
          if (error_type == *str) {
            type_found = true;
            break;
          }
        }
        if (!type_found) {
          ebs->warning("String `%s' is not a valid error type",
            error_type.c_str());
        }
      }
      const string& error_handling = ebs->get_error_handling();
      static const char * const valid_handlings[] = {
        "DEFAULT", "ERROR", "WARNING", "IGNORE", NULL };
      bool handling_found = false;
      for (const char * const *str = valid_handlings; *str; str++) {
        if (error_handling == *str) {
          handling_found = true;
          break;
        }
      }
      if (!handling_found) {
        ebs->warning("String `%s' is not a valid error handling",
          error_handling.c_str());
      }
    }
    checked = true;
  }

  char *ErrorBehaviorList::generate_code(char *str)
  {
    if (!checked) FATAL_ERROR("ErrorBehaviorList::generate_code()");
    const string all_str("ALL");
    if (ebs_all) {
      str = mputprintf(str, "TTCN_EncDec::set_error_behavior("
        "TTCN_EncDec::ET_ALL, TTCN_EncDec::EB_%s);\n",
        ebs_all->get_error_handling().c_str());
    } else {
      // reset all error behavior to default
      str = mputstr(str, "TTCN_EncDec::set_error_behavior(TTCN_EncDec::ET_ALL, "
        "TTCN_EncDec::EB_DEFAULT);\n");
    }
    for (size_t i = 0; i < ebs_m.size(); i++) {
      ErrorBehaviorSetting *ebs = ebs_m.get_nth_elem(i);
      str = mputprintf(str, "TTCN_EncDec::set_error_behavior("
        "TTCN_EncDec::ET_%s, TTCN_EncDec::EB_%s);\n",
        ebs->get_error_type().c_str(), ebs->get_error_handling().c_str());
    }
    return str;
  }

  void ErrorBehaviorList::dump(unsigned level) const
  {
    DEBUG(level, "Error behavior: (%lu pcs.)", (unsigned long) ebs_v.size());
    for (size_t i = 0; i < ebs_v.size(); i++)
      ebs_v[i]->dump(level + 1);
  }
  
  // =================================
  // ===== PrintingType
  // =================================
  
  PrintingType *PrintingType::clone() const
  {
    FATAL_ERROR("PrintingType::clone");
  }
  
  char *PrintingType::generate_code(char *str)
  {
    return mputprintf(str, ", %d", (PT_PRETTY == printing) ? 1 : 0);
  }

  // =================================
  // ===== TypeMappingTarget
  // =================================

  TypeMappingTarget::TypeMappingTarget(Common::Type *p_target_type,
    TypeMappingType_t p_mapping_type)
    : Node(), Location(), target_type(p_target_type),
    mapping_type(p_mapping_type), checked(false)
  {
    if (p_mapping_type == TM_SIMPLE && p_target_type != NULL) { // acceptable
      target_type->set_ownertype(Type::OT_TYPE_MAP_TARGET, this);
    }
    else if (p_mapping_type == TM_DISCARD && p_target_type == NULL)
    {} // also acceptable but nothing to do
    else FATAL_ERROR("TypeMappingTarget::TypeMappingTarget()");
  }

  TypeMappingTarget::TypeMappingTarget(Common::Type *p_target_type,
    TypeMappingType_t p_mapping_type, Ttcn::Reference *p_function_ref)
    : Node(), Location(), target_type(p_target_type),
    mapping_type(p_mapping_type), checked(false)
  {
    if (!p_target_type || p_mapping_type != TM_FUNCTION || !p_function_ref)
      FATAL_ERROR("TypeMappingTarget::TypeMappingTarget()");
    u.func.function_ref = p_function_ref;
    u.func.function_ptr = 0;
    target_type->set_ownertype(Type::OT_TYPE_MAP_TARGET, this);
  }

  TypeMappingTarget::TypeMappingTarget(Common::Type *p_target_type,
    TypeMappingType_t p_mapping_type,
    Common::Type::MessageEncodingType_t p_coding_type, string *p_coding_options,
    ErrorBehaviorList *p_eb_list)
    : Node(), Location(), target_type(p_target_type),
    mapping_type(p_mapping_type), checked(false)
  {
    if (!p_target_type || (p_mapping_type != TM_ENCODE &&
      p_mapping_type != TM_DECODE))
      FATAL_ERROR("TypeMappingTarget::TypeMappingTarget()");
    u.encdec.coding_type = p_coding_type;
    u.encdec.coding_options = p_coding_options;
    u.encdec.eb_list = p_eb_list;
    target_type->set_ownertype(Type::OT_TYPE_MAP_TARGET, this);
  }

  TypeMappingTarget::~TypeMappingTarget()
  {
    delete target_type;
    switch (mapping_type) {
    case TM_FUNCTION:
      delete u.func.function_ref;
      break;
    case TM_ENCODE:
    case TM_DECODE:
      delete u.encdec.coding_options;
      delete u.encdec.eb_list;
    default:
      break;
    }
  }

  TypeMappingTarget *TypeMappingTarget::clone() const
  {
    FATAL_ERROR("TypeMappingTarget::clone");
  }

  void TypeMappingTarget::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    if (target_type) target_type->set_fullname(p_fullname + ".<target_type>");
    switch (mapping_type) {
    case TM_SIMPLE:
    case TM_DISCARD:
      break;
    case TM_FUNCTION:
      u.func.function_ref->set_fullname(p_fullname + ".<function_ref>");
      break;
    case TM_ENCODE:
    case TM_DECODE:
      if (u.encdec.eb_list)
        u.encdec.eb_list->set_fullname(p_fullname + ".<errorbehavior>");
      break;
    default:
      FATAL_ERROR("TypeMappingTarget::set_fullname()");
    }
  }

  void TypeMappingTarget::set_my_scope(Common::Scope *p_scope)
  {
    if (target_type) target_type->set_my_scope(p_scope);
    switch (mapping_type) {
    case TM_SIMPLE:
    case TM_DISCARD:
    case TM_ENCODE:
    case TM_DECODE:
      break;
    case TM_FUNCTION:
      u.func.function_ref->set_my_scope(p_scope);
      break;
    default:
      FATAL_ERROR("TypeMappingTarget::set_my_scope()");
    }
  }

  const char *TypeMappingTarget::get_mapping_name() const
  {
    switch (mapping_type) {
    case TM_SIMPLE:
      return "simple";
    case TM_DISCARD:
      return "discard";
    case TM_FUNCTION:
      return "function";
    case TM_ENCODE:
      return "encode";
    case TM_DECODE:
      return "decode";
    default:
      return "<unknown mapping>";
    }
  }

  Ttcn::Def_Function_Base *TypeMappingTarget::get_function() const
  {
    if (mapping_type != TM_FUNCTION || !checked)
      FATAL_ERROR("TypeMappingTarget::get_function()");
    return u.func.function_ptr;
  }

  Type::MessageEncodingType_t TypeMappingTarget::get_coding_type() const
  {
    if (mapping_type != TM_ENCODE && mapping_type != TM_DECODE)
      FATAL_ERROR("TypeMappingTarget::get_coding_type()");
    return u.encdec.coding_type;
  }

  bool TypeMappingTarget::has_coding_options() const
  {
    if (mapping_type != TM_ENCODE && mapping_type != TM_DECODE)
      FATAL_ERROR("TypeMappingTarget::has_coding_options()");
    return u.encdec.coding_options != 0;
  }

  const string& TypeMappingTarget::get_coding_options() const
  {
    if ((mapping_type != TM_ENCODE && mapping_type != TM_DECODE) ||
        !u.encdec.coding_options)
      FATAL_ERROR("TypeMappingTarget::get_coding_options()");
    return *u.encdec.coding_options;
  }

  ErrorBehaviorList *TypeMappingTarget::get_eb_list() const
  {
    if (mapping_type != TM_ENCODE && mapping_type != TM_DECODE)
      FATAL_ERROR("TypeMappingTarget::get_eb_list()");
    return u.encdec.eb_list;
  }

  void TypeMappingTarget::chk_simple(Type *source_type)
  {
    Error_Context cntxt(this, "In `simple' mapping");
    if (!source_type->is_identical(target_type)) {
      target_type->error("The source and target types must be the same: "
        "`%s' was expected instead of `%s'",
        source_type->get_typename().c_str(),
        target_type->get_typename().c_str());
    }
  }

  void TypeMappingTarget::chk_function(Type *source_type)
  {
    Error_Context cntxt(this, "In `function' mapping");
    Assignment *t_ass = u.func.function_ref->get_refd_assignment(false);
    if (!t_ass) return;
    t_ass->chk();
    switch (t_ass->get_asstype()) {
    case Assignment::A_FUNCTION:
    case Assignment::A_FUNCTION_RVAL:
    case Assignment::A_FUNCTION_RTEMP:
    case Assignment::A_EXT_FUNCTION:
    case Assignment::A_EXT_FUNCTION_RVAL:
    case Assignment::A_EXT_FUNCTION_RTEMP:
      break;
    default:
      u.func.function_ref->error("Reference to a function or external "
        "function was expected instead of %s",
        t_ass->get_description().c_str());
      return;
    }
    u.func.function_ptr = dynamic_cast<Ttcn::Def_Function_Base*>(t_ass);
    if (!u.func.function_ptr) FATAL_ERROR("TypeMappingTarget::chk_function()");
    if (u.func.function_ptr->get_prototype() ==
        Ttcn::Def_Function_Base::PROTOTYPE_NONE) {
      u.func.function_ref->error("The referenced %s does not have `prototype' "
        "attribute", u.func.function_ptr->get_description().c_str());
      return;
    }
    Type *input_type = u.func.function_ptr->get_input_type();
    if (input_type && !source_type->is_identical(input_type)) {
      source_type->error("The input type of %s must be the same as the source "
        "type of the mapping: `%s' was expected instead of `%s'",
        u.func.function_ptr->get_description().c_str(),
        source_type->get_typename().c_str(),
        input_type->get_typename().c_str());
    }
    Type *output_type = u.func.function_ptr->get_output_type();
    if (output_type && !target_type->is_identical(output_type)) {
      target_type->error("The output type of %s must be the same as the "
        "target type of the mapping: `%s' was expected instead of `%s'",
        u.func.function_ptr->get_description().c_str(),
        target_type->get_typename().c_str(),
        output_type->get_typename().c_str());
    }
  }

  void TypeMappingTarget::chk_encode(Type *source_type)
  {
    Error_Context cntxt(this, "In `encode' mapping");
    if (!source_type->has_encoding(u.encdec.coding_type)) {
      source_type->error("Source type `%s' does not support %s encoding",
        source_type->get_typename().c_str(),
        Type::get_encoding_name(u.encdec.coding_type));
    }
    Type *stream_type = Type::get_stream_type(u.encdec.coding_type);
    if (!stream_type->is_identical(target_type)) {
      target_type->error("Target type of %s encoding should be `%s' instead "
        "of `%s'", Type::get_encoding_name(u.encdec.coding_type),
        stream_type->get_typename().c_str(),
        target_type->get_typename().c_str());
    }
    if (u.encdec.eb_list) u.encdec.eb_list->chk();
  }

  void TypeMappingTarget::chk_decode(Type *source_type)
  {
    Error_Context cntxt(this, "In `decode' mapping");
    Type *stream_type = Type::get_stream_type(u.encdec.coding_type);
    if (!stream_type->is_identical(source_type)) {
      source_type->error("Source type of %s encoding should be `%s' instead "
        "of `%s'", Type::get_encoding_name(u.encdec.coding_type),
        stream_type->get_typename().c_str(),
        source_type->get_typename().c_str());
    }
    if (!target_type->has_encoding(u.encdec.coding_type)) {
      target_type->error("Target type `%s' does not support %s encoding",
        target_type->get_typename().c_str(),
        Type::get_encoding_name(u.encdec.coding_type));
    }
    if (u.encdec.eb_list) u.encdec.eb_list->chk();
  }

  void TypeMappingTarget::chk(Type *source_type)
  {
    if (checked) return;
    checked = true;
    if (target_type) {
      Error_Context cntxt(target_type, "In target type");
      target_type->chk();
    }
    switch (mapping_type) {
    case TM_SIMPLE:
      chk_simple(source_type);
      break;
    case TM_DISCARD:
      break;
    case TM_FUNCTION:
      chk_function(source_type);
      break;
    case TM_ENCODE:
      chk_encode(source_type);
      break;
    case TM_DECODE:
      chk_decode(source_type);
      break;
    default:
      FATAL_ERROR("TypeMappingTarget::chk()");
    }
  }

  bool TypeMappingTarget::fill_type_mapping_target(
    port_msg_type_mapping_target *target, Type *source_type,
    Scope *p_scope, stringpool& pool)
  {
    bool has_sliding = false;
    if (target_type) {
      target->target_name = pool.add(target_type->get_genname_value(p_scope));
      target->target_dispname = pool.add(target_type->get_typename());
    } else {
      target->target_name = NULL;
      target->target_dispname = NULL;
    }
    switch (mapping_type) {
    case TM_SIMPLE:
      target->mapping_type = M_SIMPLE;
      break;
    case TM_DISCARD:
      target->mapping_type = M_DISCARD;
      break;
    case TM_FUNCTION:
      target->mapping_type = M_FUNCTION;
      if (!u.func.function_ptr)
        FATAL_ERROR("TypeMappingTarget::fill_type_mapping_target()");
      target->mapping.function.name =
        pool.add(u.func.function_ptr->get_genname_from_scope(p_scope));
      switch (u.func.function_ptr->get_prototype()) {
      case Ttcn::Def_Function_Base::PROTOTYPE_CONVERT:
        target->mapping.function.prototype = PT_CONVERT;
        break;
      case Ttcn::Def_Function_Base::PROTOTYPE_FAST:
        target->mapping.function.prototype = PT_FAST;
        break;
      case Ttcn::Def_Function_Base::PROTOTYPE_BACKTRACK:
        target->mapping.function.prototype = PT_BACKTRACK;
        break;
      case Ttcn::Def_Function_Base::PROTOTYPE_SLIDING:
        target->mapping.function.prototype = PT_SLIDING;
        has_sliding = true;
        break;
      default:
        FATAL_ERROR("TypeMappingTarget::fill_type_mapping_target()");
      }
      break;
    case TM_ENCODE:
    case TM_DECODE:
      if (mapping_type == TM_ENCODE) {
        target->mapping_type = M_ENCODE;
        target->mapping.encdec.typedescr_name =
          pool.add(source_type->get_genname_typedescriptor(p_scope));
      } else {
        target->mapping_type = M_DECODE;
        target->mapping.encdec.typedescr_name =
          pool.add(target_type->get_genname_typedescriptor(p_scope));
      }
      target->mapping.encdec.encoding_type =
        Type::get_encoding_name(u.encdec.coding_type);
      if (u.encdec.coding_options) target->mapping.encdec.encoding_options =
          u.encdec.coding_options->c_str();
      else target->mapping.encdec.encoding_options = NULL;
      if (u.encdec.eb_list) {
        char *str = u.encdec.eb_list->generate_code(memptystr());
        target->mapping.encdec.errorbehavior = pool.add(string(str));
        Free(str);
      } else {
        target->mapping.encdec.errorbehavior =
          "TTCN_EncDec::set_error_behavior(TTCN_EncDec::ET_ALL, "
            "TTCN_EncDec::EB_DEFAULT);\n";
      }
      break;
    default:
      FATAL_ERROR("TypeMappingTarget::fill_type_mapping_target()");
    }
    return has_sliding;
  }

  void TypeMappingTarget::dump(unsigned level) const
  {
    DEBUG(level, "target:");
    if (target_type) target_type->dump(level + 1);
    else DEBUG(level + 1, "<none>");
    DEBUG(level, "mapping type: %s", get_mapping_name());
    switch (mapping_type) {
    case TM_FUNCTION:
      u.func.function_ref->dump(level + 1);
      break;
    case TM_ENCODE:
    case TM_DECODE:
      DEBUG(level + 1, "encoding: %s",
        Type::get_encoding_name(u.encdec.coding_type));
      if (u.encdec.coding_options)
        DEBUG(level + 1, "options: %s", u.encdec.coding_options->c_str());
      if (u.encdec.eb_list) u.encdec.eb_list->dump(level + 1);
    default:
      break;
    }
  }

  // =================================
  // ===== TypeMappingTargets
  // =================================

  TypeMappingTargets::~TypeMappingTargets()
  {
    size_t nof_targets = targets_v.size();
    for (size_t i = 0; i < nof_targets; i++) delete targets_v[i];
    targets_v.clear();
  }

  TypeMappingTargets *TypeMappingTargets::clone() const
  {
    FATAL_ERROR("TypeMappingTargets::clone");
  }

  void TypeMappingTargets::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    size_t nof_targets = targets_v.size();
    for (size_t i = 0; i < nof_targets; i++) {
      targets_v[i]->set_fullname(p_fullname + ".<target" + Int2string(i + 1)
        + ">");
    }
  }

  void TypeMappingTargets::set_my_scope(Scope *p_scope)
  {
    size_t nof_targets = targets_v.size();
    for (size_t i = 0; i < nof_targets; i++)
      targets_v[i]->set_my_scope(p_scope);
  }

  void TypeMappingTargets::add_target(TypeMappingTarget *p_target)
  {
    if (!p_target) FATAL_ERROR("TypeMappingTargets::add_target()");
    targets_v.add(p_target);
  }

  void TypeMappingTargets::dump(unsigned level) const
  {
    size_t nof_targets = targets_v.size();
    DEBUG(level, "Targets: (%lu pcs.)", (unsigned long) nof_targets);
    for (size_t i = 0; i < nof_targets; i++) targets_v[i]->dump(level + 1);
  }

  // =================================
  // ===== TypeMapping
  // =================================

  TypeMapping::TypeMapping(Type *p_source_type, TypeMappingTargets *p_targets)
    : Node(), Location(), source_type(p_source_type), targets(p_targets)
  {
    if (!p_source_type || !p_targets)
      FATAL_ERROR("TypeMapping::TypeMapping()");
    source_type->set_ownertype(Type::OT_TYPE_MAP, this);
  }

  TypeMapping::~TypeMapping()
  {
    delete source_type;
    delete targets;
  }

  TypeMapping *TypeMapping::clone() const
  {
    FATAL_ERROR("TypeMapping::clone");
  }

  void TypeMapping::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    source_type->set_fullname(p_fullname + ".<source_type>");
    targets->set_fullname(p_fullname);
  }

  void TypeMapping::set_my_scope(Scope *p_scope)
  {
    source_type->set_my_scope(p_scope);
    targets->set_my_scope(p_scope);
  }

  void TypeMapping::chk()
  {
    Error_Context cntxt(this, "In type mapping");
    {
      Error_Context cntxt2(source_type, "In source type");
      source_type->chk();
    }
    size_t nof_targets = targets->get_nof_targets();
    bool has_sliding = false, has_non_sliding = false;
    for (size_t i = 0; i < nof_targets; i++) {
      TypeMappingTarget *target = targets->get_target_byIndex(i);
      target->chk(source_type);
      if (nof_targets > 1) {
        switch (target->get_mapping_type()) {
        case TypeMappingTarget::TM_DISCARD:
          if (has_sliding) target->error("Mapping `discard' cannot be used "
            "if functions with `prototype(sliding)' are referred from the same "
            "source type");
          else if (i < nof_targets - 1) target->error("Mapping `discard' must "
            "be the last target of the source type");
          break;
        case TypeMappingTarget::TM_FUNCTION: {
          Ttcn::Def_Function_Base *t_function = target->get_function();
          if (t_function) {
            switch (t_function->get_prototype()) {
            case Ttcn::Def_Function_Base::PROTOTYPE_NONE:
              break;
            case Ttcn::Def_Function_Base::PROTOTYPE_BACKTRACK:
              has_non_sliding = true;
              break;
            case Ttcn::Def_Function_Base::PROTOTYPE_SLIDING:
              has_sliding = true;
              break;
            default:
              target->error("The referenced %s must have the attribute "
                "`prototype(backtrack)' or `prototype(sliding)' when more "
                "than one targets are present",
                t_function->get_description().c_str());
            }
          }
          break; }
        case TypeMappingTarget::TM_DECODE:
          break;
        default:
          target->error("The type of the mapping must be `function', `decode' "
            "or `discard' instead of `%s' when more than one targets are "
            "present", target->get_mapping_name());
        }
      }
    }
    if (has_sliding && has_non_sliding) {
      error("If one of the mappings refers to a function with attribute "
        "`prototype(sliding)' then mappings of this source type cannot refer "
        "to functions with attribute `prototype(backtrack)'");
    }
  }

  void TypeMapping::dump(unsigned level) const
  {
    DEBUG(level, "Source type:");
    source_type->dump(level + 1);
    targets->dump(level);
  }

  // =================================
  // ===== TypeMappings
  // =================================

  TypeMappings::~TypeMappings()
  {
    size_t nof_mappings = mappings_v.size();
    for (size_t i = 0; i < nof_mappings; i++) delete mappings_v[i];
    mappings_v.clear();
    mappings_m.clear();
  }

  TypeMappings *TypeMappings::clone() const
  {
    FATAL_ERROR("TypeMappings::clone");
  }

  void TypeMappings::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    size_t nof_mappings = mappings_v.size();
    for (size_t i = 0; i < nof_mappings; i++) {
      mappings_v[i]->set_fullname(p_fullname + ".<mapping" + Int2string(i + 1)
        + ">");
    }
  }

  void TypeMappings::set_my_scope(Scope *p_scope)
  {
    size_t nof_mappings = mappings_v.size();
    for (size_t i = 0; i < nof_mappings; i++)
      mappings_v[i]->set_my_scope(p_scope);
  }

  void TypeMappings::add_mapping(TypeMapping *p_mapping)
  {
    if (checked || !p_mapping) FATAL_ERROR("TypeMappings::add_mapping()");
    mappings_v.add(p_mapping);
  }

  void TypeMappings::steal_mappings(TypeMappings *p_mappings)
  {
    if (checked || !p_mappings || p_mappings->checked)
      FATAL_ERROR("TypeMappings::steal_mappings()");
    size_t nof_mappings = p_mappings->mappings_v.size();
    for (size_t i = 0; i < nof_mappings; i++)
      mappings_v.add(p_mappings->mappings_v[i]);
    p_mappings->mappings_v.clear();
    join_location(*p_mappings);
  }

  bool TypeMappings::has_mapping_for_type(Type *p_type) const
  {
    if (!checked || !p_type)
      FATAL_ERROR("TypeMappings::has_mapping_for_type()");
    if (p_type->get_type_refd_last()->get_typetype() == Type::T_ERROR)
      return true;
    else return mappings_m.has_key(p_type->get_typename());
  }

  TypeMapping *TypeMappings::get_mapping_byType(Type *p_type) const
  {
    if (!checked || !p_type)
      FATAL_ERROR("TypeMappings::get_mapping_byType()");
    return mappings_m[p_type->get_typename()];
  }

  void TypeMappings::chk()
  {
    if (checked) return;
    checked = true;
    size_t nof_mappings = mappings_v.size();
    for (size_t i = 0; i < nof_mappings; i++) {
      TypeMapping *mapping = mappings_v[i];
      mapping->chk();
      Type *source_type = mapping->get_source_type();
      if (source_type->get_type_refd_last()->get_typetype() != Type::T_ERROR) {
        const string& source_type_name = source_type->get_typename();
        if (mappings_m.has_key(source_type_name)) {
          const char *source_type_name_str = source_type_name.c_str();
          source_type->error("Duplicate mapping for type `%s'",
            source_type_name_str);
          mappings_m[source_type_name]->note("The mapping of type `%s' is "
            "already given here", source_type_name_str);
        } else mappings_m.add(source_type_name, mapping);
      }
    }
  }

  void TypeMappings::dump(unsigned level) const
  {
    size_t nof_mappings = mappings_v.size();
    DEBUG(level, "type mappings: (%lu pcs.)", (unsigned long) nof_mappings);
    for (size_t i = 0; i < nof_mappings; i++)
      mappings_v[i]->dump(level + 1);
  }

  // =================================
  // ===== Types
  // =================================

  Types::~Types()
  {
    size_t nof_types = types.size();
    for (size_t i = 0; i < nof_types; i++) delete types[i];
    types.clear();
  }

  Types *Types::clone() const
  {
    FATAL_ERROR("Types::clone");
  }

  void Types::add_type(Type *p_type)
  {
    if (!p_type) FATAL_ERROR("Types::add_type()");
    p_type->set_ownertype(Type::OT_TYPE_LIST, this);
    types.add(p_type);
  }

  Type *Types::extract_type_byIndex(size_t n)
  {
    Type *retval = types[n];
    types[n] = 0;
    return retval;
  }

  void Types::steal_types(Types *p_tl)
  {
    if (!p_tl) FATAL_ERROR("Types::steal_types()");
    size_t nof_types = p_tl->types.size();
    for (size_t i = 0; i < nof_types; i++) types.add(p_tl->types[i]);
    p_tl->types.clear();
    join_location(*p_tl);
  }

  void Types::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    size_t nof_types = types.size();
    for (size_t i = 0; i < nof_types; i++)
      types[i]->set_fullname(p_fullname + ".<type" + Int2string(i + 1) + ">");
  }

  void Types::set_my_scope(Scope *p_scope)
  {
    size_t nof_types = types.size();
    for (size_t i = 0; i < nof_types; i++) types[i]->set_my_scope(p_scope);
  }

  void Types::dump(unsigned level) const
  {
    size_t nof_types = types.size();
    DEBUG(level, "Types: (%lu pcs.)", (unsigned long) nof_types);
    for (size_t i = 0; i < nof_types; i++) types[i]->dump(level + 1);
  }

  // =================================
  // ===== TypeSet
  // =================================

  TypeSet::~TypeSet()
  {
    types_v.clear();
    types_m.clear();
  }

  TypeSet *TypeSet::clone() const
  {
    FATAL_ERROR("TypeSet::clone()");
  }

  void TypeSet::add_type(Type *p_type)
  {
    if (!p_type) FATAL_ERROR("TypeSet::add_type()");
    types_v.add(p_type);
    types_m.add(p_type->get_typename(), p_type);
  }

  bool TypeSet::has_type(Type *p_type) const
  {
    if (!p_type) FATAL_ERROR("TypeSet::has_type()");
    if (p_type->get_type_refd_last()->get_typetype() == Type::T_ERROR)
      return true;
    else return types_m.has_key(p_type->get_typename());
  }

  size_t TypeSet::get_nof_compatible_types(Type *p_type) const
  {
    if (!p_type) FATAL_ERROR("TypeSet::get_nof_compatible_types()");
    if (p_type->get_type_refd_last()->get_typetype() == Type::T_ERROR) {
      // Return a positive answer for erroneous types.
      return 1;
    } else {
      size_t ret_val = 0;
      size_t nof_types = types_v.size();
      for (size_t i = 0; i < nof_types; i++) {
        // Don't allow type compatibility.
        if (types_v[i]->is_compatible(p_type, NULL)) ret_val++;
      }
      return ret_val;
    }
  }

  size_t TypeSet::get_index_byType(Type *p_type) const
  {
    if (!p_type) FATAL_ERROR("TypeSet::get_index_byType()");
    const string& name = p_type->get_typename();
    size_t nof_types = types_v.size();
    for (size_t i = 0; i < nof_types; i++)
      if (types_v[i]->get_typename() == name) return i;
    FATAL_ERROR("TypeSet::get_index_byType()");
    return static_cast<size_t>(-1);
  }

  void TypeSet::dump(unsigned level) const
  {
    size_t nof_types = types_v.size();
    DEBUG(level, "Types: (%lu pcs.)", (unsigned long) nof_types);
    for (size_t i = 0; i < nof_types; i++) types_v[i]->dump(level + 1);
  }

  // =================================
  // ===== PortTypeBody
  // =================================

  PortTypeBody::PortTypeBody(PortOperationMode_t p_operation_mode,
    Types *p_in_list, Types *p_out_list, Types *p_inout_list,
    bool p_in_all, bool p_out_all, bool p_inout_all)
    : Node(), Location(), my_type(0), operation_mode(p_operation_mode),
    in_list(p_in_list), out_list(p_out_list), inout_list(p_inout_list),
    in_all(p_in_all), out_all(p_out_all), inout_all(p_inout_all),
    checked(false),
    in_msgs(0), out_msgs(0), in_sigs(0), out_sigs(0),
    testport_type(TP_REGULAR), port_type(PT_REGULAR),
    provider_ref(0), provider_type(0), in_mappings(0), out_mappings(0)
  {
  }

  PortTypeBody::~PortTypeBody()
  {
    delete in_list;
    delete out_list;
    delete inout_list;
    delete in_msgs;
    delete out_msgs;
    delete in_sigs;
    delete out_sigs;
    delete provider_ref;
    delete in_mappings;
    delete out_mappings;
  }

  PortTypeBody *PortTypeBody::clone() const
  {
    FATAL_ERROR("PortTypeBody::clone");
  }

  void PortTypeBody::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    if (in_list) in_list->set_fullname(p_fullname + ".<in_list>");
    if (out_list) out_list->set_fullname(p_fullname + ".<out_list>");
    if (inout_list) inout_list->set_fullname(p_fullname + ".<inout_list>");
    if (in_msgs) in_msgs->set_fullname(p_fullname + ".<incoming_messages>");
    if (out_msgs) out_msgs->set_fullname(p_fullname + ".<outgoing_messages>");
    if (in_sigs) in_sigs->set_fullname(p_fullname + ".<incoming_signatures>");
    if (out_sigs) out_sigs->set_fullname(p_fullname + ".<outgoing_signatures>");
    if (provider_ref)
      provider_ref->set_fullname(p_fullname + ".<provider_ref>");
    if (in_mappings) in_mappings->set_fullname(p_fullname + ".<in_mappings>");
    if (out_mappings)
      out_mappings->set_fullname(p_fullname + ".<out_mappings>");
  }

  void PortTypeBody::set_my_scope(Scope *p_scope)
  {
    if (in_list) in_list->set_my_scope(p_scope);
    if (out_list) out_list->set_my_scope(p_scope);
    if (inout_list) inout_list->set_my_scope(p_scope);
    if (provider_ref) provider_ref->set_my_scope(p_scope);
    if (in_mappings) in_mappings->set_my_scope(p_scope);
    if (out_mappings) out_mappings->set_my_scope(p_scope);
  }

  void PortTypeBody::set_my_type(Type *p_type)
  {
    if (!p_type || p_type->get_typetype() != Type::T_PORT)
      FATAL_ERROR("PortTypeBody::set_my_type()");
    my_type = p_type;
  }

  TypeSet *PortTypeBody::get_in_msgs() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::get_in_msgs()");
    return in_msgs;
  }

  TypeSet *PortTypeBody::get_out_msgs() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::get_out_msgs()");
    return out_msgs;
  }

  TypeSet *PortTypeBody::get_in_sigs() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::get_in_sigs()");
    return in_sigs;
  }

  TypeSet *PortTypeBody::get_out_sigs() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::get_out_sigs()");
    return out_sigs;
  }

  bool PortTypeBody::has_queue() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::has_queue()");
    if (in_msgs || in_sigs) return true;
    if (out_sigs) {
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        Type *t_sig = out_sigs->get_type_byIndex(i)->get_type_refd_last();
        if (!t_sig->is_nonblocking_signature() ||
            t_sig->get_signature_exceptions()) return true;
      }
    }
    return false;
  }

  bool PortTypeBody::getreply_allowed() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::getreply_allowed()");
    if (out_sigs) {
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        if (!out_sigs->get_type_byIndex(i)->get_type_refd_last()
            ->is_nonblocking_signature()) return true;
      }
    }
    return false;
  }

  bool PortTypeBody::catch_allowed() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::catch_allowed()");
    if (out_sigs) {
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        if (out_sigs->get_type_byIndex(i)->get_type_refd_last()
            ->get_signature_exceptions()) return true;
      }
    }
    return false;
  }

  bool PortTypeBody::is_internal() const
  {
    if (!checked) FATAL_ERROR("PortTypeBody::is_internal_port()");
    return testport_type == TP_INTERNAL;
  }

  Type *PortTypeBody::get_address_type()
  {
    if (!checked) FATAL_ERROR("PortTypeBody::get_address_type()");
    if (testport_type == TP_ADDRESS) {
      Type *t;
      // in case of 'user' port types the address visible and supported by the
      // 'provider' port type is relevant
      if (port_type == PT_USER && provider_type) t = provider_type;
      else t = my_type;
      return t->get_my_scope()->get_scope_mod()->get_address_type();
    } else {
      // the port type does not support SUT addresses
      return 0;
    }
  }

  void PortTypeBody::add_provider_attribute()
  {
    port_type = PT_PROVIDER;
    delete provider_ref;
    provider_ref = 0;
    provider_type = 0;
    delete in_mappings;
    in_mappings = 0;
    delete out_mappings;
    out_mappings = 0;
  }

  void PortTypeBody::add_user_attribute(Ttcn::Reference *p_provider_ref,
    TypeMappings *p_in_mappings, TypeMappings *p_out_mappings)
  {
    if (!p_provider_ref || !my_type)
      FATAL_ERROR("PortTypeBody::add_user_attribute()");
    Scope *t_scope = my_type->get_my_scope();
    const string& t_fullname = get_fullname();
    port_type = PT_USER;
    delete provider_ref;
    provider_ref = p_provider_ref;
    provider_ref->set_fullname(t_fullname + ".<provider_ref>");
    provider_ref->set_my_scope(t_scope);
    provider_type = 0;
    delete in_mappings;
    in_mappings = p_in_mappings;
    if (in_mappings) {
      in_mappings->set_fullname(t_fullname + ".<in_mappings>");
      in_mappings->set_my_scope(t_scope);
    }
    delete out_mappings;
    out_mappings = p_out_mappings;
    if (out_mappings) {
      out_mappings->set_fullname(t_fullname + ".<out_mappings>");
      out_mappings->set_my_scope(t_scope);
    }
  }

  Type *PortTypeBody::get_provider_type() const
  {
    if (!checked || port_type != PT_USER)
      FATAL_ERROR("PortTypeBody::get_provider_type()");
    return provider_type;
  }

  void PortTypeBody::chk_list(const Types *list, bool is_in, bool is_out)
  {
    const char *err_msg;
    if (is_in) {
      if (is_out) err_msg = "sent or received";
      else err_msg = "received";
    } else if (is_out) err_msg = "sent";
    else {
      FATAL_ERROR("PortTypeBody::chk_list()");
      err_msg = 0;
    }
    size_t nof_types = list->get_nof_types();
    for (size_t i = 0; i < nof_types; i++) {
      Type *t = list->get_type_byIndex(i);
      t->chk();
      // check if a value/template of this type can leave the component
      if (t->is_component_internal()) {
        map<Type*,void> type_chain;
        t->chk_component_internal(type_chain, "sent or received on a port");
      }
      Type *t_last = t->get_type_refd_last();
      switch (t_last->get_typetype()) {
      case Type::T_ERROR:
        // silently ignore
        break;
      case Type::T_SIGNATURE:
        if (operation_mode == PO_MESSAGE) {
          t->error("Signature `%s' cannot be used on a message based port",
            t_last->get_typename().c_str());
        }
        if (is_in) {
          if (in_sigs && in_sigs->has_type(t_last)) {
            const string& type_name = t_last->get_typename();
            t->error("Duplicate incoming signature `%s'", type_name.c_str());
            in_sigs->get_type_byName(type_name)->note("Signature `%s' is "
              "already listed here", type_name.c_str());
          } else {
            if (!in_sigs) {
              in_sigs = new TypeSet;
              in_sigs->set_fullname(get_fullname() + ".<incoming_signatures>");
            }
            in_sigs->add_type(t);
          }
        }
        if (is_out) {
          if (out_sigs && out_sigs->has_type(t_last)) {
            const string& type_name = t_last->get_typename();
            t->error("Duplicate outgoing signature `%s'", type_name.c_str());
            out_sigs->get_type_byName(type_name)->note("Signature `%s' is "
              "already listed here", type_name.c_str());
          } else {
            if (!out_sigs) {
              out_sigs = new TypeSet;
              out_sigs->set_fullname(get_fullname() + ".<outgoing_signatures>");
            }
            out_sigs->add_type(t);
          }
        }
        break;
      default:
        // t is a normal data type
        if (operation_mode == PO_PROCEDURE) {
          t->error("Data type `%s' cannot be %s on a procedure based port",
            t_last->get_typename().c_str(), err_msg);
        }
        if (is_in) {
          if (in_msgs && in_msgs->has_type(t_last)) {
            const string& type_name = t_last->get_typename();
            t->error("Duplicate incoming message type `%s'", type_name.c_str());
            in_msgs->get_type_byName(type_name)->note("Type `%s' is already "
              "listed here", type_name.c_str());
          } else {
            if (!in_msgs) {
              in_msgs = new TypeSet;
              in_msgs->set_fullname(get_fullname() + ".<incoming_messages>");
            }
            in_msgs->add_type(t);
          }
        }
        if (is_out) {
          if (out_msgs && out_msgs->has_type(t_last)) {
            const string& type_name = t_last->get_typename();
            t->error("Duplicate outgoing message type `%s'", type_name.c_str());
            out_msgs->get_type_byName(type_name)->note("Type `%s' is already "
              "listed here", type_name.c_str());
          } else {
            if (!out_msgs) {
              out_msgs = new TypeSet;
              out_msgs->set_fullname(get_fullname() + ".<outgoing_messages>");
            }
            out_msgs->add_type(t);
          }
        }
      }
    }
  }

  void PortTypeBody::chk_user_attribute()
  {
    Error_Context cntxt(this, "In extension attribute `user'");
    // check the reference that points to the provider type
    provider_type = 0;
    PortTypeBody *provider_body = 0;
    Assignment *t_ass = provider_ref->get_refd_assignment(); // provider port
    if (t_ass) {
      if (t_ass->get_asstype() == Assignment::A_TYPE) {
        Type *t = t_ass->get_Type()->get_type_refd_last();
        if (t->get_typetype() == Type::T_PORT) {
          provider_type = t;
          provider_body = t->get_PortBody();
        } else {
          provider_ref->error("Type reference `%s' does not refer to a port "
            "type", provider_ref->get_dispname().c_str());
        }
      } else {
        provider_ref->error("Reference `%s' does not refer to a type",
          provider_ref->get_dispname().c_str());
      }
    }

    // checking the consistency of attributes in this and provider_body
    if (provider_body && testport_type != TP_INTERNAL) {
      if (provider_body->port_type != PT_PROVIDER) {
        provider_ref->error("The referenced port type `%s' must have the "
          "`provider' attribute", provider_type->get_typename().c_str());
      }
      switch (provider_body->testport_type) {
      case TP_REGULAR:
        if (testport_type == TP_ADDRESS) {
          provider_ref->error("Attribute `address' cannot be used because the "
            "provider port type `%s' does not have attribute `address'",
            provider_type->get_typename().c_str());
        }
        break;
      case TP_INTERNAL:
        provider_ref->error("Missing attribute `internal'. Provider port "
          "type `%s' has attribute `internal', which must be also present here",
          provider_type->get_typename().c_str());
      case TP_ADDRESS:
        break;
      default:
        FATAL_ERROR("PortTypeBody::chk_attributes()");
      }
      // inherit the test port API type from the provider
      testport_type = provider_body->testport_type;
    }

    // checking the incoming mappings
    if (in_mappings) {
      Error_Context cntxt2(in_mappings, "In `in' mappings");
      in_mappings->chk();
      // checking source types
      if (provider_body) {
        if (provider_body->in_msgs) {
          // check if all source types are present on the `in' list of the
          // provider
          size_t nof_mappings = in_mappings->get_nof_mappings();
          for (size_t i = 0; i < nof_mappings; i++) {
            Type *source_type =
              in_mappings->get_mapping_byIndex(i)->get_source_type();
            if (!provider_body->in_msgs->has_type(source_type)) {
              source_type->error("Source type `%s' of the `in' mapping is not "
                "present on the list of incoming messages in provider port "
                "type `%s'", source_type->get_typename().c_str(),
                provider_type->get_typename().c_str());
            }
          }
          // check if all types of the `in' list of the provider are handled by
          // the mappings
          size_t nof_msgs = provider_body->in_msgs->get_nof_types();
          for (size_t i = 0; i < nof_msgs; i++) {
            Type *msg_type = provider_body->in_msgs->get_type_byIndex(i);
            if (!in_mappings->has_mapping_for_type(msg_type)) {
              in_mappings->error("Incoming message type `%s' of provider "
                "port type `%s' is not handled by the incoming mappings",
                msg_type->get_typename().c_str(),
                provider_type->get_typename().c_str());
            }
          }
        } else {
          in_mappings->error("Invalid incoming mappings. Provider port type "
            "`%s' does not have incoming message types",
            provider_type->get_typename().c_str());
        }
      }
      // checking target types
      size_t nof_mappings = in_mappings->get_nof_mappings();
      for (size_t i = 0; i < nof_mappings; i++) {
        TypeMapping *mapping = in_mappings->get_mapping_byIndex(i);
        size_t nof_targets = mapping->get_nof_targets();
        for (size_t j = 0; j < nof_targets; j++) {
          Type *target_type =
            mapping->get_target_byIndex(j)->get_target_type();
          if (target_type && (!in_msgs || !in_msgs->has_type(target_type))) {
            target_type->error("Target type `%s' of the `in' mapping is not "
              "present on the list of incoming messages in user port type "
              "`%s'", target_type->get_typename().c_str(),
              my_type->get_typename().c_str());
          }
        }
      }
    } else if (provider_body && provider_body->in_msgs) {
      error("Missing `in' mappings to handle the incoming message types of "
        "provider port type `%s'", provider_type->get_typename().c_str());
    }

    // checking the outgoing mappings
    if (out_mappings) {
      Error_Context cntxt2(out_mappings, "In `out' mappings");
      out_mappings->chk();
      // checking source types
      if (out_msgs) {
        // check if all source types are present on the `out' list
        size_t nof_mappings = out_mappings->get_nof_mappings();
        for (size_t i = 0; i < nof_mappings; i++) {
          Type *source_type =
            out_mappings->get_mapping_byIndex(i)->get_source_type();
          if (!out_msgs->has_type(source_type)) {
            source_type->error("Source type `%s' of the `out' mapping is not "
              "present on the list of outgoing messages in user port type `%s'",
              source_type->get_typename().c_str(),
              my_type->get_typename().c_str());
          }
        }
        // check if all types of the `out' list are handled by the mappings
        size_t nof_msgs = out_msgs->get_nof_types();
        for (size_t i = 0; i < nof_msgs; i++) {
          Type *msg_type = out_msgs->get_type_byIndex(i);
          if (!out_mappings->has_mapping_for_type(msg_type)) {
            out_mappings->error("Outgoing message type `%s' of user port type "
              "`%s' is not handled by the outgoing mappings",
              msg_type->get_typename().c_str(),
              my_type->get_typename().c_str());
          }
        }
      } else {
        out_mappings->error("Invalid outgoing mappings. User port type "
          "`%s' does not have outgoing message types",
          my_type->get_typename().c_str());
      }
      // checking target types
      if (provider_body) {
        size_t nof_mappings = out_mappings->get_nof_mappings();
        for (size_t i = 0; i < nof_mappings; i++) {
          TypeMapping *mapping = out_mappings->get_mapping_byIndex(i);
          size_t nof_targets = mapping->get_nof_targets();
          for (size_t j = 0; j < nof_targets; j++) {
            Type *target_type =
              mapping->get_target_byIndex(j)->get_target_type();
            if (target_type && (!provider_body->out_msgs ||
                !provider_body->out_msgs->has_type(target_type))) {
              target_type->error("Target type `%s' of the `out' mapping is "
                "not present on the list of outgoing messages in provider "
                "port type `%s'", target_type->get_typename().c_str(),
                provider_type->get_typename().c_str());
            }
          }
        }
      }
    } else if (out_msgs) {
      error("Missing `out' mappings to handle the outgoing message types of "
        "user port type `%s'", my_type->get_typename().c_str());
    }

    // checking the compatibility of signature lists
    if (provider_body) {
      if (in_sigs) {
        size_t nof_sigs = in_sigs->get_nof_types();
        for (size_t i = 0; i < nof_sigs; i++) {
          Type *t_sig = in_sigs->get_type_byIndex(i);
          if (!provider_body->in_sigs ||
              !provider_body->in_sigs->has_type(t_sig)) {
            Type *t_sig_last = t_sig->get_type_refd_last();
            if (!t_sig_last->is_nonblocking_signature() ||
                t_sig_last->get_signature_exceptions())
              t_sig->error("Incoming signature `%s' of user port type `%s' is "
                "not present on the list of incoming signatures in provider "
                "port type `%s'", t_sig->get_typename().c_str(),
                my_type->get_typename().c_str(),
                provider_type->get_typename().c_str());
          }
        }
      }

      if (provider_body->in_sigs) {
        size_t nof_sigs = provider_body->in_sigs->get_nof_types();
        for (size_t i = 0; i < nof_sigs; i++) {
          Type *t_sig = provider_body->in_sigs->get_type_byIndex(i);
          if (!in_sigs || !in_sigs->has_type(t_sig)) {
            error("Incoming signature `%s' of provider port type `%s' is not "
              "present on the list of incoming signatures in user port type "
              "`%s'", t_sig->get_typename().c_str(),
              provider_type->get_typename().c_str(),
              my_type->get_typename().c_str());
          }
        }
      }

      if (out_sigs) {
        size_t nof_sigs = out_sigs->get_nof_types();
        for (size_t i = 0; i < nof_sigs; i++) {
          Type *t_sig = out_sigs->get_type_byIndex(i);
          if (!provider_body->out_sigs ||
              !provider_body->out_sigs->has_type(t_sig)) {
            t_sig->error("Outgoing signature `%s' of user port type `%s' is "
              "not present on the list of outgoing signatures in provider port "
              "type `%s'", t_sig->get_typename().c_str(),
              my_type->get_typename().c_str(),
              provider_type->get_typename().c_str());
          }
        }
      }

      if (provider_body->out_sigs) {
        size_t nof_sigs = provider_body->out_sigs->get_nof_types();
        for (size_t i = 0; i < nof_sigs; i++) {
          Type *t_sig = provider_body->out_sigs->get_type_byIndex(i);
          if (!out_sigs || !out_sigs->has_type(t_sig)) {
            Type *t_sig_last = t_sig->get_type_refd_last();
            if (!t_sig_last->is_nonblocking_signature() ||
                t_sig_last->get_signature_exceptions())
              error("Outgoing signature `%s' of provider port type `%s' is not "
                "present on the list of outgoing signatures in user port type "
                "`%s'", t_sig->get_typename().c_str(),
                provider_type->get_typename().c_str(),
                my_type->get_typename().c_str());
          }
        }
      }
    }
  }

  void PortTypeBody::chk()
  {
    if (checked) return;
    checked = true;

    // checking 'all' directives
    if (inout_all) {
      if (in_all) {
        warning("Redundant `in all' and `inout all' directives");
        in_all = false;
      }
      if (out_all) {
        warning("Redundant `out all' and `inout all' directives");
        out_all = false;
      }
      warning("Unsupported `inout all' directive was ignored");
    } else {
      if (in_all) warning("Unsupported `in all' directive was ignored");
      if (out_all) warning("Unsupported `out all' directive was ignored");
    }

    // checking message/signature lists
    if (in_list) {
      Error_Context cntxt(in_list, "In `in' list");
      chk_list(in_list, true, false);
    }
    if (out_list) {
      Error_Context cntxt(out_list, "In `out' list");
      chk_list(out_list, false, true);
    }
    if (inout_list) {
      Error_Context cntxt(inout_list, "In `inout' list");
      chk_list(inout_list, true, true);
    }
  }

  void PortTypeBody::chk_attributes(Ttcn::WithAttribPath *w_attrib_path)
  {
    if (!w_attrib_path || !checked || !my_type)
      FATAL_ERROR("PortTypeBody::chk_attributes()");

    Ttcn::ExtensionAttributes * extarts = parse_extattributes(w_attrib_path);
    if (extarts != 0) { // NULL means parsing error
    size_t num_atrs = extarts->size();
    for (size_t k = 0; k < num_atrs; ++k) {
      ExtensionAttribute &ea = extarts->get(k);
      switch (ea.get_type()) {
      case ExtensionAttribute::PORT_API: { // internal or address
        const PortTypeBody::TestPortAPI_t api = ea.get_api();
        if (api == PortTypeBody::TP_INTERNAL) {
          switch (testport_type) {
          case PortTypeBody::TP_REGULAR:
            break;
          case PortTypeBody::TP_INTERNAL: {
            ea.warning("Duplicate attribute `internal'");
            break; }
          case PortTypeBody::TP_ADDRESS: {
            ea.error("Attributes `address' and `internal' "
              "cannot be used at the same time");
            break; }
          default:
            FATAL_ERROR("coding_attrib_parse(): invalid testport type");
          }
          set_testport_type(PortTypeBody::TP_INTERNAL);
        }
        else if (api == PortTypeBody::TP_ADDRESS) {
          switch (testport_type) {
          case PortTypeBody::TP_REGULAR:
            break;
          case PortTypeBody::TP_INTERNAL: {
            ea.error("Attributes `internal' and `address' "
              "cannot be used at the same time");
            break; }
          case PortTypeBody::TP_ADDRESS: {
            ea.warning("Duplicate attribute `address'");
            break; }
          default:
            FATAL_ERROR("coding_attrib_parse(): invalid testport type");
          }
          set_testport_type(PortTypeBody::TP_ADDRESS);
        }
        break; }

      case ExtensionAttribute::PORT_TYPE_PROVIDER:
        switch (port_type) {
        case PortTypeBody::PT_REGULAR:
          break;
        case PortTypeBody::PT_PROVIDER: {
          ea.warning("Duplicate attribute `provider'");
          break; }
        case PortTypeBody::PT_USER: {
          ea.error("Attributes `user' and `provider' "
            "cannot be used at the same time");
          break; }
        default:
          FATAL_ERROR("coding_attrib_parse(): invalid testport type");
        }
        add_provider_attribute();
        break;

      case ExtensionAttribute::PORT_TYPE_USER:
        switch (port_type) {
        case PortTypeBody::PT_REGULAR:
          break;
        case PortTypeBody::PT_PROVIDER: {
          ea.error("Attributes `provider' and `user' "
            "cannot be used at the same time");
          break; }
        case PortTypeBody::PT_USER: {
          ea.error("Duplicate attribute `user'");
          break; }
        default:
          FATAL_ERROR("coding_attrib_parse(): invalid testport type");
        }
        Reference *ref;
        TypeMappings *in;
        TypeMappings *out;
        ea.get_user(ref, in, out);
        add_user_attribute(ref, in, out);
        break;

      case ExtensionAttribute::ANYTYPELIST:
        break; // ignore it

      case ExtensionAttribute::NONE:
        break; // ignore, do not issue "wrong type" error

      default:
        ea.error("Port can only have the following extension attributes: "
          "'provider', 'user', 'internal' or 'address'");
        break;
      } // switch
    } // next attribute
    delete extarts;
    } // if not null

    if (port_type == PT_USER) chk_user_attribute();
    else if (testport_type == TP_ADDRESS) {
      Error_Context cntxt(this, "In extension attribute `address'");
      Common::Module *my_mod = my_type->get_my_scope()->get_scope_mod();
      if (!my_mod->get_address_type())
        error("Type `address' is not defined in module `%s'",
          my_mod->get_modid().get_dispname().c_str());
    }
  }

  bool PortTypeBody::is_connectable(PortTypeBody *p_other) const
  {
    if (!checked || !p_other || !p_other->checked)
      FATAL_ERROR("Type::is_connectable()");
    if (out_msgs) {
      if (!p_other->in_msgs) return false;
      size_t nof_msgs = out_msgs->get_nof_types();
      for (size_t i = 0; i < nof_msgs; i++) {
        if (!p_other->in_msgs->has_type(out_msgs->get_type_byIndex(i)))
          return false;
      }
    }
    if (out_sigs) {
      if (!p_other->in_sigs) return false;
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        if (!p_other->in_sigs->has_type(out_sigs->get_type_byIndex(i)))
          return false;
      }
    }
    return true;
  }

  void PortTypeBody::report_connection_errors(PortTypeBody *p_other) const
  {
    if (!checked || !my_type || !p_other || !p_other->checked ||
        !p_other->my_type)
      FATAL_ERROR("PortTypeBody::report_connection_errors()");
    const string& my_typename = my_type->get_typename();
    const char *my_typename_str = my_typename.c_str();
    const string& other_typename = p_other->my_type->get_typename();
    const char *other_typename_str = other_typename.c_str();
    if (out_msgs) {
      size_t nof_msgs = out_msgs->get_nof_types();
      for (size_t i = 0; i < nof_msgs; i++) {
        Type *t_msg = out_msgs->get_type_byIndex(i);
        if (!p_other->in_msgs || !p_other->in_msgs->has_type(t_msg)) {
          t_msg->note("Outgoing message type `%s' of port type `%s' is not "
            "present on the incoming list of port type `%s'",
            t_msg->get_typename().c_str(), my_typename_str, other_typename_str);
        }
      }
    }
    if (out_sigs) {
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        Type *t_sig = out_sigs->get_type_byIndex(i);
        if (!p_other->in_sigs || !p_other->in_sigs->has_type(t_sig)) {
          t_sig->note("Outgoing signature `%s' of port type `%s' is not "
            "present on the incoming list of port type `%s'",
            t_sig->get_typename().c_str(), my_typename_str, other_typename_str);
        }
      }
    }
  }

  bool PortTypeBody::is_mappable(PortTypeBody *p_other) const
  {
    if (!checked || !p_other || !p_other->checked)
      FATAL_ERROR("PortTypeBody::is_mappable()");
    // shortcut: every port type can be mapped to itself
    if (this == p_other) return true;
    // outgoing lists of this should be covered by outgoing lists of p_other
    if (out_msgs) {
      if (!p_other->out_msgs) return false;
      size_t nof_msgs = out_msgs->get_nof_types();
      for (size_t i = 0; i < nof_msgs; i++) {
        if (!p_other->out_msgs->has_type(out_msgs->get_type_byIndex(i)))
          return false;
      }
    }
    if (out_sigs) {
      if (!p_other->out_sigs) return false;
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        if (!p_other->out_sigs->has_type(out_sigs->get_type_byIndex(i)))
          return false;
      }
    }
    // incoming lists of p_other should be covered by incoming lists of this
    if (p_other->in_msgs) {
      if (!in_msgs) return false;
      size_t nof_msgs = p_other->in_msgs->get_nof_types();
      for (size_t i = 0; i < nof_msgs; i++) {
        if (!in_msgs->has_type(p_other->in_msgs->get_type_byIndex(i)))
          return false;
      }
    }
    if (p_other->in_sigs) {
      if (!in_sigs) return false;
      size_t nof_sigs = p_other->in_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        if (!in_sigs->has_type(p_other->in_sigs->get_type_byIndex(i)))
          return false;
      }
    }
    return true;
  }

  void PortTypeBody::report_mapping_errors(PortTypeBody *p_other) const
  {
    if (!checked || !my_type || !p_other || !p_other->checked ||
        !p_other->my_type) FATAL_ERROR("PortTypeBody::report_mapping_errors()");
    const string& my_typename = my_type->get_typename();
    const char *my_typename_str = my_typename.c_str();
    const string& other_typename = p_other->my_type->get_typename();
    const char *other_typename_str = other_typename.c_str();
    if (out_msgs) {
      size_t nof_msgs = out_msgs->get_nof_types();
      for (size_t i = 0; i < nof_msgs; i++) {
        Type *t_msg = out_msgs->get_type_byIndex(i);
        if (!p_other->out_msgs || !p_other->out_msgs->has_type(t_msg)) {
          t_msg->note("Outgoing message type `%s' of test component "
            "port type `%s' is not present on the outgoing list of "
            "system port type `%s'", t_msg->get_typename().c_str(),
            my_typename_str, other_typename_str);
        }
      }
    }
    if (out_sigs) {
      size_t nof_sigs = out_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        Type *t_sig = out_sigs->get_type_byIndex(i);
        if (!p_other->out_sigs || !p_other->out_sigs->has_type(t_sig)) {
          t_sig->note("Outgoing signature `%s' of test component port type "
            "`%s' is not present on the outgoing list of system port type "
            "`%s'", t_sig->get_typename().c_str(), my_typename_str,
            other_typename_str);
        }
      }
    }
    if (p_other->in_msgs) {
      size_t nof_msgs = p_other->in_msgs->get_nof_types();
      for (size_t i = 0; i < nof_msgs; i++) {
        Type *t_msg = p_other->in_msgs->get_type_byIndex(i);
        if (!in_msgs || !in_msgs->has_type(t_msg)) {
          t_msg->note("Incoming message type `%s' of system port type `%s' "
            "is not present on the incoming list of test component port type "
            "`%s'", t_msg->get_typename().c_str(), other_typename_str,
            my_typename_str);
        }
      }
    }
    if (p_other->in_sigs) {
      size_t nof_sigs = p_other->in_sigs->get_nof_types();
      for (size_t i = 0; i < nof_sigs; i++) {
        Type *t_sig = p_other->in_sigs->get_type_byIndex(i);
        if (!in_sigs || !in_sigs->has_type(t_sig)) {
          t_sig->note("Incoming signature `%s' of system port type `%s' is "
            "not present on the incoming list of test component port type "
            "`%s'", t_sig->get_typename().c_str(), other_typename_str,
            my_typename_str);
        }
      }
    }
  }

  void PortTypeBody::generate_code(output_struct *target)
  {
    if (!checked || !my_type) FATAL_ERROR("PortTypeBody::generate_code()");
    stringpool pool;
    port_def pdef;
    memset(&pdef, 0, sizeof(pdef));
    const string& t_genname = my_type->get_genname_own();
    pdef.name = t_genname.c_str();
    pdef.dispname = my_type->get_fullname().c_str();
    pdef.filename = pool.add(Identifier::name_2_ttcn(t_genname));
    Scope *my_scope = my_type->get_my_scope();
    const Identifier& modid = my_scope->get_scope_mod_gen()->get_modid();
    pdef.module_name = modid.get_name().c_str();
    pdef.module_dispname = modid.get_ttcnname().c_str();
    if (testport_type == TP_ADDRESS) {
      Type *t_address = get_address_type();
      if (!t_address) FATAL_ERROR("PortTypeBody::generate_code()");
      pdef.address_name = pool.add(t_address->get_genname_value(my_scope));
    } else pdef.address_name = NULL;

    if (in_msgs) {
      pdef.msg_in.nElements = in_msgs->get_nof_types();
      pdef.msg_in.elements = (port_msg_type*)
        Malloc(pdef.msg_in.nElements * sizeof(*pdef.msg_in.elements));
      for (size_t i = 0; i < pdef.msg_in.nElements; i++) {
        Type *type = in_msgs->get_type_byIndex(i);
        pdef.msg_in.elements[i].name =
          pool.add(type->get_genname_value(my_scope));
        pdef.msg_in.elements[i].dispname = pool.add(type->get_typename());
      }
    } else {
      pdef.msg_in.nElements = 0;
      pdef.msg_in.elements = NULL;
    }
    if (out_msgs) {
      pdef.msg_out.nElements = out_msgs->get_nof_types();
      pdef.msg_out.elements = (port_msg_mapped_type*)
        Malloc(pdef.msg_out.nElements * sizeof(*pdef.msg_out.elements));
      for (size_t i = 0; i < pdef.msg_out.nElements; i++) {
        Type *type = out_msgs->get_type_byIndex(i);
        port_msg_mapped_type *mapped_type = pdef.msg_out.elements + i;
        mapped_type->name = pool.add(type->get_genname_value(my_scope));
        mapped_type->dispname = pool.add(type->get_typename());
        if (port_type == PT_USER) {
          if (!out_mappings) FATAL_ERROR("PortTypeBody::generate_code()");
          TypeMapping *mapping = out_mappings->get_mapping_byType(type);
          mapped_type->nTargets = mapping->get_nof_targets();
          mapped_type->targets = (port_msg_type_mapping_target*)
            Malloc(mapped_type->nTargets * sizeof(*mapped_type->targets));
          for (size_t j = 0; j < mapped_type->nTargets; j++) {
            mapping->get_target_byIndex(j)->fill_type_mapping_target(
              mapped_type->targets + j, type, my_scope, pool);
            mapped_type->targets[j].target_index = static_cast<size_t>(-1);
          }
        } else {
          mapped_type->nTargets = 0;
          mapped_type->targets = NULL;
        }
      }
    } else {
      pdef.msg_out.nElements = 0;
      pdef.msg_out.elements = NULL;
    }

    if (in_sigs) {
      pdef.proc_in.nElements = in_sigs->get_nof_types();
      pdef.proc_in.elements = (port_proc_signature*)
        Malloc(pdef.proc_in.nElements * sizeof(*pdef.proc_in.elements));
      for (size_t i = 0; i < pdef.proc_in.nElements; i++) {
        Type *signature = in_sigs->get_type_byIndex(i)->get_type_refd_last();
        pdef.proc_in.elements[i].name =
          pool.add(signature->get_genname_value(my_scope));
        pdef.proc_in.elements[i].dispname =
          pool.add(signature->get_typename());
        pdef.proc_in.elements[i].is_noblock =
          signature->is_nonblocking_signature();
        pdef.proc_in.elements[i].has_exceptions =
          signature->get_signature_exceptions() ? TRUE : FALSE;
      }
    } else {
      pdef.proc_in.nElements = 0;
      pdef.proc_in.elements = NULL;
    }
    if (out_sigs) {
      pdef.proc_out.nElements = out_sigs->get_nof_types();
      pdef.proc_out.elements = (port_proc_signature*)
        Malloc(pdef.proc_out.nElements * sizeof(*pdef.proc_out.elements));
      for (size_t i = 0; i < pdef.proc_out.nElements; i++) {
        Type *signature = out_sigs->get_type_byIndex(i)->get_type_refd_last();
        pdef.proc_out.elements[i].name =
          pool.add(signature->get_genname_value(my_scope));
        pdef.proc_out.elements[i].dispname =
          pool.add(signature->get_typename());
        pdef.proc_out.elements[i].is_noblock =
          signature->is_nonblocking_signature();
        pdef.proc_out.elements[i].has_exceptions =
          signature->get_signature_exceptions() ? TRUE : FALSE;
      }
    } else {
      pdef.proc_out.nElements = 0;
      pdef.proc_out.elements = NULL;
    }

    switch (testport_type) {
    case TP_REGULAR:
      pdef.testport_type = NORMAL;
      break;
    case TP_INTERNAL:
      pdef.testport_type = INTERNAL;
      break;
    case TP_ADDRESS:
      pdef.testport_type = ADDRESS;
      break;
    default:
      FATAL_ERROR("PortTypeBody::generate_code()");
    }

    if (port_type == PT_USER) {
      pdef.port_type = USER;
      if (!provider_type) FATAL_ERROR("PortTypeBody::generate_code()");
      pdef.provider_name =
        pool.add(provider_type->get_genname_value(my_scope));
      PortTypeBody *provider_body = provider_type->get_PortBody();
      if (provider_body->in_msgs) {
        if (!in_mappings) // !this->in_msgs OK for an all-discard mapping
          FATAL_ERROR("PortTypeBody::generate_code()");
        pdef.provider_msg_in.nElements =
          provider_body->in_msgs->get_nof_types();
        pdef.provider_msg_in.elements = (port_msg_mapped_type*)
          Malloc(pdef.provider_msg_in.nElements *
            sizeof(*pdef.provider_msg_in.elements));
        for (size_t i = 0; i < pdef.provider_msg_in.nElements; i++) {
          Type *type = provider_body->in_msgs->get_type_byIndex(i);
          port_msg_mapped_type *mapped_type = pdef.provider_msg_in.elements + i;
          mapped_type->name = pool.add(type->get_genname_value(my_scope));
          mapped_type->dispname = pool.add(type->get_typename());
          TypeMapping *mapping = in_mappings->get_mapping_byType(type);
          mapped_type->nTargets = mapping->get_nof_targets();
          mapped_type->targets = (port_msg_type_mapping_target*)
            Malloc(mapped_type->nTargets * sizeof(*mapped_type->targets));
          for (size_t j = 0; j < mapped_type->nTargets; j++) {
            TypeMappingTarget *t_target = mapping->get_target_byIndex(j);
            pdef.has_sliding |= t_target->fill_type_mapping_target(
              mapped_type->targets + j, type, my_scope, pool);
            Type *target_type = t_target->get_target_type();
            if (target_type) {
              if (!in_msgs) FATAL_ERROR("PortTypeBody::generate_code()");
              mapped_type->targets[j].target_index =
                in_msgs->get_index_byType(target_type);
            } else {
              // the message will be discarded: fill in a dummy index
              mapped_type->targets[j].target_index = static_cast<size_t>(-1);
            }
          }
        }
      } else {
        pdef.provider_msg_in.nElements = 0;
        pdef.provider_msg_in.elements = NULL;
      }
    } else {
      // "internal provider" is the same as "internal"
      if (port_type == PT_PROVIDER && testport_type != TP_INTERNAL)
        pdef.port_type = PROVIDER;
      else pdef.port_type = REGULAR;
      pdef.provider_name = NULL;
      pdef.provider_msg_in.nElements = 0;
      pdef.provider_msg_in.elements = NULL;
    }

    defPortClass(&pdef, target);
    if (generate_skeleton && testport_type != TP_INTERNAL &&
        port_type != PT_USER) generateTestPortSkeleton(&pdef);

    Free(pdef.msg_in.elements);
    for (size_t i = 0; i < pdef.msg_out.nElements; i++)
      Free(pdef.msg_out.elements[i].targets);
    Free(pdef.msg_out.elements);
    Free(pdef.proc_in.elements);
    Free(pdef.proc_out.elements);
    for (size_t i = 0; i < pdef.provider_msg_in.nElements; i++)
      Free(pdef.provider_msg_in.elements[i].targets);
    Free(pdef.provider_msg_in.elements);
  }

  void PortTypeBody::dump(unsigned level) const
  {
    switch (operation_mode) {
    case PO_MESSAGE:
      DEBUG(level, "mode: message");
      break;
    case PO_PROCEDURE:
      DEBUG(level, "mode: procedure");
      break;
    case PO_MIXED:
      DEBUG(level, "mode: mixed");
      break;
    default:
      DEBUG(level, "mode: <invalid>");
    }
    if (in_list) {
      DEBUG(level, "in list:");
      in_list->dump(level + 1);
    }
    if (in_all) DEBUG(level, "in all");
    if (out_list) {
      DEBUG(level, "out list:");
      out_list->dump(level + 1);
    }
    if (out_all) DEBUG(level, "out all");
    if (inout_list) {
      DEBUG(level, "inout list:");
      inout_list->dump(level + 1);
    }
    if (inout_all) DEBUG(level, "inout all");
    if (in_msgs) {
      DEBUG(level, "incoming messages:");
      in_msgs->dump(level + 1);
    }
    if (out_msgs) {
      DEBUG(level, "outgoing messages:");
      out_msgs->dump(level + 1);
    }
    if (in_sigs) {
      DEBUG(level, "incoming signatures:");
      in_sigs->dump(level + 1);
    }
    if (out_sigs) {
      DEBUG(level, "outgoing signatures:");
      out_sigs->dump(level + 1);
    }
    switch (testport_type) {
    case TP_REGULAR:
      break;
    case TP_INTERNAL:
      DEBUG(level, "attribute: internal");
      break;
    case TP_ADDRESS:
      DEBUG(level, "attribute: address");
      break;
    default:
      DEBUG(level, "attribute: <unknown>");
    }
    switch (port_type) {
    case PT_REGULAR:
      break;
    case PT_PROVIDER:
      DEBUG(level, "attribute: provider");
      break;
    case PT_USER:
      DEBUG(level, "attribute: user");
      break;
    default:
      DEBUG(level, "attribute: <unknown>");
    }
    if (provider_ref)
      DEBUG(level, "provider type: %s", provider_ref->get_dispname().c_str());
    if (in_mappings) {
      DEBUG(level, "in mappings:");
      in_mappings->dump(level + 1);
    }
    if (out_mappings) {
      DEBUG(level, "out mappings:");
      out_mappings->dump(level + 1);
    }
  }

  // =================================
  // ===== ExtensionAttribute
  // =================================
  static const string version_template("RnXnn");

  void ExtensionAttribute::check_product_number(const char* ABCClass, int type_number, int sequence) {
    if (NULL == ABCClass && 0 == type_number && 0 == sequence) {
      return;
    }

    int ABCLength = strlen(ABCClass);
    if (ABCLength < 3 || ABCLength > 5) {
      // incorrect ABC number
      type_ = NONE;
      return;
    }

    if (type_number >= 1000) {
      type_ = NONE;
      return;
    }

    if (sequence >= 10000 ) {
      type_ = NONE;
    }
  }

  void ExtensionAttribute::parse_version(Identifier *ver) {
    unsigned int rel, bld = 99;
    char patch[4] = {0,0,0,0};
    // The limit of max three characters is an optimization: any more than two
    // is already an error and we can quit scanning. We can't limit the field
    // to two by using "%2[...] because then we can't distinguish between R1AA
    // and R1AAA. The switch below makes the distinction between one character
    // like R1A (valid), two characters like R1AA (also valid), and
    // R1AAA (invalid).
    // in addition after the RmXnn part some characters may come (needed by TitanSim)
    char extra_junk[64];
    int scanned = sscanf(ver->get_dispname().c_str(), "R%u%3[A-HJ-NS-VX-Z]%u%63s",
      &rel, patch, &bld, extra_junk);
    value_.version_.extra_ = NULL;
    switch (scanned * (patch[2]==0)) {
    case 4: // all fields + titansim's extra stuff scanned
      if (strlen(extra_junk)>0) {
        value_.version_.extra_ = mprintf("%s", extra_junk);
      }
    case 3: // all fields scanned
    case 2: // 2 scanned, bld remained unmodified
      value_.version_.release_ = rel;
      value_.version_.build_ = bld;
      switch (patch[0]) {
      case 'A': case 'B': case 'C': case 'D':
      case 'E': case 'F': case 'G': case 'H':
        value_.version_.patch_ = patch[0] - 'A';
        break;
      case 'J': case 'K': case 'L': case 'M': case 'N':
        value_.version_.patch_ = patch[0] - 'A' - 1;
        break;
      case 'S': case 'T': case 'U': case 'V':
        value_.version_.patch_ = patch[0] - 'A' - 5;
        break;
      case 'X': case 'Y': case 'Z':
        value_.version_.patch_ = patch[0] - 'A' - 6;
        break;

      case 'I': case 'W': // forbidden
      case 'O': case 'P': case 'Q': case 'R': // forbidden
      default: // incorrect
        type_ = NONE;
        break;
      }
      break;
    default: // there was an error
      type_ = NONE;
      break;
    }
  }

  ExtensionAttribute::ExtensionAttribute(const char* ABCClass, int type_number,
    int sequence, int suffix, Identifier *ver)
  : Location(), type_(VERSION), value_()
  {
    if (ver == NULL) FATAL_ERROR("ExtensionAttribute::ExtensionAttribute()");
    value_.version_.module_ = NULL;

    check_product_number(ABCClass, type_number, sequence);
    parse_version(ver);
    if (type_ != NONE) {
      value_.version_.productNumber_ =
        NULL == ABCClass ? NULL : mprintf("%s %d %d", ABCClass, type_number, sequence);
      value_.version_.suffix_ = suffix;
      delete ver; // "took care of it"
    }
  }

  ExtensionAttribute::ExtensionAttribute(Identifier *mod, const char* ABCClass,
    int type_number, int sequence, int suffix, Identifier *ver)
  : Location(), type_(REQUIRES), value_()
  {
    if (mod == NULL || ver == NULL)
      FATAL_ERROR("ExtensionAttribute::ExtensionAttribute()");
    // store the module identifier
    value_.version_.module_ = mod;

    check_product_number(ABCClass, type_number, sequence);
    parse_version(ver);
    if (type_ == NONE) { // parse_version reported an error
      value_.version_.module_ = NULL; // disown it; caller will free
      value_.version_.suffix_ = suffix;
    } else {
      value_.version_.productNumber_ =
        NULL == ABCClass ? NULL : mprintf("%s %d %d", ABCClass, type_number, sequence);
      value_.version_.suffix_ = suffix;
      delete ver;
    }
  }

  ExtensionAttribute::ExtensionAttribute(const char* ABCClass, int type_number,
    int sequence, int suffix, Identifier* ver, extension_t et)
  : Location(), type_(et), value_()
  {
    if (ver == NULL) FATAL_ERROR("ExtensionAttribute::ExtensionAttribute()");

    switch (et) {
    case REQ_TITAN:
      check_product_number(ABCClass, type_number, sequence);
      parse_version(ver);
      if (type_ != NONE) {
        value_.version_.productNumber_ =
          NULL == ABCClass ? NULL : mprintf("%s %d %d", ABCClass, type_number, sequence);
        value_.version_.suffix_ = suffix;
        delete ver; // "took care of it"
      } else {
        value_.version_.suffix_ = suffix;
      }
      break;

    case VERSION_TEMPLATE: // RnXnn without the <>, must match exactly
      if (ver->get_name() == version_template) {
        value_.version_.productNumber_ = NULL;
        value_.version_.suffix_ = UINT_MAX;
        value_.version_.release_ = UINT_MAX;
        value_.version_.patch_   = UINT_MAX;
        value_.version_.build_   = UINT_MAX;
        delete ver;
        return;
      }
      else type_ = NONE;
      break;

    default:
      FATAL_ERROR("ExtensionAttribute::ExtensionAttribute()");
      break;
    }

  }


  ExtensionAttribute::~ExtensionAttribute()
  {
    switch (type_) {
    case NONE:
      break; // nothing to do, data has been extracted
      // TODO perhaps NONE should be the only acceptable option?
      // (no unprocessed attribute allowed)
    case PROTOTYPE:
      break;
    case ENCODE: case DECODE:
      delete value_.encdec_.s_;
      break;
    case PORT_API:
      break;
    case PORT_TYPE_USER:
      delete value_.user_.ref_;
      delete value_.user_.inmaps_;
      delete value_.user_.outmaps_;
      break;
    case PORT_TYPE_PROVIDER:
      break;
    case ERRORBEHAVIOR:
      delete value_.ebl_;
      break;
    case ANYTYPELIST:
      delete value_.anytypes_;
      break;
    case ENCDECVALUE:
      delete value_.encdecvalue_.inmaps_;
      delete value_.encdecvalue_.outmaps_;
      break;
    case VERSION:
    case VERSION_TEMPLATE:
      Free(value_.version_.productNumber_);
      Free(value_.version_.extra_);
      break;
    case REQUIRES:
      Free(value_.version_.productNumber_);
      Free(value_.version_.extra_);
      delete value_.version_.module_;
      break;
    case REQ_TITAN:
      Free(value_.version_.productNumber_);
      Free(value_.version_.extra_);
      break;
    case TRANSPARENT:
      break;
    case PRINTING:
      delete value_.pt_;
      break;
    default: // can't happen
      FATAL_ERROR("ExtensionAttribute::~ExtensionAttribute(%X)", type_);
    }
  }

  Types *ExtensionAttribute::get_types()
  {
    if (type_ != ANYTYPELIST) FATAL_ERROR("ExtensionAttribute::get_types()");
    type_ = NONE;
    return value_.anytypes_;
  }

  Ttcn::Def_Function_Base::prototype_t ExtensionAttribute::get_proto()
  {
    if (type_ != PROTOTYPE) FATAL_ERROR("ExtensionAttribute::get_proto()");
    type_ = NONE;
    return value_.proto_;
  }

  void ExtensionAttribute::get_encdec_parameters(
    Type::MessageEncodingType_t &p_encoding_type, string *&p_encoding_options)
  {
    if (type_ != ENCODE && type_ != DECODE)
      FATAL_ERROR("ExtensionAttribute::get_encdec_parameters()");
    type_ = NONE;
    p_encoding_type = value_.encdec_.mess_;
    p_encoding_options = value_.encdec_.s_;
    const_cast<ExtensionAttribute*>(this)->value_.encdec_.s_ = 0;
  }

  ErrorBehaviorList *ExtensionAttribute::get_eb_list()
  {
    if (type_ != ERRORBEHAVIOR)
      FATAL_ERROR("ExtensionAttribute::get_eb_list()");
    type_ = NONE;
    ErrorBehaviorList *retval = value_.ebl_;
    const_cast<ExtensionAttribute*>(this)->value_.ebl_ = 0;
    return retval;
  }
  
  PrintingType *ExtensionAttribute::get_printing()
  {
    if (type_ != PRINTING)
      FATAL_ERROR("ExtensionAttribute::get_printing()");
    type_ = NONE;
    PrintingType *retval =  value_.pt_;
    const_cast<ExtensionAttribute*>(this)->value_.pt_ = 0;
    return retval;
  }

  PortTypeBody::TestPortAPI_t ExtensionAttribute::get_api()
  {
    if (type_ != PORT_API)
      FATAL_ERROR("ExtensionAttribute::get_api()");
    type_ = NONE;
    return value_.api_;
  }

  void ExtensionAttribute::get_user(Reference *&ref,
    TypeMappings *&in, TypeMappings *&out)
  {
    if (type_ != PORT_TYPE_USER)
      FATAL_ERROR("ExtensionAttribute::get_user()");
    type_ = NONE;

    ref = value_.user_.ref_;
    in  = value_.user_.inmaps_;
    out = value_.user_.outmaps_;

    value_.user_.ref_     = 0;
    value_.user_.inmaps_  = 0;
    value_.user_.outmaps_ = 0;
  }

  void ExtensionAttribute::get_encdecvalue_mappings(
    TypeMappings *&in, TypeMappings *&out)
  {
    if (type_ != ENCDECVALUE)
      FATAL_ERROR("ExtensionAttribute::get_encdecvalue_mappings()");

    in = value_.encdecvalue_.inmaps_;
    out = value_.encdecvalue_.outmaps_;
  }

  //FIXME ot is update -elni kell.
  Common::Identifier *ExtensionAttribute::get_id(
      char*& product_number, unsigned int& suffix,
    unsigned int& rel, unsigned int& patch, unsigned int& bld, char*& extra)
  {
    if ( type_ != REQUIRES && type_ != REQ_TITAN
      && type_ != VERSION  && type_ != VERSION_TEMPLATE) {
      FATAL_ERROR("ExtensionAttribute::get_id()");
    }
    product_number = value_.version_.productNumber_;
    suffix = value_.version_.suffix_;
    rel   = value_.version_.release_;
    patch = value_.version_.patch_;
    bld   = value_.version_.build_;
    extra = value_.version_.extra_;
    return  value_.version_.module_;
  }

  // =================================
  // ===== ExtensionAttributes
  // =================================
  void ExtensionAttributes::add(ExtensionAttribute *a)
  {
    vector<ExtensionAttribute>::add(a);
  }

  ExtensionAttributes::~ExtensionAttributes()
  {
    for (int i = size()-1; i >= 0; --i)
      delete operator [](i);
    clear();
  }

  void ExtensionAttributes::import(ExtensionAttributes *other)
  {
    size_t n = other->size();
    for (size_t i = 0; i < n; ++i) {
      vector<ExtensionAttribute>::add((*other)[i]);
    }
    other->clear();
  }

} // namespace Ttcn
