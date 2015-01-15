///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "FieldType.hh"

#include "GeneralFunctions.hh"
#include "TTCN3Module.hh"
#include "TTCN3ModuleInventory.hh"
#include "ComplexType.hh"

int FieldType::variantIdCounter = 0;

FieldType::FieldType(ComplexType * a_complexType)
: SimpleType(a_complexType->getParser(), a_complexType->getModule(), c_unknown)
, parent(a_complexType)
, attributes()
, level(1)
, isAnyAttr(false)
, path()
, variantID()
{}

FieldType::FieldType(const FieldType & other)
: SimpleType(other)
, parent(other.parent)
, attributes()
, level(other.level)
, isAnyAttr(other.isAnyAttr)
, path(other.path)
, variantID(other.variantID)
{
  for (List<FieldType*>::iterator attr = other.attributes.begin(); attr; attr = attr->Next) {
    attributes.push_back(new FieldType(*(attr->Data)));
  }
}

FieldType::~FieldType()
{
  for (List<FieldType*>::iterator attr = attributes.begin(); attr; attr = attr->Next) {
    delete(attr->Data);
  }
}

void FieldType::setTypeOfField(const Mstring& in)
{
  type.upload(in);
}

void FieldType::setNameOfField(const Mstring& in, bool add_to_path_for_later_use, bool from_ref_attribute)
{
  if (from_ref_attribute)
    name.convertedValue = in; // name.originalValueWoPrefix not set
  else
    name.upload(in);

  variantID = generateVariantId();
  path = parent->getActualPath() + variantID + ".";
  if (add_to_path_for_later_use) parent->addToActualPath(variantID + ".");
}

void FieldType::setToAnyAttribute()
{
	isAnyAttr = true;
}

Mstring FieldType::generateVariantId()
{
  expstring_t tmp_string = memptystr();
  Mstring name1(parent->getName().convertedValue.c_str());
  Mstring name2(name.convertedValue.c_str());
  for (size_t i = 0; i != name1.size(); ++i) {
    if (name1[i] == '.') name1[i] = '_';
  }
  for (size_t i = 0; i != name2.size(); ++i) {
    if (name2[i] == '.') name2[i] = '_';
  }
  tmp_string = mputprintf(tmp_string,
    "##%s%d%s#",
    name1.c_str(),
    variantIdCounter++,
    name2.c_str());

  Mstring retval(tmp_string);
  Free(tmp_string);
  return retval;
}

void FieldType::applyRefAttribute(const Mstring& ref_value)
{
  if (!ref_value.empty()) {
    setNameOfField(ref_value, false, true);
    setTypeOfField(ref_value);
    setReference(ref_value);
  }
}

void FieldType::applyUseAttribute(UseValue use_value, unsigned long long & minOcc, unsigned long long & maxOcc)
{
  switch (use_value)
  {
  case optional:
    minOcc = 0;
    maxOcc = 1;
    break;
  case required:
    minOcc = 1;
    maxOcc = 1;
    break;
  case prohibited:
    minOcc = 0;
    maxOcc = 0;
    break;
  }
}

void FieldType::applyNamespaceAttribute(VariantMode varLabel, const Mstring& ns_list)
{
  List<Mstring> namespaces;
  if (!ns_list.empty()) {
    expstring_t valueToSplitIntoTokens = mcopystr(ns_list.c_str());
    char * token;
    token = strtok (valueToSplitIntoTokens," ");
    while (token != NULL)
    {
      namespaces.push_back(Mstring(token));
      token = strtok (NULL, " ");
    }
    Free(valueToSplitIntoTokens);
  }

  Mstring any_ns;
  bool first = true;
  // Note: libxml2 will verify the namespace list according to the schema
  // of XML Schema. It is either ##any, ##other, ##local, ##targetNamespace,
  // or a list of (namespace reference | ##local | ##targetNamespace).
  for (List<Mstring>::iterator ns = namespaces.begin(); ns; ns = ns->Next)
  {
    static const Mstring xxany("##any"), xxother("##other"), xxlocal("##local"),
      xxtargetNamespace("##targetNamespace");
    if (!first) any_ns += ',';

    if (ns->Data == xxany) {} // this must be the only element, nothing to add
    else if (ns->Data == xxother) { // this must be the only element
      any_ns += " except unqualified";
      if (module->getTargetNamespace() != "NoTargetNamespace") {
        any_ns += ", \'";
        any_ns += parent->getModule()->getTargetNamespace();
        any_ns += '\'';
      }
    }
    // The three cases below can happen multiple times
    else {
      if (first) any_ns += " from ";
      // else a comma was already added
      if (ns->Data == xxtargetNamespace) {
        any_ns += '\'';
        any_ns += parent->getModule()->getTargetNamespace();
        any_ns += '\'';
      }
      else if (ns->Data == xxlocal) {
        any_ns += "unqualified";
      }
      else {
        any_ns += '\'';
        any_ns += ns->Data;
        any_ns += '\'';
      }
    }

    first = false;
  }

  addVariant(varLabel, any_ns, true);
}

void FieldType::applyMinMaxOccursAttribute(
  unsigned long long minOccurs_value, unsigned long long maxOccurs_value,
  bool generate_list_postfix)
{
  minOccurs = minOccurs_value;
  maxOccurs = maxOccurs_value;

  if (generate_list_postfix) {
    if (minOccurs == 1 && maxOccurs == 1) {}
    else if (minOccurs == 0 && maxOccurs == 1) {
      // This would be an optional field...
      if (!parent->getEmbedInChoice().empty()
        && parent->getEmbedInChoice().back().strictValid) {
        // except in a choice we can't have optional, so we simulate it
        // with a record length(0..1) of ...
        name.list_extension = true;
      }
    }
    else {
      name.list_extension = true;
    }
  }
}

void FieldType::setLevel(int l)
{
  level = l;
  for (List<FieldType*>::iterator attr = attributes.begin(); attr; attr = attr->Next) {
    attr->Data->level = l;
  }
}

void FieldType::variantIdReplaceInPath(const Mstring& varid, const Mstring& text)
{
  const char * const temp_path = path.c_str();
  const char * temp_path_end = temp_path + path.size();
  const char * found_begin = strstr(temp_path, varid.c_str());
  const char * found_end = found_begin + varid.size();

  expstring_t result = NULL;

  if (found_begin != NULL)
  {
    result = mcopystrn(temp_path, (found_begin - temp_path));
    result = mputstr(result, text.c_str());
    result = mputstrn(result, found_end, (temp_path_end - found_end));
    path = result;
  }

  Free(result);
}

void FieldType::applyReference(const FieldType & other, bool on_attributes)
{
  type.convertedValue = other.type.convertedValue;
  type.originalValueWoPrefix = other.type.convertedValue;

  if (other.minOccurs > minOccurs || other.maxOccurs < maxOccurs) {
    if (!on_attributes) {
      expstring_t temp = memptystr();
      temp = mputprintf(
        temp,
        "The occurrence range (%llu .. %llu) of the element (%s) is not compatible "
        "with the occurrence range (%llu .. %llu) of the referenced element.",
        minOccurs,
        maxOccurs,
        name.originalValueWoPrefix.c_str(),
        other.minOccurs,
        other.maxOccurs);
      printError(module->getSchemaname(), parent->getName().originalValueWoPrefix,
        Mstring(temp));
      Free(temp);
      TTCN3ModuleInventory::getInstance().incrNumErrors();
    }
  }
  else {
    minOccurs = llmax(minOccurs, other.minOccurs);
    maxOccurs = llmin(maxOccurs, other.maxOccurs);
  }

  for (List<Mstring>::iterator var = other.variant_ref.begin(); var; var = var->Next) {
    bool found = false;
    for (List<Mstring>::iterator var1 = variant.begin(); var1; var1 = var1->Next) {
      if (var->Data == var1->Data) {
        found = true;
      }
    }
    if (!found) {
      variant.push_back(var->Data);
      variant_ref.push_back(var->Data);
    }
  }
  //	comment;

  builtInBase = other.getBuiltInBase();

  length.applyReference(other.length);
  pattern.applyReference(other.pattern);
  enumeration.applyReference(other.enumeration);
  whitespace.applyReference(other.whitespace);
  value.applyReference(other.value);

  //	reference_for_other = resolved;

  checkSortAndAddAttributes(other.attributes);

  setLevel(level + other.getLevel() - 1);
}

void FieldType::referenceForST(SimpleType const * const found_ST,
  List<FieldType*> & temp_container)
{
  outside_reference.set_resolved(found_ST);

  if (!in_name_only) {
    type = found_ST->getName();
    if (name.originalValueWoPrefix.empty()) {
      name.convertedValue = found_ST->getName().originalValueWoPrefix;
    }
    builtInBase = found_ST->getBuiltInBase();
    for (List<Mstring>::iterator var = found_ST->getVariantRef().begin(); var; var = var->Next) {
      variant.push_back(var->Data);
    }
    if (found_ST->getModule()->getTargetNamespace() != module->getTargetNamespace() &&
      found_ST->getModule()->getTargetNamespace() != "NoTargetNamespace") {
      addVariant(V_namespaceAs, found_ST->getModule()->getTargetNamespace());
    }
  }
  temp_container.push_back(this);
}

static const Mstring attribute_variant("\"attribute\"");
// The field (which may become a record field or maybe a union alternative)
// refers to a complexType
FieldType* FieldType::referenceForCT(ComplexType & found_CT,
  List<FieldType*> & temp_container, List<ImportedField> & temp_container_imported)
{
  found_CT.referenceResolving();
  outside_reference.set_resolved(&found_CT);
  if (in_name_only) {
    temp_container.push_back(this);
    return 0;
  }

  switch (found_CT.getOrigin())
  {
  case from_element:
  case from_attribute:
  case from_simpleType: {
    bool is_attribute = false;
    for (List<Mstring>::iterator it = variant.begin(); it; it = it->Next) {
      if (it->Data == attribute_variant) {
        is_attribute = true;
        break;
      }
    }

    if (!is_attribute && found_CT.getWithUnion())
    {
      // Import fields (union alternatives) from the referenced type
      for (List<FieldType*>::iterator field = found_CT.getFields().begin(); field; field = field->Next)
      {
        ImportedField fl = {
          new FieldType(*(field->Data)),
          temp_container.size() ? temp_container.back() : NULL
        };
        fl.field->level = level;

        temp_container_imported.push_back(fl);
      }
    }
    else // just refer to the found type
    {
      type.convertedValue = found_CT.getName().convertedValue;
      name.convertedValue = found_CT.getName().convertedValue;
      for (List<Mstring>::iterator var = found_CT.getVariantRef().begin(); var; var = var->Next) {
        variant.push_back(var->Data);
      }
      if (found_CT.getModule()->getTargetNamespace() != module->getTargetNamespace() &&
        found_CT.getModule()->getTargetNamespace() != "NoTargetNamespace") {
        addVariant(V_namespaceAs, found_CT.getModule()->getTargetNamespace());
      }
      //already done: outside_reference.set_resolved(&found_CT);
      temp_container.push_back(this);
    }
    break; }

  case from_complexType: {
    List<FieldType*> import;
    for (List<FieldType*>::iterator field = found_CT.getFields().begin(); field; field = field->Next)
    {
      FieldType *newField = new FieldType(*(field->Data)); // deep copy

      // Hook up the imported field with its new environment
      newField->path      = truncatePathWithOneElement(path);
      newField->variantID = newField->generateVariantId();
      newField->path     += newField->variantID + ".";

      import.push_back(newField);
    }

    import.begin()->Data->checkSortAndAddAttributes(attributes);

    for (List<FieldType*>::iterator attribute = attributes.begin(); attribute; attribute = attribute->Next) {
      delete attribute->Data;
    }
    attributes.clear();

    bool found = false;
    if (parent->getMode() != ComplexType::CT_complextype_mode)
    {
      for (List<FieldType*>::iterator imp = import.begin(), nextField; imp; imp = nextField)
      {
        nextField = imp->Next;

        if (imp->Data->getName().originalValueWoPrefix == name.originalValueWoPrefix) {
          // If an imported field has the same name as ourselves,...
          FieldType* newField = new FieldType(*this);
          newField->applyReference(*(imp->Data));
          if (found_CT.getModule()->getTargetNamespace() != module->getTargetNamespace() &&
            found_CT.getModule()->getTargetNamespace() != "NoTargetNamespace") {
            newField->addVariant(V_namespaceAs, found_CT.getModule()->getTargetNamespace());
          }
          //already done: outside_reference.set_resolved(&found_CT);
          temp_container.push_back(newField); 
          delete imp->Data;
          import.remove(imp); // backwards remove cheap
          found = true;
          break;
        }
      }
    }

    if (!found) {
      visible = isBuiltInType(type.convertedValue);
      FieldType* newField = new FieldType(*this);
      if (found_CT.getModule()->getTargetNamespace() != module->getTargetNamespace() &&
        found_CT.getModule()->getTargetNamespace() != "NoTargetNamespace") {
        newField->addVariant(V_namespaceAs, found_CT.getModule()->getTargetNamespace());
      }
      //already done: outside_reference.set_resolved(&found_CT);
      found_CT.addToNameDepList(newField);
      temp_container.push_back(newField);
    }

    for (List<FieldType*>::iterator imp = import.begin(); imp; imp = imp->Next)
    {
      ImportedField fl = {
        new FieldType(*(imp->Data)),
        temp_container.size() ? temp_container.back() : NULL
      };
      fl.field->setLevel(level + imp->Data->getLevel() - 1);
      if (mode == SimpleType::restrictionMode) fl.field->visible = false;

      temp_container_imported.push_back(fl);
      delete imp->Data;
    }
    return this;
  }

  case from_group:
    type.originalValueWoPrefix.clear();
    type.convertedValue = found_CT.getName().convertedValue;
    name.convertedValue = found_CT.getName().convertedValue;
    for (List<Mstring>::iterator var = found_CT.getVariantRef().begin(); var; var = var->Next) {
      variant.push_back(var->Data);
    }
    if (found_CT.getModule()->getTargetNamespace() != module->getTargetNamespace() &&
      found_CT.getModule()->getTargetNamespace() != "NoTargetNamespace") {
      addVariant(V_namespaceAs, found_CT.getModule()->getTargetNamespace());
    }
    //already done: outside_reference.set_resolved(&found_CT);
    temp_container.push_back(this);
    break;

  case from_attributeGroup:
    for (List<FieldType*>::iterator refAttr = found_CT.getFields().back()->attributes.begin(); refAttr; refAttr = refAttr->Next)
    {
      ImportedField fl = {
        new FieldType(*(refAttr->Data)),
        temp_container.size() ? temp_container.back() : NULL
      };

      if (fl.field->getName().originalValueWoPrefix != "attr") {
        fl.field->addVariant(V_attribute);
      }
      if (found_CT.getModule()->getTargetNamespace() != module->getTargetNamespace() &&
        found_CT.getModule()->getTargetNamespace() != "NoTargetNamespace") {
        fl.field->addVariant(V_namespaceAs, found_CT.getModule()->getTargetNamespace());
      }

      temp_container_imported.push_back(fl);
    }
    return this;

  case from_unknown:
    break;
  }
  return 0;
}

void FieldType::addAttribute(FieldType * attr)
{
  attributes.push_back(attr);
}

void FieldType::sortAttributes()
{
  bool found = false;
  for (List<FieldType*>::iterator attr = attributes.begin(); attr; attr = attr->Next)
  {
    if (attr->Data->getName().convertedValue == "order" || attr->Data->getName().convertedValue == "embed_values") {
      FieldType * temp = attr->Data;
      attr->Data = attributes.begin()->Data;
      attributes.begin()->Data = temp;
      found = true;
      break;
    }
  }
  for (List<FieldType*>::iterator attr = attributes.begin(); attr; attr = attr->Next)
  {
    if (attr == attributes.begin() && found)
      continue;
    for (List<FieldType*>::iterator attr2 = attr->Next; attr2; attr2 = attr2->Next)
    {
      if (attr->Data->getName().convertedValue > attr2->Data->getName().convertedValue) {
        FieldType * temp = attr->Data;
        attr->Data = attr2->Data;
        attr2->Data = temp;
      }
    }
  }

  // AnyAttribute must be after the other attributes in the generated tccn files:
  for (List<FieldType*>::iterator attr = attributes.begin(); attr; attr = attr->Next)
  {
    if (attr->Data->isAnyAttribute())
    {
      FieldType * temp = attr->Data;
      for (List<FieldType*>::iterator attr2 = attr; attr2->Next; attr2 = attr2->Next)
      {
        attr2->Data = attr2->Next->Data;
      }
      attributes.end()->Data = temp;
      break;
    }
  }
}

void FieldType::checkSortAndAddAttributes(const List<FieldType*> & referencedAttributes)
{
  const List<FieldType*> origAttributes(attributes); // copy the pointers
  attributes.clear();

  // Creates a modifiable list with copies of the elements.
  // Note that the elements are pointers; they are aliased!
  List<FieldType*> refAttributes(referencedAttributes);

  for (List<FieldType*>::iterator origAttr = origAttributes.begin(), nextAttr; origAttr; origAttr = nextAttr)
  {
    nextAttr = origAttr->Next;

    bool found = false;
    for (List<FieldType*>::iterator refAttr = refAttributes.begin(); refAttr; refAttr = refAttr->Next)
    {
      if (origAttr->Data->getName().convertedValue == refAttr->Data->getName().convertedValue) {
        origAttr->Data->applyReference(*(refAttr->Data), true);
        attributes.push_back(origAttr->Data);
        refAttributes.remove(refAttr); // backwards remove cheap
        found = true;
        break;
      }
    }
    if (!found) {
      attributes.push_back(origAttr->Data);
    }
  }

  for (List<FieldType*>::iterator refAttr = refAttributes.begin(); refAttr; refAttr = refAttr->Next)
  {
    FieldType * newAttribute = new FieldType(*(refAttr->Data));
    newAttribute->path = truncatePathWithOneElement(path);
    newAttribute->variantID = newAttribute->generateVariantId();
    newAttribute->path += newAttribute->variantID + ".";
    attributes.push_back(newAttribute);
  }

  sortAttributes();
}

void FieldType::dump(unsigned int depth) const
{
  fprintf(stderr, "%*s %sField '%s' -> '%s' at %p\n",    depth * 2, "", isVisible() ? "" : "(hidden)",
    name.originalValueWoPrefix.c_str(), name.convertedValue.c_str(), (const void*)this);
  fprintf(stderr, "%*s type %s, level %d\n", (depth+1) * 2, "", type.convertedValue.c_str(), level);
  fprintf(stderr, "%*s (%llu .. %llu)\n"   , (depth+1) * 2, "", minOccurs, maxOccurs);
  fprintf(stderr, "%*s %d attributes\n"    , (depth+1) * 2, "", (int)attributes.size());

  for (List<FieldType*>::iterator attr = attributes.begin(); attr; attr = attr->Next) {
    attr->Data->dump(depth+2);
  }
  fprintf(stderr, "%*s %d variants: ", (depth+1) * 2, "", (int)variant.size());
  for (List<Mstring>::iterator var = variant.begin(); var; var = var->Next) {
    fprintf(stderr, "%s, ", var->Data.c_str());
  }
  fprintf(stderr, "\n%*s path =/%s/"  , (depth+1) * 2, "", path.c_str());
  fprintf(stderr, "\n%*s varid=|%s|\n", (depth+1) * 2, "", variantID.c_str());
}
