///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef GENERAL_TYPES_H_
#define GENERAL_TYPES_H_

#include "Mstring.hh"

enum ConstructType
{
  c_unknown,
  c_schema,
  c_simpleType,
  c_complexType,
  c_element,
  c_attribute,
  c_attributeGroup,
  c_group,
  c_annotation,
  c_include,
  c_import
};

enum NameConversionMode
{
  nameMode,
  typeMode,
  fieldMode
};

enum UseValue
{
  optional,
  prohibited,
  required
};

enum FormValue
{
  notset,
  qualified,
  unqualified
};

/** This type just stores the textual information about an XML namespace */
class NamespaceType
{
public:
  Mstring prefix;
  Mstring uri;

  NamespaceType()
  : prefix(), uri()
  {}

  NamespaceType(const Mstring& p, const Mstring& u)
  : prefix(p), uri(u)
  {}

  bool operator < (const NamespaceType & rhs) const { return uri < rhs.uri; }
  bool operator== (const NamespaceType & rhs) const {
    return (uri == rhs.uri) && (prefix == rhs.prefix);
  }
};

class QualifiedName {
public:
  Mstring nsuri;
  Mstring name;
  bool    dup;

  QualifiedName()
  : nsuri(), name(), dup(false)
  {}

  QualifiedName(const Mstring& ns, const Mstring nm)
  : nsuri(ns), name(nm), dup(false)
  {}

  bool operator <(const QualifiedName& rhs) const { return name < rhs.name; }
  bool operator==(const QualifiedName& rhs) const {
    return (nsuri == rhs.nsuri) && (name == rhs.name);
  }
};

enum wanted { want_CT, want_ST };

#endif /*GENERAL_TYPES_H_*/
