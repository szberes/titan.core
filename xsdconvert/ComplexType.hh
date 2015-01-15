///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef COMPLEXTYPE_H_
#define COMPLEXTYPE_H_

#include "RootType.hh"

class FieldType;
class SimpleType;

class EmbeddedType
{
public:
  int depth;
  bool valid;
  bool strictValid;
  unsigned long long int minOccurs;
  unsigned long long int maxOccurs;
  FieldType * order_attribute;

  explicit EmbeddedType (int a_depth=0, unsigned long long int a_minOccurs=1, unsigned long long int a_maxOccurs=1,
    FieldType * a_order_attribute=NULL)
  : depth(a_depth)
  , valid(true)
  , strictValid(true)
  , minOccurs(a_minOccurs)
  , maxOccurs(a_maxOccurs)
  , order_attribute(a_order_attribute)
  {}
};

class GenerationType
{
public:
  FieldType * field;
  int depth;
  FieldType * order_attribute;
  FieldType * embed_values_attribute;
  int max_alt;

  explicit GenerationType (FieldType * a_field = NULL, int a_depth = 0,
    FieldType * a_order_attribute = NULL, FieldType * a_embed_values_attribute = NULL, int a_max_alt = 0)
  : field(a_field)
  , depth(a_depth)
  , order_attribute(a_order_attribute)
  , embed_values_attribute(a_embed_values_attribute)
  , max_alt(a_max_alt)
  {}
};

class AttrBaseType
{
public:
  FieldType * base;
  int depth;
  bool valid;

  explicit AttrBaseType (FieldType * a_base = NULL, int a_depth = 0)
  : base(a_base)
  , depth(a_depth)
  , valid(true)
  {}
};


/**
 * Type that contains information coming from XSD complexTypes, model group definitions
 * and attributeGroups
 *
 * Source in XSD:
 *
 * 	* <complexType>, <group> and <attributeGroup> element whose parent element is <schema>
 *
 * Result in TTCN-3:
 *
 * 	* TTCN-3 type
 *
 */
class ComplexType : public RootType
{
public:
  enum ComplexType_Mode
  {
    CT_simpletype_mode,
    CT_complextype_mode,
    CT_undefined_mode
  };
  enum CT_fromST
  {
    fromTagUnion,
    fromTagNillable,
    fromTagComplexType
  };
  enum Resolv_State {
    No,
    InProgress,
    Yes
  };

private:
  List<FieldType*> fields;
  List<FieldType*> fields_final;

  int actualLevel;
  Mstring actualPath;

  List<ComplexType_Mode> ctmode;
  List<GenerationType> fieldGenInfo;
  List<GenerationType> recGenInfo;
  List<AttrBaseType> attributeBases;
  List<EmbeddedType> embed_inSequence;
  List<EmbeddedType> embed_inChoice;
  List<EmbeddedType> embed_inAll;
  bool with_union;
  Resolv_State resolved;

  FieldType * generateField ();
  FieldType * generateAttribute ();
  FieldType * generateRecord (unsigned long long int a_minOccurs, unsigned long long int a_maxOccurs);
  FieldType * generateUnion (unsigned long long int a_minOccurs, unsigned long long int a_maxOccurs);

  void initialSettings ();

  void reference_resolving_funtion (List<FieldType*> & container);
  void sortAttributes ();
  void nameConversion_names (const List<NamespaceType> & ns);
  void nameConversion_types (const List<NamespaceType> & ns);
  void nameConversion_fields (const List<NamespaceType> & ns);
  void indent (FILE * file, int x) { for (int l = 0; l < x; ++l) fprintf(file, "\t"); }

  void printVariant (FILE * file);

public:
  ComplexType (XMLParser * a_parser, TTCN3Module * a_module, ConstructType a_construct);
  ComplexType (const ComplexType & other);
  ComplexType (const SimpleType & other, CT_fromST c);
  ~ComplexType ();

  /** Virtual methods
   *  inherited from RootType
   */
  void loadWithValues ();
  void printToFile (FILE * file);

  void modifyValues ();
  void referenceResolving ();
  void nameConversion (NameConversionMode mode, const List<NamespaceType> & ns);
  void finalModification ();
  bool hasUnresolvedReference ();
  void dump (unsigned int depth) const ;

  void everything_into_fields_final ();

  void addToActualPath (const Mstring& text) {actualPath += text;}

  void setWithUnion (bool b) {with_union = b;}

  const List<EmbeddedType> & getEmbedInChoice () const {return embed_inChoice;}

  const List<FieldType*> & getFields () const {return fields;}
  const List<FieldType*> & getFieldsFinal () const {return fields_final;}
  bool getWithUnion () const {return with_union;}

  int getActualLevel () const {return actualLevel;}
  const Mstring & getActualPath () const {return actualPath;}
  ComplexType_Mode getMode() const {
    return ctmode.empty() ? CT_undefined_mode : ctmode.front();
  }
};

#endif /* COMPLEXTYPE_H_ */
