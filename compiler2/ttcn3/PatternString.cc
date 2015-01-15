///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "PatternString.hh"
#include "../../common/pattern.hh"
#include "../CompilerError.hh"
#include "../Code.hh"

#include "TtcnTemplate.hh"

namespace Ttcn {

  // =================================
  // ===== PatternString::ps_elem_t
  // =================================

  struct PatternString::ps_elem_t {
    enum kind_t {
      PSE_STR,
      PSE_REF,
      PSE_REFDSET
    } kind;
    union {
      string *str;
      Ttcn::Reference *ref;
    };
    ps_elem_t(kind_t p_kind, const string& p_str);
    ps_elem_t(kind_t p_kind, Ttcn::Reference *p_ref);
    ps_elem_t(const ps_elem_t& p);
    ~ps_elem_t();
    ps_elem_t* clone() const;
    void set_fullname(const string& p_fullname);
    void set_my_scope(Scope *p_scope);
    void chk_ref(PatternString::pstr_type_t pstr_type, Type::expected_value_t expected_value);
    void set_code_section(GovernedSimple::code_section_t p_code_section);
  };

  PatternString::ps_elem_t::ps_elem_t(kind_t p_kind, const string& p_str)
    : kind(p_kind)
  {
    str = new string(p_str);
  }

  PatternString::ps_elem_t::ps_elem_t(kind_t p_kind, Ttcn::Reference *p_ref)
    : kind(p_kind)
  {
    if (!p_ref) FATAL_ERROR("PatternString::ps_elem_t::ps_elem_t()");
    ref = p_ref;
  }

  PatternString::ps_elem_t::~ps_elem_t()
  {
    switch(kind) {
    case PSE_STR:
      delete str;
      break;
    case PSE_REF:
    case PSE_REFDSET:
      delete ref;
      break;
    } // switch kind
  }

  PatternString::ps_elem_t* PatternString::ps_elem_t::clone() const
  {
    FATAL_ERROR("PatternString::ps_elem_t::clone");
  }

  void PatternString::ps_elem_t::set_fullname(const string& p_fullname)
  {
    switch(kind) {
    case PSE_REF:
    case PSE_REFDSET:
      ref->set_fullname(p_fullname);
      break;
    default:
      ;
    } // switch kind
  }

  void PatternString::ps_elem_t::set_my_scope(Scope *p_scope)
  {
    switch(kind) {
    case PSE_REF:
    case PSE_REFDSET:
      ref->set_my_scope(p_scope);
      break;
    default:
      ;
    } // switch kind
  }

  void PatternString::ps_elem_t::chk_ref(PatternString::pstr_type_t pstr_type, Type::expected_value_t expected_value)
  {
    if (kind != PSE_REF) FATAL_ERROR("PatternString::ps_elem_t::chk_ref()");
    Value* v = 0;
    Value* v_last = 0;
    Common::Assignment* ass = ref->get_refd_assignment();
    if (!ass)
      return;
    Ttcn::FieldOrArrayRefs* t_subrefs = ref->get_subrefs();
    Type* ref_type = ass->get_Type()->get_type_refd_last()->get_field_type(
      t_subrefs, expected_value);
    Type::typetype_t tt;
    switch (pstr_type) {
      case PatternString::CSTR_PATTERN:
        tt = Type::T_CSTR;
        if (ref_type->get_typetype() != Type::T_CSTR)
          TTCN_pattern_error("Type of the referenced %s '%s' should be "
            "'charstring'", ass->get_assname(), ref->get_dispname().c_str());
        break;
      case PatternString::USTR_PATTERN:
        tt = ref_type->get_typetype();
        if (tt != Type::T_CSTR && tt != Type::T_USTR)
          TTCN_pattern_error("Type of the referenced %s '%s' should be either "
            "'charstring' or 'universal charstring'", ass->get_assname(),
            ref->get_dispname().c_str());
        break;
      default:
        FATAL_ERROR("Unknown pattern string type");
    }
    Type* refcheckertype = Type::get_pooltype(tt);
    switch (ass->get_asstype()) {
    case Common::Assignment::A_MODULEPAR_TEMP:
    case Common::Assignment::A_VAR_TEMPLATE:
      // error reporting moved up
      break;
    case Common::Assignment::A_TEMPLATE: {
      Template* templ = ass->get_Template();
      refcheckertype->chk_this_template_ref(templ);
      refcheckertype->chk_this_template_generic(templ, INCOMPLETE_ALLOWED,
        OMIT_ALLOWED, ANY_OR_OMIT_ALLOWED, SUB_CHK, NOT_IMPLICIT_OMIT, 0);
      switch (templ->get_templatetype()) {
      case Template::SPECIFIC_VALUE:
        v_last = templ->get_specific_value();
        break;
      case Template::CSTR_PATTERN: {
        Ttcn::PatternString* ps = templ->get_cstr_pattern();
        if (!ps->has_refs())
          v_last = ps->get_value();
        break; }
      case Template::USTR_PATTERN: {
        Ttcn::PatternString* ps = templ->get_ustr_pattern();
        if (!ps->has_refs())
          v_last = ps->get_value();
        break; }
      default:
        TTCN_pattern_error("Unable to resolve referenced '%s' to character "
          "string type. '%s' template cannot be used.",
          ref->get_dispname().c_str(), templ->get_templatetype_str());
        break;
      }
      break; }
    default: {
      Reference *t_ref = ref->clone();
      t_ref->set_location(*ref);
      v = new Value(Value::V_REFD, t_ref);
      v->set_my_governor(refcheckertype);
      v->set_my_scope(ref->get_my_scope());
      v->set_location(*ref);
      refcheckertype->chk_this_value(v, 0, expected_value,
        INCOMPLETE_NOT_ALLOWED, OMIT_NOT_ALLOWED, SUB_CHK);
      v_last = v->get_value_refd_last();
    }
    }
    if (v_last && (v_last->get_valuetype() == Value::V_CSTR ||
      v_last->get_valuetype() == Value::V_USTR)) {
      // the reference points to a constant
      // substitute the reference with the known value
      delete ref;
      kind = PSE_STR;
      if (v_last->get_valuetype() == Value::V_CSTR)
        str = new string(v_last->get_val_str());
      else
        str = new string(v_last->get_val_ustr().get_stringRepr_for_pattern());
    }
    delete v;
  }

  void PatternString::ps_elem_t::set_code_section
    (GovernedSimple::code_section_t p_code_section)
  {
    switch(kind) {
    case PSE_REF:
    case PSE_REFDSET:
      ref->set_code_section(p_code_section);
      break;
    default:
      ;
    } // switch kind
  }

  // =================================
  // ===== PatternString
  // =================================

  PatternString::PatternString(const PatternString& p)
    : Node(p), my_scope(0), pattern_type(p.pattern_type)
  {
    size_t nof_elems = p.elems.size();
    for (size_t i = 0; i < nof_elems; i++) elems.add(p.elems[i]->clone());
  }

  PatternString::ps_elem_t *PatternString::get_last_elem() const
  {
    if (elems.empty()) return 0;
    ps_elem_t *last_elem = elems[elems.size() - 1];
    if (last_elem->kind == ps_elem_t::PSE_STR) return last_elem;
    else return 0;
  }

  PatternString::~PatternString()
  {
    size_t nof_elems = elems.size();
    for (size_t i = 0; i < nof_elems; i++) delete elems[i];
    elems.clear();
    delete cstr_value;
  }

  PatternString *PatternString::clone() const
  {
    return new PatternString(*this);
  }

  void PatternString::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    size_t nof_elems = elems.size();
    for(size_t i = 0; i < nof_elems; i++) elems[i]->set_fullname(p_fullname);
  }

  void PatternString::set_my_scope(Scope *p_scope)
  {
    my_scope = p_scope;
    size_t nof_elems = elems.size();
    for (size_t i = 0; i < nof_elems; i++) elems[i]->set_my_scope(p_scope);
  }

  void PatternString::set_code_section
    (GovernedSimple::code_section_t p_code_section)
  {
    size_t nof_elems = elems.size();
    for (size_t i = 0; i < nof_elems; i++)
      elems[i]->set_code_section(p_code_section);
  }

  void PatternString::addChar(char c)
  {
    ps_elem_t *last_elem = get_last_elem();
    if (last_elem) *last_elem->str += c;
    else elems.add(new ps_elem_t(ps_elem_t::PSE_STR, string(c)));
  }

  void PatternString::addString(const char *p_str)
  {
    ps_elem_t *last_elem = get_last_elem();
    if (last_elem) *last_elem->str += p_str;
    else elems.add(new ps_elem_t(ps_elem_t::PSE_STR, string(p_str)));
  }

  void PatternString::addString(const string& p_str)
  {
    ps_elem_t *last_elem = get_last_elem();
    if (last_elem) *last_elem->str += p_str;
    else elems.add(new ps_elem_t(ps_elem_t::PSE_STR, p_str));
  }

  void PatternString::addRef(Ttcn::Reference *p_ref)
  {
    elems.add(new ps_elem_t(ps_elem_t::PSE_REF, p_ref));
  }

  void PatternString::addRefdCharSet(Ttcn::Reference *p_ref)
  {
    elems.add(new ps_elem_t(ps_elem_t::PSE_REFDSET, p_ref));
  }

  string PatternString::get_full_str() const
  {
    string s;
    for(size_t i=0; i<elems.size(); i++) {
      ps_elem_t *pse=elems[i];
      switch(pse->kind) {
      case ps_elem_t::PSE_STR:
        s+=*pse->str;
        break;
      case ps_elem_t::PSE_REFDSET:
        s+="\\N";
        /* no break */
      case ps_elem_t::PSE_REF:
        s+='{';
        s+=pse->ref->get_dispname();
        s+='}';
      } // switch kind
    } // for
    return s;
  }

  void PatternString::set_pattern_type(pstr_type_t p_type) {
    pattern_type = p_type;
  }

  PatternString::pstr_type_t PatternString::get_pattern_type() const {
    return pattern_type;
  }

  bool PatternString::has_refs() const
  {
    for (size_t i = 0; i < elems.size(); i++) {
      switch (elems[i]->kind) {
      case ps_elem_t::PSE_REF:
      case ps_elem_t::PSE_REFDSET:
        return true;
      default:
        break;
      }
    }
    return false;
  }

  void PatternString::chk_refs(Type::expected_value_t expected_value)
  {
    for(size_t i=0; i<elems.size(); i++) {
      ps_elem_t *pse=elems[i];
      switch(pse->kind) {
      case ps_elem_t::PSE_STR:
        break;
      case ps_elem_t::PSE_REFDSET:
        /* actually, not supported */
        break;
      case ps_elem_t::PSE_REF:
        pse->chk_ref(pattern_type, expected_value);
        break;
      } // switch kind
    } // for
  }

  /** \todo implement */
  void PatternString::chk_recursions(ReferenceChain&)
  {

  }

  void PatternString::chk_pattern()
  {
    string str;
    for (size_t i = 0; i < elems.size(); i++) {
      ps_elem_t *pse = elems[i];
      if (pse->kind != ps_elem_t::PSE_STR)
	FATAL_ERROR("PatternString::chk_pattern()");
      str += *pse->str;
    }
    char* posix_str = 0;
    switch (pattern_type) {
      case CSTR_PATTERN:
        posix_str = TTCN_pattern_to_regexp(str.c_str());
        break;
      case USTR_PATTERN:
        posix_str = TTCN_pattern_to_regexp_uni(str.c_str());
    }
    Free(posix_str);
  }

  bool PatternString::chk_self_ref(Common::Assignment *lhs)
  {
    for (size_t i = 0, e = elems.size(); i < e; ++i) {
      ps_elem_t *pse = elems[i];
      switch (pse->kind) {
      case ps_elem_t::PSE_STR:
        break;
      case ps_elem_t::PSE_REFDSET:
        /* actually, not supported */
        break;
      case ps_elem_t::PSE_REF: {
        Ttcn::Assignment *ass = pse->ref->get_refd_assignment();
        if (ass == lhs) return true;
        break; }
      } // switch
    }
    return false;
  }

  void PatternString::join_strings()
  {
    // points to the previous string element otherwise it is NULL
    ps_elem_t *prev_str = 0;
    for (size_t i = 0; i < elems.size(); ) {
      ps_elem_t *pse = elems[i];
      if (pse->kind == ps_elem_t::PSE_STR) {
        const string& str = *pse->str;
	if (str.size() > 0) {
	  // the current element is a non-empty string
	  if (prev_str) {
	    // append str to prev_str and drop pse
	    *prev_str->str += str;
	    delete pse;
	    elems.replace(i, 1);
	    // don't increment i
	  } else {
	    // keep pse for the next iteration
	    prev_str = pse;
	    i++;
	  }
	} else {
	  // the current element is an empty string
	  // simply drop it
	  delete pse;
	  elems.replace(i, 1);
	  // don't increment i
	}
      } else {
        // pse is not a string
	// forget prev_str
	prev_str = 0;
	i++;
      }
    }
  }

  string PatternString::create_charstring_literals(Common::Module *p_mod)
  {
    /* The cast is there for the benefit of OPTIONAL<CHARSTRING>, because
     * it doesn't have operator+(). Only the first member needs the cast
     * (the others will be automagically converted to satisfy
     * CHARSTRING::operator+(const CHARSTRING&) ) */
    string s;
    if (pattern_type == CSTR_PATTERN)
      s = "CHARSTRING_template(STRING_PATTERN, (CHARSTRING)";
    else
      s = "UNIVERSAL_CHARSTRING_template(STRING_PATTERN, (CHARSTRING)";
    size_t nof_elems = elems.size();
    if (nof_elems > 0) {
      // the pattern is not empty
      for (size_t i = 0; i < nof_elems; i++) {
	if (i > 0) s += " + ";
	ps_elem_t *pse = elems[i];
	switch (pse->kind) {
	case ps_elem_t::PSE_STR:
          s += p_mod->add_charstring_literal(*pse->str);
          break;
	case ps_elem_t::PSE_REFDSET:
          /* actually, not supported */
          FATAL_ERROR("PatternString::create_charstring_literals()");
          break;
	case ps_elem_t::PSE_REF: {
	  expression_struct expr;
	  Code::init_expr(&expr);
	  pse->ref->generate_code(&expr);
	  if (expr.preamble || expr.postamble)
	    FATAL_ERROR("PatternString::create_charstring_literals()");
          s += expr.expr;
          Common::Assignment* assign = pse->ref->get_refd_assignment();

          if ((assign->get_asstype() == Common::Assignment::A_TEMPLATE
            || assign->get_asstype() == Common::Assignment::A_MODULEPAR_TEMP
            || assign->get_asstype() == Common::Assignment::A_VAR_TEMPLATE))
          {
            if ((assign->get_Type()->get_typetype() == Type::T_CSTR
              || assign->get_Type()->get_typetype() == Type::T_USTR)) {
              s += ".get_single_value()";
            }
            else {
              s += ".valueof()";
            }
          }

	  Code::free_expr(&expr);
	  break; }
	} // switch kind
      } // for
    } else {
      // empty pattern: create an empty string literal for it
      s += p_mod->add_charstring_literal(string());
    }
    s += ')';
    return s;
  }

  void PatternString::dump(unsigned level) const
  {
    DEBUG(level, "%s", get_full_str().c_str());
  }

  Common::Value* PatternString::get_value() {
    if (!cstr_value && !has_refs())
      cstr_value = new Common::Value(Common::Value::V_CSTR,
        new string(get_full_str()));
    return cstr_value;
  }

} // namespace Ttcn

  // =================================
  // ===== TTCN_pattern_XXXX
  // =================================

/* These functions are used by common charstring pattern parser. */

void TTCN_pattern_error(const char *fmt, ...)
{
  char *msg=mcopystr("Charstring pattern: ");
  msg=mputstr(msg, fmt);
  va_list args;
  va_start(args, fmt);
  Common::Error_Context::report_error(0, msg, args);
  va_end(args);
  Free(msg);
}

void TTCN_pattern_warning(const char *fmt, ...)
{
  char *msg=mcopystr("Charstring pattern: ");
  msg=mputstr(msg, fmt);
  va_list args;
  va_start(args, fmt);
  Common::Error_Context::report_warning(0, msg, args);
  va_end(args);
  Free(msg);
}
