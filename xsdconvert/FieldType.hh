///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef FIELDTYPE_HH_
#define FIELDTYPE_HH_

#include "SimpleType.hh"
#include "GeneralTypes.hh"

class ComplexType;
class FieldType;

struct ImportedField {
  /// The imported field
  FieldType *field;

  /** The original field (probably called "base") which caused the import,
   *  or any other previous field. The point is that the imported field
   *  follows the "original" field. */
  FieldType *orig;
};

/**
 * Type that contains information of a field of a TTCN-3 record or union
 *
 */
class FieldType : public SimpleType
{
  ComplexType * parent;

  List<FieldType*> attributes;

  int level;

  bool isAnyAttr;

  /// Attribute qualifier
  Mstring path;
  Mstring variantID;

  static int variantIdCounter;

public:
  explicit FieldType (ComplexType * a_complextype);
  FieldType (const FieldType & other);
  FieldType & operator = (const FieldType & rhs);
  virtual ~FieldType ();

  void setTypeOfField (const Mstring& in);
  void setNameOfField (const Mstring& in, bool add_to_path_for_later_use, bool from_ref_attribute = false);
  void setToAnyAttribute();
  void applyRefAttribute (const Mstring& ref_value);
  void applyUseAttribute (UseValue use_value, unsigned long long & minOcc, unsigned long long & maxOcc);
  void applyNillableAttribute (const Mstring& nil_value);
  void applyNamespaceAttribute (VariantMode varLabel, const Mstring& ns_list);

  void applyMinMaxOccursAttribute (unsigned long long minOccurs_value, unsigned long long maxOccurs_value,
    bool generate_list_postfix = false);

  void setLevel (int l);
  void incrLevel () {level++;}

  int getLevel () const {return level;}
  const Mstring& getPath () const {return path;}
  const Mstring& getVariantId () const {return variantID;}

  bool isOptional () const {return minOccurs == 0 && maxOccurs == 1 && !name.list_extension;}
  bool isAnyAttribute() const {return isAnyAttr;}

  Mstring generateVariantId ();
  void variantIdReplaceInPath (const Mstring& varid, const Mstring& text);

  void applyReference (const FieldType & other, bool on_attributes = false);

  void referenceForST (SimpleType const * const found_ST,
    List<FieldType*> & temp_container);

  /**  Resolves the reference to the given ComplexType. The fields of the referenced type
   * will be copied into one of the containers.
   * @return 0 if the current field has been added to the container (the field can not be deleted)
   *         "this" if a copy of the field has been added (the field can be deleted)
   */
  FieldType* referenceForCT (ComplexType & found_CT,
    List<FieldType*> & temp_container, List<ImportedField> & temp_container_imported);

  List<FieldType*> & getAttributes () {return attributes;}
  void addAttribute (FieldType * attr);
  void sortAttributes ();
  void checkSortAndAddAttributes (const List<FieldType*> & referencedAttributes);

  void dump (unsigned int depth) const;
};

#endif /* FIELDTYPE_HH_ */
