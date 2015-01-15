///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef BASETYPE_HH_
#define BASETYPE_HH_

#include "GeneralTypes.hh"
#include "Mstring.hh"
#include "List.hh"

#include <cmath> // for using "pow" function
#include <cfloat>
#include <cctype>
#include <cerrno>
#include <climits>
#ifndef ULLONG_MAX
#define ULLONG_MAX 18446744073709551615ULL
#endif
#ifndef LLONG_MIN
#define LLONG_MIN -9223372036854775808LL
#endif
#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807LL
#endif

extern bool c_flag_used;
extern bool e_flag_used;


enum VariantMode
{
  V_anyAttributes,
  V_anyElement,
  V_attribute,
  V_attributeFormQualified,
  V_attributeGroup,
  V_controlNamespace,
  V_defaultForEmpty,
  V_element,
  V_elementFormQualified,
  V_embedValues,
  V_formAs,
  V_list,
  V_nameAs,
  V_namespaceAs,
  V_onlyValue,
  V_untagged,
  V_useNil,
  V_useNumber,
  V_useOrder,
  V_useUnion,
  V_whiteSpace
};

enum OriginType
{
  from_simpleType,
  from_element,
  from_attribute,
  from_complexType,
  from_group,
  from_attributeGroup,
  from_unknown
};

class NameType
{
public:
  Mstring originalValueWoPrefix;
  Mstring convertedValue;
  bool list_extension;

  NameType (): originalValueWoPrefix(), convertedValue(), list_extension(false) {}
  // Default copy constructor, assignment operator and destructor are used

  void upload (const Mstring& input)
  {
    if (input.empty()) return;
    convertedValue = input;
    originalValueWoPrefix = input.getValueWithoutPrefix(':');
  }
};

class SimpleType;
class XMLParser;
class TTCN3Module;

/**
 * This type is used as the base class for the used classes
 * that represent the main datatypes in the generated TTCN-3 modules
 *
 * It is installed to have possibility to store main types in one container
 *
 */

class RootType
{
protected:
  XMLParser * 	parser; // no responsibility for this member
  TTCN3Module *	module; // no responsibility for this member

  NameType name;
  NameType type;
  Mstring id;
  unsigned long long int minOccurs;
  unsigned long long int maxOccurs;
  List<Mstring> variant;
  List<Mstring> variant_ref;
  Mstring comment;

  ConstructType construct;
  OriginType origin;
  bool visible;
  /// List of types that depend on this one.
  /// Used to propagate the effect of name conversion to the dependents
  List<SimpleType*> nameDepList; // no responsibility for elements

public:
  RootType(XMLParser * a_parser, TTCN3Module * a_module, ConstructType a_construct);
  virtual ~RootType () {}
  // Default copy constructor and assignment operator is used

  virtual void loadWithValues () = 0;
  virtual void printToFile (FILE * file) = 0;

  virtual void modifyValues () {}

  virtual void referenceResolving () {}
  virtual void nameConversion (NameConversionMode, const List<NamespaceType> &) {}
  virtual void finalModification () {}

  virtual bool hasUnresolvedReference () {return false;}
  virtual void dump (unsigned int) const {}

  void setNameValue (const Mstring& str) {name.convertedValue = str;}
  void setTypeValue (const Mstring& str) {type.convertedValue = str;}
  void useNameListProperty () {name.convertedValue += "_list";}
  void setInvisible () {visible = false;}
  void addToNameDepList (SimpleType * t) {nameDepList.push_back(t);}

  const NameType & getName () const {return name;}
  const NameType & getType () const {return type;}
  unsigned long long int getMinOccurs () const {return minOccurs;}
  unsigned long long int getMaxOccurs () const {return maxOccurs;}
  const List<Mstring> & getVariant () const {return variant;}
  const List<Mstring> & getVariantRef () const {return variant_ref;}
  ConstructType getConstruct () const {return construct;}
  OriginType getOrigin () const {return origin;}
  bool isVisible () const {return visible;}
  XMLParser * getParser () const {return parser;}
  TTCN3Module * getModule () const {return module;}

  void addVariant (VariantMode var, const Mstring& var_value = empty_string, bool into_variant_ref = false);
  void printVariant (FILE * file);

  void addComment (const Mstring& text);
  void printComment (FILE * file);

  void printMinOccursMaxOccurs (FILE * file, bool inside_union,
    bool empty_allowed = true) const;

  friend bool compareTypes (RootType * lhs, RootType * rhs);
};

inline bool compareTypes (RootType * lhs, RootType * rhs)
{
  return lhs->name.originalValueWoPrefix < rhs->name.originalValueWoPrefix;
}

#endif /* BASETYPE_HH_ */
