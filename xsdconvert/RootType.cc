///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "RootType.hh"

#include "TTCN3Module.hh"

RootType::RootType(XMLParser * a_parser, TTCN3Module * a_module, ConstructType a_construct)
: parser(a_parser)
, module(a_module)
, name()
, type()
, id()
, minOccurs(1)
, maxOccurs(1)
, variant()
, variant_ref()
, comment()
, construct(a_construct)
, origin(from_unknown)
, visible(true)
, nameDepList()
{
  switch (a_construct)
  {
  case c_schema:
  case c_annotation:
  case c_include:
  case c_import:
    //redundant: origin = from_unknown;
    break;
  case c_unknown: // because when using fields in complextypes we set construct to c_unknown
  case c_simpleType:
    origin = from_simpleType;
    type.upload(Mstring("anySimpleType"));
    break;
  case c_element:
    origin = from_element;
    type.upload(Mstring("anyType"));
    addVariant(V_element);
    break;
  case c_attribute:
    origin = from_attribute;
    type.upload(Mstring("anySimpleType"));
    addVariant(V_attribute);
    break;
  case c_complexType:
    origin = from_complexType;
    type.upload(Mstring("record"));
    break;
  case c_group:
    origin = from_group;
    type.upload(Mstring("record"));
    addVariant(V_untagged);
    break;
  case c_attributeGroup:
    origin = from_attributeGroup;
    type.upload(Mstring("record"));
    addVariant(V_attributeGroup);
    visible = false;
    break;
  }
}

void RootType::addVariant(VariantMode var, const Mstring& var_value, bool into_variant_ref)
{
  Mstring variantstring;

  switch (var)
  {
  case V_anyAttributes:
    variantstring = "\"anyAttributes" + var_value + "\"";
    break;
  case V_anyElement:
    variantstring = "\"anyElement" + var_value + "\"";
    break;
  case V_attribute:
    variantstring = "\"attribute\"";
    break;
  case V_attributeFormQualified:
    variantstring = "\"attributeFormQualified\"";
    break;
  case V_attributeGroup:
    variantstring = "\"attributeGroup\"";
    break;
  case V_controlNamespace:
    variantstring = "\"controlNamespace" + var_value + "\"";
    break;
  case V_defaultForEmpty:
    variantstring = "\"defaultForEmpty as \'" + var_value + "\'\"";  // chapter 7.1.5
    break;
  case V_element:
    variantstring = "\"element\"";
    break;
  case V_elementFormQualified:
    variantstring = "\"elementFormQualified\"";
    break;
  case V_embedValues:
    variantstring = "\"embedValues\"";
    break;
  case V_formAs:
    variantstring = "\"form as " + var_value + "\"";
    break;
  case V_list:
    variantstring = "\"list\"";
    break;
  case V_nameAs:
    variantstring = "\"name as \'" + var_value + "\'\"";
    break;
  case V_namespaceAs: {
    Mstring prefix;
    Mstring uri;
    for (List<NamespaceType>::iterator namesp = module->getDeclaredNamespaces().begin(); namesp; namesp = namesp->Next) {
      if (namesp->Data.uri == var_value) {
        prefix = namesp->Data.prefix;
        uri = namesp->Data.uri;
        break;
      }
    }
    if (prefix.empty() || uri.empty()) {
      break;
    }
    variantstring = "\"namespace as \'" + uri + "\' prefix \'" + prefix + "\'\"";
    break; }
  case V_onlyValue:
    variantstring = var_value;
    break;
  case V_untagged:
    variantstring = "\"untagged\"";
    break;
  case V_useNil:
    variantstring = "\"useNil\"";
    break;
  case V_useNumber:
    variantstring = "\"useNumber\"";
    break;
  case V_useOrder:
    variantstring = "\"useOrder\"";
    break;
  case V_useUnion:
    variantstring = "\"useUnion\"";
    break;
  case V_whiteSpace:
    variantstring = "\"whiteSpace " + var_value + "\"";
    break;
  }

  if (!variantstring.empty()) {
    variant.push_back(variantstring);
    if (into_variant_ref) {
      variant_ref.push_back(variantstring);
    }
  }
}

void RootType::printVariant(FILE * file)
{
  if (!e_flag_used && !variant.empty())
  {
    fprintf(file, "\nwith {\n");
    for (List<Mstring>::iterator var = variant.end(); var; var = var->Prev)
    {
      fprintf(file, "variant %s;\n", var->Data.c_str());
    }
    fprintf(file, "}");
  }
}

void RootType::addComment(const Mstring& text)
{
  comment += "/* " + text + " */\n";
}

void RootType::printComment(FILE * file)
{
  if (!c_flag_used)
  {
    fprintf(file, "%s", comment.c_str());
  }
}

void RootType::printMinOccursMaxOccurs(FILE * file, bool inside_union,
  bool empty_allowed /* = true */) const
{
  unsigned long long tmp_minOccurs = minOccurs;
  if (minOccurs == 0 && !empty_allowed) tmp_minOccurs = 1ULL;

  if (maxOccurs == 1)
  {
    if (minOccurs == 0) {
      if (inside_union || name.list_extension) {
        fputs("record length(", file);
        if (empty_allowed) fputs("0 .. ", file);
        // else: length(1..1) is shortened to length(1)
        fputs("1) of ", file);
      }
      // else it's optional which is not printed from here
    }
    else if (minOccurs == 1) {
      // min==max==1; do nothing unless...
      if (name.convertedValue == "embed_values") fputs("record length(1) of ", file);
    }
  }
  else if (maxOccurs == ULLONG_MAX) {
    if (minOccurs == 0) {
      fputs("record ", file);
      if (!empty_allowed) fputs("length(1 .. infinity) ", file);
      fputs("of ", file);
    }
    else fprintf(file, "record length(%llu .. infinity) of ", tmp_minOccurs);
  }
  else
  {
    fprintf(file, "record length(%llu .. %llu) of ", tmp_minOccurs, maxOccurs);
  }
}
