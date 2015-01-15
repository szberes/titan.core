///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "../common/memory.h"
#include "datatypes.h"
#include "record_of.h"
#include "encdec.h"

#include "main.hh"
#include "ttcn3/compiler.h"

/** code generation for original runtime */
static void defRecordOfClass1(const struct_of_def *sdef, output_struct *output);
static void defRecordOfTemplate1(const struct_of_def *sdef, output_struct *output);
/** code generation for alternative runtime (TITAN_RUNTIME_2) */
static void defRecordOfClass2(const struct_of_def *sdef, output_struct *output);
static void defRecordOfTemplate2(const struct_of_def *sdef, output_struct *output);

void defRecordOfClass(const struct_of_def *sdef, output_struct *output)
{
  if (use_runtime_2) defRecordOfClass2(sdef, output);
  else defRecordOfClass1(sdef, output);
}

void defRecordOfTemplate(const struct_of_def *sdef, output_struct *output)
{
  if (use_runtime_2) defRecordOfTemplate2(sdef, output);
  else defRecordOfTemplate1(sdef, output);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void defRecordOfClass1(const struct_of_def *sdef, output_struct *output)
{
  char *def = NULL, *src = NULL;
  const char *name = sdef->name, *dispname = sdef->dispname;
  const char *type = sdef->type;
  boolean ber_needed = sdef->isASN1 && enable_ber();
  boolean raw_needed = sdef->hasRaw && enable_raw();
  boolean text_needed = sdef->hasText && enable_text();
  boolean xer_needed = sdef->hasXer && enable_xer();
  boolean json_needed = sdef->hasJson && enable_json();

  /* Class definition and private data members */
  def = mputprintf(def,
#ifndef NDEBUG
    "// written by %s in " __FILE__ " at %d\n"
#endif
    "class %s : public Base_Type {\n"
    "struct recordof_setof_struct {\n"
    "int ref_count;\n"
    "int n_elements;\n"
    "%s **value_elements;\n"
    "} *val_ptr;\n"
    "Vector<int> refd_indices;\n"
    "int max_refd_index;\n"
#ifndef NDEBUG
      , __FUNCTION__, __LINE__
#endif
    , name, type);

  /* constant unbound element */
  def = mputprintf(def, "\nstatic const %s UNBOUND_ELEM;\n", type);
  src = mputprintf(src, "\nconst %s %s::UNBOUND_ELEM;\n", type, name);

  /* private member functions */
  def = mputprintf(def,
    "private:\n"
    "friend boolean operator==(null_type null_value, "
	  "const %s& other_value);\n", name);

  if (sdef->kind == SET_OF) {
    /* callback function for comparison */
    def = mputstr(def, "static boolean compare_function("
	"const Base_Type *left_ptr, int left_index, "
        "const Base_Type *right_ptr, int right_index);\n");
    src = mputprintf(src, "boolean %s::compare_function("
	"const Base_Type *left_ptr, int left_index, "
        "const Base_Type *right_ptr, int right_index)\n"
        "{\n"
        "if (((const %s*)left_ptr)->val_ptr == NULL) "
        "TTCN_error(\"The left operand of comparison is an unbound value of "
        "type %s.\");\n"
        "if (((const %s*)right_ptr)->val_ptr == NULL) "
        "TTCN_error(\"The right operand of comparison is an unbound value of "
        "type %s.\");\n"
        "if (((const %s*)left_ptr)->val_ptr->value_elements[left_index] != NULL){\n"
        "if (((const %s*)right_ptr)->val_ptr->value_elements[right_index] != NULL){\n"
        "return *((const %s*)left_ptr)->val_ptr->value_elements[left_index] == "
        "*((const %s*)right_ptr)->val_ptr->value_elements[right_index];\n"
        "} else return FALSE;\n"
        "} else {\n"
        "return ((const %s*)right_ptr)->val_ptr->value_elements[right_index] == NULL;\n"
        "}\n"
        "}\n\n", name, name, dispname, name, dispname, name, name, name, name, name);
  }
  
  /* function that returns the actual number of elements */
  def = mputstr(def, "int get_nof_elements() const;\n");
  src = mputprintf(src,
    "int %s::get_nof_elements() const\n"
    "{\n"
    "  int nof_elements = (val_ptr != NULL) ? val_ptr->n_elements : 0;\n"
    "  if (!refd_indices.empty()) {\n"
    "    while (nof_elements > 0) {\n"
    "      if (is_elem_bound(nof_elements - 1)) {\n"
    "        break;\n"
    "      }\n"
    "      --nof_elements;\n"
    "    }\n"
    "  }\n"
    "  return nof_elements;\n"
    "}\n\n", name);
  
  /* element boundness check function */
  def = mputstr(def, "bool is_elem_bound(int index) const;\n");
  src = mputprintf(src,
    "bool %s::is_elem_bound(int index) const\n"
    "{\n"
    "  return val_ptr->value_elements[index] != NULL &&\n"
    "    val_ptr->value_elements[index]->is_bound();\n"
    "}\n\n", name);
  
  /* function that calculates and caches the maximum referenced index */
  def = mputstr(def, "int get_max_refd_index();\n");
  src = mputprintf(src,
    "int %s::get_max_refd_index()\n"
    "{\n"
    "  if (refd_indices.empty()) {\n"
    "    return -1;\n"
    "  }\n"
    "  if (-1 == max_refd_index) {\n"
    "    for (size_t i = 0; i < refd_indices.size(); ++i) {\n"
    "      if (refd_indices[i] > max_refd_index) {\n"
    "        max_refd_index = refd_indices[i];\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "  return max_refd_index;\n"
    "}\n\n", name);
  
  /* referenced index check function */
  def = mputstr(def, "bool is_index_refd(int index);\n");
  src = mputprintf(src,
    "bool %s::is_index_refd(int index)\n"
    "{\n"
    "  for (size_t i = 0; i < refd_indices.size(); ++i) {\n"
    "    if (index == refd_indices[i]) {\n"
    "      return true;\n"
    "    }\n"
    "  }\n"
    "  return false;\n"
    "}\n\n", name);

  /* public member functions */
  def = mputstr(def, "\npublic:\n");
  def = mputprintf(def, "  typedef %s of_type;\n", sdef->type);

  /* constructors */
  def = mputprintf(def, "%s();\n", name);
  src = mputprintf(src,
    "%s::%s()\n"
    "{\n"
    "val_ptr = NULL;\n"
    "max_refd_index = -1;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s(null_type other_value);\n", name);
  src = mputprintf(src,
    "%s::%s(null_type)\n"
    "{\n"
    "val_ptr = new recordof_setof_struct;\n"
    "val_ptr->ref_count = 1;\n"
    "val_ptr->n_elements = 0;\n"
    "val_ptr->value_elements = NULL;\n"
    "max_refd_index = -1;\n"
    "}\n\n", name, name);

  /* copy constructor */
  def = mputprintf(def, "%s(const %s& other_value);\n", name, name);
  src = mputprintf(src,
     "%s::%s(const %s& other_value)\n"
     "{\n"
     "if (!other_value.is_bound()) "
     "TTCN_error(\"Copying an unbound value of type %s.\");\n"
     "if (other_value.refd_indices.empty()) {\n"
     "val_ptr = other_value.val_ptr;\n"
     "val_ptr->ref_count++;\n"
     "}\n"
     "else {\n"
     // there are references to at least one element => the array must be copied
     "val_ptr = NULL;\n"
     "int nof_elements = other_value.get_nof_elements();\n"
     "set_size(nof_elements);\n"
     "for (int i = 0; i < nof_elements; ++i) {\n"
     "if (other_value.is_elem_bound(i)) {\n"
     "val_ptr->value_elements[i] = new %s(*(other_value.val_ptr->value_elements[i]));\n"
     "}\n"
     "}\n"
     "}\n"
     "max_refd_index = -1;\n"
     "}\n\n", name, name, name, dispname, type);

  /* destructor */
  def = mputprintf(def, "~%s();\n\n", name);
  src = mputprintf(src,
    "%s::~%s()\n"
    "{\n"
    "clean_up();\n"
    "if (val_ptr != NULL) val_ptr = NULL;\n"
    "}\n\n", name, name);

  /* clean_up function */
  def = mputstr(def, "void clean_up();\n");
  src = mputprintf
    (src,
     "void %s::clean_up()\n"
     "{\n"
     "if (val_ptr != NULL) {\n"
     "if (val_ptr->ref_count > 1) {\n"
     "val_ptr->ref_count--;\n"
     "val_ptr = NULL;\n"
     "}\n"
     "else if (val_ptr->ref_count == 1) {\n"
     "if (refd_indices.empty()) {\n"
     "for (int elem_count = 0; elem_count < val_ptr->n_elements;\n"
     "elem_count++)\n"
     "if (val_ptr->value_elements[elem_count] != NULL)\n"
     "delete val_ptr->value_elements[elem_count];\n"
     "free_pointers((void**)val_ptr->value_elements);\n"
     "delete val_ptr;\n"
     "val_ptr = NULL;\n"
     "}\n"
     "else {\n"
     "set_size(0);\n"
     "}\n"
     "}\n"
     "else\n"
     "TTCN_error(\"Internal error: Invalid reference counter in a record "
     "of/set of value.\");\n"
     "}\n"
     "}\n\n", name);

  /* assignment operators */
  def = mputprintf(def, "%s& operator=(null_type other_value);\n", name);
  src = mputprintf(src,
    "%s& %s::operator=(null_type)\n"
    "{\n"
    "set_size(0);\n"
    "return *this;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s& operator=(const %s& other_value);\n\n",
                   name, name);
  src = mputprintf(src,
    "%s& %s::operator=(const %s& other_value)\n"
    "{\n"
    "if (!other_value.is_bound()) "
    "TTCN_error(\"Assigning an unbound value of type %s.\");\n"
    "if (this != &other_value) {\n"
    "if (refd_indices.empty() && other_value.refd_indices.empty()) {\n"
    "clean_up();\n"
    "val_ptr = other_value.val_ptr;\n"
    "val_ptr->ref_count++;\n"
    "}\n"
    "else {\n"
    // there are references to at least one element => the array must be copied
    "int nof_elements = other_value.get_nof_elements();\n"
    "set_size(nof_elements);\n"
    "for (int i = 0; i < nof_elements; ++i) {\n"
    "if (other_value.is_elem_bound(i)) {\n"
    "if (val_ptr->value_elements[i] == NULL) {\n"
    "val_ptr->value_elements[i] = new %s;\n"
    "}\n"
    "*val_ptr->value_elements[i] = *other_value.val_ptr->value_elements[i];\n"
    "}\n"
    "else if (val_ptr->value_elements[i] != NULL) {\n"
    "if (is_index_refd(i)) {\n"
    "val_ptr->value_elements[i]->clean_up();\n"
    "}\n"
    "else {\n"
    "delete val_ptr->value_elements[i];\n"
    "val_ptr->value_elements[i] = NULL;\n"
    "}\n"
    "}\n"
    "}\n"
    "}\n"
    "}\n"
    "return *this;\n"
    "}\n\n", name, name, name, dispname, type);

  /* comparison operators */
  def = mputstr(def, "boolean operator==(null_type other_value) const;\n");
  src = mputprintf(src,
    "boolean %s::operator==(null_type) const\n"
    "{\n"
    "if (val_ptr == NULL)\n"
    "TTCN_error(\"The left operand of comparison is an unbound value of "
    "type %s.\");\n"
    "return get_nof_elements() == 0 ;\n"
    "}\n\n", name, dispname);

  def = mputprintf(def, "boolean operator==(const %s& other_value) const;\n",
                   name);
  src = mputprintf(src,
    "boolean %s::operator==(const %s& other_value) const\n"
    "{\n"
    "if (val_ptr == NULL) "
    "TTCN_error(\"The left operand of comparison is an unbound value of type "
    "%s.\");\n"
    "if (other_value.val_ptr == NULL) "
    "TTCN_error(\"The right operand of comparison is an unbound value of type "
    "%s.\");\n"
    "if (val_ptr == other_value.val_ptr) return TRUE;\n", name, name,
    dispname, dispname);
  if (sdef->kind == SET_OF) {
    src = mputstr(src,
       "return compare_set_of(this, get_nof_elements(), &other_value, "
       "other_value.get_nof_elements(), compare_function);\n");
  } else {
    src = mputstr
      (src,
       "if (get_nof_elements() != other_value.get_nof_elements())\n"
       "return FALSE;\n"
       "for (int elem_count = 0; elem_count < get_nof_elements(); elem_count++){\n"
       "if (is_elem_bound(elem_count)){\n"
       "if (other_value.is_elem_bound(elem_count)){\n"
       "  if (*val_ptr->value_elements[elem_count] != "
          "*(other_value.val_ptr)->value_elements[elem_count]) "
          "return FALSE;\n"
       "} else return FALSE;\n"
       "} else {\n"
       "if (other_value.is_elem_bound(elem_count)) return FALSE;\n"
       "}\n"
       "}\n"
       "return TRUE;\n");
  }
  src = mputstr(src, "}\n\n");

  def = mputstr(def, "inline boolean operator!=(null_type other_value) const "
                "{ return !(*this == other_value); }\n");
  def = mputprintf(def, "inline boolean operator!=(const %s& other_value) "
                   "const { return !(*this == other_value); }\n\n", name);

  /* indexing operators */
  /* Non-const operator[] is allowed to extend the record-of */
  def = mputprintf(def, "%s& operator[](int index_value);\n", type);
  src = mputprintf(src,
    "%s& %s::operator[](int index_value)\n"
    "{\n"
    "if (index_value < 0) TTCN_error(\"Accessing an element of type %s "
    "using a negative index: %%d.\", index_value);\n"
    "if (val_ptr == NULL) {\n"
    "val_ptr = new recordof_setof_struct;\n"
    "val_ptr->ref_count = 1;\n"
    "val_ptr->n_elements = 0;\n"
    "val_ptr->value_elements = NULL;\n"
    "} else if (val_ptr->ref_count > 1) {\n" /* copy-on-write */
    "struct recordof_setof_struct *new_val_ptr = new recordof_setof_struct;\n"
    "new_val_ptr->ref_count = 1;\n"
    "new_val_ptr->n_elements = (index_value >= val_ptr->n_elements) ? "
    "index_value + 1 : val_ptr->n_elements;\n"
    "new_val_ptr->value_elements = "
    "(%s**)allocate_pointers(new_val_ptr->n_elements);\n"
    "for (int elem_count = 0; elem_count < val_ptr->n_elements; "
    "elem_count++){\n"
    "if (val_ptr->value_elements[elem_count] != NULL){\n"
    "new_val_ptr->value_elements[elem_count] = "
    "new %s(*(val_ptr->value_elements[elem_count]));\n"
    "}\n"
    "}\n"
    "clean_up();\n"
    "val_ptr = new_val_ptr;\n"
    "}\n"
    "if (index_value >= val_ptr->n_elements) set_size(index_value + 1);\n"
    "if (val_ptr->value_elements[index_value] == NULL) {\n"
    "val_ptr->value_elements[index_value] = new %s;\n"
    "}\n"
    "return *val_ptr->value_elements[index_value];\n"
    "}\n\n", type, name, dispname, type, type, type);

  def = mputprintf(def, "%s& operator[](const INTEGER& index_value);\n",
                   type);
  src = mputprintf(src,
    "%s& %s::operator[](const INTEGER& index_value)\n"
    "{\n"
    "index_value.must_bound(\"Using an unbound integer value for indexing "
    "a value of type %s.\");\n"
    "return (*this)[(int)index_value];\n"
    "}\n\n", type, name, dispname);

  /* Const operator[] throws an error if over-indexing */
  def = mputprintf(def, "const %s& operator[](int index_value) const;\n",
                   type);
  src = mputprintf(src,
    "const %s& %s::operator[](int index_value) const\n"
    "{\n"
    "if (val_ptr == NULL)\n"
    "TTCN_error(\"Accessing an element in an unbound value of type %s.\");\n"
    "if (index_value < 0) TTCN_error(\"Accessing an element of type %s "
    "using a negative index: %%d.\", index_value);\n"
    "if (index_value >= get_nof_elements()) TTCN_error(\"Index overflow in "
    "a value of type %s: The index is %%d, but the value has only %%d "
    "elements.\", index_value, get_nof_elements());\n"
    "return (val_ptr->value_elements[index_value] != NULL) ?\n"
    "*val_ptr->value_elements[index_value] : UNBOUND_ELEM;\n"
    "}\n\n", type, name, dispname, dispname, dispname);

  def = mputprintf(def, "const %s& operator[](const INTEGER& index_value) "
                   "const;\n\n", type);
  src = mputprintf(src,
    "const %s& %s::operator[](const INTEGER& index_value) const\n"
    "{\n"
    "index_value.must_bound(\"Using an unbound integer value for indexing "
    "a value of type %s.\");\n"
    "return (*this)[(int)index_value];\n"
    "}\n\n", type, name, dispname);

  /* rotation operators */
  def = mputprintf(def,
    "%s operator<<=(int rotate_count) const;\n"
    "%s operator<<=(const INTEGER& rotate_count) const;\n"
    "%s operator>>=(int rotate_count) const;\n"
    "%s operator>>=(const INTEGER& rotate_count) const;\n\n",
    name, name, name, name);
  src = mputprintf(src,
    "%s %s::operator<<=(int rotate_count) const\n"
    "{\n"
    "return *this >>= (-rotate_count);\n"
    "}\n\n"
    "%s %s::operator<<=(const INTEGER& rotate_count) const\n"
    "{\n"
    "rotate_count.must_bound(\""
    "Unbound integer operand of rotate left operator.\");\n"
    "return *this >>= (int)(-rotate_count);\n"
    "}\n\n"
    "%s %s::operator>>=(const INTEGER& rotate_count) const\n"
    "{\n"
    "rotate_count.must_bound(\""
    "Unbound integer operand of rotate right operator.\");\n"
    "return *this >>= (int)rotate_count;\n"
    "}\n\n"
    "%s %s::operator>>=(int rotate_count) const\n"
    "{\n"
    "if (val_ptr == NULL) "
    "TTCN_error(\"Performing rotation operation on an unbound value of type "
    "%s.\");\n"
    "int nof_elements = get_nof_elements();\n"
    "if (nof_elements == 0) return *this;\n"
    "int rc;\n"
    "if (rotate_count>=0) rc = rotate_count %% nof_elements;\n"
    "else rc = nof_elements - ((-rotate_count) %% nof_elements);\n"
    "if (rc == 0) return *this;\n"
    "%s ret_val;\n"
    "ret_val.set_size(nof_elements);\n"
    "for (int i=0; i<nof_elements; i++) {\n"
    "if (is_elem_bound(i)) {\n"
    "ret_val.val_ptr->value_elements[(i+rc)%%nof_elements] ="
      "new %s(*val_ptr->value_elements[i]);\n"
    "}\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n",
    name, name, name, name, name, name, name, name, dispname, name, type);

  /* concatenation */
  def = mputprintf(def,
    "%s operator+(const %s& other_value) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s::operator+(const %s& other_value) const\n"
    "{\n"
    "if (val_ptr == NULL || other_value.val_ptr == NULL) "
      "TTCN_error(\"Unbound operand of %s concatenation.\");\n"
    "int nof_elements = get_nof_elements();\n"
    "if (nof_elements == 0) return other_value;\n"
    "int other_value_nof_elements = other_value.get_nof_elements();"
    "if (other_value_nof_elements == 0) return *this;\n"
    "%s ret_val;\n"
    "ret_val.set_size(nof_elements + other_value_nof_elements);\n"
    "for (int i=0; i<nof_elements; i++) {\n"
    "if (is_elem_bound(i)) {\n"
    "ret_val.val_ptr->value_elements[i] = new %s(*val_ptr->value_elements[i]);\n"
    "}\n"
    "}\n"
    "for (int i=0; i<other_value_nof_elements; i++) {\n"
    "if (other_value.is_elem_bound(i)) {\n"
    "ret_val.val_ptr->value_elements[i + nof_elements] = "
    "new %s(*other_value.val_ptr->value_elements[i]);\n"
    "}\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n", name, name, name, dispname, name, type, type);

  /* substr() */
  def = mputprintf(def,
    "%s substr(int index, int returncount) const;\n\n", name);
  src = mputprintf(src,
    "%s %s::substr(int index, int returncount) const\n"
    "{\n"
    "if (val_ptr == NULL) "
      "TTCN_error(\"The first argument of substr() is an unbound value of "
        "type %s.\");\n"
    "check_substr_arguments(get_nof_elements(), index, returncount, "
      "\"%s\",\"element\");\n"
    "%s ret_val;\n"
    "ret_val.set_size(returncount);\n"
    "for (int i=0; i<returncount; i++) {\n"
    "if (is_elem_bound(i+index)) {\n"
    "ret_val.val_ptr->value_elements[i] = "
    "new %s(*val_ptr->value_elements[i+index]);\n"
    "}\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n", name, name, dispname, dispname, name, type);

  /* replace() */
  def = mputprintf(def,
    "%s replace(int index, int len, const %s& repl) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s::replace(int index, int len, const %s& repl) const\n"
    "{\n"
    "if (val_ptr == NULL) "
      "TTCN_error(\"The first argument of replace() is an unbound value of "
        "type %s.\");\n"
    "if (repl.val_ptr == NULL) "
      "TTCN_error(\"The fourth argument of replace() is an unbound value of "
        "type %s.\");\n"
    "int nof_elements = get_nof_elements();\n"
    "check_replace_arguments(nof_elements, index, len, "
      "\"%s\",\"element\");\n"
    "%s ret_val;\n"
    "int repl_nof_elements = repl.get_nof_elements();\n"
    "ret_val.set_size(nof_elements + repl_nof_elements - len);\n"
    "for (int i = 0; i < index; i++) {\n"
    "if (is_elem_bound(i)) {\n"
    "ret_val.val_ptr->value_elements[i] = new %s(*val_ptr->value_elements[i]);\n"
    "}\n"
    "}\n"
    "for (int i = 0; i < repl_nof_elements; i++) {\n"
    "if (repl.is_elem_bound(i)) {\n"
    "ret_val.val_ptr->value_elements[i+index] = "
    "new %s(*repl.val_ptr->value_elements[i]);\n"
    "}\n"
    "}\n"
    "for (int i = 0; i < nof_elements - index - len; i++) {\n"
    "if (is_elem_bound(index+i+len)) {\n"
    "ret_val.val_ptr->value_elements[index+i+repl_nof_elements] = "
    "new %s(*val_ptr->value_elements[index+i+len]);\n"
    "}\n"
    "}\n"
    "return ret_val;\n"
  "}\n\n", name, name, name, dispname, dispname, dispname, name, type, type, type);
  def = mputprintf(def,
    "%s replace(int index, int len, const %s_template& repl) const;\n\n",
    name, name);
  src = mputprintf(src,
    "%s %s::replace(int index, int len, const %s_template& repl) const\n"
    "{\n"
    "if (!repl.is_value()) TTCN_error(\"The fourth argument of function "
      "replace() is a template with non-specific value.\");\n"
    "return replace(index, len, repl.valueof());\n"
    "}\n\n", name, name, name);

  /* set_size function */
  def = mputstr(def, "void set_size(int new_size);\n");
  src = mputprintf(src, "void %s::set_size(int new_size)\n"
    "{\n"
    "if (new_size < 0) TTCN_error(\"Internal error: Setting a negative size "
      "for a value of type %s.\");\n"
    "if (val_ptr == NULL) {\n"
    "val_ptr = new recordof_setof_struct;\n"
    "val_ptr->ref_count = 1;\n"
    "val_ptr->n_elements = 0;\n"
    "val_ptr->value_elements = NULL;\n"
    "} else if (val_ptr->ref_count > 1) {\n" /* copy-on-write */
    "struct recordof_setof_struct *new_val_ptr = new recordof_setof_struct;\n"
    "new_val_ptr->ref_count = 1;\n"
    "new_val_ptr->n_elements = (new_size < val_ptr->n_elements) ? "
    "new_size : val_ptr->n_elements;\n"
    "new_val_ptr->value_elements = "
    "(%s**)allocate_pointers(new_val_ptr->n_elements);\n"
    "for (int elem_count = 0; elem_count < new_val_ptr->n_elements; "
    "elem_count++) {\n"
    "if (val_ptr->value_elements[elem_count] != NULL){\n"
    "new_val_ptr->value_elements[elem_count] = "
    "new %s(*(val_ptr->value_elements[elem_count]));\n"
    "}\n"
    "}\n"
    "clean_up();\n"
    "val_ptr = new_val_ptr;\n"
    "}\n"
    "if (new_size > val_ptr->n_elements) {\n"
    "val_ptr->value_elements = (%s**)"
    "reallocate_pointers((void**)val_ptr->value_elements, "
    "val_ptr->n_elements, new_size);\n"
    "#ifdef TITAN_MEMORY_DEBUG_SET_RECORD_OF\n"
    "if((val_ptr->n_elements/1000)!=(new_size/1000)) "
    "TTCN_warning(\"New size of type %s: %%d\",new_size);\n"
    "#endif\n"
    "val_ptr->n_elements = new_size;\n"
    "} else if (new_size < val_ptr->n_elements) {\n"
    "for (int elem_count = new_size; elem_count < val_ptr->n_elements; "
    "elem_count++) {\n"
    "if (val_ptr->value_elements[elem_count] != NULL) {\n"
    "if (is_index_refd(elem_count)) {\n"
    "val_ptr->value_elements[elem_count]->clean_up();\n"
    "}\n"
    "else {\n"
    "delete val_ptr->value_elements[elem_count];\n"
    "val_ptr->value_elements[elem_count] = 0;\n"
    "}\n"
    "}\n"
    "}\n"
    "if (new_size <= get_max_refd_index()) {\n"
    "new_size = get_max_refd_index() + 1;\n"
    "}\n"
    "if (new_size < val_ptr->n_elements) {\n"
    "val_ptr->value_elements = (%s**)"
    "reallocate_pointers((void**)val_ptr->value_elements, "
    "val_ptr->n_elements, new_size);\n"
    "val_ptr->n_elements = new_size;\n"
    "}\n"
    "}\n"
    "}\n\n", name, dispname, type, type, type, dispname, type);

  /* is_bound function */
  def = mputstr(def,
    "boolean is_bound() const;\n");
  src = mputprintf(src,
    "boolean %s::is_bound() const\n"
    "{\n"
    "if (refd_indices.empty()) {\n"
    "return (val_ptr != NULL);\n"
    "}\n"
    "return (get_nof_elements() != 0);\n"
    "}\n\n", name);

  /* is_present function */
  def = mputstr(def,
    "inline boolean is_present() const { return is_bound(); }\n");

  /* is_value function */
  def = mputstr(def,
    "boolean is_value() const;\n");
  src = mputprintf(src,
    "boolean %s::is_value() const\n"
    "{\n"
    "if (val_ptr == NULL) return false;\n"
    "for(int i = 0; i < get_nof_elements(); ++i) {\n"
    "if (!is_elem_bound(i) || "
    "!val_ptr->value_elements[i]->is_value()) return FALSE;\n"
    "}\n"
    "return TRUE;\n"
    "}\n\n", name);

  /* sizeof operation */
  def = mputstr(def,
    "int size_of() const;\n"
    "int n_elem() const { return size_of(); }\n");
  src = mputprintf(src,
    "int %s::size_of() const\n"
    "{\n"
    "if (val_ptr == NULL) "
    "TTCN_error(\"Performing sizeof operation on an unbound value of type "
    "%s.\");\n"
    "return get_nof_elements();\n"
    "}\n\n", name, dispname);

  /* lengthof operation */
  def = mputstr(def, "int lengthof() const;\n");
  src = mputprintf(src,
    "int %s::lengthof() const\n"
    "{\n"
    "if (val_ptr == NULL) "
      "TTCN_error(\"Performing lengthof operation on an unbound value of type "
    "%s.\");\n"
    "for (int my_length=get_nof_elements(); my_length>0; my_length--) "
      "if (is_elem_bound(my_length-1)) return my_length;\n"
    "return 0;\n"
    "}\n\n", name, dispname);

  /* log function */
  def = mputstr(def, "void log() const;\n");
  src = mputprintf
    (src,
     "void %s::log() const\n"
     "{\n"
     "if (val_ptr == NULL) {;\n"
     "TTCN_Logger::log_event_unbound();\n"
     "return;\n"
     "}\n"
     "switch (get_nof_elements()) {\n"
     "case 0:\n"
     "TTCN_Logger::log_event_str(\"{ }\");\n"
     "break;\n"
     "default:\n"
     "TTCN_Logger::log_event_str(\"{ \");\n"
     "for (int elem_count = 0; elem_count < get_nof_elements(); "
     "elem_count++) {\n"
     "if (elem_count > 0) TTCN_Logger::log_event_str(\", \");\n"
     "(*this)[elem_count].log();\n"
     "}\n"
     "TTCN_Logger::log_event_str(\" }\");\n"
     "}\n"
     "}\n\n", name);

  /* set_param function */
  def = mputstr(def, "void set_param(Module_Param& param);\n");
  src = mputprintf(src,
    "void %s::set_param(Module_Param& param)\n"
    "{\n"
    "  if (dynamic_cast<Module_Param_Name*>(param.get_id()) != NULL &&\n"
    "      param.get_id()->next_name()) {\n"
    // Haven't reached the end of the module parameter name
    // => the name refers to one of the elements, not to the whole record of
    "    char* param_field = param.get_id()->get_current_name();\n"
    "    if (param_field[0] < '0' || param_field[0] > '9') {\n"
    "      param.error(\"Unexpected record field name in module parameter, expected a valid\"\n"
    "        \" index for %s type `%s'\");\n"
    "    }\n"
    "    int param_index = -1;\n"
    "    sscanf(param_field, \"%%d\", &param_index);\n"
    "    (*this)[param_index].set_param(param);\n"
    "    return;\n"
    "  }\n"
    "  param.basic_check(Module_Param::BC_VALUE|Module_Param::BC_LIST, \"%s value\");\n"
    "  switch (param.get_operation_type()) {\n"
    "  case Module_Param::OT_ASSIGN:\n"
    "    if (param.get_type()==Module_Param::MP_Value_List && param.get_size()==0) {\n"
    "      *this = NULL_VALUE;\n"
    "      return;\n"
    "    }\n"
    "    switch (param.get_type()) {\n"
    "    case Module_Param::MP_Value_List:\n"
    "      set_size(param.get_size());\n"
    "      for (size_t i=0; i<param.get_size(); ++i) {\n"
    "        Module_Param* const curr = param.get_elem(i);\n"
    "        if (curr->get_type()!=Module_Param::MP_NotUsed) {\n"
    "          (*this)[i].set_param(*curr);\n"
    "        }\n"
    "      }\n"
    "      break;\n"
    "    case Module_Param::MP_Indexed_List:\n"
    "      for (size_t i=0; i<param.get_size(); ++i) {\n"
    "        Module_Param* const curr = param.get_elem(i);\n"
    "        (*this)[curr->get_id()->get_index()].set_param(*curr);\n"
    "      }\n"
    "      break;\n"
    "    default:\n"
    "      param.type_error(\"%s value\", \"%s\");\n"
    "    }\n"
    "    break;\n"
    "  case Module_Param::OT_CONCAT:\n"
    "    switch (param.get_type()) {\n"
    "    case Module_Param::MP_Value_List: {\n"
    "      if (!is_bound()) *this = NULL_VALUE;\n"
    "      int start_idx = lengthof();\n"
    "      for (size_t i=0; i<param.get_size(); ++i) {\n"
    "        Module_Param* const curr = param.get_elem(i);\n"
    "        if ((curr->get_type()!=Module_Param::MP_NotUsed)) {\n"
    "          (*this)[start_idx+(int)i].set_param(*curr);\n"
    "        }\n"
    "      }\n"
    "    } break;\n"
    "    case Module_Param::MP_Indexed_List:\n"
    "      param.error(\"Cannot concatenate an indexed value list\");\n"
    "      break;\n"
    "    default:\n"
    "      param.type_error(\"%s value\", \"%s\");\n"
    "    }\n"
    "    break;\n"
    "  default:\n"
    "    TTCN_error(\"Internal error: Unknown operation type.\");\n"
    "  }\n"
    "}\n", name, sdef->kind == RECORD_OF ? "record of" : "set of",
    dispname, sdef->kind == RECORD_OF ? "record of" : "set of",
    sdef->kind == RECORD_OF ? "record of" : "set of", dispname,
    sdef->kind == RECORD_OF ? "record of" : "set of", dispname);

  /* set implicit omit function, recursive */
  def = mputstr(def, "  void set_implicit_omit();\n");
  src = mputprintf(src,
    "void %s::set_implicit_omit()\n{\n"
    "if (val_ptr == NULL) return;\n"
    "for (int i = 0; i < get_nof_elements(); i++) {\n"
    "if (is_elem_bound(i)) val_ptr->value_elements[i]->set_implicit_omit();\n"
    "}\n}\n\n", name);
  
  /* functions for inserting and removing references to elements of the record of */
  def = mputstr(def, "  void add_refd_index(int index);\n");
  src = mputprintf(src, 
    "void %s::add_refd_index(int index)\n"
    "{\n"
    "  refd_indices.push_back(index);\n"
    "  if (index > get_max_refd_index()) {\n"
    "  max_refd_index = index;\n"
    "  }\n"
    "}\n\n", name);
  
  def = mputstr(def, "  void remove_refd_index(int index);\n");
  src = mputprintf(src,
    "void %s::remove_refd_index(int index)\n"
    "{\n"
    "  for (size_t i = refd_indices.size(); i > 0; --i) {\n"
    "    if (refd_indices[i - 1] == index) {\n"
    "      refd_indices.erase_at(i - 1);\n"
    "      break;\n"
    "    }\n"
    "  }\n"
    "  if (get_max_refd_index() == index) {\n"
    "    max_refd_index = -1;\n"
    "  }\n"
    "}\n\n", name);

  /* encoding / decoding functions */
  def = mputstr(def, "void encode_text(Text_Buf& text_buf) const;\n");
  src = mputprintf(src,
    "void %s::encode_text(Text_Buf& text_buf) const\n"
    "{\n"
    "if (val_ptr == NULL) "
    "TTCN_error(\"Text encoder: Encoding an unbound value of type %s.\");\n"
    "text_buf.push_int(get_nof_elements());\n"
    "for (int elem_count = 0; elem_count < get_nof_elements(); "
    "elem_count++)\n"
    "(*this)[elem_count].encode_text(text_buf);\n"
    "}\n\n", name, dispname);

  def = mputstr(def, "void decode_text(Text_Buf& text_buf);\n");
  src = mputprintf(src,
    "void %s::decode_text(Text_Buf& text_buf)\n"
    "{\n"
    "int new_size = text_buf.pull_int().get_val();\n"
    "if (new_size < 0)\n TTCN_error(\"Text decoder: Negative size "
    "was received for a value of type %s.\");\n"
    "set_size(new_size);\n"
    "for (int elem_count = 0; elem_count < new_size; "
    "elem_count++) {\n"
    "if (val_ptr->value_elements[elem_count] == NULL) {\n"
    "val_ptr->value_elements[elem_count] = new %s;\n"
    "}\n"
    "val_ptr->value_elements[elem_count]->decode_text(text_buf);\n"
    "}\n"
    "}\n\n", name, dispname, type);

  if(ber_needed || raw_needed || text_needed || xer_needed || json_needed)
    def_encdec(name, &def, &src, ber_needed, raw_needed, text_needed,
               xer_needed, json_needed, FALSE);

  if(text_needed){
   src=mputprintf(src,
      "int %s::TEXT_encode(const TTCN_Typedescriptor_t& p_td,"
      " TTCN_Buffer& p_buf) const{\n"
      "  int encoded_length=0;\n"
      "  if(p_td.text->begin_encode){\n"
      "    p_buf.put_cs(*p_td.text->begin_encode);\n"
      "    encoded_length+=p_td.text->begin_encode->lengthof();\n"
      "  }\n"
      "  if(val_ptr==NULL) {\n"
      "    TTCN_EncDec_ErrorContext::error\n"
      "      (TTCN_EncDec::ET_UNBOUND, \"Encoding an unbound value.\");\n"
      "    if(p_td.text->end_encode){\n"
      "      p_buf.put_cs(*p_td.text->end_encode);\n"
      "      encoded_length+=p_td.text->end_encode->lengthof();\n"
      "    }\n"
      "    return encoded_length;\n"
      "  }\n"
      "  for(int a=0;a<get_nof_elements();a++){\n"
      "   if(a!=0 && p_td.text->separator_encode){\n"
      "    p_buf.put_cs(*p_td.text->separator_encode);\n"
      "    encoded_length+=p_td.text->separator_encode->lengthof();\n"
      "   }\n"
      "   encoded_length+=(*this)[a].TEXT_encode(%s_descr_,p_buf);\n"
      "  }\n"
      "  if(p_td.text->end_encode){\n"
      "    p_buf.put_cs(*p_td.text->end_encode);\n"
      "    encoded_length+=p_td.text->end_encode->lengthof();\n"
      "  }\n"
      "  return encoded_length;\n"
      "}\n"
      ,name,sdef->oftypedescrname
      );
    src = mputprintf(src,
      "int %s::TEXT_decode(const TTCN_Typedescriptor_t& p_td,"
      " TTCN_Buffer& p_buf, Limit_Token_List& limit, boolean no_err"
      ", boolean first_call){\n"
      "  int decoded_length=0;\n"
      "  size_t pos=p_buf.get_pos();\n"
      "  boolean sep_found=FALSE;\n"
      "  int sep_length=0;\n"
      "  int ml=0;\n"
      "  if(p_td.text->begin_decode){\n"
      "    int tl;\n"
      "    if((tl=p_td.text->begin_decode->match_begin(p_buf))<0){\n"
      "          if(no_err)return -1;\n"
      "          TTCN_EncDec_ErrorContext::error\n"
      "              (TTCN_EncDec::ET_TOKEN_ERR, \"The specified token '%%s'"
      " not found for '%%s': \",(const char*)*(p_td.text->begin_decode)"
      ", p_td.name);\n"
      "          return 0;\n"
      "        }\n"
      "    decoded_length+=tl;\n"
      "    p_buf.increase_pos(tl);\n"
      "  }\n"
      "  if(p_td.text->end_decode){\n"
      "    limit.add_token(p_td.text->end_decode);\n"
      "    ml++;\n"
      "  }\n"
      "  if(p_td.text->separator_decode){\n"
      "    limit.add_token(p_td.text->separator_decode);\n"
      "    ml++;\n"
      "  }\n"
      "  if(first_call) {\n"
      "    set_size(0);\n"
      "  }\n"
      "  int more=get_nof_elements();\n"
      "  while(TRUE){\n"
      "    %s *val=new %s;\n"
      "    pos=p_buf.get_pos();\n"
      "    int len=val->TEXT_decode(%s_descr_,p_buf,limit,TRUE);\n"
      "    if(len==-1 || (len==0 && !limit.has_token())){\n"
      "      p_buf.set_pos(pos);\n"
      "      delete val;\n"
      "      if(sep_found){\n"
      "        p_buf.set_pos(p_buf.get_pos()-sep_length);\n"
      "        decoded_length-=sep_length;\n"
      "      }\n"
      "      break;\n"
      "    }\n"
      "    sep_found=FALSE;\n"
      "    if (refd_indices.empty()) {\n"
      "      val_ptr->value_elements = (%s**)reallocate_pointers"
      "((void**)val_ptr->value_elements, val_ptr->n_elements, "
      "val_ptr->n_elements + 1);\n"
      "      val_ptr->value_elements[val_ptr->n_elements]=val;\n"
      "      val_ptr->n_elements++;\n"
      "    }\n"
      "    else {\n"
      "      (*this)[get_nof_elements()] = *val;\n"
      "      delete val;\n"
      "    }\n"
      "    decoded_length+=len;\n"
      "    if(p_td.text->separator_decode){\n"
      "      int tl;\n"
      "      if((tl=p_td.text->separator_decode->match_begin(p_buf))<0){\n"
      "        break;\n"
      "      }\n"
      "      decoded_length+=tl;\n"
      "      p_buf.increase_pos(tl);\n"
      "      sep_length=tl;\n"
      "      sep_found=TRUE;\n"
      "    } else if(p_td.text->end_decode){\n"
      "      int tl;\n"
      "      if((tl=p_td.text->end_decode->match_begin(p_buf))!=-1){\n"
      "        decoded_length+=tl;\n"
      "        p_buf.increase_pos(tl);\n"
      "        limit.remove_tokens(ml);\n"
      "        return decoded_length;\n"
      "      }\n"
      "    } else if(limit.has_token(ml)){\n"
      "      int tl;\n"
      "      if((tl=limit.match(p_buf,ml))==0){\n"
      "        sep_found=FALSE;\n"
      "        break;\n"
      "      }\n"
      "    }\n"
      "  }\n"
      ,name,type,type,sdef->oftypedescrname,type
     );
    src = mputstr(src,
      "   limit.remove_tokens(ml);\n"
      "  if(p_td.text->end_decode){\n"
      "    int tl;\n"
      "    if((tl=p_td.text->end_decode->match_begin(p_buf))<0){\n"
      "          if(no_err){"
      "            if(!first_call){\n"
      "              set_size(more);\n"
      "            }\n"
      "            return -1;\n"
      "          }\n"
      "          TTCN_EncDec_ErrorContext::error"
      "(TTCN_EncDec::ET_TOKEN_ERR, \"The specified token '%s'"
      " not found for '%s': \",(const char*)*(p_td.text->end_decode)"
      ",p_td.name);\n"
      "          return decoded_length;\n"
      "        }\n"
      "    decoded_length+=tl;\n"
      "    p_buf.increase_pos(tl);\n"
      "  }\n"
      "  if(get_nof_elements()==0){\n"
      "    if(!(p_td.text->end_decode || p_td.text->begin_decode)) {\n"
      "      if(no_err)return -1;\n"
      "      TTCN_EncDec_ErrorContext::error"
      "(TTCN_EncDec::ET_TOKEN_ERR, \"No record/set of member found.\");\n"
      "      return decoded_length;\n"
      "    }\n"
      "  }\n"
      "  if(!first_call && more==get_nof_elements() && "
      "!(p_td.text->end_decode || p_td.text->begin_decode)) return -1;\n"
      "  return decoded_length;\n"
      "}\n"
     );
  }

  /* BER functions */
  if(ber_needed) {
    /* BER_encode_TLV() */
    src=mputprintf
      (src,
       "ASN_BER_TLV_t* %s::BER_encode_TLV(const TTCN_Typedescriptor_t&"
       " p_td, unsigned p_coding) const\n"
       "{\n"
       "  BER_chk_descr(p_td);\n"
       "  ASN_BER_TLV_t *new_tlv=BER_encode_chk_bound(is_bound());\n"
       "  if(!new_tlv) {\n"
       "    new_tlv=ASN_BER_TLV_t::construct(NULL);\n"
       "    TTCN_EncDec_ErrorContext ec;\n"
       "    for(int elem_i=0; elem_i<get_nof_elements(); elem_i++) {\n"
       "      ec.set_msg(\"Component #%%d: \", elem_i);\n"
       "      new_tlv->add_TLV((*this)[elem_i].BER_encode_TLV"
       "(%s_descr_, p_coding));\n"
       "    }\n"
       "%s"
       "  }\n"
       "  new_tlv=ASN_BER_V2TLV(new_tlv, p_td, p_coding);\n"
       "  return new_tlv;\n"
       "}\n"
       "\n"
       /* BER_decode_TLV() */
       "boolean %s::BER_decode_TLV(const TTCN_Typedescriptor_t& p_td,"
       " const ASN_BER_TLV_t& p_tlv, unsigned L_form)\n"
       "{\n"
       "  BER_chk_descr(p_td);\n"
       "  ASN_BER_TLV_t stripped_tlv;\n"
       "  BER_decode_strip_tags(*p_td.ber, p_tlv, L_form, stripped_tlv);\n"
       "  TTCN_EncDec_ErrorContext ec_0(\"While decoding '%%s' type: \","
       " p_td.name);\n"
       "  stripped_tlv.chk_constructed_flag(TRUE);\n"
       "  set_size(0);\n"
       "  size_t V_pos=0;\n"
       "  ASN_BER_TLV_t tmp_tlv;\n"
       "  TTCN_EncDec_ErrorContext ec_1(\"Component #\");\n"
       "  TTCN_EncDec_ErrorContext ec_2(\"0: \");\n"
       "  while(BER_decode_constdTLV_next(stripped_tlv, V_pos, L_form, "
       "tmp_tlv)) {\n"
       "    (*this)[get_nof_elements()].BER_decode_TLV(%s_descr_, tmp_tlv, "
       "L_form);\n"
       "    ec_2.set_msg(\"%%d: \", get_nof_elements());\n"
       "  }\n"
       "  return TRUE;\n"
       "}\n"
       "\n"
       , name, sdef->oftypedescrname
       , sdef->kind==SET_OF?"    new_tlv->sort_tlvs();\n":""
       , name, sdef->oftypedescrname
       );

    if(sdef->has_opentypes) {
      /* BER_decode_opentypes() */
      def=mputstr
        (def,
         "void BER_decode_opentypes(TTCN_Type_list& p_typelist,"
         " unsigned L_form);\n");
      src=mputprintf
        (src,
         "void %s::BER_decode_opentypes(TTCN_Type_list& p_typelist,"
         " unsigned L_form)\n"
         "{\n"
         "  p_typelist.push(this);\n"
         "  TTCN_EncDec_ErrorContext ec_0(\"Component #\");\n"
         "  TTCN_EncDec_ErrorContext ec_1;\n"
         "  for(int elem_i=0; elem_i<get_nof_elements(); elem_i++) {\n"
         "    ec_1.set_msg(\"%%d: \", elem_i);\n"
         "    (*this)[elem_i].BER_decode_opentypes(p_typelist,"
         " L_form);\n"
         "  }\n"
         "  p_typelist.pop();\n"
         "}\n"
         "\n"
         , name
         );
    }
  }

  if(raw_needed){
      src=mputprintf(src,
    "int %s::RAW_decode(const TTCN_Typedescriptor_t& p_td, TTCN_Buffer& p_buf, "
    "int limit, raw_order_t top_bit_ord, boolean /*no_err*/, int sel_field"
    ", boolean first_call){\n"
    "  int prepaddlength=p_buf.increase_pos_padd(p_td.raw->prepadding);\n"
    "  limit-=prepaddlength;\n"
    "  int decoded_length=0;\n"
    "  int decoded_field_length=0;\n"
    "  size_t start_of_field=0;\n"
    "  if(first_call) {\n"
    "    set_size(0);\n"
    "  }\n"
    "  int start_field=get_nof_elements();\n"
    "  if(p_td.raw->fieldlength || sel_field!=-1){\n"
    "    int a=0;\n"
    "    if(sel_field==-1) sel_field=p_td.raw->fieldlength;\n"
    "    for(a=0;a<sel_field;a++){\n"
    "      decoded_field_length=(*this)[a+start_field].RAW_decode(%s_descr_,"
    "p_buf,limit,top_bit_ord,TRUE);\n"
    "      if(decoded_field_length < 0) return decoded_field_length;\n"
    "      decoded_length+=decoded_field_length;\n"
    "      limit-=decoded_field_length;\n"
    "    }\n"
    "  } else {\n"
    "    int a=start_field;\n"
    "    if(limit==0){\n"
    "      if(!first_call) return -1;\n"
    "      return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n"
    "    }\n"
    "    while(limit>0){\n"
    "      start_of_field=p_buf.get_pos_bit();\n"
    "      decoded_field_length=(*this)[a].RAW_decode(%s_descr_,p_buf,limit,"
    "top_bit_ord,TRUE);\n"
    "      if(decoded_field_length < 0){\n"
    "        set_size(get_nof_elements() - 1);\n"
    "        p_buf.set_pos_bit(start_of_field);\n"
    "        if(a>start_field){\n"
    "        return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n"
    "        } else return -1;\n"
    "      }\n"
    "      decoded_length+=decoded_field_length;\n"
    "      limit-=decoded_field_length;\n"
    "      a++;\n"
    ,name,sdef->oftypedescrname,sdef->oftypedescrname
    );
    if(sdef->raw.extension_bit!=XDEFNO && sdef->raw.extension_bit!=XDEFDEFAULT){
      src=mputprintf(src,
    "      if (%sp_buf.get_last_bit())\n"
    "        return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n", sdef->raw.extension_bit == XDEFYES ? "" : "!");
    }
      src=mputprintf(src,
    "    }\n"
    "  }\n"
    " return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n"
    "}\n\n"
    "int %s::RAW_encode(const TTCN_Typedescriptor_t& p_td,"
    "RAW_enc_tree& myleaf) const{\n"
    "  int encoded_length=0;\n"
    "  int encoded_num_of_records=p_td.raw->fieldlength?"
    "smaller(get_nof_elements(), p_td.raw->fieldlength)"
    ":get_nof_elements();\n"
    "  myleaf.isleaf=FALSE;\n"
    "  myleaf.rec_of=TRUE;\n"
    "  myleaf.body.node.num_of_nodes=encoded_num_of_records;\n"
    "  myleaf.body.node.nodes=init_nodes_of_enc_tree(encoded_num_of_records);\n"
    "  for(int a=0;a<encoded_num_of_records;a++){\n"
    "    myleaf.body.node.nodes[a]=new RAW_enc_tree(TRUE,&myleaf,"
    "&(myleaf.curr_pos),a,%s_descr_.raw);\n"
    "    encoded_length+=(*this)[a].RAW_encode(%s_descr_,"
    "*myleaf.body.node.nodes[a]);\n"
    "  }\n"
    " return myleaf.length=encoded_length;\n}\n\n"
    , name, sdef->oftypedescrname, sdef->oftypedescrname
    );
  }

  if (xer_needed) { /* XERSTUFF encoder codegen for record-of, RT1 */
    def = mputstr(def,
      "char **collect_ns(const XERdescriptor_t& p_td, size_t& num, bool& def_ns) const;\n");

    /* Write the body of the XER encoder/decoder functions. The declaration
     * is written by def_encdec() in encdec.c */
    src = mputprintf(src,
      "boolean %s::can_start(const char *name, const char *uri, "
      "XERdescriptor_t const& xd, unsigned int flavor) {\n"
      "  boolean e_xer = is_exer(flavor);\n"
      "  if (e_xer && (xd.xer_bits & ANY_ELEMENT)) "
      , name
      );
    if (sdef->nFollowers) {
      /* If there are optional fields following the record-of, then seeing
       * {any XML tag that belongs to those fields} where the record-of may be
       * means that the record-of is empty. */
      size_t f;
      src = mputstr(src, "{\n");
      for (f = 0; f < sdef->nFollowers; ++f) {
        src = mputprintf(src,
          "    if (%s::can_start(name, uri, %s_xer_, flavor)) return FALSE;\n"
          , sdef->followers[f].type
          , sdef->followers[f].typegen
          );
      }
      src = mputstr(src,
        "    return TRUE;\n"
        "  }\n");
    }
    else src = mputstr(src, "return TRUE;\n");

    src = mputprintf(src,
      "  if ((!e_xer || !(xd.xer_bits & UNTAGGED)) && !(flavor & XER_RECOF)) return "
      "check_name(name, xd, e_xer) && (!e_xer || check_namespace(uri, xd));\n"
      "  else return %s::can_start(name, uri, %s_xer_, flavor | XER_RECOF);\n"
      "}\n\n"
      , sdef->type
      , sdef->oftypedescrname
      );

    src = mputprintf(src,
      "char ** %s::collect_ns(const XERdescriptor_t& p_td, size_t& num, bool& def_ns) const {\n"
      "  size_t num_collected;\n"
      "  char **collected_ns = Base_Type::collect_ns(p_td, num_collected, def_ns);\n"
      /* The above may throw but then nothing was allocated. */
      "  if (val_ptr) try {\n"
      "    char **new_ns;\n"
      "    size_t num_new;\n"
      "    for (int i = 0; i < get_nof_elements(); ++i) {\n"
      "      bool def_ns_1 = false;"
      "      new_ns = (*this)[i].collect_ns(%s_xer_, num_new, def_ns_1);\n"
      "      merge_ns(collected_ns, num_collected, new_ns, num_new);\n"
      "      def_ns = def_ns || def_ns_1;\n" /* alas, no ||= */
      "    }\n"
      "  }\n"
      "  catch (...) {\n"
      /* Probably a TC_Error thrown from elements[i]->collect_ns() if e.g.
       * encoding an unbound value. */
      "    while (num_collected > 0) Free(collected_ns[--num_collected]);\n"
      "    Free(collected_ns);\n"
      "    throw;\n"
      "  }\n"
      "  num = num_collected;\n"
      "  return collected_ns;\n"
      "}\n\n"
      , name, sdef->oftypedescrname);

    src=mputprintf(src,
      "int %s::XER_encode(const XERdescriptor_t& p_td,"
      " TTCN_Buffer& p_buf, unsigned int p_flavor, int p_indent) const\n{\n"
      "  if (val_ptr == 0) TTCN_error(\"Attempt to XER-encode an unbound record of\");\n" /* TODO type name */
      "  int encoded_length=(int)p_buf.get_len();\n"
      "  boolean e_xer = is_exer(p_flavor);\n"
      "  boolean own_tag = !(e_xer && p_indent && ((p_td.xer_bits & (ANY_ELEMENT|ANY_ATTRIBUTES|UNTAGGED))\n"
      "    || (p_flavor & USE_TYPE_ATTR)));\n"
      "  boolean indenting = !is_canonical(p_flavor) && own_tag;\n"
      "%s" /* Factor out p_indent if not attribute */
      "  int nof_elements = get_nof_elements();\n"
      "  if (nof_elements==0) {\n" /* Empty record of */
      , name
      , sdef->xerAttribute ? "" : "  if (indenting) do_indent(p_buf, p_indent);\n"
      );
    if (sdef->xerAttribute) {
      src=mputstr(src,
        "    if (e_xer) {\n" /* Empty attribute. */
        "      begin_attribute(p_td, p_buf);\n"
        "      p_buf.put_c('\\'');\n"
        "    } else\n");
    }
    else {
      src = mputstr(src,
        "    if (own_tag)");
    }
    src=mputprintf(src,
      "    {\n" /* Do empty element tag */
      "%s"
      "      p_buf.put_c('<');\n"
      "      if (e_xer) write_ns_prefix(p_td, p_buf);\n"
      "      p_buf.put_s((size_t)p_td.namelens[e_xer]-2, (const unsigned char*)p_td.names[e_xer]);\n"
      /* namespace declarations for the toplevel element */
      "      if (e_xer && p_indent==0)\n"
      "      {\n"
      "        size_t num_collected = 0;\n"
      "        char **collected_ns = NULL;\n"
      "        bool def_ns = false;\n"
      "        collected_ns = collect_ns(p_td, num_collected, def_ns);\n"
      "        for (size_t cur_coll = 0; cur_coll < num_collected; ++cur_coll) {\n"
      "          p_buf.put_s(strlen(collected_ns[cur_coll]), (cbyte*)collected_ns[cur_coll]);\n"
      "          Free(collected_ns[cur_coll]);\n"
      "        }\n"
      "        Free(collected_ns);\n"
      "      }\n"

      "      p_buf.put_s(2 + indenting, (const unsigned char*)\"/>\\n\");\n"
      "    }\n"
      "  }\n"
      "  else {\n" /* Not empty record of. Start tag or attribute */
      , sdef->xerAttribute ? "      if (indenting) do_indent(p_buf, p_indent);\n" : ""
      );
    if (sdef->xerAnyAttrElem) {
      src = mputstr(src,
        "    if (e_xer && (p_td.xer_bits & ANY_ATTRIBUTES)) {\n"
        "      static const universal_char sp = { 0,0,0,' ' };\n"
        "      static const universal_char tb = { 0,0,0,9 };\n"
        "      size_t buf_len = p_buf.get_len(), shorter = 0;\n"
        "      const unsigned char * const buf_data = p_buf.get_data();\n"
        "      if (buf_data[buf_len - 1 - shorter] == '\\n') ++shorter;\n"
        "      if (buf_data[buf_len - 1 - shorter] == '>' ) ++shorter;\n"
        "      unsigned char saved[4];\n"
        "      memcpy(saved, buf_data + (buf_len - shorter), shorter);\n"
        "      p_buf.increase_length(-shorter);\n"
        "      for (int i = 0; i < nof_elements; ++i) {\n"
        "        TTCN_EncDec_ErrorContext ec_0(\"Attribute %d: \", i);\n"
        "        if (!is_elem_bound(i)) {\n"
        "          TTCN_EncDec_ErrorContext::error(TTCN_EncDec::ET_UNBOUND,\n"
        "            \"Encoding an unbound universal charstring value.\");\n"
        "          continue;\n"
        "        }\n"
        "        size_t len = val_ptr->value_elements[i]->lengthof();\n"
        "        for (;;) {\n"
        "          const UNIVERSAL_CHARSTRING_ELEMENT& ue = (*val_ptr->value_elements[i])[len - 1];\n"
        "          if (sp == ue || tb == ue) --len;\n"
        "          else break;\n"
        "        }\n"
        "        size_t j, sp_at = 0;\n"
        /* Find the "separators" in each string */
        "        for (j = 0; j < len; j++) {\n"
        "          UNIVERSAL_CHARSTRING_ELEMENT ue = (*val_ptr->value_elements[i])[j];\n"
        "          if (sp_at) {\n"
        "            if (sp == ue || tb == ue) {}\n"
        "            else break;\n"
        "          } else {\n"
        "            if (sp == ue || tb == ue) sp_at = j;\n"
        "          }\n"
        "        } // next j\n"
        "        size_t buf_start = p_buf.get_len();\n"
        /* Now write them */
        "        if (sp_at > 0) {\n"
        "          char * ns = mprintf(\" xmlns:b%d='\", i);\n"
        "          size_t ns_len = mstrlen(ns);\n"
        "          p_buf.put_s(ns_len, (const unsigned char*)ns);\n"

        "          UNIVERSAL_CHARSTRING before(sp_at, (const universal_char*)(*val_ptr->value_elements[i]));\n"
        "          before.XER_encode(UNIVERSAL_CHARSTRING_xer_, p_buf, p_flavor | ANY_ATTRIBUTES, p_indent);\n"
        // Ensure the namespace abides to its restrictions
        "          if (p_td.xer_bits & (ANY_FROM | ANY_EXCEPT)) {\n"
        "            TTCN_Buffer ns_buf;\n"
        "            before.encode_utf8(ns_buf);\n"
        "            CHARSTRING cs;\n"
        "            ns_buf.get_string(cs);\n"
        "            check_namespace_restrictions(p_td, (const char*)cs);\n"
        "          }\n"

        "          p_buf.put_c('\\'');\n"
        "          p_buf.put_c(' ');\n"

        /* Keep just the "b%d" part from ns */
        "          p_buf.put_s(ns_len - 9, (const unsigned char*)ns + 7);\n"
        "          p_buf.put_c(':');\n"
        "          Free(ns);\n"
        "        }\n"
        "        else {\n"
        "          p_buf.put_c(' ');\n"
        "          j = 0;\n"
        // Make sure the unqualified namespace is allowed
        "          if (p_td.xer_bits & (ANY_FROM | ANY_EXCEPT)) {\n"
        "            check_namespace_restrictions(p_td, NULL);\n"
        "          }\n"
        "        }\n"

        "        UNIVERSAL_CHARSTRING after(len - j, (const universal_char*)(*val_ptr->value_elements[i]) + j);\n"
        "        after.XER_encode(UNIVERSAL_CHARSTRING_xer_, p_buf, p_flavor | ANY_ATTRIBUTES, p_indent);\n"
        // Put this attribute in a dummy element and walk through it to check its validity
        "        TTCN_Buffer check_buf;\n"
        "        check_buf.put_s(2, (unsigned char*)\"<a\");\n"
        "        check_buf.put_s(p_buf.get_len() - buf_start, p_buf.get_data() + buf_start);\n"
        "        check_buf.put_s(2, (unsigned char*)\"/>\");"
        "        XmlReaderWrap checker(check_buf);\n"
        "        while (1 == checker.Read()) ;\n"
        "      }\n"

        "      p_buf.put_s(shorter, saved);\n" /* restore the '>' and anything after */
        "    } else {\n");
    }
    if (sdef->xerAttribute) {
      src=mputstr(src,
        "    if (e_xer) {\n"
        "      begin_attribute(p_td, p_buf);\n"
        "    } else\n");
    }
    src=mputprintf(src,
      "    if (own_tag) {\n"
      "%s"
      "      p_buf.put_c('<');\n"
      "      boolean write_ns = (e_xer && p_indent==0);\n"
      "      boolean keep_newline = (indenting && !(e_xer && (p_td.xer_bits & XER_LIST)));\n"
      "      if (e_xer) write_ns_prefix(p_td, p_buf);\n"
      "      p_buf.put_s((size_t)p_td.namelens[e_xer]-write_ns-(write_ns || !keep_newline), "
      "(const unsigned char*)p_td.names[e_xer]);\n"

      /* namespace declarations for the toplevel element */
      "      if (e_xer && p_indent==0)\n"
      "      {\n"
      "        size_t num_collected = 0;\n"
      "        char **collected_ns = NULL;\n"
      "        bool def_ns = false;\n"
      "        collected_ns = collect_ns(p_td, num_collected, def_ns);\n"
      "        for (size_t cur_coll = 0; cur_coll < num_collected; ++cur_coll) {\n"
      "          p_buf.put_s(strlen(collected_ns[cur_coll]), (cbyte*)collected_ns[cur_coll]);\n"
      "          Free(collected_ns[cur_coll]);\n"
      "        }\n"
      "        Free(collected_ns);\n"
      "        p_buf.put_s(1 + keep_newline, (cbyte*)\">\\n\");\n"
      "      }\n"
      , sdef->xerAttribute ? "      if (indenting) do_indent(p_buf, p_indent);\n" : ""
      );
    if (sdef->xmlValueList) {
      src=mputstr(src, "      if (indenting && !e_xer) do_indent(p_buf, p_indent+1);\n"); /* !e_xer or GDMO */
    }
    src=mputstr(src,
      "    }\n"
      "    p_flavor |= XER_RECOF | (p_td.xer_bits & XER_LIST);\n"
      "    TTCN_EncDec_ErrorContext ec_0(\"Index \");\n"
      "    TTCN_EncDec_ErrorContext ec_1;\n"
      );
    src=mputprintf(src,
      "    for (int i = 0; i < nof_elements; ++i) {\n"
      "      ec_1.set_msg(\"%%d: \", i);\n"
      "      if (e_xer && (p_td.xer_bits & XER_LIST) && i>0) p_buf.put_c(' ');\n"
      "      (*this)[i].XER_encode(%s_xer_, p_buf, p_flavor, p_indent+own_tag);\n"
      "    }\n"
      "    if (indenting && !is_exerlist(p_flavor)) {\n",
      sdef->oftypedescrname
    );
    if (sdef->xmlValueList) {
      src=mputstr(src, "      if (!e_xer) p_buf.put_c('\\n');\n"); /* !e_xer or GDMO */
    }
    src=mputstr(src,
      "      do_indent(p_buf, p_indent);\n"
      "    }\n");
    if (sdef->xerAttribute) {
      src=mputstr(src,
        "    if (e_xer) p_buf.put_c('\\'');\n"
        "    else\n");
    }
    src=mputstr(src,
      "    if (own_tag){\n"
      "      p_buf.put_c('<');\n"
      "      p_buf.put_c('/');\n"
      "      if (e_xer) write_ns_prefix(p_td, p_buf);\n"
      "      p_buf.put_s((size_t)p_td.namelens[e_xer]-!indenting, (const unsigned char*)p_td.names[e_xer]);\n"
      "    }\n");
    if (sdef->xerAnyAttrElem) {
      src = mputstr(src, "  }\n");
    }
    src=mputstr(src,
      "  }\n" /* end if(no elements) */
      "  return (int)p_buf.get_len() - encoded_length;\n"
      "}\n\n"
      );

    src = mputprintf(src, /* XERSTUFF decoder codegen for record-of */
#ifndef NDEBUG
      "// written by %s in " __FILE__ " at %d\n"
#endif
      "int %s::XER_decode(const XERdescriptor_t& p_td, XmlReaderWrap& p_reader, "
      "unsigned int p_flavor)\n{\n"
      "  boolean e_xer = is_exer(p_flavor);\n"
      "  int xerbits = p_td.xer_bits;\n"
      "  if (p_flavor & XER_TOPLEVEL) xerbits &= ~UNTAGGED;\n"
      "  boolean own_tag = !(e_xer && ((xerbits & (ANY_ELEMENT|ANY_ATTRIBUTES|UNTAGGED))\n"
      "    || (p_flavor & USE_TYPE_ATTR)));\n" /* incase the parent has USE-UNION */
      /* not toplevel anymore and remove the flags for USE-UNION the oftype doesn't need them */
      "  p_flavor &= ~XER_TOPLEVEL & ~XER_LIST & ~USE_TYPE_ATTR;\n" 
      "  int rd_ok=1, xml_depth=-1;\n"
      "  set_size(0);\n" /* empty but initialized array */
      "  int type = 0;\n" /* none */
      "  if (own_tag) for (rd_ok = p_reader.Ok(); rd_ok == 1; rd_ok = p_reader.Read()) {\n"
      "    type = p_reader.NodeType();\n"
      "    if (e_xer && (p_td.xer_bits & XER_ATTRIBUTE)) {\n"
      "      if ((XML_READER_TYPE_ELEMENT   == type && p_reader.MoveToFirstAttribute() == 1)\n"
      "        || XML_READER_TYPE_ATTRIBUTE == type) {\n"
      "        verify_name(p_reader, p_td, e_xer);\n"
      "        break;"
      "      }\n"
      "    }\n"
      "    if (e_xer && (p_td.xer_bits & XER_LIST)) {\n"
      "      if (XML_READER_TYPE_TEXT == type) break;\n"
      "    }\n"
      "    else {\n"
      "      if (XML_READER_TYPE_ELEMENT == type) {\n"
      "        verify_name(p_reader, p_td, e_xer);\n"
      "        xml_depth = p_reader.Depth();\n"
      "        break;\n"
      "      }\n"
      "    }\n" /* endif(e_xer && list) */
      "  }\n" /* next read */
      "  else xml_depth = p_reader.Depth();\n"
      "  p_flavor |= XER_RECOF;\n"
#ifndef NDEBUG
      , __FUNCTION__, __LINE__
#endif
      , name
      );

    src = mputprintf(src,
      "  if (e_xer && (p_td.xer_bits & XER_LIST)) {\n" /* LIST decoding*/
      "    char *x_val = (char*)p_reader.NewValue();\n" /* we own it */
      "    size_t x_pos = 0;\n"
      "    size_t x_len = strlen(x_val);\n"
      /* The string contains a bunch of values separated by whitespace.
       * Tokenize the string and create a new buffer which looks like
       * an XML element (<ns:name xmlns:ns='uri'>value</ns:name>),
       * then use that to decode the value. */
      "    for(char * str = strtok(x_val, \" \\t\\x0A\\x0D\"); str != 0; str = strtok(x_val + x_pos, \" \\t\\x0A\\x0D\")) {\n"
      // Calling strtok with NULL won't work here, since the decoded element can have strtok calls aswell
      "      x_pos += strlen(str) + 1;\n"
      "      TTCN_Buffer buf_2;\n"
      "      buf_2.put_c('<');\n"
      "      write_ns_prefix(%s_xer_, buf_2);\n"
      "      const char * const exer_name = %s_xer_.names[1];\n"
      "      boolean i_can_has_ns = %s_xer_.my_module != 0 && %s_xer_.ns_index != -1;\n"
      /* If it has a namespace, chop off the '>' from the end */
      "      buf_2.put_s((size_t)%s_xer_.namelens[1]-1-i_can_has_ns, (cbyte*)exer_name);\n"
      "      if (i_can_has_ns) {\n"
      "        const namespace_t * const pns = %s_xer_.my_module->get_ns(%s_xer_.ns_index);\n"
      "        buf_2.put_s(7 - (*pns->px == 0), (cbyte*)\" xmlns:\");\n"
      "        buf_2.put_s(strlen(pns->px), (cbyte*)pns->px);\n"
      "        buf_2.put_s(2, (cbyte*)\"='\");\n"
      "        buf_2.put_s(strlen(pns->ns), (cbyte*)pns->ns);\n"
      "        buf_2.put_s(2, (cbyte*)\"'>\");\n"
      "      }\n"
      /* start tag completed */
      "      buf_2.put_s(strlen(str), (cbyte*)str);\n"
      "      buf_2.put_c('<');\n"
      "      buf_2.put_c('/');\n"
      "      write_ns_prefix(%s_xer_, buf_2);\n"
      "      buf_2.put_s((size_t)%s_xer_.namelens[1], (cbyte*)exer_name);\n"
      "      XmlReaderWrap reader_2(buf_2);\n"
      "      rd_ok = reader_2.Read();\n" /* Move to the start element. */
      /* Don't move to the #text, that's the callee's responsibility. */
      /* The call to the non-const operator[] creates a new element object,
       * then we call its XER_decode with the temporary XML reader. */
      "      (*this)[get_nof_elements()].XER_decode(%s_xer_, reader_2, p_flavor);\n"
      "      if (p_flavor & EXIT_ON_ERROR && !(*this)[get_nof_elements() - 1].is_bound()) {\n"
      "        if (1 == get_nof_elements()) {\n"
      // Failed to decode even the first element
      "          clean_up();\n"
      "        } else {\n"
      // Some elements were successfully decoded -> only delete the last one
      "          set_size(get_nof_elements() - 1);\n"
      "        }\n"
      "        xmlFree(x_val);\n"
      "       return -1;\n"
      "      }\n"
      "      if (x_pos >= x_len) break;\n"
      "    }\n"
      "    xmlFree(x_val);\n"
      "    if ((p_td.xer_bits & XER_ATTRIBUTE)) ;\n"
      /* Let the caller do AdvanceAttribute() */
      "    else if (own_tag) {\n"
      "      p_reader.Read();\n" /* on closing tag */
      "      p_reader.Read();\n" /* past it */
      "    }\n"
      "  }\n"
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
    );

    src = mputprintf(src,
      "  else {\n"
      "    if (p_flavor & PARENT_CLOSED) ;\n"
      /* Nothing to do, but do not advance past the parent's element */
      "    else if (own_tag && p_reader.IsEmptyElement()) rd_ok = p_reader.Read();\n"
      /* It's our XML empty element: nothing to do, skip past it */
      "    else {\n"
      /* Note: there is no p_reader.Read() at the end of the loop below.
       * Each element is supposed to consume enough to leave the next element
       * well-positioned. */
      "      for (rd_ok = own_tag ? p_reader.Read() : p_reader.Ok(); rd_ok == 1; ) {\n"
      "        type = p_reader.NodeType();\n"
      "        if (XML_READER_TYPE_ELEMENT == type)\n"
      "        {\n");
    if (sdef->xerAnyAttrElem) {
      src = mputprintf(src,
        "          if (e_xer && (p_td.xer_bits & ANY_ELEMENT)) {\n"
        /* This is a record-of with ANY-ELEMENT applied, which is really meant
         * for the element type (a string), so behave like a record-of
         * (string with ANY-ELEMENT): call the non-const operator[]
         * to create a new element, then read the entire XML element into it. */
        "            (*this)[get_nof_elements()] = (const char*)p_reader.ReadOuterXml();\n"
        /* Consume the element, then move ahead */
        "            for (rd_ok = p_reader.Read(); rd_ok == 1 && p_reader.Depth() > xml_depth; rd_ok = p_reader.Read()) {}\n"
        "            if (p_reader.NodeType() != XML_READER_TYPE_ELEMENT) rd_ok = p_reader.Read();\n"
        "          } else");
    }
    src = mputprintf(src,
      "          {\n"
      /* An untagged record-of ends if it encounters an element with a name
       * that doesn't match its component */
      "            if (!own_tag && !can_start((const char*)p_reader.LocalName(), "
      "(const char*)p_reader.NamespaceUri(), %s_xer_, p_flavor)) {\n"
      "              for (; rd_ok == 1 && p_reader.Depth() > xml_depth; rd_ok = p_reader.Read()) ;\n"
      "              break;\n"
      "            }\n"
      /* The call to the non-const operator[] creates the element */
      "            (*this)[get_nof_elements()].XER_decode(%s_xer_, p_reader, p_flavor);\n"
      "          }\n"
      "        }\n"
      "        else if (XML_READER_TYPE_END_ELEMENT == type) {\n"
      "          for (; p_reader.Depth() > xml_depth; rd_ok = p_reader.Read()) ;\n"
      "          if (own_tag) {\n"
      "            verify_end(p_reader, p_td, xml_depth, e_xer);\n"
      "            rd_ok = p_reader.Read();\n" /* move forward one last time */
      "          }\n"
      "          break;\n"
      "        }\n"
      "        else {\n"
      "          rd_ok = p_reader.Read();\n"
      "        }\n"
      "      }\n" /* next read */
      "    }\n" /* if not empty element */
      "  }\n" /* if not LIST */
      "  return 1;\n"
      "}\n\n"
      , sdef->oftypedescrname, sdef->oftypedescrname
    );
  }
  if (json_needed) {
    // JSON encode, RT1
    src = mputprintf(src,
      "int %s::JSON_encode(const TTCN_Typedescriptor_t&, JSON_Tokenizer& p_tok) const\n"
      "{\n"
      "  if (!is_bound()) {\n"
      "    TTCN_EncDec_ErrorContext::error(TTCN_EncDec::ET_UNBOUND,\n"
      "      \"Encoding an unbound value of type %s.\");\n"
      "    return -1;\n"
      "  }\n\n"
      "  int enc_len = p_tok.put_next_token(JSON_TOKEN_ARRAY_START, NULL);\n"
      "  for(int i = 0; i < get_nof_elements(); ++i) {\n"
      "    int ret_val = (*this)[i].JSON_encode(%s_descr_, p_tok);\n"
      "    if (0 > ret_val) break;\n"
      "    enc_len += ret_val;\n"
      "  }\n"
      "  enc_len += p_tok.put_next_token(JSON_TOKEN_ARRAY_END, NULL);\n"
      "  return enc_len;\n"
      "}\n\n"
      , name, dispname, sdef->oftypedescrname);
    
    // JSON decode, RT1
    src = mputprintf(src,
      "int %s::JSON_decode(const TTCN_Typedescriptor_t&, JSON_Tokenizer& p_tok, boolean p_silent)\n"
      "{\n"
      "  json_token_t token = JSON_TOKEN_NONE;\n"
      "  int dec_len = p_tok.get_next_token(&token, NULL, NULL);\n"
      "  if (JSON_TOKEN_ERROR == token) {\n"
      "    JSON_ERROR(TTCN_EncDec::ET_INVAL_MSG, JSON_DEC_BAD_TOKEN_ERROR, \"\");\n"
      "    return JSON_ERROR_FATAL;\n"
      "  }\n"
      "  else if (JSON_TOKEN_ARRAY_START != token) {\n"
      "    return JSON_ERROR_INVALID_TOKEN;\n"
      "  }\n\n"
      "  set_size(0);\n"
      "  while (true) {\n"
      "    size_t buf_pos = p_tok.get_buf_pos();\n"
      "    %s* val = new %s;\n"
      "    int ret_val = val->JSON_decode(%s_descr_, p_tok, p_silent);\n"
      "    if (JSON_ERROR_INVALID_TOKEN == ret_val) {\n"
      "      p_tok.set_buf_pos(buf_pos);\n"
      "      delete val;\n"
      "      break;\n"
      "    }\n"
      "    else if (JSON_ERROR_FATAL == ret_val) {\n"
      "      delete val;\n"
      "      if (p_silent) {\n"
      "        clean_up();\n"
      "      }\n"
      "      return JSON_ERROR_FATAL;\n"
      "    }\n"
      "    if (refd_indices.empty()) {\n"
      "      val_ptr->value_elements = (%s**)reallocate_pointers(\n"
      "        (void**)val_ptr->value_elements, val_ptr->n_elements, val_ptr->n_elements + 1);\n"
      "      val_ptr->value_elements[val_ptr->n_elements] = val;\n"
      "      val_ptr->n_elements++;\n"
      "      dec_len += ret_val;\n"
      "    }\n"
      "    else {\n"
      "      (*this)[get_nof_elements()] = *val;\n"
      "      delete val;\n"
      "    }\n"
      "  }\n\n"
      "  dec_len += p_tok.get_next_token(&token, NULL, NULL);\n"
      "  if (JSON_TOKEN_ARRAY_END != token) {\n"
      "    JSON_ERROR(TTCN_EncDec::ET_INVAL_MSG, JSON_DEC_REC_OF_END_TOKEN_ERROR, \"\");\n"
      "    if (p_silent) {\n"
      "      clean_up();\n"
      "    }\n"
      "    return JSON_ERROR_FATAL;\n"
      "  }\n\n"
      "  return dec_len;\n"
      "}\n\n"
      , name, type, type, sdef->oftypedescrname, type);
  }
  /* end of class */
  def = mputstr(def, "};\n\n");

  output->header.class_decls = mputprintf(output->header.class_decls,
                                          "class %s;\n", name);
  output->header.class_defs = mputstr(output->header.class_defs, def);
  Free(def);
  output->source.methods = mputstr(output->source.methods, src);
  Free(src);
  /* Copied from record.c.  */
  output->header.function_prototypes =
	mputprintf(output->header.function_prototypes,
	"extern boolean operator==(null_type null_value, const %s& "
	  "other_value);\n", name);
  output->source.function_bodies =
	mputprintf(output->source.function_bodies,
	"boolean operator==(null_type, const %s& other_value)\n"
	"{\n"
    "if (other_value.val_ptr == NULL)\n"
	"TTCN_error(\"The right operand of comparison is an unbound value of "
    "type %s.\");\n"
	"return other_value.get_nof_elements() == 0;\n"
	"}\n\n", name, dispname);

  output->header.function_prototypes =
	mputprintf(output->header.function_prototypes,
	"inline boolean operator!=(null_type null_value, const %s& "
	  "other_value) "
	"{ return !(null_value == other_value); }\n", name);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void defRecordOfClassMemAllocOptimized(const struct_of_def *sdef, output_struct *output)
{
  char *def = NULL, *src = NULL;
  const char *name = sdef->name, *dispname = sdef->dispname;
  const char *type = sdef->type;
  boolean ber_needed = sdef->isASN1 && enable_ber();
  boolean raw_needed = sdef->hasRaw && enable_raw();
  boolean text_needed = sdef->hasText && enable_text();
  boolean xer_needed = sdef->hasXer && enable_xer();
  boolean json_needed = sdef->hasJson && enable_json();

  /* Class definition and private data members */
  def = mputprintf(def,
#ifndef NDEBUG
    "// written by %s in " __FILE__ " at %d\n"
#endif
    "class %s : public Base_Type {\n"
    "int n_elements;\n"
    "%s* value_elements;\n"
#ifndef NDEBUG
      , __FUNCTION__, __LINE__
#endif
    , name, type);

  /* private member functions */
  def = mputprintf(def,
    "private:\n"
    "friend boolean operator==(null_type null_value, "
	  "const %s& other_value);\n", name);

  def = mputprintf(def,
    "void copy_value(const %s& other_value);\n", name);
  src = mputprintf(src,
    "void %s::copy_value(const %s& other_value)\n"
    "{\n"
    "if (other_value.n_elements==-1) {\n"
    "TTCN_error(\"Copying an unbound value of type %s.\");\n"
    "} else if (other_value.n_elements==0) {\n"
    "n_elements = 0;\n"
    "value_elements = NULL;\n"
    "} else {\n"
    "n_elements = other_value.n_elements;\n"
    "value_elements = new %s[n_elements];\n"
    "for (int act_elem=0; act_elem<n_elements; act_elem++) {\n"
    "  if (other_value.value_elements[act_elem].is_bound()) {\n"
    "    value_elements[act_elem] = other_value.value_elements[act_elem];\n"
    "  }\n"
    "}\n"
    "}\n"
    "}\n\n",
    name, name, dispname, type);

  if (sdef->kind == SET_OF) {
    /* callback function for comparison */
    def = mputstr(def, "static boolean compare_function("
	"const Base_Type *left_ptr, int left_index, "
        "const Base_Type *right_ptr, int right_index);\n");
    src = mputprintf(src, "boolean %s::compare_function("
	"const Base_Type *left_ptr, int left_index, "
        "const Base_Type *right_ptr, int right_index)\n"
        "{\n"
        "if (((const %s*)left_ptr)->n_elements==-1) "
        "TTCN_error(\"The left operand of comparison is an unbound value of "
        "type %s.\");\n"
        "if (((const %s*)right_ptr)->n_elements==-1) "
        "TTCN_error(\"The right operand of comparison is an unbound value of "
        "type %s.\");\n"
        "if (((const %s*)left_ptr)->value_elements[left_index].is_bound()){\n"
        "if (((const %s*)right_ptr)->value_elements[right_index].is_bound()){\n"
        "return ((const %s*)left_ptr)->value_elements[left_index] == "
        "((const %s*)right_ptr)->value_elements[right_index];\n"
        "} else return FALSE;\n"
        "} else {\n"
        "return !((const %s*)right_ptr)->value_elements[right_index].is_bound();\n"
        "}\n"
        "}\n\n", name, name, dispname, name, dispname, name, name, name, name, name);
  }

  /* public member functions */
  def = mputstr(def, "\npublic:\n");
  def = mputprintf(def, "  typedef %s of_type;\n", sdef->type);

  /* constructors */
  def = mputprintf(def, "%s(): n_elements(-1), value_elements(NULL) {}\n", name);

  def = mputprintf(def, "%s(null_type other_value): n_elements(0), value_elements(NULL) {}\n", name);

  /* copy constructor */
  def = mputprintf(def, "%s(const %s& other_value) { copy_value(other_value); }\n", name, name);

  /* destructor */
  def = mputprintf(def, "~%s() { clean_up(); }\n\n", name);

  /* clean_up function */
  def = mputstr(def, "void clean_up();\n");
  src = mputprintf(src,
    "void %s::clean_up()\n"
    "{\n"
    "if (n_elements!=-1) {\n"
    "delete[] value_elements;\n"
    "n_elements = -1;\n"
    "value_elements = NULL;\n"
    "}\n"
    "}\n\n", name);

  /* assignment operators */
  def = mputprintf(def, "%s& operator=(null_type other_value);\n", name);
  src = mputprintf(src,
    "%s& %s::operator=(null_type)\n"
    "{\n"
    "clean_up();\n"
    "n_elements=0;\n"
    "value_elements=NULL;\n"
    "return *this;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s& operator=(const %s& other_value);\n\n",
                   name, name);
  src = mputprintf(src,
    "%s& %s::operator=(const %s& other_value)\n"
    "{\n"
    "if (other_value.n_elements == -1) "
      "TTCN_error(\"Assigning an unbound value of type %s.\");\n"
    "if (this != &other_value) {\n"
    "clean_up();\n"
    "copy_value(other_value);\n"
    "}\n"
    "return *this;\n"
    "}\n\n", name, name, name, dispname);

  /* comparison operators */
  def = mputstr(def, "boolean operator==(null_type other_value) const;\n");
  src = mputprintf(src,
    "boolean %s::operator==(null_type) const\n"
    "{\n"
    "if (n_elements==-1)\n"
      "TTCN_error(\"The left operand of comparison is an unbound value of "
      "type %s.\");\n"
    "return n_elements==0;\n"
    "}\n\n", name, dispname);

  def = mputprintf(def, "boolean operator==(const %s& other_value) const;\n",
                   name);
  src = mputprintf(src,
    "boolean %s::operator==(const %s& other_value) const\n"
    "{\n"
    "if (n_elements==-1) "
      "TTCN_error(\"The left operand of comparison is an unbound value of type "
      "%s.\");\n"
    "if (other_value.n_elements==-1) "
      "TTCN_error(\"The right operand of comparison is an unbound value of type "
      "%s.\");\n"
    "if (this==&other_value) return TRUE;\n", name, name,
    dispname, dispname);

  if (sdef->kind == SET_OF) {
    src = mputstr(src,
       "return compare_set_of(this, n_elements, &other_value, "
       "other_value.n_elements, compare_function);\n");
  } else {
    src = mputstr(src,
      "if (n_elements!=other_value.n_elements) return FALSE;\n"
      "for (int elem_count = 0; elem_count < n_elements; elem_count++){\n"
      "if (value_elements[elem_count].is_bound()){\n"
      "if (other_value.value_elements[elem_count].is_bound()){\n"
      "  if (value_elements[elem_count] != "
        "other_value.value_elements[elem_count]) return FALSE;\n"
      "} else return FALSE;\n"
      "} else {\n"
      "if (other_value.value_elements[elem_count].is_bound()) "
        "return FALSE;\n"
      "}\n"
      "}\n"
      "return TRUE;\n");
  }
  src = mputstr(src, "}\n\n");

  def = mputstr(def, "inline boolean operator!=(null_type other_value) const "
                "{ return !(*this == other_value); }\n");
  def = mputprintf(def, "inline boolean operator!=(const %s& other_value) "
                   "const { return !(*this == other_value); }\n\n", name);

  /* indexing operators */
  /* Non-const operator[] is allowed to extend the record-of */
  def = mputprintf(def, "%s& operator[](int index_value);\n", type);
  src = mputprintf(src,
    "%s& %s::operator[](int index_value)\n"
    "{\n"
    "if (index_value < 0) TTCN_error(\"Accessing an element of type %s "
      "using a negative index: %%d.\", index_value);\n"
    "if (index_value >= n_elements) set_size(index_value + 1);\n"
    "return value_elements[index_value];\n"
    "}\n\n", type, name, dispname);

  def = mputprintf(def, "%s& operator[](const INTEGER& index_value);\n",
                   type);
  src = mputprintf(src,
    "%s& %s::operator[](const INTEGER& index_value)\n"
    "{\n"
    "index_value.must_bound(\"Using an unbound integer value for indexing "
    "a value of type %s.\");\n"
    "return (*this)[(int)index_value];\n"
    "}\n\n", type, name, dispname);

  /* Const operator[] throws an error if over-indexing */
  def = mputprintf(def, "const %s& operator[](int index_value) const;\n",
                   type);
  src = mputprintf(src,
    "const %s& %s::operator[](int index_value) const\n"
    "{\n"
    "if (n_elements==-1) TTCN_error(\"Accessing an element in an unbound "
      "value of type %s.\");\n"
    "if (index_value<0) TTCN_error(\"Accessing an element of type %s "
      "using a negative index: %%d.\", index_value);\n"
    "if (index_value>=n_elements) TTCN_error(\"Index overflow in a value "
      "of type %s: The index is %%d, but the value has only %%d elements.\""
      ", index_value, n_elements);\n"
    "return value_elements[index_value];\n"
    "}\n\n", type, name, dispname, dispname, dispname);

  def = mputprintf(def, "const %s& operator[](const INTEGER& index_value) "
                   "const;\n\n", type);
  src = mputprintf(src,
    "const %s& %s::operator[](const INTEGER& index_value) const\n"
    "{\n"
    "index_value.must_bound(\"Using an unbound integer value for indexing "
    "a value of type %s.\");\n"
    "return (*this)[(int)index_value];\n"
    "}\n\n", type, name, dispname);

  /* rotation operators */
  def = mputprintf(def,
    "%s operator<<=(int rotate_count) const;\n"
    "%s operator<<=(const INTEGER& rotate_count) const;\n"
    "%s operator>>=(int rotate_count) const;\n"
    "%s operator>>=(const INTEGER& rotate_count) const;\n\n",
    name, name, name, name);
  src = mputprintf(src,
    "%s %s::operator<<=(int rotate_count) const\n"
    "{\n"
    "return *this >>= (-rotate_count);\n"
    "}\n\n"
    "%s %s::operator<<=(const INTEGER& rotate_count) const\n"
    "{\n"
    "rotate_count.must_bound(\""
    "Unbound integer operand of rotate left operator.\");\n"
    "return *this >>= (int)(-rotate_count);\n"
    "}\n\n"
    "%s %s::operator>>=(const INTEGER& rotate_count) const\n"
    "{\n"
    "rotate_count.must_bound(\""
    "Unbound integer operand of rotate right operator.\");\n"
    "return *this >>= (int)rotate_count;\n"
    "}\n\n"
    "%s %s::operator>>=(int rotate_count) const\n"
    "{\n"
    "if (n_elements==-1) TTCN_error(\"Performing rotation operation on an "
      "unbound value of type %s.\");\n"
    "if (n_elements==0) return *this;\n"
    "int rc;\n"
    "if (rotate_count>=0) rc = rotate_count %% n_elements;\n"
    "else rc = n_elements - ((-rotate_count) %% n_elements);\n"
    "if (rc == 0) return *this;\n"
    "%s ret_val;\n"
    "ret_val.set_size(n_elements);\n"
    "for (int i=0; i<n_elements; i++) {\n"
    "if (value_elements[i].is_bound()) "
      "ret_val.value_elements[(i+rc)%%n_elements] = value_elements[i];\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n",
    name, name, name, name, name, name, name, name, dispname, name);

  /* concatenation */
  def = mputprintf(def,
    "%s operator+(const %s& other_value) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s::operator+(const %s& other_value) const\n"
    "{\n"
    "if (n_elements==-1 || other_value.n_elements==-1) "
      "TTCN_error(\"Unbound operand of %s concatenation.\");\n"
    "if (n_elements==0) return other_value;\n"
    "if (other_value.n_elements==0) return *this;\n"
    "%s ret_val;\n"
    "ret_val.set_size(n_elements+other_value.n_elements);\n"
    "for (int i=0; i<n_elements; i++) {\n"
    "if (value_elements[i].is_bound()) "
      "ret_val.value_elements[i] = value_elements[i];\n"
    "}\n"
    "for (int i=0; i<other_value.n_elements; i++) {\n"
    "if (other_value.value_elements[i].is_bound()) "
      "ret_val.value_elements[i+n_elements] = other_value.value_elements[i];\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n", name, name, name, dispname, name);

  /* substr() */
  def = mputprintf(def,
    "%s substr(int index, int returncount) const;\n\n", name);
  src = mputprintf(src,
    "%s %s::substr(int index, int returncount) const\n"
    "{\n"
    "if (n_elements==-1) TTCN_error(\"The first argument of substr() is an "
      "unbound value of type %s.\");\n"
    "check_substr_arguments(n_elements, index, returncount, \"%s\",\"element\");\n"
    "%s ret_val;\n"
    "ret_val.set_size(returncount);\n"
    "for (int i=0; i<returncount; i++) {\n"
    "if (value_elements[i+index].is_bound()) "
      "ret_val.value_elements[i] = value_elements[i+index];\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n", name, name, dispname, dispname, name);

  /* replace() */
  def = mputprintf(def,
    "%s replace(int index, int len, const %s& repl) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s::replace(int index, int len, const %s& repl) const\n"
    "{\n"
    "if (n_elements==-1) TTCN_error(\"The first argument of replace() is an "
      "unbound value of type %s.\");\n"
    "if (repl.n_elements==-1) TTCN_error(\"The fourth argument of replace() "
      "is an unbound value of type %s.\");\n"
    "check_replace_arguments(n_elements, index, len, \"%s\",\"element\");\n"
    "%s ret_val;\n"
    "ret_val.set_size(n_elements + repl.n_elements - len);\n"
    "for (int i = 0; i < index; i++) {\n"
    "if (value_elements[i].is_bound()) "
      "ret_val.value_elements[i] = value_elements[i];\n"
    "}\n"
    "for (int i = 0; i < repl.n_elements; i++) {\n"
    "if (repl.value_elements[i].is_bound()) "
      "ret_val.value_elements[i+index] = repl.value_elements[i];\n"
    "}\n"
    "for (int i = 0; i < n_elements - index - len; i++) {\n"
    "if (value_elements[index+i+len].is_bound()) "
      "ret_val.value_elements[index+i+repl.n_elements] = value_elements[index+i+len];\n"
    "}\n"
    "return ret_val;\n"
  "}\n\n", name, name, name, dispname, dispname, dispname, name);
  def = mputprintf(def,
    "%s replace(int index, int len, const %s_template& repl) const;\n\n",
    name, name);
  src = mputprintf(src,
    "%s %s::replace(int index, int len, const %s_template& repl) const\n"
    "{\n"
    "if (!repl.is_value()) TTCN_error(\"The fourth argument of function "
      "replace() is a template with non-specific value.\");\n"
    "return replace(index, len, repl.valueof());\n"
    "}\n\n", name, name, name);

  /* set_size function */
  def = mputstr(def, "void set_size(int new_size);\n");
  src = mputprintf(src, "void %s::set_size(int new_size)\n"
    "{\n"
    "if (new_size<0) TTCN_error(\"Internal error: Setting a negative size for "
      "a value of type %s.\");\n"
    "if (new_size==n_elements) return;\n"
    "if (new_size==0) {\n"
    "  clean_up();\n"
    "  n_elements = 0;\n"
    "  value_elements = NULL;\n"
    "  return;\n"
    "}\n"
    "%s* new_elem_v = new %s[new_size];\n"
    "for (int act_elem = 0; act_elem<n_elements; act_elem++) {\n"
    "  if (act_elem>=new_size) break;\n"
    "  if (value_elements[act_elem].is_bound()) new_elem_v[act_elem] = value_elements[act_elem];\n"
    "}\n"
    "clean_up();\n"
    "#ifdef TITAN_MEMORY_DEBUG_SET_RECORD_OF\n"
    "if((n_elements/1000)!=(new_size/1000)) "
    "TTCN_warning(\"New size of type %s: %%d\",new_size);\n"
    "#endif\n"
    "n_elements = new_size;\n"
    "value_elements = new_elem_v;\n"
    "}\n\n", name, dispname, type, type, dispname);

  /* is_bound function */
  def = mputstr(def,
    "inline boolean is_bound() const {return n_elements!=-1; }\n");

  /* is_present function */
  def = mputstr(def,
    "inline boolean is_present() const { return is_bound(); }\n");

  /* is_value function */
  def = mputstr(def,
    "boolean is_value() const;\n");
  src = mputprintf(src,
    "boolean %s::is_value() const\n"
    "{\n"
    "if (n_elements==-1) return FALSE;\n"
    "for (int i = 0; i < n_elements; ++i) {\n"
    "  if (!value_elements[i].is_value()) return FALSE;\n"
    "}\n"
    "return TRUE;\n"
    "}\n\n", name);

  /* sizeof operation */
  def = mputstr(def,
    "int size_of() const;\n"
    "int n_elem() const { return size_of(); }\n");
  src = mputprintf(src,
    "int %s::size_of() const\n"
    "{\n"
    "if (n_elements==-1) TTCN_error(\"Performing sizeof operation on an "
      "unbound value of type %s.\");\n"
    "return n_elements;\n"
    "}\n\n", name, dispname);

  /* lengthof operation */
  def = mputstr(def, "int lengthof() const;\n");
  src = mputprintf(src,
    "int %s::lengthof() const\n"
    "{\n"
    "if (n_elements==-1) TTCN_error(\"Performing lengthof operation on an "
      "unbound value of type %s.\");\n"
    "for (int my_length=n_elements; my_length>0; my_length--) "
      "if (value_elements[my_length-1].is_bound()) return my_length;\n"
    "return 0;\n"
    "}\n\n", name, dispname);

  /* log function */
  def = mputstr(def, "void log() const;\n");
  src = mputprintf(src,
    "void %s::log() const\n"
    "{\n"
    "if (n_elements==-1) {;\n"
    "TTCN_Logger::log_event_unbound();\n"
    "return;\n"
    "}\n"
    "switch (n_elements) {\n"
    "case 0:\n"
    "TTCN_Logger::log_event_str(\"{ }\");\n"
    "break;\n"
    "default:\n"
    "TTCN_Logger::log_event_str(\"{ \");\n"
    "for (int elem_count = 0; elem_count < n_elements; elem_count++) {\n"
    "if (elem_count > 0) TTCN_Logger::log_event_str(\", \");\n"
    "value_elements[elem_count].log();\n"
    "}\n"
    "TTCN_Logger::log_event_str(\" }\");\n"
    "}\n"
    "}\n\n", name);

  /* set_param function */ /* this is an exact copy of the previous one in this source file, if we didn't forget... */
  def = mputstr(def, "void set_param(Module_Param& param);\n");
  src = mputprintf(src,
    "void %s::set_param(Module_Param& param)\n"
    "{\n"
    "  if (dynamic_cast<Module_Param_Name*>(param.get_id()) != NULL &&\n"
    "      param.get_id()->next_name()) {\n"
    // Haven't reached the end of the module parameter name
    // => the name refers to one of the elements, not to the whole record of
    "    char* param_field = param.get_id()->get_current_name();\n"
    "    if (param_field[0] < '0' || param_field[0] > '9') {\n"
    "      param.error(\"Unexpected record field name in module parameter, expected a valid\"\n"
    "        \" index for %s type `%s'\");\n"
    "    }\n"
    "    int param_index = -1;\n"
    "    sscanf(param_field, \"%%d\", &param_index);\n"
    "    (*this)[param_index].set_param(param);\n"
    "    return;\n"
    "  }\n"
    "  param.basic_check(Module_Param::BC_VALUE|Module_Param::BC_LIST, \"%s value\");\n"
    "  switch (param.get_operation_type()) {\n"
    "  case Module_Param::OT_ASSIGN:\n"
    "    if (param.get_type()==Module_Param::MP_Value_List && param.get_size()==0) {\n"
    "      *this = NULL_VALUE;\n"
    "      return;\n"
    "    }\n"
    "    switch (param.get_type()) {\n"
    "    case Module_Param::MP_Value_List:\n"
    "      set_size(param.get_size());\n"
    "      for (size_t i=0; i<param.get_size(); ++i) {\n"
    "        Module_Param* const curr = param.get_elem(i);\n"
    "        if (curr->get_type()!=Module_Param::MP_NotUsed) {\n"
    "          (*this)[i].set_param(*curr);\n"
    "        }\n"
    "      }\n"
    "      break;\n"
    "    case Module_Param::MP_Indexed_List:\n"
    "      for (size_t i=0; i<param.get_size(); ++i) {\n"
    "        Module_Param* const curr = param.get_elem(i);\n"
    "        (*this)[curr->get_id()->get_index()].set_param(*curr);\n"
    "      }\n"
    "      break;\n"
    "    default:\n"
    "      param.type_error(\"%s value\", \"%s\");\n"
    "    }\n"
    "    break;\n"
    "  case Module_Param::OT_CONCAT:\n"
    "    switch (param.get_type()) {\n"
    "    case Module_Param::MP_Value_List: {\n"
    "      if (!is_bound()) *this = NULL_VALUE;\n"
    "      int start_idx = lengthof();\n"
    "      for (size_t i=0; i<param.get_size(); ++i) {\n"
    "        Module_Param* const curr = param.get_elem(i);\n"
    "        if ((curr->get_type()!=Module_Param::MP_NotUsed)) {\n"
    "          (*this)[start_idx+(int)i].set_param(*curr);\n"
    "        }\n"
    "      }\n"
    "    } break;\n"
    "    case Module_Param::MP_Indexed_List:\n"
    "      param.error(\"Cannot concatenate an indexed value list\");\n"
    "      break;\n"
    "    default:\n"
    "      param.type_error(\"%s value\", \"%s\");\n"
    "    }\n"
    "    break;\n"
    "  default:\n"
    "    TTCN_error(\"Internal error: Unknown operation type.\");\n"
    "  }\n"
    "}\n", name, sdef->kind == RECORD_OF ? "record of" : "set of", dispname,
    sdef->kind == RECORD_OF ? "record of" : "set of",
    sdef->kind == RECORD_OF ? "record of" : "set of", dispname,
    sdef->kind == RECORD_OF ? "record of" : "set of", dispname);

  /* encoding / decoding functions */
  def = mputstr(def, "void encode_text(Text_Buf& text_buf) const;\n");
  src = mputprintf(src,
    "void %s::encode_text(Text_Buf& text_buf) const\n"
    "{\n"
    "if (n_elements==-1) "
      "TTCN_error(\"Text encoder: Encoding an unbound value of type %s.\");\n"
    "text_buf.push_int(n_elements);\n"
    "for (int elem_count = 0; elem_count < n_elements; elem_count++)\n"
      "value_elements[elem_count].encode_text(text_buf);\n"
    "}\n\n", name, dispname);

  def = mputstr(def, "void decode_text(Text_Buf& text_buf);\n");
  src = mputprintf(src,
    "void %s::decode_text(Text_Buf& text_buf)\n"
    "{\n"
    "clean_up();\n"
    "n_elements = text_buf.pull_int().get_val();\n"
    "if (n_elements < 0) TTCN_error(\"Text decoder: Negative size "
      "was received for a value of type %s.\");\n"
    "if (n_elements==0) {\n"
    "  value_elements = NULL;\n"
    "  return;\n"
    "}\n"
    "value_elements = new %s[n_elements];\n"
    "for (int elem_count = 0; elem_count < n_elements; elem_count++) {\n"
    "  value_elements[elem_count].decode_text(text_buf);\n"
    "}\n"
    "}\n\n", name, dispname, type);

  if(ber_needed || raw_needed || text_needed || xer_needed || json_needed)
    def_encdec(name, &def, &src, ber_needed, raw_needed, text_needed,
               xer_needed, json_needed, FALSE);

  if (text_needed) {
    src=mputprintf(src,
      "int %s::TEXT_encode(const TTCN_Typedescriptor_t& p_td,"
      " TTCN_Buffer& p_buf) const{\n"
      "  int encoded_length=0;\n"
      "  if(p_td.text->begin_encode){\n"
      "    p_buf.put_cs(*p_td.text->begin_encode);\n"
      "    encoded_length+=p_td.text->begin_encode->lengthof();\n"
      "  }\n"
      "  if(n_elements==-1) {\n"
      "    TTCN_EncDec_ErrorContext::error\n"
      "      (TTCN_EncDec::ET_UNBOUND, \"Encoding an unbound value.\");\n"
      "    if(p_td.text->end_encode){\n"
      "      p_buf.put_cs(*p_td.text->end_encode);\n"
      "      encoded_length+=p_td.text->end_encode->lengthof();\n"
      "    }\n"
      "    return encoded_length;\n"
      "  }\n"
      "  for(int a=0;a<n_elements;a++){\n"
      "   if(a!=0 && p_td.text->separator_encode){\n"
      "    p_buf.put_cs(*p_td.text->separator_encode);\n"
      "    encoded_length+=p_td.text->separator_encode->lengthof();\n"
      "   }\n"
      "   encoded_length+=value_elements[a].TEXT_encode(%s_descr_,p_buf);\n"
      "  }\n"
      "  if(p_td.text->end_encode){\n"
      "    p_buf.put_cs(*p_td.text->end_encode);\n"
      "    encoded_length+=p_td.text->end_encode->lengthof();\n"
      "  }\n"
      "  return encoded_length;\n"
      "}\n"
      ,name,sdef->oftypedescrname
      );
    src = mputprintf(src,
      "int %s::TEXT_decode(const TTCN_Typedescriptor_t& p_td,"
      " TTCN_Buffer& p_buf, Limit_Token_List& limit, boolean no_err"
      ", boolean first_call){\n"
      "  int decoded_length=0;\n"
      "  size_t pos=p_buf.get_pos();\n"
      "  boolean sep_found=FALSE;\n"
      "  int sep_length=0;\n"
      "  int ml=0;\n"
      "  if(p_td.text->begin_decode){\n"
      "    int tl;\n"
      "    if((tl=p_td.text->begin_decode->match_begin(p_buf))<0){\n"
      "          if(no_err)return -1;\n"
      "          TTCN_EncDec_ErrorContext::error\n"
      "              (TTCN_EncDec::ET_TOKEN_ERR, \"The specified token '%%s'"
      " not found for '%%s': \",(const char*)*(p_td.text->begin_decode)"
      ", p_td.name);\n"
      "          return 0;\n"
      "        }\n"
      "    decoded_length+=tl;\n"
      "    p_buf.increase_pos(tl);\n"
      "  }\n"
      "  if(p_td.text->end_decode){\n"
      "    limit.add_token(p_td.text->end_decode);\n"
      "    ml++;\n"
      "  }\n"
      "  if(p_td.text->separator_decode){\n"
      "    limit.add_token(p_td.text->separator_decode);\n"
      "    ml++;\n"
      "  }\n"
      "  if(first_call) {\n"
      "    set_size(0);\n"
      "  }\n"
      "  int more=n_elements;\n"
      "  while(TRUE){\n"
      "    %s val;\n"
      "    pos=p_buf.get_pos();\n"
      "    int len=val.TEXT_decode(%s_descr_,p_buf,limit,TRUE);\n"
      "    if(len==-1 || (len==0 && !limit.has_token())){\n"
      "      p_buf.set_pos(pos);\n"
      "      if(sep_found){\n"
      "        p_buf.set_pos(p_buf.get_pos()-sep_length);\n"
      "        decoded_length-=sep_length;\n"
      "      }\n"
      "      break;\n"
      "    }\n"
      "    sep_found=FALSE;\n"
      "    set_size(n_elements+1);\n"
      "    value_elements[n_elements-1]=val;\n"
      "    decoded_length+=len;\n"
      "    if(p_td.text->separator_decode){\n"
      "      int tl;\n"
      "      if((tl=p_td.text->separator_decode->match_begin(p_buf))<0){\n"
      "        break;\n"
      "      }\n"
      "      decoded_length+=tl;\n"
      "      p_buf.increase_pos(tl);\n"
      "      sep_length=tl;\n"
      "      sep_found=TRUE;\n"
      "    } else if(p_td.text->end_decode){\n"
      "      int tl;\n"
      "      if((tl=p_td.text->end_decode->match_begin(p_buf))!=-1){\n"
      "        decoded_length+=tl;\n"
      "        p_buf.increase_pos(tl);\n"
      "        limit.remove_tokens(ml);\n"
      "        return decoded_length;\n"
      "      }\n"
      "    } else if(limit.has_token(ml)){\n"
      "      int tl;\n"
      "      if((tl=limit.match(p_buf,ml))==0){\n"
      "        sep_found=FALSE;\n"
      "        break;\n"
      "      }\n"
      "    }\n"
      "  }\n"
      ,name,type,sdef->oftypedescrname
     );
    src = mputstr(src,
      "   limit.remove_tokens(ml);\n"
      "  if(p_td.text->end_decode){\n"
      "    int tl;\n"
      "    if((tl=p_td.text->end_decode->match_begin(p_buf))<0){\n"
      "          if(no_err){"
      "            if(!first_call){\n"
      "              set_size(more);\n"
      "            }\n"
      "            return -1;\n"
      "          }\n"
      "          TTCN_EncDec_ErrorContext::error"
      "(TTCN_EncDec::ET_TOKEN_ERR, \"The specified token '%s'"
      " not found for '%s': \",(const char*)*(p_td.text->end_decode)"
      ",p_td.name);\n"
      "          return decoded_length;\n"
      "        }\n"
      "    decoded_length+=tl;\n"
      "    p_buf.increase_pos(tl);\n"
      "  }\n"
      "  if(n_elements==0){\n"
      "    if(p_td.text->end_decode || p_td.text->begin_decode) n_elements=0;\n"
      "    else {\n"
      "      if(no_err)return -1;\n"
      "      TTCN_EncDec_ErrorContext::error"
      "(TTCN_EncDec::ET_TOKEN_ERR, \"No record/set of member found.\");\n"
      "      return decoded_length;\n"
      "    }\n"
      "  }\n"
      "  if(!first_call && more==n_elements && "
      "!(p_td.text->end_decode || p_td.text->begin_decode)) return -1;\n"
      "  return decoded_length;\n"
      "}\n"
     );
  }

  /* BER functions */
  if(ber_needed) {
    /* BER_encode_TLV() */
    src=mputprintf
      (src,
       "ASN_BER_TLV_t* %s::BER_encode_TLV(const TTCN_Typedescriptor_t&"
       " p_td, unsigned p_coding) const\n"
       "{\n"
       "  BER_chk_descr(p_td);\n"
       "  ASN_BER_TLV_t *new_tlv=BER_encode_chk_bound(is_bound());\n"
       "  if(!new_tlv) {\n"
       "    new_tlv=ASN_BER_TLV_t::construct(NULL);\n"
       "    TTCN_EncDec_ErrorContext ec;\n"
       "    for(int elem_i=0; elem_i<n_elements; elem_i++) {\n"
       "      ec.set_msg(\"Component #%%d: \", elem_i);\n"
       "      new_tlv->add_TLV(value_elements[elem_i].BER_encode_TLV"
       "(%s_descr_, p_coding));\n"
       "    }\n"
       "%s"
       "  }\n"
       "  new_tlv=ASN_BER_V2TLV(new_tlv, p_td, p_coding);\n"
       "  return new_tlv;\n"
       "}\n"
       "\n"
       /* BER_decode_TLV() */
       "boolean %s::BER_decode_TLV(const TTCN_Typedescriptor_t& p_td,"
       " const ASN_BER_TLV_t& p_tlv, unsigned L_form)\n"
       "{\n"
       "  BER_chk_descr(p_td);\n"
       "  ASN_BER_TLV_t stripped_tlv;\n"
       "  BER_decode_strip_tags(*p_td.ber, p_tlv, L_form, stripped_tlv);\n"
       "  TTCN_EncDec_ErrorContext ec_0(\"While decoding '%%s' type: \","
       " p_td.name);\n"
       "  stripped_tlv.chk_constructed_flag(TRUE);\n"
       "  set_size(0);\n"
       "  size_t V_pos=0;\n"
       "  ASN_BER_TLV_t tmp_tlv;\n"
       "  TTCN_EncDec_ErrorContext ec_1(\"Component #\");\n"
       "  TTCN_EncDec_ErrorContext ec_2(\"0: \");\n"
       "  while(BER_decode_constdTLV_next(stripped_tlv, V_pos, L_form, "
       "tmp_tlv)) {\n"
       "  set_size(n_elements+1);\n"
       "  value_elements[n_elements-1].BER_decode_TLV(%s_descr_, tmp_tlv, "
         "L_form);\n"
       "  ec_2.set_msg(\"%%d: \", n_elements);\n"
       "  }\n"
       "  return TRUE;\n"
       "}\n"
       "\n"
       , name, sdef->oftypedescrname
       , sdef->kind==SET_OF?"    new_tlv->sort_tlvs();\n":""
       , name, sdef->oftypedescrname
       );

    if(sdef->has_opentypes) {
      /* BER_decode_opentypes() */
      def=mputstr
        (def,
         "void BER_decode_opentypes(TTCN_Type_list& p_typelist,"
         " unsigned L_form);\n");
      src=mputprintf
        (src,
         "void %s::BER_decode_opentypes(TTCN_Type_list& p_typelist,"
         " unsigned L_form)\n"
         "{\n"
         "  p_typelist.push(this);\n"
         "  TTCN_EncDec_ErrorContext ec_0(\"Component #\");\n"
         "  TTCN_EncDec_ErrorContext ec_1;\n"
         "  for(int elem_i=0; elem_i<n_elements; elem_i++) {\n"
         "    ec_1.set_msg(\"%%d: \", elem_i);\n"
         "    value_elements[elem_i].BER_decode_opentypes(p_typelist,"
         " L_form);\n"
         "  }\n"
         "  p_typelist.pop();\n"
         "}\n"
         "\n"
         , name
         );
    }
  }

  if(raw_needed){
      src=mputprintf(src,
    "int %s::RAW_decode(const TTCN_Typedescriptor_t& p_td, TTCN_Buffer& p_buf, "
    "int limit, raw_order_t top_bit_ord, boolean /*no_err*/, int sel_field"
    ", boolean first_call){\n"
    "  int prepaddlength=p_buf.increase_pos_padd(p_td.raw->prepadding);\n"
    "  limit-=prepaddlength;\n"
    "  int decoded_length=0;\n"
    "  int decoded_field_length=0;\n"
    "  size_t start_of_field=0;\n"
    "  if (first_call) set_size(0);\n"
    "  int start_field=n_elements;\n"
    "  if(p_td.raw->fieldlength || sel_field!=-1){\n"
    "    int a=0;\n"
    "    if(sel_field==-1) sel_field=p_td.raw->fieldlength;\n"
    "    for(a=0;a<sel_field;a++){\n"
    "      decoded_field_length=(*this)[a+start_field].RAW_decode(%s_descr_,"
    "p_buf,limit,top_bit_ord,TRUE);\n"
    "      if(decoded_field_length < 0) return decoded_field_length;\n"
    "      decoded_length+=decoded_field_length;\n"
    "      limit-=decoded_field_length;\n"
    "    }\n"
    "    if(a==0) n_elements=0;\n"
    "  } else {\n"
    "    int a=start_field;\n"
    "    if(limit==0){\n"
    "      if(!first_call) return -1;\n"
    "      n_elements=0;\n"
    "      return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n"
    "    }\n"
    "    while(limit>0){\n"
    "      start_of_field=p_buf.get_pos_bit();\n"
    "      decoded_field_length=(*this)[a].RAW_decode(%s_descr_,p_buf,limit,"
    "top_bit_ord,TRUE);\n"
    "      if(decoded_field_length < 0){\n"
    /*"        delete &(*this)[a];\n"*/
    "        n_elements--;\n"
    "        p_buf.set_pos_bit(start_of_field);\n"
    "        if(a>start_field){\n"
    "        return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n"
    "        } else return -1;\n"
    "      }\n"
    "      decoded_length+=decoded_field_length;\n"
    "      limit-=decoded_field_length;\n"
    "      a++;\n"
    ,name,sdef->oftypedescrname,sdef->oftypedescrname
    );
    if(sdef->raw.extension_bit!=XDEFNO && sdef->raw.extension_bit!=XDEFDEFAULT){
      src=mputprintf(src,
    "      if (%sp_buf.get_last_bit())\n"
    "        return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n", sdef->raw.extension_bit == XDEFYES ? "" : "!");
    }
      src=mputprintf(src,
    "    }\n"
    "  }\n"
    " return decoded_length+p_buf.increase_pos_padd(p_td.raw->padding)"
    "+prepaddlength;\n"
    "}\n\n"
    "int %s::RAW_encode(const TTCN_Typedescriptor_t& p_td,"
    "RAW_enc_tree& myleaf) const{\n"
    "  int encoded_length=0;\n"
    "  int encoded_num_of_records=p_td.raw->fieldlength?"
    "smaller(n_elements, p_td.raw->fieldlength):n_elements;\n"
    "  myleaf.isleaf=FALSE;\n"
    "  myleaf.rec_of=TRUE;\n"
    "  myleaf.body.node.num_of_nodes=encoded_num_of_records;\n"
    "  myleaf.body.node.nodes=init_nodes_of_enc_tree(encoded_num_of_records);\n"
    "  for(int a=0;a<encoded_num_of_records;a++){\n"
    "    myleaf.body.node.nodes[a]=new RAW_enc_tree(TRUE,&myleaf,"
    "&(myleaf.curr_pos),a,%s_descr_.raw);\n"
    "    encoded_length+=(*this)[a].RAW_encode(%s_descr_,"
    "*myleaf.body.node.nodes[a]);\n"
    "  }\n"
    " return myleaf.length=encoded_length;\n}\n\n"
    , name, sdef->oftypedescrname, sdef->oftypedescrname
    );
  }

  if (xer_needed) { /* XERSTUFF encoder codegen for record-of, RT1 */
    def = mputstr(def,
      "char **collect_ns(const XERdescriptor_t& p_td, size_t& num, bool& def_ns) const;\n");

    /* Write the body of the XER encoder/decoder functions. The declaration
     * is written by def_encdec() in encdec.c */
    src = mputprintf(src,
      "boolean %s::can_start(const char *name, const char *uri, "
      "XERdescriptor_t const& xd, unsigned int flavor) {\n"
      "  boolean e_xer = is_exer(flavor);\n"
      "  if (e_xer && (xd.xer_bits & ANY_ELEMENT)) "
      , name
      );
    if (sdef->nFollowers) {
      /* If there are optional fields following the record-of, then seeing
       * {any XML tag that belongs to those fields} where the record-of may be
       * means that the record-of is empty. */
      size_t f;
      src = mputstr(src, "{\n");
      for (f = 0; f < sdef->nFollowers; ++f) {
        src = mputprintf(src,
          "    if (%s::can_start(name, uri, %s_xer_, flavor)) return FALSE;\n"
          , sdef->followers[f].type
          , sdef->followers[f].typegen
          );
      }
      src = mputstr(src,
        "    return TRUE;\n"
        "  }\n");
    }
    else src = mputstr(src, "return TRUE;\n");

    src = mputprintf(src,
      "  if ((!e_xer || !(xd.xer_bits & UNTAGGED)) && !(flavor & XER_RECOF)) return "
      "check_name(name, xd, e_xer) && (!e_xer || check_namespace(uri, xd));\n"
      "  else return %s::can_start(name, uri, %s_xer_, flavor | XER_RECOF);\n"
      "}\n\n"
      , sdef->type
      , sdef->oftypedescrname
      );

    src = mputprintf(src,
      "char ** %s::collect_ns(const XERdescriptor_t& p_td, size_t& num, bool& def_ns) const {\n"
      "  size_t num_collected;\n"
      "  char **collected_ns = Base_Type::collect_ns(p_td, num_collected, def_ns);\n"
      /* The above may throw but then nothing was allocated. */
      "  if (n_elements!=-1) try {\n"
      "    char **new_ns;\n"
      "    size_t num_new;\n"
      "    for (int i = 0; i < n_elements; ++i) {\n"
      "      bool def_ns_1 = false;"
      "      new_ns = value_elements[i].collect_ns(%s_xer_, num_new, def_ns_1);\n"
      "      merge_ns(collected_ns, num_collected, new_ns, num_new);\n"
      "      def_ns = def_ns || def_ns_1;\n" /* alas, no ||= */
      "    }\n"
      "  }\n"
      "  catch (...) {\n"
      /* Probably a TC_Error thrown from elements[i]->collect_ns() if e.g.
       * encoding an unbound value. */
      "    while (num_collected > 0) Free(collected_ns[--num_collected]);\n"
      "    Free(collected_ns);\n"
      "    throw;\n"
      "  }\n"
      "  num = num_collected;\n"
      "  return collected_ns;\n"
      "}\n\n"
      , name, sdef->oftypedescrname);

    src=mputprintf(src,
      "int %s::XER_encode(const XERdescriptor_t& p_td,"
      " TTCN_Buffer& p_buf, unsigned int p_flavor, int p_indent) const\n{\n"
      "  if (n_elements==-1) TTCN_error(\"Attempt to XER-encode an unbound record of\");\n" /* TODO type name */
      "  int encoded_length=(int)p_buf.get_len();\n"
      "  boolean e_xer = is_exer(p_flavor);\n"
      "  boolean own_tag = !(e_xer && p_indent && ((p_td.xer_bits & (ANY_ELEMENT|ANY_ATTRIBUTES|UNTAGGED))\n"
      "    || (p_flavor & USE_TYPE_ATTR)));\n"
      "  boolean indenting = !is_canonical(p_flavor) && own_tag;\n"
      "%s" /* Factor out p_indent if not attribute */
      "  if (n_elements==0) {\n" /* Empty record of */
      , name
      , sdef->xerAttribute ? "" : "  if (indenting) do_indent(p_buf, p_indent);\n"
      );
    if (sdef->xerAttribute) {
      src=mputstr(src,
        "    if (e_xer) {\n" /* Empty attribute. */
        "      begin_attribute(p_td, p_buf);\n"
        "      p_buf.put_c('\\'');\n"
        "    } else\n");
    }
    else {
      src = mputstr(src,
        "    if (own_tag)");
    }
    src=mputprintf(src,
      "    {\n" /* Do empty element tag */
      "%s"
      "      p_buf.put_c('<');\n"
      "      if (e_xer) write_ns_prefix(p_td, p_buf);\n"
      "      p_buf.put_s((size_t)p_td.namelens[e_xer]-2, (const unsigned char*)p_td.names[e_xer]);\n"
      /* namespace declarations for the toplevel element */
      "      if (e_xer && p_indent==0)\n"
      "      {\n"
      "        size_t num_collected = 0;\n"
      "        char **collected_ns = NULL;\n"
      "        bool def_ns = false;\n"
      "        collected_ns = collect_ns(p_td, num_collected, def_ns);\n"
      "        for (size_t cur_coll = 0; cur_coll < num_collected; ++cur_coll) {\n"
      "          p_buf.put_s(strlen(collected_ns[cur_coll]), (cbyte*)collected_ns[cur_coll]);\n"
      "          Free(collected_ns[cur_coll]);\n"
      "        }\n"
      "        Free(collected_ns);\n"
      "      }\n"

      "      p_buf.put_s(2 + indenting, (const unsigned char*)\"/>\\n\");\n"
      "    }\n"
      "  }\n"
      "  else {\n" /* Not empty record of. Start tag or attribute */
      , sdef->xerAttribute ? "      if (indenting) do_indent(p_buf, p_indent);\n" : ""
      );
    if (sdef->xerAnyAttrElem) {
      src = mputstr(src,
        "    if (e_xer && (p_td.xer_bits & ANY_ATTRIBUTES)) {\n"
        "      static const universal_char sp = { 0,0,0,' ' };\n"
        "      static const universal_char tb = { 0,0,0,9 };\n"
        "      size_t buf_len = p_buf.get_len(), shorter = 0;\n"
        "      const unsigned char * const buf_data = p_buf.get_data();\n"
        "      if (buf_data[buf_len - 1 - shorter] == '\\n') ++shorter;\n"
        "      if (buf_data[buf_len - 1 - shorter] == '>' ) ++shorter;\n"
        "      unsigned char saved[4];\n"
        "      memcpy(saved, buf_data + (buf_len - shorter), shorter);\n"
        "      p_buf.increase_length(-shorter);\n"
        "      for (int i = 0; i < n_elements; ++i) {\n"
        "        TTCN_EncDec_ErrorContext ec_0(\"Attribute %d: \", i);\n"
        "        size_t len = value_elements[i].lengthof();\n"
        "        for (;;) {\n"
        "          const UNIVERSAL_CHARSTRING_ELEMENT& ue = value_elements[i][len - 1];\n"
        "          if (sp == ue || tb == ue) --len;\n"
        "          else break;\n"
        "        }\n"
        "        size_t j, sp_at = 0;\n"
        /* Find the "separators" in each string */
        "        for (j = 0; j < len; j++) {\n"
        "          UNIVERSAL_CHARSTRING_ELEMENT ue = value_elements[i][j];\n"
        "          if (sp_at) {\n"
        "            if (sp == ue || tb == ue) {}\n"
        "            else break;\n"
        "          } else {\n"
        "            if (sp == ue || tb == ue) sp_at = j;\n"
        "          }\n"
        "        } // next j\n"
        "        size_t buf_start = p_buf.get_len();\n"
        /* Now write them */
        "        if (sp_at > 0) {\n"
        "          char * ns = mprintf(\" xmlns:b%d='\", i);\n"
        "          size_t ns_len = mstrlen(ns);\n"
        "          p_buf.put_s(ns_len, (const unsigned char*)ns);\n"

        "          UNIVERSAL_CHARSTRING before(sp_at, (const universal_char*)(value_elements[i]));\n"
        "          before.XER_encode(UNIVERSAL_CHARSTRING_xer_, p_buf, p_flavor | ANY_ATTRIBUTES, p_indent);\n"
        // Ensure the namespace abides to its restrictions
        "          if (p_td.xer_bits & (ANY_FROM | ANY_EXCEPT)) {\n"
        "            TTCN_Buffer ns_buf;\n"
        "            before.encode_utf8(ns_buf);\n"
        "            CHARSTRING cs;\n"
        "            ns_buf.get_string(cs);\n"
        "            check_namespace_restrictions(p_td, (const char*)cs);\n"
        "          }\n"

        "          p_buf.put_c('\\'');\n"
        "          p_buf.put_c(' ');\n"

        /* Keep just the "b%d" part from ns */
        "          p_buf.put_s(ns_len - 9, (const unsigned char*)ns + 7);\n"
        "          p_buf.put_c(':');\n"
        "          Free(ns);\n"
        "        }\n"
        "        else {\n"
        "          p_buf.put_c(' ');\n"
        "          j = 0;\n"
        // Make sure the unqualified namespace is allowed
        "          if (p_td.xer_bits & (ANY_FROM | ANY_EXCEPT)) {\n"
        "            check_namespace_restrictions(p_td, NULL);\n"
        "          }\n"
        "        }\n"

        "        UNIVERSAL_CHARSTRING after(len - j, (const universal_char*)(value_elements[i]) + j);\n"
        "        after.XER_encode(UNIVERSAL_CHARSTRING_xer_, p_buf, p_flavor | ANY_ATTRIBUTES, p_indent);\n"
        // Put this attribute in a dummy element and walk through it to check its validity
        "        TTCN_Buffer check_buf;\n"
        "        check_buf.put_s(2, (unsigned char*)\"<a\");\n"
        "        check_buf.put_s(p_buf.get_len() - buf_start, p_buf.get_data() + buf_start);\n"
        "        check_buf.put_s(2, (unsigned char*)\"/>\");"
        "        XmlReaderWrap checker(check_buf);\n"
        "        while (1 == checker.Read()) ;\n"
        "      }\n"

        "      p_buf.put_s(shorter, saved);\n" /* restore the '>' and anything after */
        "    } else {\n");
    }
    if (sdef->xerAttribute) {
      src=mputstr(src,
        "    if (e_xer) {\n"
        "      begin_attribute(p_td, p_buf);\n"
        "    } else\n");
    }
    src=mputprintf(src,
      "    if (own_tag) {\n"
      "%s"
      "      p_buf.put_c('<');\n"
      "      boolean write_ns = (e_xer && p_indent==0);\n"
      "      boolean keep_newline = (indenting && !(e_xer && (p_td.xer_bits & XER_LIST)));\n"
      "      if (e_xer) write_ns_prefix(p_td, p_buf);\n"
      "      p_buf.put_s((size_t)p_td.namelens[e_xer]-write_ns-(write_ns || !keep_newline), "
      "(const unsigned char*)p_td.names[e_xer]);\n"

      /* namespace declarations for the toplevel element */
      "      if (e_xer && p_indent==0)\n"
      "      {\n"
      "        size_t num_collected = 0;\n"
      "        char **collected_ns = NULL;\n"
      "        bool def_ns = false;\n"
      "        collected_ns = collect_ns(p_td, num_collected, def_ns);\n"
      "        for (size_t cur_coll = 0; cur_coll < num_collected; ++cur_coll) {\n"
      "          p_buf.put_s(strlen(collected_ns[cur_coll]), (cbyte*)collected_ns[cur_coll]);\n"
      "          Free(collected_ns[cur_coll]);\n"
      "        }\n"
      "        Free(collected_ns);\n"
      "        p_buf.put_s(1 + keep_newline, (cbyte*)\">\\n\");\n"
      "      }\n"
      , sdef->xerAttribute ? "      if (indenting) do_indent(p_buf, p_indent);\n" : ""
      );
    if (sdef->xmlValueList) {
      src=mputstr(src, "      if (indenting && !e_xer) do_indent(p_buf, p_indent+1);\n"); /* !e_xer or GDMO */
    }
    src=mputstr(src,
      "    }\n"
      "    p_flavor |= XER_RECOF | (p_td.xer_bits & XER_LIST);\n"
      "    TTCN_EncDec_ErrorContext ec_0(\"Index \");\n"
      "    TTCN_EncDec_ErrorContext ec_1;\n"
      );
    src=mputprintf(src,
      "    for (int i = 0; i < n_elements; ++i) {\n"
      "      ec_1.set_msg(\"%%d: \", i);\n"
      "      if (e_xer && (p_td.xer_bits & XER_LIST) && i>0) p_buf.put_c(' ');\n"
      "      value_elements[i].XER_encode(%s_xer_, p_buf, p_flavor, p_indent+own_tag);\n"
      "    }\n"
      "    if (indenting && !is_exerlist(p_flavor)) {\n",
      sdef->oftypedescrname
    );
    if (sdef->xmlValueList) {
      src=mputstr(src, "      if (!e_xer) p_buf.put_c('\\n');\n"); /* !e_xer or GDMO */
    }
    src=mputstr(src,
      "      do_indent(p_buf, p_indent);\n"
      "    }\n");
    if (sdef->xerAttribute) {
      src=mputstr(src,
        "    if (e_xer) p_buf.put_c('\\'');\n"
        "    else\n");
    }
    src=mputstr(src,
      "    if (own_tag){\n"
      "      p_buf.put_c('<');\n"
      "      p_buf.put_c('/');\n"
      "      if (e_xer) write_ns_prefix(p_td, p_buf);\n"
      "      p_buf.put_s((size_t)p_td.namelens[e_xer]-!indenting, (const unsigned char*)p_td.names[e_xer]);\n"
      "    }\n");
    if (sdef->xerAnyAttrElem) {
      src = mputstr(src, "  }\n");
    }
    src=mputstr(src,
      "  }\n" /* end if(no elements) */
      "  return (int)p_buf.get_len() - encoded_length;\n"
      "}\n\n"
      );

    src = mputprintf(src, /* XERSTUFF decoder codegen for record-of */
#ifndef NDEBUG
      "// written by %s in " __FILE__ " at %d\n"
#endif
      "int %s::XER_decode(const XERdescriptor_t& p_td, XmlReaderWrap& p_reader, "
      "unsigned int p_flavor)\n{\n"
      "  boolean e_xer = is_exer(p_flavor);\n"
      "  int xerbits = p_td.xer_bits;\n"
      "  if (p_flavor & XER_TOPLEVEL) xerbits &= ~UNTAGGED;\n"
      "  boolean own_tag = !(e_xer && ((xerbits & (ANY_ELEMENT|ANY_ATTRIBUTES|UNTAGGED))"
      "    || (p_flavor & USE_TYPE_ATTR)));\n" /* incase the parent has USE-UNION */
      /* not toplevel anymore and remove the flags for USE-UNION the oftype doesn't need them */
      "  p_flavor &= ~XER_TOPLEVEL & ~XER_LIST & ~USE_TYPE_ATTR;\n" 
      "  int rd_ok=1, xml_depth=-1;\n"
      "  *this = NULL_VALUE;\n" /* empty but initialized array */
      "  int type = 0;\n" /* none */
      "  if (own_tag) for (rd_ok = p_reader.Ok(); rd_ok == 1; rd_ok = p_reader.Read()) {\n"
      "    type = p_reader.NodeType();\n"
      "    if (e_xer && (p_td.xer_bits & XER_ATTRIBUTE)) {\n"
      "      if ((XML_READER_TYPE_ELEMENT   == type && p_reader.MoveToFirstAttribute() == 1)\n"
      "        || XML_READER_TYPE_ATTRIBUTE == type) {\n"
      "        verify_name(p_reader, p_td, e_xer);\n"
      "        break;"
      "      }\n"
      "    }\n"
      "    if (e_xer && (p_td.xer_bits & XER_LIST)) {\n"
      "      if (XML_READER_TYPE_TEXT == type) break;\n"
      "    }\n"
      "    else {\n"
      "      if (XML_READER_TYPE_ELEMENT == type) {\n"
      "        verify_name(p_reader, p_td, e_xer);\n"
      "        xml_depth = p_reader.Depth();\n"
      "        break;\n"
      "      }\n"
      "    }\n" /* endif(e_xer && list) */
      "  }\n" /* next read */
      "  else xml_depth = p_reader.Depth();\n"
      "  p_flavor |= XER_RECOF;\n"
#ifndef NDEBUG
      , __FUNCTION__, __LINE__
#endif
      , name
      );

    src = mputprintf(src,
      "  if (e_xer && (p_td.xer_bits & XER_LIST)) {\n" /* LIST decoding*/
      "    char *x_val = (char*)p_reader.NewValue();\n" /* we own it */
      "    size_t x_pos = 0;\n"
      "    size_t x_len = strlen(x_val);\n"
      /* The string contains a bunch of values separated by whitespace.
       * Tokenize the string and create a new buffer which looks like
       * an XML element (<ns:name xmlns:ns='uri'>value</ns:name>),
       * then use that to decode the value. */
      "    for(char * str = strtok(x_val, \" \\t\\x0A\\x0D\"); str != 0; str = strtok(x_val + x_pos, \" \\t\\x0A\\x0D\")) {\n"
      // Calling strtok with NULL won't work here, since the decoded element can have strtok calls aswell
      "      x_pos += strlen(str) + 1;\n"
      "      TTCN_Buffer buf_2;\n"
      "      buf_2.put_c('<');\n"
      "      write_ns_prefix(%s_xer_, buf_2);\n"
      "      const char * const exer_name = %s_xer_.names[1];\n"
      "      boolean i_can_has_ns = %s_xer_.my_module != 0 && %s_xer_.ns_index != -1;\n"
      /* If it has a namespace, chop off the '>' from the end */
      "      buf_2.put_s((size_t)%s_xer_.namelens[1]-1-i_can_has_ns, (cbyte*)exer_name);\n"
      "      if (i_can_has_ns) {\n"
      "        const namespace_t * const pns = %s_xer_.my_module->get_ns(%s_xer_.ns_index);\n"
      "        buf_2.put_s(7 - (*pns->px == 0), (cbyte*)\" xmlns:\");\n"
      "        buf_2.put_s(strlen(pns->px), (cbyte*)pns->px);\n"
      "        buf_2.put_s(2, (cbyte*)\"='\");\n"
      "        buf_2.put_s(strlen(pns->ns), (cbyte*)pns->ns);\n"
      "        buf_2.put_s(2, (cbyte*)\"'>\");\n"
      "      }\n"
      /* start tag completed */
      "      buf_2.put_s(strlen(str), (cbyte*)str);\n"
      "      buf_2.put_c('<');\n"
      "      buf_2.put_c('/');\n"
      "      write_ns_prefix(%s_xer_, buf_2);\n"
      "      buf_2.put_s((size_t)%s_xer_.namelens[1], (cbyte*)exer_name);\n"
      "      XmlReaderWrap reader_2(buf_2);\n"
      "      rd_ok = reader_2.Read();\n" /* Move to the start element. */
      /* Don't move to the #text, that's the callee's responsibility. */
      /* The call to the non-const operator[] creates a new element object,
       * then we call its XER_decode with the temporary XML reader. */
      "      (*this)[n_elements].XER_decode(%s_xer_, reader_2, p_flavor);\n"
      "      if (p_flavor & EXIT_ON_ERROR && !(*this)[n_elements - 1].is_bound()) {\n"
      "        if (1 == n_elements) {\n"
      // Failed to decode even the first element
      "          clean_up();\n"
      "        } else {\n"
      // Some elements were successfully decoded -> only delete the last one
      "          set_size(n_elements - 1);\n"
      "        }\n"
      "        xmlFree(x_val);\n"
      "       return -1;\n"
      "      }\n"
      "      if (x_pos >= x_len) break;\n"
      "    }\n"
      "    xmlFree(x_val);\n"
      "    if ((p_td.xer_bits & XER_ATTRIBUTE)) ;\n"
      /* Let the caller do AdvanceAttribute() */
      "    else if (own_tag) {\n"
      "      p_reader.Read();\n" /* on closing tag */
      "      p_reader.Read();\n" /* past it */
      "    }\n"
      "  }\n"
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
      , sdef->oftypedescrname, sdef->oftypedescrname
    );

    src = mputprintf(src,
      "  else {\n"
      "    if (p_flavor & PARENT_CLOSED) ;\n"
      /* Nothing to do, but do not advance past the parent's element */
      "    else if (own_tag && p_reader.IsEmptyElement()) rd_ok = p_reader.Read();\n"
      /* It's our XML empty element: nothing to do, skip past it */
      "    else {\n"
      /* Note: there is no p_reader.Read() at the end of the loop below.
       * Each element is supposed to consume enough to leave the next element
       * well-positioned. */
      "      for (rd_ok = own_tag ? p_reader.Read() : p_reader.Ok(); rd_ok == 1; ) {\n"
      "        type = p_reader.NodeType();\n"
      "        if (XML_READER_TYPE_ELEMENT == type)\n"
      "        {\n");
    if (sdef->xerAnyAttrElem) {
      src = mputprintf(src,
        "          if (e_xer && (p_td.xer_bits & ANY_ELEMENT)) {\n"
        /* This is a record-of with ANY-ELEMENT applied, which is really meant
         * for the element type (a string), so behave like a record-of
         * (string with ANY-ELEMENT): call the non-const operator[]
         * to create a new element, then read the entire XML element into it. */
        "            (*this)[n_elements] = (const char*)p_reader.ReadOuterXml();\n"
        /* Consume the element, then move ahead */
        "            for (rd_ok = p_reader.Read(); rd_ok == 1 && p_reader.Depth() > xml_depth; rd_ok = p_reader.Read()) {}\n"
        "            if (p_reader.NodeType() != XML_READER_TYPE_ELEMENT) rd_ok = p_reader.Read();\n"
        "          } else");
    }
    src = mputprintf(src,
      "          {\n"
      /* An untagged record-of ends if it encounters an element with a name
       * that doesn't match its component */
      "            if (!own_tag && !can_start((const char*)p_reader.LocalName(), "
      "(const char*)p_reader.NamespaceUri(), %s_xer_, p_flavor)) {\n"
      "              for (; rd_ok == 1 && p_reader.Depth() > xml_depth; rd_ok = p_reader.Read()) ;\n"
      "              break;\n"
      "            }\n"
      /* The call to the non-const operator[] creates the element */
      "            operator [](n_elements).XER_decode(%s_xer_, p_reader, p_flavor);\n"
      "          }\n"
      "        }\n"
      "        else if (XML_READER_TYPE_END_ELEMENT == type) {\n"
      "          for (; p_reader.Depth() > xml_depth; rd_ok = p_reader.Read()) ;\n"
      "          if (own_tag) {\n"
      "            verify_end(p_reader, p_td, xml_depth, e_xer);\n"
      "            rd_ok = p_reader.Read();\n" /* move forward one last time */
      "          }\n"
      "          break;\n"
      "        }\n"
      "        else {\n"
      "          rd_ok = p_reader.Read();\n"
      "        }\n"
      "      }\n" /* next read */
      "    }\n" /* if not empty element */
      "  }\n" /* if not LIST */
      "  return 1;\n"
      "}\n\n"
      , sdef->oftypedescrname, sdef->oftypedescrname
    );
  }
  if (json_needed) {
    // JSON encode, RT1, mem. alloc. optimised
    src = mputprintf(src,
      "int %s::JSON_encode(const TTCN_Typedescriptor_t&, JSON_Tokenizer& p_tok) const\n"
      "{\n"
      "  if (!is_bound()) {\n"
      "    TTCN_EncDec_ErrorContext::error(TTCN_EncDec::ET_UNBOUND,\n"
      "      \"Encoding an unbound value of type %s.\");\n"
      "    return -1;\n"
      "  }\n\n"
      "  int enc_len = p_tok.put_next_token(JSON_TOKEN_ARRAY_START, NULL);\n"
      "  for(int i = 0; i < n_elements; ++i) {\n"
      "    int ret_val = value_elements[i].JSON_encode(%s_descr_, p_tok);\n"
      "    if (0 > ret_val) break;\n"
      "    enc_len += ret_val;\n"
      "  }\n"
      "  enc_len += p_tok.put_next_token(JSON_TOKEN_ARRAY_END, NULL);\n"
      "  return enc_len;\n"
      "}\n\n"
      , name, dispname, sdef->oftypedescrname);
    
    // JSON decode, RT1, mem. alloc. optimised
    src = mputprintf(src,
      "int %s::JSON_decode(const TTCN_Typedescriptor_t&, JSON_Tokenizer& p_tok, boolean p_silent)\n"
      "{\n"
      "  json_token_t token = JSON_TOKEN_NONE;\n"
      "  int dec_len = p_tok.get_next_token(&token, NULL, NULL);\n"
      "  if (JSON_TOKEN_ERROR == token) {\n"
      "    JSON_ERROR(TTCN_EncDec::ET_INVAL_MSG, JSON_DEC_BAD_TOKEN_ERROR, \"\");\n"
      "    return JSON_ERROR_FATAL;\n"
      "  }\n"
      "  else if (JSON_TOKEN_ARRAY_START != token) {\n"
      "    return JSON_ERROR_INVALID_TOKEN;\n"
      "  }\n\n"
      "  set_size(0);\n"
      "  while (true) {\n"
      "    size_t buf_pos = p_tok.get_buf_pos();\n"
      "    %s val;\n"
      "    int ret_val = val.JSON_decode(%s_descr_, p_tok, p_silent);\n"
      "    if (JSON_ERROR_INVALID_TOKEN == ret_val) {\n"
      "      p_tok.set_buf_pos(buf_pos);\n"
      "      break;\n"
      "    }\n"
      "    else if (JSON_ERROR_FATAL == ret_val) {\n"
      "      if (p_silent) {\n"
      "        clean_up();\n"
      "      }\n"
      "      return JSON_ERROR_FATAL;\n"
      "    }\n"
      "    set_size(n_elements + 1);\n"
      "    value_elements[n_elements - 1] = val;\n"
      "    dec_len += ret_val;\n"
      "  }\n\n"
      "  dec_len += p_tok.get_next_token(&token, NULL, NULL);\n"
      "  if (JSON_TOKEN_ARRAY_END != token) {\n"
      "    JSON_ERROR(TTCN_EncDec::ET_INVAL_MSG, JSON_DEC_REC_OF_END_TOKEN_ERROR, \"\");\n"
      "    if (p_silent) {\n"
      "      clean_up();\n"
      "    }\n"
      "    return JSON_ERROR_FATAL;\n"
      "  }\n\n"
      "  return dec_len;\n"
      "}\n\n"
      , name, type, sdef->oftypedescrname);
  }
  
  /* functions for keeping track of referenced indices (only implemented for regular record of class) */
  def = mputstr(def,
    "void add_refd_index(int) {}\n"
    "void remove_refd_index(int) {}\n\n");
  /* end of class */
  def = mputstr(def, "};\n\n");

  output->header.class_decls = mputprintf(output->header.class_decls,
                                          "class %s;\n", name);
  output->header.class_defs = mputstr(output->header.class_defs, def);
  Free(def);
  output->source.methods = mputstr(output->source.methods, src);
  Free(src);
  /* Copied from record.c.  */
  output->header.function_prototypes =
	mputprintf(output->header.function_prototypes,
	"extern boolean operator==(null_type null_value, const %s& "
	  "other_value);\n", name);
  output->source.function_bodies =
	mputprintf(output->source.function_bodies,
	"boolean operator==(null_type, const %s& other_value)\n"
	"{\n"
    "if (other_value.n_elements==-1)\n"
	"TTCN_error(\"The right operand of comparison is an unbound value of "
    "type %s.\");\n"
	"return other_value.n_elements == 0;\n"
	"}\n\n", name, dispname);

  output->header.function_prototypes =
	mputprintf(output->header.function_prototypes,
	"inline boolean operator!=(null_type null_value, const %s& "
	  "other_value) "
	"{ return !(null_value == other_value); }\n", name);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void defRecordOfClass2(const struct_of_def *sdef, output_struct *output)
{
  char *def = NULL, *src = NULL;
  const char *name = sdef->name;
  const char *type = sdef->type;
  boolean raw_needed = sdef->hasRaw && enable_raw();
  boolean xer_needed = sdef->hasXer && enable_xer();

  /* Class definition */
  def = mputprintf(def,
                   "class %s : public Record_Of_Type {\n", name);

  /* constant unbound element */
  def = mputprintf(def, "\nstatic const %s UNBOUND_ELEM;\n", type);
  src = mputprintf(src, "\nconst %s %s::UNBOUND_ELEM;\n", type, name);

  /* public member functions */
  def = mputstr(def, "\npublic:\n");

  /* constructors */
  def = mputprintf(def, "%s(): Record_Of_Type() {}\n", name);
  def = mputprintf(def, "%s(null_type other_value): Record_Of_Type(other_value) {}\n", name);
  /* copy constructor */
  def = mputprintf(def, "%s(const %s& other_value): Record_Of_Type(other_value) {}\n", name, name);
  /* destructor */
  def = mputprintf(def, "~%s() { clean_up(); }\n\n", name);

  /* assignment operators */
  def = mputprintf(def, "inline %s& operator=(null_type other_value) "
    "{ set_val(other_value); return *this; }\n", name);
  def = mputprintf(def, "inline %s& operator=(const %s& other_value) "
    "{ set_value(&other_value); return *this; }\n\n", name, name);

  /* comparison operators */
  def = mputprintf(def, "inline boolean operator==(const %s& other_value) const "
    "{ return is_equal(&other_value); }\n", name);
  def = mputprintf(def, "boolean operator!=(const %s& other_value) const "
    "{ return !is_equal(&other_value); }\n", name);

  /* indexing operators */
  def = mputprintf(def,
    "%s& operator[](int index_value);\n"
    "%s& operator[](const INTEGER& index_value);\n"
    "const %s& operator[](int index_value) const;\n"
    "const %s& operator[](const INTEGER& index_value) const;\n",
    type,
    type,
    type,
    type);

  src = mputprintf(src,
    "%s& %s::operator[](int index_value) { return *(static_cast<%s*>(get_at(index_value))); }\n"
    "%s& %s::operator[](const INTEGER& index_value) { return *(static_cast<%s*>(get_at(index_value))); }\n"
    "const %s& %s::operator[](int index_value) const { return *(static_cast<const %s*>(get_at(index_value))); }\n"
    "const %s& %s::operator[](const INTEGER& index_value) const { return *(static_cast<const %s*>(get_at(index_value))); }\n\n",
    type, name, type,
    type, name, type,
    type, name, type,
    type, name, type);

  /* rotation operators */
  def = mputprintf(def,
    "%s operator<<=(int rotate_count) const;\n"
    "%s operator<<=(const INTEGER& rotate_count) const;\n"
    "%s operator>>=(int rotate_count) const;\n"
    "%s operator>>=(const INTEGER& rotate_count) const;\n\n",
    name, name, name, name);
  src = mputprintf(src,
    "%s %s::operator<<=(int rotate_count) const\n"
    "{\n"
    "return *this >>= (-rotate_count);\n"
    "}\n\n"
    "%s %s::operator<<=(const INTEGER& rotate_count) const\n"
    "{\n"
    "%s rec_of;\n"
    "return *((%s*)rotl(rotate_count, &rec_of));\n"
    "}\n\n"
    "%s %s::operator>>=(const INTEGER& rotate_count) const\n"
    "{\n"
    "%s rec_of;\n"
    "return *((%s*)rotr(rotate_count, &rec_of));\n"
    "}\n\n"
    "%s %s::operator>>=(int rotate_count) const\n"
    "{\n"
    "%s rec_of;\n"
    "return *((%s*)rotr(rotate_count, &rec_of));\n"
    "}\n\n",
    name, name, name, name, name, name, name, name, name, name, name,
    name, name, name);

  /* concatenation */
  def = mputprintf(def,
    "%s operator+(const %s& other_value) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s::operator+(const %s& other_value) const\n"
    "{\n"
    "%s rec_of;\n"
    "return *((%s*)concat(&other_value, &rec_of));\n"
    "}\n\n", name, name, name, name, name);

  /* substr() */
  def = mputprintf(def,
    "%s substr(int index, int returncount) const;\n\n", name);
  src = mputprintf(src,
    "%s %s::substr(int index, int returncount) const\n"
    "{\n"
    "%s rec_of;\n"
    "substr_(index, returncount, &rec_of);\n"
    "return rec_of;\n"
    "}\n\n", name, name, name);

  /* replace() */
  def = mputprintf(def,
    "%s replace(int index, int len, const %s& repl) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s::replace(int index, int len, const %s& repl) const\n"
    "{\n"
    "%s rec_of;\n"
    "replace_(index, len, &repl, &rec_of);\n"
    "return rec_of;\n"
    "}\n\n", name, name, name, name);
  def = mputprintf(def,
    "%s replace(int index, int len, const %s_template& repl) const;\n\n",
    name, name);
  src = mputprintf(src,
    "%s %s::replace(int index, int len, const %s_template& repl) const\n"
    "{\n"
    "%s rec_of;\n"
    "replace_(index, len, &repl, &rec_of);\n"
    "return rec_of;\n"
    "}\n\n", name, name, name, name);

  def = mputprintf(def,
    "Base_Type* clone() const { return new %s(*this); }\n"
    "const TTCN_Typedescriptor_t* get_descriptor() const;\n"
    "const TTCN_Typedescriptor_t* get_elem_descr() const;\n"
    "Base_Type* create_elem() const;\n"
    "const Base_Type* get_unbound_elem() const;\n"
    "boolean is_set() const { return %s; }\n",
    name,
    (sdef->kind == SET_OF) ? "TRUE" : "FALSE");

  src = mputprintf(src,
    "Base_Type* %s::create_elem() const { return new %s; }\n"
    "const Base_Type* %s::get_unbound_elem() const { return &UNBOUND_ELEM; }\n"
    "const TTCN_Typedescriptor_t* %s::get_descriptor() const { return &%s_descr_; }\n"
    "const TTCN_Typedescriptor_t* %s::get_elem_descr() const { return &%s_descr_; }\n\n",
    name, type,
    name,
    name, name,
    name, sdef->oftypedescrname);

  /* helper functions called by enc/dec members of the ancestor class */
  if (raw_needed) {
    def = mputprintf(def, "int rawdec_ebv() const { return %d; }\n",
      (int)sdef->raw.extension_bit);
  }
  if (xer_needed) {
    def = mputprintf(def, "boolean isXerAttribute() const { return %s; }\n"
      "virtual boolean can_start_v(const char * name, const char *uri, "
      "XERdescriptor_t const& xd, unsigned int);\n"
      "static  boolean can_start  (const char * name, const char *uri, "
      "XERdescriptor_t const& xd, unsigned int);\n",
      sdef->xerAttribute ? "TRUE" : "FALSE");
    src = mputprintf(src,
      /* The virtual can_start_v hands off to the static can_start.
       * We must make a virtual call in Record_Of_Type::XER_decode because
       * we don't know the actual type (derived from Record_Of_Type) */
      "boolean %s::can_start_v(const char *name, const char *uri, "
      "XERdescriptor_t const& xd, unsigned int flavor) {\n"
      "  return can_start(name, uri, xd, flavor);\n"
      "}\n\n"
      "boolean %s::can_start(const char *name, const char *uri, "
      "XERdescriptor_t const& xd, unsigned int flavor) {\n"
      "  boolean e_xer = is_exer(flavor);\n"
      /* a record-of with ANY-ELEMENT can start with any tag
       * :-( with some exceptions )-: */
      "  if (e_xer && (xd.xer_bits & ANY_ELEMENT)) "
      , name, name
      );

    if (sdef->nFollowers) {
      size_t f;
      src = mputstr(src, "{\n");
      for (f = 0; f < sdef->nFollowers; ++f) {
        src = mputprintf(src,
          "    if (%s::can_start(name, uri, %s_xer_, flavor)) return FALSE;\n"
          , sdef->followers[f].type
          , sdef->followers[f].typegen
          );
      }
      src = mputstr(src,
        "    return TRUE;\n"
        "  }\n");
    }
    else {
      src = mputstr(src, "return TRUE;\n");
    }
    src = mputprintf(src,
      /* if EXER and UNTAGGED, it can begin with the tag of the element,
       * otherwise it must be the tag of the type itself,
       * specified in the supplied parameter.
       * If flavor contains UNTAGGED, that's a signal to go directly
       * to the embedded type. */
      "  if (!e_xer || !((xd.xer_bits|flavor) & UNTAGGED)) return "
      "check_name(name, xd, e_xer) && (!e_xer || check_namespace(uri, xd));\n"
      "  else return %s::can_start(name, uri, %s_xer_, flavor | XER_RECOF);\n"
      "}\n\n", sdef->type, sdef->oftypedescrname);
    def = mputprintf(def, "boolean isXmlValueList() const { return %s; }\n\n",
      sdef->xmlValueList ? "TRUE" : "FALSE");
  }
  else {
    /* The call in XER_decode is still there, can_start_v must exist. */
    def = mputstr(def,
      "virtual boolean can_start_v(const char *, const char *, "
      "XERdescriptor_t const&, unsigned int) { return FALSE; }\n");
  }

  /* end of class */
  def = mputstr(def, "};\n\n");

  output->header.class_decls = mputprintf(output->header.class_decls,
                                          "class %s;\n", name);
  output->header.class_defs = mputstr(output->header.class_defs, def);
  Free(def);
  output->source.methods = mputstr(output->source.methods, src);
  Free(src);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void defRecordOfTemplate1(const struct_of_def *sdef, output_struct *output)
{
  char *def = NULL, *src = NULL;
  const char *name = sdef->name, *dispname = sdef->dispname;
  const char *type = sdef->type;
  const char *base_class = sdef->kind == RECORD_OF ? "Record_Of_Template" :
    "Restricted_Length_Template";

  /* Class definition and private data members */
  def = mputprintf(def,
    "class %s_template : public %s {\n"
    "union {\n"
    "struct {\n"
    "int n_elements;\n"
    "%s_template **value_elements;\n"
    "} single_value;\n"
    "struct {\n"
    "unsigned int n_values;\n"
    "%s_template *list_value;\n"
    "} value_list;\n", name, base_class, type, name);
  if (sdef->kind == SET_OF) {
    def = mputprintf(def,
      "struct {\n"
      "unsigned int n_items;\n"
      "%s_template *set_items;\n"
      "} value_set;\n", type);
  }
  def = mputstr(def, "};\n");

  /* private member functions */

  /* copy_value function */
  def = mputprintf(def, "void copy_value(const %s& other_value);\n", name);
  src = mputprintf(src,
    "void %s_template::copy_value(const %s& other_value)\n"
    "{\n"
    "if (!other_value.is_bound()) "
    "TTCN_error(\"Initialization of a template of type %s with an unbound "
      "value.\");\n"
    "single_value.n_elements = other_value.size_of();\n"
    "single_value.value_elements = "
      "(%s_template**)allocate_pointers(single_value.n_elements);\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++) {\n"
    "if (other_value[elem_count].is_bound()) {\n"
    "single_value.value_elements[elem_count] = "
      "new %s_template(other_value[elem_count]);\n"
    "} else {\n"
    "single_value.value_elements[elem_count] = new %s_template;\n"
    "}\n"
    "}\n"
    "set_selection(SPECIFIC_VALUE);\n"
    "}\n\n", name, name, dispname, type, type, type);

  /* copy_template function */
  def = mputprintf(def, "void copy_template(const %s_template& "
                   "other_value);\n", name);
  src = mputprintf(src,
    "void %s_template::copy_template(const %s_template& other_value)\n"
    "{\n"
    "switch (other_value.template_selection) {\n"
    "case SPECIFIC_VALUE:\n"
    "single_value.n_elements = other_value.single_value.n_elements;\n"
    "single_value.value_elements = "
      "(%s_template**)allocate_pointers(single_value.n_elements);\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++) {\n"
    "if (UNINITIALIZED_TEMPLATE != "
    "other_value.single_value.value_elements[elem_count]->get_selection()) {\n"
    "single_value.value_elements[elem_count] = new %s_template"
      "(*other_value.single_value.value_elements[elem_count]);\n"
    "} else {\n"
    "single_value.value_elements[elem_count] = new %s_template;\n"
    "}\n"
    "}\n"
    "case OMIT_VALUE:\n"
    "case ANY_VALUE:\n"
    "case ANY_OR_OMIT:\n"
    "break;\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "value_list.n_values = other_value.value_list.n_values;\n"
    "value_list.list_value = new %s_template[value_list.n_values];\n"
    "for (unsigned int list_count = 0; list_count < value_list.n_values; "
      "list_count++)\n"
    "value_list.list_value[list_count].copy_template("
      "other_value.value_list.list_value[list_count]);\n"
    "break;\n", name, name, type, type, type, name);
  if (sdef->kind == SET_OF) {
    src = mputprintf(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "value_set.n_items = other_value.value_set.n_items;\n"
      "value_set.set_items = new %s_template[value_set.n_items];\n"
      "for (unsigned int set_count = 0; set_count < value_set.n_items; "
	"set_count++)\n"
      "value_set.set_items[set_count] = "
	"other_value.value_set.set_items[set_count];\n"
      "break;\n", type);
  }
  src = mputprintf(src,
    "default:\n"
    "TTCN_error(\"Copying an uninitialized/unsupported template of type "
      "%s.\");\n"
    "break;\n"
    "}\n"
    "set_selection(other_value);\n"
    "}\n\n", dispname);

  /* callback function for matching specific values */
  def = mputstr(def,
    "static boolean match_function_specific(const Base_Type *value_ptr, "
      "int value_index, const Restricted_Length_Template *template_ptr, "
      "int template_index);\n");
  src = mputprintf(src,
    "boolean %s_template::match_function_specific(const Base_Type *value_ptr, "
      "int value_index, const Restricted_Length_Template *template_ptr, "
      "int template_index)\n"
    "{\n"
    "if (value_index >= 0) return ((const %s_template*)template_ptr)->"
      "single_value.value_elements[template_index]->"
      "match((*(const %s*)value_ptr)[value_index]);\n"
    "else return ((const %s_template*)template_ptr)->"
      "single_value.value_elements[template_index]->is_any_or_omit();\n"
    "}\n\n", name, name, name, name);

  if (sdef->kind == SET_OF) {
    /* callback function for matching superset and subset */
    def = mputstr(def,
      "static boolean match_function_set(const Base_Type *value_ptr, "
	"int value_index, const Restricted_Length_Template *template_ptr, "
	"int template_index);\n");
    src = mputprintf(src,
      "boolean %s_template::match_function_set(const Base_Type *value_ptr, "
	"int value_index, const Restricted_Length_Template *template_ptr, "
	"int template_index)\n"
      "{\n"
      "if (value_index >= 0) return ((const %s_template*)template_ptr)->"
	"value_set.set_items[template_index].match("
	"(*(const %s*)value_ptr)[value_index]);\n"
      "else return ((const %s_template*)template_ptr)->"
	"value_set.set_items[template_index].is_any_or_omit();\n"
      "}\n\n", name, name, name, name);

    /* callback function for log_match_heuristics */
    def = mputstr(def,
      "static void log_function(const Base_Type *value_ptr, "
	"const Restricted_Length_Template *template_ptr,"
    " int index_value, int index_template);\n");
    src = mputprintf(src,
      "void %s_template::log_function(const Base_Type *value_ptr, "
	"const Restricted_Length_Template *template_ptr,"
    " int index_value, int index_template)\n"
      "{\n"
      "if (value_ptr != NULL && template_ptr != NULL)"
      "((const %s_template*)template_ptr)"
      "->single_value.value_elements[index_template]"
      "->log_match((*(const %s*)value_ptr)[index_value]);\n"
      "else if (value_ptr != NULL) (*(const %s*)value_ptr)[index_value].log();\n"
      "else if (template_ptr != NULL) ((const %s_template*)template_ptr)"
	"->single_value.value_elements[index_template]->log();\n"
      "}\n\n", name, name, name, name, name);
  }

  /* public member functions */
  def = mputstr(def, "\npublic:\n");

  /* constructors */
  def = mputprintf(def, "%s_template();\n", name);
  src = mputprintf(src, "%s_template::%s_template()\n"
                   "{\n"
                   "}\n\n", name, name);

  def = mputprintf(def, "%s_template(template_sel other_value);\n", name);
  src = mputprintf(src, "%s_template::%s_template(template_sel other_value)\n"
                   " : %s(other_value)\n"
                   "{\n"
                   "check_single_selection(other_value);\n"
                   "}\n\n", name, name, base_class);

  def = mputprintf(def, "%s_template(null_type other_value);\n", name);
  src = mputprintf(src, "%s_template::%s_template(null_type)\n"
                   " : %s(SPECIFIC_VALUE)\n"
                   "{\n"
                   "single_value.n_elements = 0;\n"
                   "single_value.value_elements = NULL;\n"
                   "}\n\n", name, name, base_class);

  def = mputprintf(def, "%s_template(const %s& other_value);\n", name, name);
  src = mputprintf(src, "%s_template::%s_template(const %s& other_value)\n"
                   "{\n"
                   "copy_value(other_value);\n"
                   "}\n\n", name, name, name);

  def = mputprintf(def, "%s_template(const OPTIONAL<%s>& other_value);\n",
                   name, name);
  src = mputprintf(src,
    "%s_template::%s_template(const OPTIONAL<%s>& other_value)\n"
    "{\n"
    "switch (other_value.get_selection()) {\n"
    "case OPTIONAL_PRESENT:\n"
    "copy_value((const %s&)other_value);\n"
    "break;\n"
    "case OPTIONAL_OMIT:\n"
    "set_selection(OMIT_VALUE);\n"
    "break;\n"
    "default:\n"
    "TTCN_error(\"Creating a template of type %s from an unbound optional "
      "field.\");\n"
    "}\n"
    "}\n\n", name, name, name, name, dispname);

  /* copy constructor */
  def = mputprintf(def, "%s_template(const %s_template& other_value);\n",
                   name, name);
  src = mputprintf(src,
    "%s_template::%s_template(const %s_template& other_value)\n"
    " : %s()\n"
    "{\n"
    "copy_template(other_value);\n"
    "}\n\n", name, name, name, base_class);

  /* destructor */
  def = mputprintf(def, "~%s_template();\n\n", name);
  src = mputprintf(src,
    "%s_template::~%s_template()\n"
    "{\n"
    "clean_up();\n"
    "}\n\n", name, name);

  /* clean_up function */
  def = mputstr(def, "void clean_up();\n");
  src = mputprintf(src,
    "void %s_template::clean_up()\n"
    "{\n"
    "switch (template_selection) {\n"
    "case SPECIFIC_VALUE:\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++)\n"
    "delete single_value.value_elements[elem_count];\n"
    "free_pointers((void**)single_value.value_elements);\n"
    "break;\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "delete [] value_list.list_value;\n", name);
  if (sdef->kind == SET_OF) {
    src = mputstr(src,
      "break;\n"
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "delete [] value_set.set_items;\n");
  }
  src = mputstr(src,
    "default:\n"
    "break;\n"
    "}\n"
    "template_selection = UNINITIALIZED_TEMPLATE;\n"
    "}\n\n");

  /* assignment operators */
  def = mputprintf(def, "%s_template& operator=(template_sel other_value);\n",
                   name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(template_sel other_value)\n"
    "{\n"
    "check_single_selection(other_value);\n"
    "clean_up();\n"
    "set_selection(other_value);\n"
    "return *this;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s_template& operator=(null_type other_value);\n",
                   name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(null_type)\n"
    "{\n"
    "clean_up();\n"
    "set_selection(SPECIFIC_VALUE);\n"
    "single_value.n_elements = 0;\n"
    "single_value.value_elements = NULL;\n"
    "return *this;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s_template& operator=(const %s& other_value);\n",
                   name, name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(const %s& other_value)\n"
    "{\n"
    "clean_up();\n"
    "copy_value(other_value);\n"
    "return *this;\n"
    "}\n\n", name, name, name);

  def = mputprintf(def, "%s_template& operator=(const OPTIONAL<%s>& "
                   "other_value);\n", name, name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(const OPTIONAL<%s>& other_value)\n"
    "{\n"
    "clean_up();\n"
    "switch (other_value.get_selection()) {\n"
    "case OPTIONAL_PRESENT:\n"
    "copy_value((const %s&)other_value);\n"
    "break;\n"
    "case OPTIONAL_OMIT:\n"
    "set_selection(OMIT_VALUE);\n"
    "break;\n"
    "default:\n"
    "TTCN_error(\"Assignment of an unbound optional field to a template of "
      "type %s.\");\n"
    "}\n"
    "return *this;\n"
    "}\n\n", name, name, name, name, dispname);

  def = mputprintf(def, "%s_template& operator=(const %s_template& "
                   "other_value);\n\n", name, name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(const %s_template& other_value)\n"
    "{\n"
    "if (&other_value != this) {\n"
    "clean_up();\n"
    "copy_template(other_value);\n"
    "}\n"
    "return *this;\n"
    "}\n\n", name, name, name);

  /* indexing operators */
  /* Non-const operator[] is allowed to extend */
  def = mputprintf(def, "%s_template& operator[](int index_value);\n", type);
  src = mputprintf(src,
    "%s_template& %s_template::operator[](int index_value)\n"
    "{\n"
    "if (index_value < 0) TTCN_error(\"Accessing an element of a template "
      "for type %s using a negative index: %%d.\", index_value);\n"
      "switch (template_selection)\n"
      "{\n"
      "  case SPECIFIC_VALUE:\n"
      "    if(index_value < single_value.n_elements) break;\n"
      "    // no break\n"
      "  case OMIT_VALUE:\n"
      "  case ANY_VALUE:\n"
      "  case ANY_OR_OMIT:\n"
      "  case UNINITIALIZED_TEMPLATE:\n"
      "    set_size(index_value + 1);\n"
      "    break;\n"
      "  default:\n"
      "    TTCN_error(\"Accessing an "
           "element of a non-specific template for type %s.\");\n"
      "    break;\n"
      "}\n"
    "return *single_value.value_elements[index_value];\n"
    "}\n\n", type, name, dispname, dispname);

  def = mputprintf(def, "%s_template& operator[](const INTEGER& "
                   "index_value);\n", type);
  src = mputprintf(src,
     "%s_template& %s_template::operator[](const INTEGER& index_value)\n"
     "{\n"
     "index_value.must_bound(\"Using an unbound integer value for indexing "
       "a template of type %s.\");\n"
     "return (*this)[(int)index_value];\n"
     "}\n\n", type, name, dispname);

  /* Const operator[] throws an error if over-indexing */
  def = mputprintf(def, "const %s_template& operator[](int index_value) "
                   "const;\n", type);
  src = mputprintf(src,
    "const %s_template& %s_template::operator[](int index_value) const\n"
    "{\n"
    "if (index_value < 0) TTCN_error(\"Accessing an element of a template "
      "for type %s using a negative index: %%d.\", index_value);\n"
    "if (template_selection != SPECIFIC_VALUE) TTCN_error(\"Accessing an "
      "element of a non-specific template for type %s.\");\n"
    "if (index_value >= single_value.n_elements) "
      "TTCN_error(\"Index overflow in a template of type %s: "
      "The index is %%d, but the template has only %%d elements.\", "
      "index_value, single_value.n_elements);\n"
    "return *single_value.value_elements[index_value];\n"
    "}\n\n", type, name, dispname, dispname, dispname);

  def = mputprintf(def, "const %s_template& operator[](const INTEGER& "
                   "index_value) const;\n\n", type);
  src = mputprintf(src,
    "const %s_template& %s_template::operator[](const INTEGER& index_value) "
    "const\n"
    "{\n"
    "index_value.must_bound(\"Using an unbound integer value for indexing "
      "a template of type %s.\");\n"
    "return (*this)[(int)index_value];\n"
    "}\n\n", type, name, dispname);

  /* set_size function */
  def = mputstr(def, "void set_size(int new_size);\n");
  src = mputprintf(src, "void %s_template::set_size(int new_size)\n"
    "{\n"
    "if (new_size < 0) TTCN_error(\"Internal error: Setting a negative size "
      "for a template of type %s.\");\n"
    "template_sel old_selection = template_selection;\n"
    "if (old_selection != SPECIFIC_VALUE) {\n"
    "clean_up();\n"
    "set_selection(SPECIFIC_VALUE);\n"
    "single_value.n_elements = 0;\n"
    "single_value.value_elements = NULL;\n"
    "}\n"
    "if (new_size > single_value.n_elements) {\n"
    "single_value.value_elements = (%s_template**)reallocate_pointers((void**)"
      "single_value.value_elements, single_value.n_elements, new_size);\n"
    "if (old_selection == ANY_VALUE || old_selection == ANY_OR_OMIT) {\n"
    "for (int elem_count = single_value.n_elements; elem_count < new_size; "
      "elem_count++)\n"
    "single_value.value_elements[elem_count] = new %s_template(ANY_VALUE);\n"
    "} else {\n"
    "for (int elem_count = single_value.n_elements; elem_count < new_size; "
      "elem_count++)\n"
    "single_value.value_elements[elem_count] = new %s_template;\n"
    "}\n"
    "single_value.n_elements = new_size;\n"
    "} else if (new_size < single_value.n_elements) {\n"
    "for (int elem_count = new_size; elem_count < single_value.n_elements; "
      "elem_count++)\n"
    "delete single_value.value_elements[elem_count];\n"
    "single_value.value_elements = (%s_template**)reallocate_pointers((void**)"
      "single_value.value_elements, single_value.n_elements, new_size);\n"
    "single_value.n_elements = new_size;\n"
    "}\n"
    "}\n\n", name, dispname, type, type, type, type);

  /* raw length */
  def = mputstr(def, "int n_elem() const;\n");
  src = mputprintf(src,
    "int %s_template::n_elem() const\n"
    "{\n"
    "  switch (template_selection) {\n"
    "  case SPECIFIC_VALUE:\n"
    "    return single_value.n_elements;\n"
    "    break;\n"
    "  case VALUE_LIST:\n"
    "    return value_list.n_values;\n"
    "    break;\n", name);
/*  if (sdef->kind == SET_OF) {
    src = mputprintf(src,
      );
  }*/
  src = mputstr(src, "  default:\n"
    "    TTCN_error(\"Performing n_elem\");\n"
    "  }\n"
    "}\n\n"
    );

  /* sizeof operation */
  def = mputstr(def,
    "int size_of(boolean is_size) const;\n"
    "inline int size_of() const { return size_of(TRUE); }\n"
    "inline int lengthof() const { return size_of(FALSE); }\n"
  );
  src = mputprintf(src,
    "int %s_template::size_of(boolean is_size) const\n"
    "{\n"
    "const char* op_name = is_size ? \"size\" : \"length\";\n"
    "int min_size;\n"
    "boolean has_any_or_none;\n"
    "if (is_ifpresent) TTCN_error(\"Performing %%sof() operation on a "
      "template of type %s which has an ifpresent attribute.\", op_name);\n"
    "switch (template_selection)\n"
    "{\n"
    "case SPECIFIC_VALUE: {\n"
    "  min_size = 0;\n"
    "  has_any_or_none = FALSE;\n"
    "  int elem_count = single_value.n_elements;\n"
    "  if (!is_size) { while (elem_count>0 && !single_value.value_elements"
      "[elem_count-1]->is_bound()) elem_count--; }\n"
    "  for (int i=0; i<elem_count; i++) {\n"
    "    switch (single_value.value_elements[i]->get_selection()) {\n"
    "    case OMIT_VALUE:\n"
    "      TTCN_error(\"Performing %%sof() operation on a template of type "
      "%s containing omit element.\", op_name);\n"
    "    case ANY_OR_OMIT:\n"
    "      has_any_or_none = TRUE;\n"
    "      break;\n"
    "    default:\n"
    "      min_size++;\n"
    "      break;\n"
    "    }\n"
    "  }\n"
    "} break;\n",
    name, dispname, dispname);
  if (sdef->kind == SET_OF) {
    src = mputprintf(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH: {\n"
      "  min_size = 0;\n"
      "  has_any_or_none = FALSE;\n"
      "  int elem_count = value_set.n_items;\n"
      "  if (!is_size) { while (elem_count>0 && !value_set.set_items"
        "[elem_count-1].is_bound()) elem_count--; }\n"
      "  for (int i=0; i<elem_count; i++) {\n"
      "    switch (value_set.set_items[i].get_selection())\n"
      "    {\n"
      "    case OMIT_VALUE:\n"
      "      TTCN_error(\"Performing %%sof() operation on a template of type "
        "%s containing omit element.\", op_name);\n"
      "    case ANY_OR_OMIT:\n"
      "      has_any_or_none = TRUE;\n"
      "      break;\n"
      "    default:\n"
      "      min_size++;\n"
      "      break;\n"
      "    }\n"
      "  }\n"
      "  if (template_selection==SUPERSET_MATCH) {\n"
      "    has_any_or_none = TRUE;\n"
      "   } else {\n"
      "    int max_size = min_size;\n"
      "    min_size = 0;\n"
      "    if (!has_any_or_none) { // [0,max_size]\n"
      "      switch (length_restriction_type) {\n"
      "      case NO_LENGTH_RESTRICTION:\n"
      "        if (max_size==0) return 0;\n"
      "        TTCN_error(\"Performing %%sof() operation on a template of "
        "type %s with no exact size.\", op_name);\n"
      "      case SINGLE_LENGTH_RESTRICTION:\n"
      "        if (length_restriction.single_length<=max_size)\n"
      "          return length_restriction.single_length;\n"
      "        TTCN_error(\"Performing %%sof() operation on an invalid "
        "template of type %s. The maximum size (%%d) contradicts the length "
        "restriction (%%d).\", op_name, max_size, "
        "length_restriction.single_length);\n"
      "      case RANGE_LENGTH_RESTRICTION:\n"
      "        if (max_size==length_restriction.range_length.min_length) {\n"
      "          return max_size;\n"
      "        } else if (max_size>length_restriction.range_length.min_length)"
        "{\n"
      "          TTCN_error(\"Performing %%sof() operation on a template of "
        "type %s with no exact size.\", op_name);\n"
      "        } else\n"
      "          TTCN_error(\"Performing %%sof() operation on an invalid "
        "template of type %s. Maximum size (%%d) contradicts the length "
        "restriction (%%d..%%d).\", op_name, max_size, "
        "length_restriction.range_length.min_length, "
        "length_restriction.range_length.max_length);\n"
      "      default:\n"
      "        TTCN_error(\"Internal error: Template has invalid length "
        "restriction type.\");\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "} break;\n",
      dispname, dispname, dispname, dispname, dispname);
  } /* set of */
  src = mputprintf(src,
    "case OMIT_VALUE:\n"
    "  TTCN_error(\"Performing %%sof() operation on a template of type %s "
      "containing omit value.\", op_name);\n"
    "case ANY_VALUE:\n"
    "case ANY_OR_OMIT:\n"
    "  min_size = 0;\n"
    "  has_any_or_none = TRUE;\n"
    "  break;\n"
    "case VALUE_LIST:\n"
    "{\n"
    "  if (value_list.n_values<1)\n"
    "    TTCN_error(\"Performing %%sof() operation on a "
      "template of type %s containing an empty list.\", op_name);\n"
    "  int item_size = value_list.list_value[0].size_of(is_size);\n"
    "  for (unsigned int i = 1; i < value_list.n_values; i++) {\n"
    "    if (value_list.list_value[i].size_of(is_size)!=item_size)\n"
    "      TTCN_error(\"Performing %%sof() operation on a template of type "
      "%s containing a value list with different sizes.\", op_name);\n"
    "  }\n"
    "  min_size = item_size;\n"
    "  has_any_or_none = FALSE;\n"
    "  break;\n"
    "}\n"
    "case COMPLEMENTED_LIST:\n"
    "  TTCN_error(\"Performing %%sof() operation on a template of type %s "
      "containing complemented list.\", op_name);\n"
    "default:\n"
    "  TTCN_error(\"Performing %%sof() operation on an "
      "uninitialized/unsupported template of type %s.\", op_name);\n"
    "}\n"
    "return check_section_is_single(min_size, has_any_or_none, "
      "op_name, \"a\", \"template of type %s\");\n"
    "}\n\n",
    dispname, dispname, dispname, dispname, dispname, dispname);

  /* match operation */
  def = mputprintf(def, "boolean match(const %s& other_value) const;\n",
                   name);
  src = mputprintf(src,
    "boolean %s_template::match(const %s& other_value) const\n"
    "{\n"
    "if (!other_value.is_bound()) return FALSE;\n"
    "int value_length = other_value.size_of();\n"
    "if (!match_length(value_length)) return FALSE;\n"
    "switch (template_selection) {\n"
    "case SPECIFIC_VALUE:\n"
    "return match_%s_of(&other_value, value_length, this, "
      "single_value.n_elements, match_function_specific);\n"
    "case OMIT_VALUE:\n"
    "return FALSE;\n"
    "case ANY_VALUE:\n"
    "case ANY_OR_OMIT:\n"
    "return TRUE;\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "for (unsigned int list_count = 0; list_count < value_list.n_values; "
      "list_count++)\n"
    "if (value_list.list_value[list_count].match(other_value)) "
      "return template_selection == VALUE_LIST;\n"
    "return template_selection == COMPLEMENTED_LIST;\n",
    name, name, sdef->kind == RECORD_OF ? "record" : "set");
  if (sdef->kind == SET_OF) {
    src = mputstr(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "return match_set_of(&other_value, value_length, this, "
	"value_set.n_items, match_function_set);\n");
  }
  src = mputprintf(src,
    "default:\n"
    "TTCN_error(\"Matching with an uninitialized/unsupported template "
      "of type %s.\");\n"
    "}\n"
    "return FALSE;\n"
    "}\n\n", dispname);

    /* is_bound function */
  def = mputstr(def,
    "inline boolean is_bound() const \n"
    "  {return template_selection != UNINITIALIZED_TEMPLATE; }\n");

  /* is_value operation */
  def = mputstr(def, "boolean is_value() const;\n");
  src = mputprintf(src,
    "boolean %s_template::is_value() const\n"
    "{\n"
    "if (template_selection != SPECIFIC_VALUE || is_ifpresent) return false;\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++)\n"
    "if (!single_value.value_elements[elem_count]->is_value()) return false;\n"
    "return true;\n"
    "}\n\n", name);

  /* valueof operation */
  def = mputprintf(def, "%s valueof() const;\n", name);
  src = mputprintf(src,
    "%s %s_template::valueof() const\n"
    "{\n"
    "if (template_selection != SPECIFIC_VALUE || is_ifpresent) TTCN_error(\""
    "Performing a valueof or send operation on a non-specific template of type "
    "%s.\");\n"
    "%s ret_val;\n"
    "ret_val.set_size(single_value.n_elements);\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++)\n"
    "if (single_value.value_elements[elem_count]->is_bound()) {\n"
    "ret_val[elem_count] = single_value.value_elements[elem_count]->valueof();\n"
    "}\n"
    "return ret_val;\n"
    "}\n\n", name, name, dispname, name);

  /* substr() predefined function for templates */
  def = mputprintf(def,
    "%s substr(int index, int returncount) const;\n\n", name);
  src = mputprintf(src,
    "%s %s_template::substr(int index, int returncount) const\n"
    "{\n"
    "if (!is_value()) TTCN_error(\"The first argument of function substr() is "
      "a template with non-specific value.\");\n"
    "return valueof().substr(index, returncount);\n"
  "}\n\n", name, name);

  /* replace() predefined function for templates */
  def = mputprintf(def,
    "%s replace(int index, int len, const %s_template& repl) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s_template::replace(int index, int len, const %s_template& repl) const\n"
    "{\n"
    "if (!is_value()) TTCN_error(\"The first argument of function replace() is "
      "a template with non-specific value.\");\n"
    "if (!repl.is_value()) TTCN_error(\"The fourth argument of function "
      "replace() is a template with non-specific value.\");\n"
    "return valueof().replace(index, len, repl.valueof());\n"
  "}\n\n", name, name, name);
  def = mputprintf(def,
    "%s replace(int index, int len, const %s& repl) const;\n\n", name, name);
  src = mputprintf(src,
    "%s %s_template::replace(int index, int len, const %s& repl) const\n"
    "{\n"
    "if (!is_value()) TTCN_error(\"The first argument of function replace() is "
      "a template with non-specific value.\");\n"
    "return valueof().replace(index, len, repl);\n"
  "}\n\n", name, name, name);

  /* value list and set handling operators */
  def = mputstr(def,
    "void set_type(template_sel template_type, unsigned int list_length);\n");
  src = mputprintf(src,
    "void %s_template::set_type(template_sel template_type, "
    "unsigned int list_length)\n"
    "{\n"
    "clean_up();\n"
    "switch (template_type) {\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "value_list.n_values = list_length;\n"
    "value_list.list_value = new %s_template[list_length];\n"
    "break;\n", name, name);
  if (sdef->kind == SET_OF) {
    src = mputprintf(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "value_set.n_items = list_length;\n"
      "value_set.set_items = new %s_template[list_length];\n"
      "break;\n", type);
  }
  src = mputprintf(src,
    "default:\n"
    "TTCN_error(\"Internal error: Setting an invalid type for a template of "
      "type %s.\");\n"
    "}\n"
    "set_selection(template_type);\n"
    "}\n\n", dispname);

  def = mputprintf(def,
    "%s_template& list_item(unsigned int list_index);\n", name);
  src = mputprintf(src,
    "%s_template& %s_template::list_item(unsigned int list_index)\n"
    "{\n"
    "if (template_selection != VALUE_LIST && "
      "template_selection != COMPLEMENTED_LIST) "
      "TTCN_error(\"Internal error: Accessing a list element of a non-list "
      "template of type %s.\");\n"
    "if (list_index >= value_list.n_values) "
      "TTCN_error(\"Internal error: Index overflow in a value list template "
      "of type %s.\");\n"
    "return value_list.list_value[list_index];\n"
    "}\n\n", name, name, dispname, dispname);

  if (sdef->kind == SET_OF) {
    def = mputprintf(def,
      "%s_template& set_item(unsigned int set_index);\n", type);
    src = mputprintf(src,
      "%s_template& %s_template::set_item(unsigned int set_index)\n"
      "{\n"
      "if (template_selection != SUPERSET_MATCH && "
	"template_selection != SUBSET_MATCH) "
	"TTCN_error(\"Internal error: Accessing a set element of a non-set "
	"template of type %s.\");\n"
      "if (set_index >= value_set.n_items) "
	"TTCN_error(\"Internal error: Index overflow in a set template of "
	"type %s.\");\n"
      "return value_set.set_items[set_index];\n"
      "}\n\n", type, name, dispname, dispname);
  }

  /* logging functions */
  def = mputstr(def, "void log() const;\n");
  src = mputprintf
    (src,
     "void %s_template::log() const\n"
     "{\n"
     "switch (template_selection) {\n"
     "case SPECIFIC_VALUE:\n"
     "if (single_value.n_elements > 0) {\n"
     "TTCN_Logger::log_event_str(\"{ \");\n"
     "for (int elem_count = 0; elem_count < single_value.n_elements; "
	"elem_count++) {\n"
      "if (elem_count > 0) TTCN_Logger::log_event_str(\", \");\n", name);
  if (sdef->kind == RECORD_OF) {
    src = mputstr(src,
      "if (permutation_starts_at(elem_count)) "
	"TTCN_Logger::log_event_str(\"permutation(\");\n");
  }
  src = mputstr(src, "single_value.value_elements[elem_count]->log();\n");
  if (sdef->kind == RECORD_OF) {
    src = mputstr(src,
      "if (permutation_ends_at(elem_count)) TTCN_Logger::log_char(')');\n");
  }
  src = mputstr(src,
    "}\n"
    "TTCN_Logger::log_event_str(\" }\");\n"
    "} else TTCN_Logger::log_event_str(\"{ }\");\n"
    "break;\n"
    "case COMPLEMENTED_LIST:\n"
    "TTCN_Logger::log_event_str(\"complement\");\n"
    "case VALUE_LIST:\n"
    "TTCN_Logger::log_char('(');\n"
    "for (unsigned int list_count = 0; list_count < value_list.n_values; "
      "list_count++) {\n"
    "if (list_count > 0) TTCN_Logger::log_event_str(\", \");\n"
    "value_list.list_value[list_count].log();\n"
    "}\n"
    "TTCN_Logger::log_char(')');\n"
    "break;\n");
  if (sdef->kind == SET_OF) {
    src = mputstr(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "TTCN_Logger::log_event(\"%s(\", template_selection == SUPERSET_MATCH "
	"? \"superset\" : \"subset\");\n"
      "for (unsigned int set_count = 0; set_count < value_set.n_items; "
	"set_count++) {\n"
      "if (set_count > 0) TTCN_Logger::log_event_str(\", \");\n"
      "value_set.set_items[set_count].log();\n"
      "}\n"
      "TTCN_Logger::log_char(')');\n"
      "break;\n");
  }
  src = mputstr(src,
     "default:\n"
     "log_generic();\n"
     "}\n"
     "log_restricted();\n"
     "log_ifpresent();\n"
     "}\n\n");

  def = mputprintf(def, "void log_match(const %s& match_value) const;\n",
                   name);
  src = mputprintf(src, "void %s_template::log_match(const %s& match_value) "
                   "const\n"
    "{\n"
    "if(TTCN_Logger::VERBOSITY_COMPACT == TTCN_Logger::get_matching_verbosity()){\n"
    "if(match(match_value)){\n"
    "TTCN_Logger::print_logmatch_buffer();\n"
    "TTCN_Logger::log_event_str(\" matched\");\n"
    "}else{\n", name, name);

  if (sdef->kind == RECORD_OF) {
    src = mputstr(src,
    "if (template_selection == SPECIFIC_VALUE && "
    "single_value.n_elements > 0 && get_number_of_permutations() == 0 && "
    "single_value.n_elements == match_value.size_of()) {\n"
    "size_t previous_size = TTCN_Logger::get_logmatch_buffer_len();\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
    "elem_count++) {\n"
    "if(!single_value.value_elements[elem_count]->match(match_value[elem_count])){\n"
    "TTCN_Logger::log_logmatch_info(\"[%d]\", elem_count);\n"
    "single_value.value_elements[elem_count]->log_match(match_value[elem_count]);\n"
    "TTCN_Logger::set_logmatch_buffer_len(previous_size);\n"
    "}\n"
    "}\n"
    "log_match_length(single_value.n_elements);\n"
    "} else {\n"
    "TTCN_Logger::print_logmatch_buffer();\n"
    "match_value.log();\n"
    "TTCN_Logger::log_event_str(\" with \");\n"
    "log();\n"
    "TTCN_Logger::log_event_str(\" unmatched\");\n"
    "}\n");
  }
  if (sdef->kind == SET_OF) {
    src = mputstr(src,
    "size_t previous_size = TTCN_Logger::get_logmatch_buffer_len();\n"
    "if (template_selection == SPECIFIC_VALUE)\n"
    "  log_match_heuristics(&match_value, match_value.size_of(), this, "
    "single_value.n_elements, match_function_specific, log_function);\n"
    "else{\n"
    "if(previous_size != 0){\n"
    "TTCN_Logger::print_logmatch_buffer();\n"
    "TTCN_Logger::set_logmatch_buffer_len(previous_size);\n"
    "TTCN_Logger::log_event_str(\":=\");\n"
    "}\n"
    "}\n"
    "match_value.log();\n"
    "TTCN_Logger::log_event_str(\" with \");\n"
    "log();\n"
    "TTCN_Logger::log_event_str(\" unmatched\");\n"
    );
  }

  src = mputstr(src,	  "}\n"
    "return;\n"
    "}\n");
  if (sdef->kind == RECORD_OF) {
    /* logging by element is meaningful for 'record of' only */
    src = mputstr(src,
      "if (template_selection == SPECIFIC_VALUE && "
      "single_value.n_elements > 0 && get_number_of_permutations() == 0 && "
      "single_value.n_elements == match_value.size_of()) {\n"
      "TTCN_Logger::log_event_str(\"{ \");\n"
      "for (int elem_count = 0; elem_count < single_value.n_elements; "
	"elem_count++) {\n"
      "if (elem_count > 0) TTCN_Logger::log_event_str(\", \");\n"
      "single_value.value_elements[elem_count]->log_match"
      "(match_value[elem_count]);\n"
      "}\n"
      "TTCN_Logger::log_event_str(\" }\");\n"
      "log_match_length(single_value.n_elements);\n"
      "} else {\n");
  }
  src = mputstr(src,
    "match_value.log();\n"
    "TTCN_Logger::log_event_str(\" with \");\n"
    "log();\n"
    "if (match(match_value)) TTCN_Logger::log_event_str(\" matched\");\n");
  if (sdef->kind == SET_OF) {
    src = mputstr(src, "else {\n"
      "TTCN_Logger::log_event_str(\" unmatched\");\n"
      "if (template_selection == SPECIFIC_VALUE) log_match_heuristics("
	"&match_value, match_value.size_of(), this, single_value.n_elements, "
	"match_function_specific, log_function);\n"
      "}\n");
  } else {
    src = mputstr(src, "else TTCN_Logger::log_event_str(\" unmatched\");\n"
      "}\n");
  }
  src = mputstr(src, "}\n\n");

  /* encoding/decoding functions */
  def = mputstr(def, "void encode_text(Text_Buf& text_buf) const;\n");
  src = mputprintf(src,
    "void %s_template::encode_text(Text_Buf& text_buf) const\n"
    "{\n"
    "encode_text_%s(text_buf);\n"
    "switch (template_selection) {\n"
    "case SPECIFIC_VALUE:\n"
    "text_buf.push_int(single_value.n_elements);\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++)\n"
    "single_value.value_elements[elem_count]->encode_text(text_buf);\n"
    "case OMIT_VALUE:\n"
    "case ANY_VALUE:\n"
    "case ANY_OR_OMIT:\n"
    "break;\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "text_buf.push_int(value_list.n_values);\n"
    "for (unsigned int list_count = 0; list_count < value_list.n_values; "
      "list_count++)\n"
    "value_list.list_value[list_count].encode_text(text_buf);\n"
    "break;\n", name, sdef->kind == RECORD_OF ? "permutation" : "restricted");
  if (sdef->kind == SET_OF) {
    src = mputstr(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "text_buf.push_int(value_set.n_items);\n"
      "for (unsigned int set_count = 0; set_count < value_set.n_items; "
	"set_count++)\n"
      "value_set.set_items[set_count].encode_text(text_buf);\n"
      "break;\n");
  }
  src = mputprintf(src,
    "default:\n"
    "TTCN_error(\"Text encoder: Encoding an uninitialized/unsupported "
      "template of type %s.\");\n"
    "}\n"
    "}\n\n", dispname);

  def = mputstr(def, "void decode_text(Text_Buf& text_buf);\n");
  src = mputprintf(src,
    "void %s_template::decode_text(Text_Buf& text_buf)\n"
    "{\n"
    "clean_up();\n"
    "decode_text_%s(text_buf);\n"
    "switch (template_selection) {\n"
    "case SPECIFIC_VALUE:\n"
    "single_value.n_elements = text_buf.pull_int().get_val();\n"
    "if (single_value.n_elements < 0) TTCN_error(\"Text decoder: Negative "
    "size was received for a template of type %s.\");\n"
    "single_value.value_elements = "
    "(%s_template**)allocate_pointers(single_value.n_elements);\n"
    "for (int elem_count = 0; elem_count < single_value.n_elements; "
      "elem_count++) {\n"
    "single_value.value_elements[elem_count] = new %s_template;\n"
    "single_value.value_elements[elem_count]->decode_text(text_buf);\n"
    "}\n"
    "case OMIT_VALUE:\n"
    "case ANY_VALUE:\n"
    "case ANY_OR_OMIT:\n"
    "break;\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "value_list.n_values = text_buf.pull_int().get_val();\n"
    "value_list.list_value = new %s_template[value_list.n_values];\n"
    "for (unsigned int list_count = 0; list_count < value_list.n_values; "
      "list_count++)\n"
    "value_list.list_value[list_count].decode_text(text_buf);\n"
    "break;\n", name, sdef->kind == RECORD_OF ? "permutation" : "restricted",
    dispname, type, type, name);
  if (sdef->kind == SET_OF) {
    src = mputprintf(src,
      "case SUPERSET_MATCH:\n"
      "case SUBSET_MATCH:\n"
      "value_set.n_items = text_buf.pull_int().get_val();\n"
      "value_set.set_items = new %s_template[value_set.n_items];\n"
      "for (unsigned int set_count = 0; set_count < value_set.n_items; "
	"set_count++)\n"
      "value_set.set_items[set_count].decode_text(text_buf);\n"
      "break;\n", type);
  }
  src = mputprintf(src,
    "default:\n"
    "TTCN_error(\"Text decoder: An unknown/unsupported selection was "
    "received for a template of type %s.\");\n"
    "}\n"
    "}\n\n", dispname);

  /* TTCN-3 ispresent() function */
  def = mputstr(def, "boolean is_present() const;\n");
  src = mputprintf(src,
    "boolean %s_template::is_present() const\n"
    "{\n"
    "if (template_selection==UNINITIALIZED_TEMPLATE) return FALSE;\n"
    "return !match_omit();\n"
    "}\n\n", name);

  /* match_omit() */
  def = mputstr(def, "boolean match_omit() const;\n");
  src = mputprintf(src,
    "boolean %s_template::match_omit() const\n"
    "{\n"
    "if (is_ifpresent) return TRUE;\n"
    "switch (template_selection) {\n"
    "case OMIT_VALUE:\n"
    "case ANY_OR_OMIT:\n"
    "return TRUE;\n"
    "case VALUE_LIST:\n"
    "case COMPLEMENTED_LIST:\n"
    "for (unsigned int i=0; i<value_list.n_values; i++)\n"
    "if (value_list.list_value[i].match_omit())\n"
    "return template_selection==VALUE_LIST;\n"
    "return template_selection==COMPLEMENTED_LIST;\n"
    "default:\n"
    "return FALSE;\n"
    "}\n"
    "return FALSE;\n"
    "}\n\n", name);

  /* set_param() */
  def = mputstr(def, "void set_param(Module_Param& param);\n");
  src = mputprintf(src,
    "void %s_template::set_param(Module_Param& param)\n"
    "{\n"
    "  if (dynamic_cast<Module_Param_Name*>(param.get_id()) != NULL &&\n"
    "      param.get_id()->next_name()) {\n"
    // Haven't reached the end of the module parameter name
    // => the name refers to one of the elements, not to the whole record of
    "    char* param_field = param.get_id()->get_current_name();\n"
    "    if (param_field[0] < '0' || param_field[0] > '9') {\n"
    "      param.error(\"Unexpected record field name in module parameter, expected a valid\"\n"
    "        \" index for %s template type `%s'\");\n"
    "    }\n"
    "    int param_index = -1;\n"
    "    sscanf(param_field, \"%%d\", &param_index);\n"
    "    (*this)[param_index].set_param(param);\n"
    "    return;\n"
    "  }\n"
    "  param.basic_check(Module_Param::BC_TEMPLATE|Module_Param::BC_LIST, \"%s of template\");\n"
    "  switch (param.get_type()) {\n"
    "  case Module_Param::MP_Omit:\n"
    "    *this = OMIT_VALUE;\n"
    "    break;\n"
    "  case Module_Param::MP_Any:\n"
    "    *this = ANY_VALUE;\n"
    "    break;\n"
    "  case Module_Param::MP_AnyOrNone:\n"
    "    *this = ANY_OR_OMIT;\n"
    "    break;\n"
    "  case Module_Param::MP_List_Template:\n"
    "  case Module_Param::MP_ComplementList_Template:\n"
    "    set_type(param.get_type()==Module_Param::MP_List_Template ? VALUE_LIST : COMPLEMENTED_LIST, param.get_size());\n"
    "    for (size_t p_i=0; p_i<param.get_size(); p_i++) {\n"
    "      list_item(p_i).set_param(*param.get_elem(p_i));\n"
    "    }\n"
    "    break;\n"
    "  case Module_Param::MP_Indexed_List:\n"
    "    if (template_selection!=SPECIFIC_VALUE) set_size(0);\n"
    "    for (size_t p_i=0; p_i<param.get_size(); ++p_i) {\n"
    "      (*this)[(int)(param.get_elem(p_i)->get_id()->get_index())].set_param(*param.get_elem(p_i));\n"
    "    }\n"
    "    break;\n",
    name, sdef->kind==RECORD_OF?"record":"set", dispname, sdef->kind==RECORD_OF?"record":"set");
  if (sdef->kind == RECORD_OF) {
    src = mputstr(src,
    "  case Module_Param::MP_Value_List: {\n"
    "    set_size(param.get_size());\n"
    "    int curr_idx = 0;\n"
    "    for (size_t p_i=0; p_i<param.get_size(); ++p_i) {\n"
    "      switch (param.get_elem(p_i)->get_type()) {\n"
    "      case Module_Param::MP_NotUsed:\n"
    "        curr_idx++;\n"
    "        break;\n"
    "      case Module_Param::MP_Permutation_Template: {\n"
    "        int perm_start_idx = curr_idx;\n"
    "        for (size_t perm_i=0; perm_i<param.get_elem(p_i)->get_size(); perm_i++) {\n"
    "          (*this)[curr_idx].set_param(*(param.get_elem(p_i)->get_elem(perm_i)));\n"
    "          curr_idx++;\n"
    "        }\n"
    "        int perm_end_idx = curr_idx - 1;\n"
    "        add_permutation(perm_start_idx, perm_end_idx);\n"
    "      } break;\n"
    "      default:\n"
    "        (*this)[curr_idx].set_param(*param.get_elem(p_i));\n"
    "        curr_idx++;\n"
    "      }\n"
    "    }\n"
    "  } break;\n");
  } else {
    src = mputstr(src,
    "  case Module_Param::MP_Value_List:\n"
    "    set_size(param.get_size());\n"
    "    for (size_t p_i=0; p_i<param.get_size(); ++p_i) {\n"
    "      if (param.get_elem(p_i)->get_type()!=Module_Param::MP_NotUsed) {\n"
    "        (*this)[p_i].set_param(*param.get_elem(p_i));\n"
    "      }\n"
    "    }\n"
    "    break;\n"
    "  case Module_Param::MP_Superset_Template:\n"
    "  case Module_Param::MP_Subset_Template:\n"
    "    set_type(param.get_type()==Module_Param::MP_Superset_Template ? SUPERSET_MATCH : SUBSET_MATCH, param.get_size());\n"
    "    for (size_t p_i=0; p_i<param.get_size(); p_i++) {\n"
    "      set_item(p_i).set_param(*param.get_elem(p_i));\n"
    "    }\n"
    "    break;\n");
  }
  src = mputprintf(src,
    "  default:\n"
    "    param.type_error(\"%s of template\", \"%s\");\n"
    "  }\n"
    "  is_ifpresent = param.get_ifpresent();\n"
    "  set_length_range(param);\n"
    "}\n\n", sdef->kind==RECORD_OF?"record":"set", dispname);

  /* check template restriction */
  def = mputstr(def, "void check_restriction(template_res t_res, "
    "const char* t_name=NULL) const;\n");
  src = mputprintf(src,
    "void %s_template::check_restriction("
      "template_res t_res, const char* t_name) const\n"
    "{\n"
    "if (template_selection==UNINITIALIZED_TEMPLATE) return;\n"
    "switch ((t_name&&(t_res==TR_VALUE))?TR_OMIT:t_res) {\n"
    "case TR_OMIT:\n"
    "if (template_selection==OMIT_VALUE) return;\n"
    "case TR_VALUE:\n"
    "if (template_selection!=SPECIFIC_VALUE || is_ifpresent) break;\n"
    "for (int i=0; i<single_value.n_elements; i++) "
    "single_value.value_elements[i]->check_restriction("
      "t_res, t_name ? t_name : \"%s\");\n"
    "return;\n"
    "case TR_PRESENT:\n"
    "if (!match_omit()) return;\n"
    "break;\n"
    "default:\n"
    "return;\n"
    "}\n"
    "TTCN_error(\"Restriction `%%s' on template of type %%s "
      "violated.\", get_res_name(t_res), t_name ? t_name : \"%s\");\n"
    "}\n\n", name, dispname, dispname);

  /* end of class */
  def = mputstr(def, "};\n\n");

  output->header.class_decls = mputprintf(output->header.class_decls,
                                          "class %s_template;\n", name);
  output->header.class_defs = mputstr(output->header.class_defs, def);
  Free(def);
  output->source.methods = mputstr(output->source.methods, src);
  Free(src);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void defRecordOfTemplate2(const struct_of_def *sdef, output_struct *output)
{
  char *def = NULL, *src = NULL;
  const char *name = sdef->name;
  const char *type = sdef->type;
  const char *base_class = sdef->kind == RECORD_OF ? "Record_Of_Template" : "Set_Of_Template";

  /* Class definition */
  def = mputprintf(def, "class %s_template : public %s {\n", name, base_class);

  /* public member functions */
  def = mputstr(def, "\npublic:\n");

  /* constructors */
  def = mputprintf(def, "%s_template() {}\n", name);

  def = mputprintf(def, "%s_template(template_sel other_value): %s(other_value) "
    "{ check_single_selection(other_value); }\n", name, base_class);

  def = mputprintf(def, "%s_template(null_type other_value);\n", name);
  src = mputprintf(src, "%s_template::%s_template(null_type)\n"
                   " : %s(SPECIFIC_VALUE)\n"
                   "{\n"
                   "single_value.n_elements = 0;\n"
                   "single_value.value_elements = NULL;\n"
                   "}\n\n", name, name, base_class);

  def = mputprintf(def, "%s_template(const %s& other_value) "
    "{ copy_value(&other_value); }\n", name, name);
  def = mputprintf(def, "%s_template(const OPTIONAL<%s>& other_value) "
    "{ copy_optional(&other_value); }\n", name, name);

  /* copy constructor */
  def = mputprintf(def, "%s_template(const %s_template& other_value): %s() { copy_template(other_value); }\n",
                   name, name, base_class);

  /* assignment operators */
  def = mputprintf(def, "%s_template& operator=(template_sel other_value);\n",
                   name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(template_sel other_value)\n"
    "{\n"
    "check_single_selection(other_value);\n"
    "clean_up();\n"
    "set_selection(other_value);\n"
    "return *this;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s_template& operator=(null_type other_value);\n",
                   name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(null_type)\n"
    "{\n"
    "clean_up();\n"
    "set_selection(SPECIFIC_VALUE);\n"
    "single_value.n_elements = 0;\n"
    "single_value.value_elements = NULL;\n"
    "return *this;\n"
    "}\n\n", name, name);

  def = mputprintf(def, "%s_template& operator=(const %s& other_value);\n",
                   name, name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(const %s& other_value)\n"
    "{\n"
    "clean_up();\n"
    "copy_value(&other_value);\n"
    "return *this;\n"
    "}\n\n", name, name, name);

  def = mputprintf(def, "%s_template& operator=(const OPTIONAL<%s>& "
                   "other_value);\n", name, name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(const OPTIONAL<%s>& other_value)\n"
    "{\n"
    "clean_up();\n"
    "copy_optional(&other_value);\n"
    "return *this;\n"
    "}\n\n", name, name, name);

  def = mputprintf(def, "%s_template& operator=(const %s_template& "
                   "other_value);\n\n", name, name);
  src = mputprintf(src,
    "%s_template& %s_template::operator=(const %s_template& other_value)\n"
    "{\n"
    "if (&other_value != this) {\n"
    "clean_up();\n"
    "copy_template(other_value);\n"
    "}\n"
    "return *this;\n"
    "}\n\n", name, name, name);

  /* indexing operators */
  def = mputprintf(def,
    "%s_template& operator[](int index_value);\n"
    "%s_template& operator[](const INTEGER& index_value);\n"
    "const %s_template& operator[](int index_value) const;\n"
    "const %s_template& operator[](const INTEGER& index_value) const;\n",
    type,
    type,
    type,
    type);

  src = mputprintf(src,
    "%s_template& %s_template::operator[](int index_value) { return *(static_cast<%s_template*>(get_at(index_value))); }\n"
    "%s_template& %s_template::operator[](const INTEGER& index_value) { return *(static_cast<%s_template*>(get_at(index_value))); }\n"
    "const %s_template& %s_template::operator[](int index_value) const { return *(static_cast<const %s_template*>(get_at(index_value))); }\n"
    "const %s_template& %s_template::operator[](const INTEGER& index_value) const { return *(static_cast<const %s_template*>(get_at(index_value))); }\n\n",
    type, name, type,
    type, name, type,
    type, name, type,
    type, name, type);

  /* match operation */
  def = mputprintf(def, "inline boolean match(const %s& match_value) const "
    "{ return matchv(&match_value); }\n", name);

  /* valueof operation */
  def = mputprintf(def, "%s valueof() const;\n", name);
  src = mputprintf(src,
    "%s %s_template::valueof() const\n"
    "{\n"
    "%s ret_val;\n"
    "valueofv(&ret_val);\n"
    "return ret_val;\n"
    "}\n\n", name, name, name);

  /* substr() predefined function for templates */
  def = mputprintf(def,
    "%s substr(int index, int returncount) const;\n\n", name);
  src = mputprintf(src,
    "%s %s_template::substr(int index, int returncount) const\n"
    "{\n"
    "%s rec_of;\n"
    "substr_(index, returncount, &rec_of);\n"
    "return rec_of;\n"
    "}\n\n", name, name, name);

  /* replace() predefined function for templates */
  def = mputprintf(def,
    "%s replace(int index, int len, const %s_template& repl) const;\n\n",
    name, name);
  src = mputprintf(src,
    "%s %s_template::replace(int index, int len, const %s_template& repl) const\n"
    "{\n"
    "%s rec_of;\n"
    "replace_(index, len, &repl, &rec_of);\n"
    "return rec_of;\n"
    "}\n\n", name, name, name, name);
  def = mputprintf(def,
    "%s replace(int index, int len, const %s& repl) const;\n\n",
    name, name);
  src = mputprintf(src,
    "%s %s_template::replace(int index, int len, const %s& repl) const\n"
    "{\n"
    "%s rec_of;\n"
    "replace_(index, len, &repl, &rec_of);\n"
    "return rec_of;\n"
    "}\n\n", name, name, name, name);

  /* value list and set handling operators */
  def = mputprintf(def,
    "inline %s_template& list_item(int list_index) { return *(static_cast<%s_template*>(get_list_item(list_index))); }\n", name, name);

  if (sdef->kind == SET_OF) {
    def = mputprintf(def, "%s_template& set_item(int set_index);\n", type);
    src = mputprintf(src,
      "%s_template& %s_template::set_item(int set_index) "
        "{ return *(static_cast<%s_template*>(get_set_item(set_index))); }\n",
      type, name, type);
  }

  /* logging functions */
  def = mputprintf(def, "inline void log_match(const %s& match_value) const "
    "{ log_matchv(&match_value); }\n", name);

  /* virtual helper functions */
  def = mputprintf(def,
    "%s* create() const { return new %s_template; }\n"
    "Base_Template* create_elem() const;\n"
    "const TTCN_Typedescriptor_t* get_descriptor() const;\n",
    base_class, name);
  src = mputprintf(src,
    "Base_Template* %s_template::create_elem() const { return new %s_template; }\n"
    "const TTCN_Typedescriptor_t* %s_template::get_descriptor() const { return &%s_descr_; }\n",
    name, type,
    name, name);

  /* end of class */
  def = mputstr(def, "};\n\n");

  output->header.class_decls = mputprintf(output->header.class_decls,
                                          "class %s_template;\n", name);
  output->header.class_defs = mputstr(output->header.class_defs, def);
  Free(def);
  output->source.methods = mputstr(output->source.methods, src);
  Free(src);
}
