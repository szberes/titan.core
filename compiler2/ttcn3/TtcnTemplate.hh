///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef _Ttcn_Template_HH
#define _Ttcn_Template_HH

#include "../Int.hh"
#include "../Value.hh"
#include "../vector.hh"
#include "AST_ttcn3.hh"
#include "Templatestuff.hh"

namespace Ttcn {

  using namespace Common;

  class PatternString;
  class ValueRange;
  class LengthRestriction;
  class Templates;
  class NamedTemplate;
  class NamedTemplates;
  class IndexedTemplate;
  class IndexedTemplates;

  /** Represents a (single or compound) template body. */
  class Template : public GovernedSimple {
  public:


    /** type of template */
    enum templatetype_t {
      TEMPLATE_ERROR, /**< erroneous template */
      TEMPLATE_NOTUSED, /**< not used symbol (-) */
      OMIT_VALUE, /**< omit */
      ANY_VALUE, /**< any value (?) */
      ANY_OR_OMIT, /**< any or omit (*) */
      SPECIFIC_VALUE, /**< specific value */
      TEMPLATE_REFD, /**< reference to another template */
      TEMPLATE_INVOKE, /** template returning invoke */
      TEMPLATE_LIST, /**< value list notation */
      NAMED_TEMPLATE_LIST, /**< assignment notation */
      INDEXED_TEMPLATE_LIST, /**< assignment notation with array indices */
      VALUE_LIST, /**< value list match */
      COMPLEMENTED_LIST, /**< complemented list match */
      VALUE_RANGE, /**< value range match */
      SUPERSET_MATCH, /**< superset match */
      SUBSET_MATCH, /**< subset match */
      PERMUTATION_MATCH, /**< permutation match */
      ALL_FROM, /**< "all from" as part of a larger list e.g. permutation, superset, subset, etc. */
      VALUE_LIST_ALL_FROM, /**< "all from" in a value list, itself in a larger list */
      BSTR_PATTERN, /**< bitstring pattern */
      HSTR_PATTERN, /**< hexstring pattern */
      OSTR_PATTERN, /**< octetstring pattern */
      CSTR_PATTERN, /**< character string pattern */
      USTR_PATTERN  /**< universal charstring pattern */
    };

    /** Status codes for the verification of template body completeness. */
    enum completeness_t {
      C_MUST_COMPLETE, /**< the body must be completely specified */
      C_MAY_INCOMPLETE, /**< the body may be incompletely specified */
      C_PARTIAL /**< some parts of the body may be incomplete,
                 * the others must be complete */
    };

  private:
    /** Determines type of the template. Only this field is used at
     *  ANY_VALUE, ANY_OR_OMIT and OMIT_VALUE templates */
    templatetype_t templatetype;

    /** This union holds the ``body'' of the template. The
     *  templatetype field is used to select between different
     *  containers. */
    union {
      /** Holds the concrete single value of the template for single
       *  types */
      Value *specific_value;
      struct {
        Ref_base *ref;
	Template *refd; /**< cache */
        Template *refd_last; /**< cache */
      } ref;
      /** Used for TEMPLATE_LIST, VALUE_LIST, COMPLEMENTED_LIST,
       * SUPERSET_MATCH, SUBSET_MATCH and PERMUTATION_MATCH constructs. */
      Templates *templates;
      /** Used by NAMED_TEMPLATE_LIST */
      NamedTemplates *named_templates;
      /** Used by INDEXED_TEMPLATE_LIST */
      IndexedTemplates *indexed_templates;
      /** Used by ALL_FROM */
      Template *all_from;
      /** This is used for VALUE_RANGE template upper and lower
       *  bound. */
      ValueRange *value_range;
      /** Pattern contains basic string templates containing the any
       *  element (?), any element or no element (*) wildcards .
       *  Used by BSTR_PATTERN, HSTR_PATTERN, OSTR_PATTERN */
      string *pattern;
      /** PatternString contains the character string pattern matching
       *  constructions (including references).
       *  Used by CSTR_PATTERN, USTR_PATTERN */
      PatternString *pstring;
      /** Used by TEMPLATE_INVOKE */
      struct {
        Value *v;
        Ttcn::ParsedActualParameters *t_list;
        Ttcn::ActualParList *ap_list;
      } invoke;
    } u;

    /** This points to the type of the template */
    Type *my_governor;

    /** Length restriction */
    LengthRestriction *length_restriction;

    /** Ifpresent flag. */
    bool is_ifpresent;

    /** Indicates whether it has been verified that the template is free of
     * matching symbols. */
    bool specific_value_checked;

    /** Indicates whether the embedded templates contain PERMUTATION_MATCH.
     * Used only in case of TEMPLATE_LIST */
    bool has_permutation;

    /** If false, it indicates that some all from could not be computed
     *  at compile-time, so it will need to be computed at runtime.
     *  It starts out as true. TODO find a better name */
    bool flattened;

    /** Pointer to the base template (or template field) that this
     *  template is derived from. It is set in modified templates
     *  (including in-line modified templates) and only if the
     *  compiler is able to determine the base template. It is NULL
     *  otherwise. */
    Template *base_template;

    /** Copy constructor. For Template::clone() only.
     *  This is a hit-and-miss affair. Some template types can be cloned;
     *  others cause an immediate FATAL_ERROR. */
    Template(const Template& p);

    /** Copy assignment disabled */
    Template& operator=(const Template& p);

    /** Destroys the innards of the template */
    void clean_up();

    string create_stringRepr();

  public:
    /** Constructor for TEMPLATE_ERROR, TEMPLATE_NOTUSED, OMIT_VALUE,
     * ANY_VALUE, ANY_OR_OMIT and */
    Template(templatetype_t tt);

    /** Constructor for SPECIFIC_VALUE */
    Template(Value *v);

    /** Constructor for TEMPLATE_REFD */
    Template(Ref_base *p_ref);

    /** Constructor for TEMPLATE_LIST, VALUE_LIST, COMPLEMENTED_LIST,
     * SUPERSET_MATCH, SUBSET_MATCH and PERMUTATION_MATCH */
    Template(templatetype_t tt, Templates *ts);

    /** Constructor for ALL_FROM */
    Template(Template*);

    /** Constructor for VALUE_LIST_ALL_FROM
     *  Takes over the innards of \a t, then deletes it.
     *  @pre tt == VALUE_LIST_ALL_FROM */
    Template(templatetype_t tt, Template *t);

    /** Constructor for NAMED_TEMPLATE_LIST */
    Template(NamedTemplates *nts);

    /** Constructor for INDEXED_TEMPLATE_LIST */
    Template(IndexedTemplates *its);

    /** Constructor for VALUE_RANGE */
    Template(ValueRange *vr);

    /** Constructor for BSTR_PATTERN, HSTR_PATTERN and OSTR_PATTERN */
    Template(templatetype_t tt, string *p_patt);

    /** Constructor for CSTR_PATTERN. */
    Template(PatternString *p_ps);

    virtual ~Template();

    virtual Template* clone() const;

    virtual void set_fullname(const string& p_fullname);
    virtual void set_my_scope(Scope *p_scope);
    void set_genname_recursive(const string& p_genname);
    void set_genname_prefix(const char *p_genname_prefix);
    void set_code_section(code_section_t p_code_section);

    /** Transforms the template to type \a p_templatetype.
     *  Works only if the change is possible */
    void set_templatetype(templatetype_t p_templatetype);
    templatetype_t get_templatetype() const { return templatetype; }
    /** Returns a C string containing the type (e.g. "any or omit")
     *  of the template. */
    const char *get_templatetype_str() const;

    /** Returns whether the template is a specific value containing a single
     * identifier. */
    bool is_undef_lowerid();
    /** If the embedded specific value contains a single identifier it converts
     * that to a referenced value or referenced template. */
    void set_lowerid_to_ref();

    /** If this template is used in an expression, what is its return type? */
    Type::typetype_t get_expr_returntype(Type::expected_value_t exp_val);
    /** If this template is used in an expression, what is its governor?
     *  If has no governor, but is reference, then tries the
     *  referenced stuff... If not known, returns NULL. */
    Type *get_expr_governor(Type::expected_value_t exp_val);

    /** Sets the governor type. */
    virtual void set_my_governor(Type *p_gov);
    /** Gets the governor type. */
    virtual Type* get_my_governor() const;

    void set_length_restriction(LengthRestriction *p_lr);
    LengthRestriction *get_length_restriction() const
      { return length_restriction; }
    bool is_length_restricted() const { return length_restriction != 0; }

    void set_ifpresent(bool p_ifpr) { is_ifpresent = p_ifpr; }
    bool get_ifpresent() const { return is_ifpresent; }

    void set_base_template(Template *p_base) { base_template = p_base; }
    Template *get_base_template() const { return base_template; }

    /** Returns the condition for the completeness of template body \a this,
     * which is a 'record of' or 'set of' template.
     * Argument \a incomplete_allowed indicates whether incomplete template
     * bodies are allowed in the context at all. */
    completeness_t get_completeness_condition_seof(bool incomplete_allowed);
    /** Returns the condition for the completeness of template body \a this,
     * which is a union template.
     * Argument \a incomplete_allowed indicates whether incomplete template
     * bodies are allowed in the context at all. Argument \a p_fieldname points
     * to a field of \a this that is examined. It is useful when \a this is
     * erroneous and contains multiple fields. */
    completeness_t get_completeness_condition_choice(bool incomplete_allowed,
      const Identifier& p_fieldname);

    void add_named_temp(NamedTemplate* nt);

    Value *get_specific_value() const;
    Ref_base *get_reference() const;
    ValueRange *get_value_range() const;
    PatternString* get_cstr_pattern() const;
    PatternString* get_ustr_pattern() const;
    size_t get_nof_comps() const;
    Template *get_temp_byIndex(size_t n) const;
    NamedTemplate *get_namedtemp_byIndex(size_t n) const;
    IndexedTemplate *get_indexedtemp_byIndex(size_t n) const;
    Template *get_all_from() const;
    /** Returns the number of elements in a VALUE_LIST. The elements of
     * embedded PERMUTATION_MATCH constructs are counted recursively. */
    size_t get_nof_listitems() const;
    /** Returns the \a n th element of a VALUE_LIST. The embedded
     * PERMUTATION_MATCH constructs are also counted. */
    Template *get_listitem_byIndex(size_t n) const;

    Template* get_template_refd_last(ReferenceChain *refch=0);
    Template* get_refd_sub_template(Ttcn::FieldOrArrayRefs *subrefs,
                                    bool usedInIsbound,
                                    ReferenceChain *refch);
  private:
    Template* get_template_refd(ReferenceChain *refch);
    Template* get_refd_field_template(const Identifier& field_id,
      const Location& loc, bool usedInIsbound, ReferenceChain *refch);
    Template* get_refd_array_template(Value *array_index, bool usedInIsbound,
                                      ReferenceChain *refch);

    bool compile_time() const;
  public:
    /** Returns whether \a this contains '*' as inside value. Applicable to:
     * TEMPLATE_LIST, SUPERSET_MATCH, SUBSET_MATCH, PERMUTATION_MATCH */
    bool temps_contains_anyornone_symbol() const;
    /** Returns the number of elements not counting the '*'s. Applicable to:
     * TEMPLATE_LIST, SUPERSET_MATCH, SUBSET_MATCH, PERMUTATION_MATCH */
    size_t get_nof_comps_not_anyornone() const;
    /** Returns whether the string pattern contains '*' as inside value.
     * Applicable to: BSTR_PATTERN, HSTR_PATTERN, OSTR_PATTERN, CSTR_PATTERN */
    bool pattern_contains_anyornone_symbol() const;
    /** Returns the length of string pattern not counting the '*'s.
     * Applicable to: BSTR_PATTERN, HSTR_PATTERN, OSTR_PATTERN, CSTR_PATTERN */
    size_t get_min_length_of_pattern() const;

    /** Returns whether the template can be converted to a value
     *  (i.e. it is a specific value without wildcards). */
    bool is_Value() const;
    /** Converts the template to a value recursively. The returned
     *  value is newly allocated (i.e. the caller is responsible for
     *  the deallocation).
     *  If the template type was SPECIFIC_VALUE, the value returned is
     *  extracted from the template, which becomes TEMPLATE_ERROR. */
    Value *get_Value();

    /** Returns whether the template consists of a single reference
     *  (i.e. it is a single specific value with an embedded
     *  non-parameterized reference). */
    bool is_Ref() const;
    /** Converts the template to a reference. The returned reference
     *  is newly allocated (i.e. the caller is responsible for the
     *  deallocation). */
    Ref_base *get_Ref();

    /** Checks for circular references within embedded templates */
    void chk_recursions(ReferenceChain& refch);

    /** Checks whether the template (including the embedded fields) contains
     * no matching symbols. Argument \a allow_omit is used because OMIT_VALUE
     * is allowed only in embedded fields. */
    void chk_specific_value(bool allow_omit);
    void chk_specific_value_generic();
    void chk_invoke();

    /** Copy template elements from the "all from" into the template.
     *
     * @param from_permutation \a true if the "all from" occurs inside
     * a permutation_match (in which case the target of the "all from"
     * is allowed to be AnyElementsOrNone).
     * @return \a false if some elements could not be computed at compile time,
     * else \a true.
     */
    bool flatten(bool from_permutation);
    bool is_flattened() const { return flattened; }
    bool has_allfrom() const;

    static Templates *harbinger(Template *t, bool from_permutation, bool killer);

    /** helper functions used by definitions which check their template restr.*/
    /** returns the restriction name token */
    static const char* get_restriction_name(template_restriction_t tr);
    /** if a template has restriction \a tr then what is the restriction
        on it's subfields */
    static template_restriction_t get_sub_restriction(
      template_restriction_t tr, Ref_base* ref);
    /** returns C++ code that checks the restriction */
    static char* generate_restriction_check_code(char* str,
      const char* name, template_restriction_t tr);
    /** Return if the required template restriction \a needed_tr is
     *  not satisfied by the given \a refd_tr */
    static bool is_less_restrictive(template_restriction_t needed_tr,
      template_restriction_t refd_tr);
  private:
    /** helper functions used by chk_restriction() */
    bool chk_restriction_named_list(const char* definition_name,
      map<string, void>& checked_map, size_t needed_checked_cnt);
    bool chk_restriction_refd(const char* definition_name,
      template_restriction_t template_restriction);
  public:
    /** Checks if this template conforms to the restriction, return value:
     *  false = always satisfies restriction -> no runtime check needed or
     *          never satisfies restriction -> compiler error(s)
     *  true  = possibly violates restriction, cannot be determined at compile
     *          time -> runtime check needed and compiler warning given when
     *          inadequate restrictions are used, in other cases there's
     *          no warning
     *  The return value is used by code generation to avoid useless checks */
    bool chk_restriction(const char* definition_name,
                         template_restriction_t template_restriction);

    /** Public entry points for code generation. */
    /** Generates the equivalent C++ code for the template. It is used
     *  when the template is part of a complex expression
     *  (e.g. argument of a function that accepts a template). The
     *  generated code fragments are appended to the fields of visitor
     *  \a expr. */
    void generate_code_expr(expression_struct *expr,
      template_restriction_t template_restriction = TR_NONE);
    /** Generates a C++ code sequence, which initializes the C++
     *  object named \a name with the contents of the template. The
     *  code sequence is appended to argument \a str and the resulting
     *  string is returned. */
    char *generate_code_init(char *str, const char *name);
    /** Walks through the template recursively and appends the C++
     *  initialization sequence of all (directly or indirectly)
     *  referenced non-parameterized templates to \a str and returns
     *  the resulting string.  Templates imported from other modules
     *  are not visited. */
    char *rearrange_init_code(char *str);

  private:
    /** Private helper functions for code generation. */

    /** Helper function for \a generate_code_init_refd(). Returns whether
     * the string returned by \a get_single_expr() can be used to initialize
     * the template instead of using the re-arrangement algorithm.
     * Applicable to referenced templates only. */
    bool use_single_expr_for_init();

    /** Helper function for \a generate_code_init(). It handles the
     *  referenced templates. */
    char *generate_code_init_refd(char *str, const char *name);
    char *generate_code_init_invoke(char *str, const char *name);
    /** Generates the C++ equivalent of \a this into \a expr and
     *  performs the necessary reordering: appends the initializer
     *  sequence of the dependent templates to \a str and returns the
     *  resulting string.  It is used when \a this is a referenced
     *  template that is embedded into the body of a non-parameterized
     *  template.  Note that the final content of \a expr is not
     *  necessarily identical to the C++ equivalent of \a
     *  u.ref.ref. */
    char *generate_rearrange_init_code_refd(char *str, expression_struct *expr);
    /** Helper function for \a generate_code_init(). It handles the value list
     *  notation and the assignment notation with array indices (for 'record
     *  of' and 'set of' types). Embedded permutation matching constructs are
     *  also handled. */
    char *generate_code_init_seof(char *str, const char *name);
    /** Helper function for \a generate_code_init_seof(). Used when \a this is
     *  a member of a value list notation or an embedded permutation match.
     *  Parameter \a name contains the C++ reference that points to the parent
     *  template. Parameter \a index is a straightened index in case of
     *  embedded permutations. Parameter \a element_type_genname contains a
     *  C++ class name that represents the element type of the 'record of' or
     *  'set of' construct (which is the governor of \a this); if a temporary
     *  variable is created, this would be used as its type. */
    char *generate_code_init_seof_element(char *str, const char *name,
      const char* index, const char *element_type_genname);
    /** Helper function for \a generate_code_init(). It handles the
     *  assignment notation for record/set/union types. */
    char *generate_code_init_se(char *str, const char *name);
    /** Helper function for \a generate_code_init(). It handles the
     *  value list and complemented list matching constructs. Flag \a
     *  is_complemented is set to true for complemented lists and
     *  false for value lists. */
    char *generate_code_init_list(char *str, const char *name,
      bool is_complemented);
    /** Helper function for \a generate_code_init(). It handles the superset
     *  and subset constructs. Flag \a is_superset is set to true for superset
     *  and false for subset. Note that both constructs have similar data
     *  representations in runtime. */
    char *generate_code_init_set(char *str, const char *name,
      bool is_superset);

    char *generate_code_init_all_from(char *str, const char *name);
    char *generate_code_init_all_from_list(char *str, const char *name);

    /** Helper function for \a generate_code_expr() and get_single_expr().
     * It handles the invoke operation. */
    void generate_code_expr_invoke(expression_struct *expr);

    /** Helper function for \a rearrange_init_code(). It handles the
     *  referenced templates (i.e. it does the real work). */
    char *rearrange_init_code_refd(char *str);
    char *rearrange_init_code_invoke(char *str);

    /** Returns whether the C++ initialization sequence requires a
     *  temporary variable reference to be introduced for efficiency
     *  reasons. */
    bool needs_temp_ref();

  public:
    /** Returns whether the template can be represented by an in-line
     *  C++ expression. */
    bool has_single_expr();
    /** Returns the equivalent C++ expression. It can be used only if
     *  \a has_single_expr() returns true. Argument \a cast_needed
     *  indicates whether the generic wildcards have to be explicitly
     *  converted to the appropriate type. */
    string get_single_expr(bool cast_needed);

    virtual void dump(unsigned level) const;
  };

  /** Represents an in-line template. Consists of:
   *  - an optional type
   *  - an optional template reference with or without actual parameter list
   *    (in case of in-line modified template)
   *  - a mandatory template body
   */
  class TemplateInstance : public Node, public Location {
  private: // all pointers owned
    Type     *type; // type before the colon, may be null
    Ref_base *derived_reference; // base template, may be null
    Template *template_body; // must not be null

    /** Copy constructor disabled. */
    TemplateInstance(const TemplateInstance& p);
    /** Copy assignment disabled. */
    TemplateInstance& operator=(const TemplateInstance& p);
  public:
    /** Constructor
     *
     * @param p_type type (the one before the colon, optional)
     * @param p_ref  reference to the base template (for a modified template)
     * @param p_body template body (must not be NULL)
     */
    TemplateInstance(Type *p_type, Ref_base *p_ref, Template *p_body);
    ~TemplateInstance();

    virtual TemplateInstance *clone() const;
    virtual void set_fullname(const string& p_fullname);
    virtual void set_my_scope(Scope *p_scope);

    Type*     get_Type()       const { return type; }
    Ref_base* get_DerivedRef() const { return derived_reference; }
    Template* get_Template()   const { return template_body; }
    // it can return null pointer
    Def_Template* get_Referenced_Base_Template();

    /** Give up ownership of \a type, \a derived_reference and
     *  \a template_body */
    void release();

    /** If this template instance is used in an expression, what is its
     *  return type? */
    Type::typetype_t get_expr_returntype(Type::expected_value_t exp_val);
    /** If this template instance is used in an expression, what is its
     *  governor? Returns NULL if the governor cannot be determined. */
    Type *get_expr_governor(Type::expected_value_t exp_val);

    /** Checks the entire template instance against \a governor. */
    void chk(Type *governor);
    /** Checks the member \a type (if present) against \a governor.
     *  Returns the type that shall be considered in further checks. */
    Type *chk_Type(Type *governor);
    /** Checks the derived reference against \a governor.
     *  If \a governor is NULL it is checked against the \a type member.
     *  Returns the type that shall be considered when checking
     *  \a template_body. */
    Type *chk_DerivedRef(Type *governor);
    /** Checks for circular references. */
    void chk_recursions(ReferenceChain& refch);

    bool is_string_type(Type::expected_value_t exp_val);

    bool chk_restriction(const char* definition_name,
      template_restriction_t template_restriction);

    /** Returns whether the template instance can be represented by an in-line
     *  C++ expression. */
    bool has_single_expr();
    void set_code_section(GovernedSimple::code_section_t p_code_section);
    bool needs_temp_ref();
    void generate_code(expression_struct *expr,
                       template_restriction_t template_restriction = TR_NONE);
    /** Appends the initialization sequence of the referred templates
     *  to \a str.  Only those templates are considered that are in
     *  the same module as \a this. */
    char *rearrange_init_code(char *str);

    /** Appends the string representation of the template instance to
     *  \a str. */
    void append_stringRepr(string& str) const;

    virtual void dump(unsigned level) const;

    /** Returns the single value if the template is single value or NULL. */
    Value* get_specific_value() const;

    bool is_only_specific_value() const {
      return (derived_reference==NULL &&
        template_body->get_templatetype()==Template::SPECIFIC_VALUE &&
        !template_body->is_length_restricted() &&
        !template_body->get_ifpresent());
    }
  };

} // namespace Ttcn

#endif // _Ttcn_Template_HH
