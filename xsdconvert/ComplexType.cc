///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "ComplexType.hh"

#include "GeneralFunctions.hh"
#include "XMLParser.hh"
#include "TTCN3Module.hh"
#include "TTCN3ModuleInventory.hh"
#include "SimpleType.hh"
#include "FieldType.hh"

#include <assert.h>

ComplexType::ComplexType(XMLParser * a_parser, TTCN3Module * a_module, ConstructType a_construct)
: RootType(a_parser, a_module, a_construct)
, fields()
, fields_final()
, actualLevel(1)
, actualPath()
, ctmode()
, fieldGenInfo()
, recGenInfo()
, attributeBases()
, embed_inSequence()
, embed_inChoice()
, embed_inAll()
, with_union(false)
, resolved(No)
{
  initialSettings();
  ctmode.push_back(CT_undefined_mode);
}

ComplexType::ComplexType(const ComplexType & other)
: RootType(other)
, fields()
, fields_final()
, actualLevel(other.actualLevel)
, actualPath(other.actualPath)
, ctmode(other.ctmode)
, fieldGenInfo(other.fieldGenInfo)
, recGenInfo(other.recGenInfo)
, attributeBases(other.attributeBases)
, embed_inSequence(other.embed_inSequence)
, embed_inChoice(other.embed_inChoice)
, embed_inAll(other.embed_inAll)
, with_union(other.with_union)
, resolved(other.resolved)
{
  for (List<FieldType*>::iterator field = other.fields.begin(); field; field = field->Next) {
    fields.push_back(new FieldType(*(field->Data)));
  }
  for (List<FieldType*>::iterator field = other.fields_final.begin(); field; field = field->Next) {
    fields_final.push_back(new FieldType(*(field->Data)));
  }
}

ComplexType::ComplexType(const SimpleType & other, CT_fromST c)
: RootType(other)
, fields()
, fields_final()
, actualLevel(1)
, actualPath()
, ctmode()
, fieldGenInfo()
, recGenInfo()
, attributeBases()
, embed_inSequence()
, embed_inChoice()
, embed_inAll()
, with_union(false)
, resolved(No)
{
  initialSettings();

  module->replaceLastMainType(this);
  module->setActualXsdConstruct(c_complexType);
  construct = c_complexType;
  ctmode.push_back(CT_simpletype_mode);

  switch (c)
  {
  case fromTagUnion:
    type.upload(Mstring("union"));
    with_union = true;
    break;
  case fromTagNillable:
    addVariant(V_useNil);
    type.upload(Mstring("record"));
    break;
  case fromTagComplexType:
    type.upload(Mstring("record"));
    break;
  }
}

ComplexType::~ComplexType()
{
  for (List<FieldType*>::iterator field = fields.begin(); field; field = field->Next) {
    delete(field->Data);
  }
}

void ComplexType::initialSettings()
{
  FieldType * invisibleField = new FieldType(this);
  fields.push_back(invisibleField);

  invisibleField->setNameOfField(Mstring("onlyForStoringAttributes"), false);
  invisibleField->setInvisible();

  attributeBases.push_back(AttrBaseType(invisibleField));
  recGenInfo.push_back(GenerationType(NULL, 2));
}

void ComplexType::loadWithValues()
{
  const XMLParser::TagAttributes & atts = parser->getActualTagAttributes();

  switch (parser->getActualTagName())
  {
  case XMLParser::n_sequence:
    if (!embed_inChoice.empty() && embed_inChoice.back().valid) {
      generateRecord(atts.minOccurs, atts.maxOccurs);
      embed_inChoice.back().strictValid = false;
    }
    else if (atts.minOccurs != 1 || atts.maxOccurs != 1) {
      generateRecord(atts.minOccurs, atts.maxOccurs);
    }

    embed_inSequence.push_back(EmbeddedType(parser->getActualDepth(), atts.minOccurs, atts.maxOccurs));
    break;

  case XMLParser::n_choice: {
    if (module->getActualXsdConstruct() == c_group) {
      type.upload(Mstring("union"));
      break;
    }
    generateUnion(atts.minOccurs, atts.maxOccurs);

    embed_inChoice.push_back(EmbeddedType(parser->getActualDepth()));
    break; }

  case XMLParser::n_all: {
    FieldType * new_attribute = generateAttribute();
    new_attribute->setTypeOfField(Mstring("enumerated"));
    new_attribute->setNameOfField(Mstring("order"), false);
    new_attribute->setBuiltInBase(Mstring("string"));
    new_attribute->getEnumeration().modified = true;
    new_attribute->applyMinMaxOccursAttribute(0, ULLONG_MAX);

    if (recGenInfo.size() == 1) {
      addVariant(V_useOrder); // use the default false - from TR HL10354
    }
    else {
      recGenInfo.back().field->addVariant(V_useOrder); // use the default false - from TR HL10354
    }

    embed_inAll.push_back(EmbeddedType(parser->getActualDepth(), atts.minOccurs, atts.maxOccurs, new_attribute));
    break; }

  case XMLParser::n_restriction:
  case XMLParser::n_extension:
    if (atts.base.getValueWithoutPrefix(':') == name.convertedValue) // TODO recusivity
    {
      /*			fields_for_restriction.back()->setTypeOfField(name.value + "_anonymus_extension");
			ComplexType * new_CT = new ComplexType(*this);
			module->addMainType(new_CT);
			new_CT->setNameOfMainType(name.value + "_anonymus_extension", doc->getSchemaname(), doc->getTargetNamespace());
       */		}
    else if (ctmode.back() == CT_simpletype_mode) {
      if (!fieldGenInfo.empty()) {
        fieldGenInfo.back().field->setTypeOfField(atts.base);
        fieldGenInfo.back().field->setReference(atts.base);
        if (parser->getActualTagName() == XMLParser::n_restriction)
          fieldGenInfo.back().field->setMode(SimpleType::restrictionMode);
        else
          fieldGenInfo.back().field->setMode(SimpleType::extensionMode);
        attributeBases.push_back(AttrBaseType(fieldGenInfo.back().field, parser->getActualDepth()));
      }
    }
    else if (ctmode.back() == CT_complextype_mode){
      FieldType * new_field = generateField();
      new_field->setTypeOfField(atts.base);
      new_field->setNameOfField(Mstring("base"), false);
      new_field->setReference(atts.base);
      if (parser->getActualTagName() == XMLParser::n_restriction)
        fieldGenInfo.back().field->setMode(SimpleType::restrictionMode);
      else
        fieldGenInfo.back().field->setMode(SimpleType::extensionMode);
      new_field->addVariant(V_untagged);
      attributeBases.push_back(AttrBaseType(new_field, parser->getActualDepth()));
      if (atts.base.getValueWithoutPrefix(':') == "anyType") new_field->setInvisible();
    }
    break;

  case XMLParser::n_element: {
    if (atts.nillable)
    {
      FieldType * new_field = generateField();
      if (ctmode.back() == CT_simpletype_mode) {
        if (atts.type.empty()) {
          new_field->setTypeOfField(Mstring("record"));
          new_field->setNameOfField(Mstring("content"), true);
          actualLevel++;
          recGenInfo.push_back(GenerationType(new_field, parser->getActualDepth() + 2));
        }
        else {
          new_field->setTypeOfField(atts.type);
          new_field->setNameOfField(Mstring("content"), false);
          new_field->setReference(atts.type, true);
        }
        new_field->applyMinMaxOccursAttribute(0, 1);
      }
      else {
        new_field->setTypeOfField(Mstring("record"));
        new_field->setNameOfField(atts.name, true);
        new_field->addVariant(V_useNil, empty_string, true);
        new_field->applyMinMaxOccursAttribute(atts.minOccurs, atts.maxOccurs, true);
        actualLevel++;
        recGenInfo.push_back(GenerationType(new_field, parser->getActualDepth()));
        attributeBases.push_back(AttrBaseType(new_field, parser->getActualDepth()));

        FieldType * new_field2 = generateField();
        if (atts.type.empty()) {
          new_field2->setTypeOfField(Mstring("record"));
          new_field2->setNameOfField(Mstring("content"), true);
          actualLevel++;
          recGenInfo.push_back(GenerationType(new_field2, parser->getActualDepth() + 2));
        }
        else {
          new_field2->setTypeOfField(atts.type);
          new_field2->setNameOfField(Mstring("content"), false);
          new_field2->setReference(atts.type, true);
        }
        new_field2->applyMinMaxOccursAttribute(0, 1);
     }
     ctmode.push_back(CT_simpletype_mode);
     return;
    }

    FieldType * new_field = generateField();
    unsigned long long int temp_minOccurs = atts.minOccurs;
    unsigned long long int temp_maxOccurs = atts.maxOccurs;

    if (!embed_inAll.empty() && embed_inAll.back().valid) {
      assert((embed_inAll.back().minOccurs | 1ULL) == 1ULL); // 0 or 1
      temp_minOccurs = llmin(atts.minOccurs, embed_inAll.back().minOccurs);
      assert(embed_inAll.back().maxOccurs == 1ULL); // which makes the following somewhat redundant
      temp_maxOccurs = llmax(atts.maxOccurs, embed_inAll.back().maxOccurs);

      if (embed_inAll.back().depth + 1 == parser->getActualDepth()) {
        // we are inside an <xs:all>
        if ((temp_minOccurs | 1ULL) != 1ULL) {
          printError(getModule()->getSchemaname(), name.convertedValue,
            Mstring("Inside <all>, minOccurs must be 0 or 1"));
          TTCN3ModuleInventory::incrNumErrors();
        }

        if (temp_maxOccurs != 1ULL) {
          printError(getModule()->getSchemaname(), name.convertedValue,
            Mstring("Inside <all>, maxOccurs must be 1"));
          TTCN3ModuleInventory::incrNumErrors();
        }
      }

      Mstring res, var;
      XSDName2TTCN3Name(atts.name, embed_inAll.back().order_attribute->getEnumeration().items_string,
        enum_id_name, res, var);
      embed_inAll.back().valid = false;
      // We now throw away the result of the name transformation above.
      // The only lasting effect is the addition to the enumeration items.
    }
    if (!embed_inSequence.empty() && embed_inSequence.back().valid) {
      if (embed_inSequence.back().maxOccurs == 0) {
        temp_minOccurs = 0;
        temp_maxOccurs = 0;
      }
    }

    if (atts.ref.empty()) {
      new_field->setReference(atts.type, true);
    }
    else {
      new_field->applyRefAttribute(atts.ref);
    }
    new_field->setTypeOfField(atts.type);
    new_field->setNameOfField(atts.name, false);
    new_field->applyDefaultAttribute(atts.default_);
    new_field->applyFixedAttribute(atts.fixed);
    new_field->applyMinMaxOccursAttribute(temp_minOccurs, temp_maxOccurs, true);
    attributeBases.push_back(AttrBaseType(new_field, parser->getActualDepth()));

    ctmode.push_back(CT_simpletype_mode);
    break; }

  case XMLParser::n_attribute: {
    FieldType * new_attribute = generateAttribute();
    if (module->getActualXsdConstruct() != c_attributeGroup) {
      new_attribute->addVariant(V_attribute);
    }
    unsigned long long int temp_minOccurs = atts.minOccurs;
    unsigned long long int temp_maxOccurs = atts.maxOccurs;
    if (atts.ref.empty()) {
      new_attribute->setReference(atts.type, true);
    }
    else {
      new_attribute->applyRefAttribute(atts.ref);
    }
    new_attribute->setTypeOfField(atts.type);
    new_attribute->setNameOfField(atts.name, false);
    new_attribute->applyDefaultAttribute(atts.default_);
    new_attribute->applyFixedAttribute(atts.fixed);
    new_attribute->applyUseAttribute(atts.use, temp_minOccurs, temp_maxOccurs);
    new_attribute->applyMinMaxOccursAttribute(temp_minOccurs, temp_maxOccurs, true);
    ctmode.push_back(CT_simpletype_mode);
    break; }

  case XMLParser::n_any: {
    FieldType * new_field = generateField();
    new_field->setTypeOfField(Mstring("xsd:string"));
    new_field->setNameOfField(Mstring("elem"), false);
    new_field->applyMinMaxOccursAttribute(atts.minOccurs, atts.maxOccurs, true);
    new_field->applyNamespaceAttribute(V_anyElement, atts.namespace_);
    break; }

  case XMLParser::n_anyAttribute: {
    FieldType * new_field = generateAttribute();
    new_field->setTypeOfField(Mstring("xsd:string"));
    new_field->setNameOfField(Mstring("attr"), false);
    new_field->setToAnyAttribute();
    new_field->applyMinMaxOccursAttribute(0, ULLONG_MAX);
    new_field->applyNamespaceAttribute(V_anyAttributes, atts.namespace_);
    break; }

  case XMLParser::n_attributeGroup:
    name.upload(atts.name);
    if (!atts.ref.empty()) {
      FieldType * new_attribute = generateAttribute();
      new_attribute->applyRefAttribute(atts.ref);
    }
    ctmode.push_back(CT_complextype_mode);
    break;

  case XMLParser::n_group: {
    if (parser->getActualDepth() == 1) {
      name.upload(atts.name);
      break;
    }

    FieldType * new_field = generateField();
    new_field->applyRefAttribute(atts.ref);
    if (atts.minOccurs != 1 || atts.maxOccurs != 1) {
      new_field->applyMinMaxOccursAttribute(atts.minOccurs, atts.maxOccurs, true);
    }
    ctmode.push_back(CT_complextype_mode);
    break; }

  case XMLParser::n_union: {
    if (fieldGenInfo.empty()) {
      type.upload(Mstring("union"));
      addVariant(V_useUnion);
    }
    else {
      fieldGenInfo.back().field->setTypeValue(Mstring("union"));
      fieldGenInfo.back().field->addVariant(V_useUnion);
      fieldGenInfo.push_back(GenerationType(fieldGenInfo.back().field, parser->getActualDepth()));
      recGenInfo.push_back(GenerationType(fieldGenInfo.back().field, parser->getActualDepth()));
      actualLevel++;
      addToActualPath(fieldGenInfo.back().field->getVariantId() + ".");
    }
    with_union = true;
    List<Mstring> types;
    if (!atts.memberTypes.empty()) {
      expstring_t valueToSplitIntoTokens = mcopystr(atts.memberTypes.c_str());
      char * token;
      token = strtok (valueToSplitIntoTokens," ");
      while (token != NULL)
      {
        types.push_back(Mstring(token));
        token = strtok (NULL, " ");
      }
      Free(valueToSplitIntoTokens);
      for (List<Mstring>::iterator memberType = types.begin(); memberType; memberType = memberType->Next)
      {
        Mstring tmp_name = memberType->Data.getValueWithoutPrefix(':');
        FieldType * new_field = generateField();
        new_field->setNameOfField(tmp_name, false);
        new_field->setTypeOfField(memberType->Data);
        new_field->setReference(memberType->Data);
      }
    }
    break; }

  case XMLParser::n_simpleType:
  case XMLParser::n_simpleContent: {
    Mstring base;
    if (with_union)
    {
      expstring_t tmp_string = NULL;
      if (recGenInfo.back().max_alt == 0) {
        tmp_string = mcopystr("alt_");
      }
      else
      {
        tmp_string = mprintf("alt_%d", recGenInfo.back().max_alt);
      }
      base = tmp_string; // NULL is OK
      Free(tmp_string);
      recGenInfo.back().max_alt++;
    }
    else {
      base = "base";
    }
    if (parser->getParentTagName() != XMLParser::n_element &&
      parser->getParentTagName() != XMLParser::n_attribute)
    {
      FieldType * new_field = generateField();
      new_field->setNameOfField(base, false);
      if (new_field->getName().convertedValue == "base")
      {
        new_field->addVariant(V_untagged); // name = base
      }
      else
      {
        new_field->addVariant(V_nameAs, empty_string, true); // name = alt_x
      }
      if (!embed_inChoice.empty()) embed_inChoice.back().valid = false;
    }
    if (!embed_inChoice.empty()) embed_inChoice.back().strictValid = false;

    ctmode.push_back(CT_simpletype_mode);
    break; }

  case XMLParser::n_complexType:
    name.upload(atts.name);
    // fall through
  case XMLParser::n_complexContent:
    if (!fieldGenInfo.empty())
    {
      if (fieldGenInfo.back().field->getType().convertedValue != "record") {
        fieldGenInfo.back().field->setTypeOfField(Mstring("record"));
        fieldGenInfo.back().field->setNameOfField(empty_string, true);
        actualLevel++;
        recGenInfo.push_back(GenerationType(fieldGenInfo.back().field, parser->getActualDepth()));
      }
    }
    if (atts.mixed)
    {
      FieldType * new_attribute = generateAttribute();
      new_attribute->setTypeOfField(Mstring("string"));
      new_attribute->setNameOfField(Mstring("embed_values"), false);
      new_attribute->applyMinMaxOccursAttribute(0, ULLONG_MAX);
      if (attributeBases.size() == 1) {
        addVariant(V_embedValues);
      }
      else {
        attributeBases.back().base->addVariant(V_embedValues); // TR HL14121
      }
      recGenInfo.back().embed_values_attribute = new_attribute;
    }
    ctmode.push_back(CT_complextype_mode);
    if (!embed_inChoice.empty()) {
      embed_inChoice.back().valid = false;
      embed_inChoice.back().strictValid = false;
    }
    break;

  case XMLParser::n_length:
  case XMLParser::n_minLength:
  case XMLParser::n_maxLength:
  case XMLParser::n_pattern:
  case XMLParser::n_enumeration:
  case XMLParser::n_whiteSpace:
  case XMLParser::n_minInclusive:
  case XMLParser::n_maxInclusive:
  case XMLParser::n_minExclusive:
  case XMLParser::n_maxExclusive:
  case XMLParser::n_totalDigits:
    if (!fieldGenInfo.empty())
      fieldGenInfo.back().field->loadWithValues();
    break;

  case XMLParser::n_annotation:
  case XMLParser::n_documentation:
    break;

  case XMLParser::n_label:
    addComment(Mstring("LABEL:"));
    break;

  case XMLParser::n_definition:
    addComment(Mstring("DEFINITION:"));
    break;

  default:
    break;
  }
}

// called from endelementHandler
void ComplexType::modifyValues()
{
  if (!fieldGenInfo.empty() && parser->getActualDepth() == fieldGenInfo.back().depth) {
    fieldGenInfo.pop_back();
  }
  if (!recGenInfo.empty() && parser->getActualDepth() == recGenInfo.back().depth && recGenInfo.size() != 1) {
    actualLevel--;

    actualPath = truncatePathWithOneElement(actualPath);

    if (recGenInfo.size() > 1) {
      recGenInfo.pop_back();
    }
  }
  if (!embed_inSequence.empty() && parser->getActualDepth() == embed_inSequence.back().depth + 1) {
    embed_inSequence.back().valid = true;
  }
  if (!embed_inSequence.empty() && parser->getActualDepth() == embed_inSequence.back().depth) {
    embed_inSequence.pop_back();
  }
  if (!embed_inChoice.empty() && parser->getActualDepth() == embed_inChoice.back().depth + 1) {
    embed_inChoice.back().valid = true;
    embed_inChoice.back().strictValid = true;
  }
  if (!embed_inChoice.empty() && parser->getActualDepth() == embed_inChoice.back().depth) {
    embed_inChoice.pop_back();
  }
  if (!embed_inAll.empty() && parser->getActualDepth() == embed_inAll.back().depth + 1) {
    embed_inAll.back().valid = true;
  }
  if (!embed_inAll.empty() && parser->getActualDepth() == embed_inAll.back().depth) {
    embed_inAll.pop_back();
  }
  // why is there no "valid = true" for attributeBases?
  if (!attributeBases.empty() && parser->getActualDepth() == attributeBases.back().depth) {
    attributeBases.pop_back();
  }

  switch (parser->getActualTagName())
  {
  case XMLParser::n_simpleType:
  case XMLParser::n_simpleContent:
  case XMLParser::n_complexType:
  case XMLParser::n_complexContent:
  case XMLParser::n_element:
  case XMLParser::n_attribute:
  case XMLParser::n_group:
  case XMLParser::n_attributeGroup:
    if (ctmode.size() == 3 && ctmode.front() == CT_undefined_mode
      && ctmode.begin()->Next->Data == CT_complextype_mode) {
      // the first two are always: 1."undefined", 2."complex"
      ctmode.front() = ctmode.back();
    }
    ctmode.pop_back();
    break;
  default:
    break;
  }
  return;
}

FieldType * ComplexType::generateField()
{
  FieldType * new_field = new FieldType(this);
  fields.push_back(new_field);

  new_field->setLevel(actualLevel);
  new_field->setElementFormAs(module->getElementFormDefault());

  fieldGenInfo.push_back(GenerationType(new_field, parser->getActualDepth()));

  return new_field;
}

FieldType * ComplexType::generateAttribute()
{
  FieldType * theBase = attributeBases.back().base;
  FieldType * new_attribute = new FieldType(this);
  theBase->addAttribute(new_attribute);

  new_attribute->setLevel(theBase->getLevel());
  if (theBase->getType().convertedValue == "record" || theBase->getType().convertedValue == "union") {
    new_attribute->incrLevel();
  }
  new_attribute->setAttributeFormAs(module->getAttributeFormDefault());

  fieldGenInfo.push_back(GenerationType(new_attribute, parser->getActualDepth()));

  return new_attribute;
}

FieldType * ComplexType::generateRecord(unsigned long long int a_minOccurs, unsigned long long int a_maxOccurs)
{
  FieldType * new_record = new FieldType(this);
  fields.push_back(new_record);

  new_record->setTypeOfField(Mstring("record"));
  new_record->setNameOfField(Mstring("sequence"), true);
  new_record->addVariant(V_untagged, empty_string, true);
  new_record->setLevel(actualLevel++);
  new_record->setElementFormAs(module->getElementFormDefault());
  new_record->applyMinMaxOccursAttribute(a_minOccurs, a_maxOccurs, true);

  fieldGenInfo.push_back(GenerationType(new_record, parser->getActualDepth()));
  recGenInfo.push_back(GenerationType(new_record, parser->getActualDepth()));

  return new_record;
}

FieldType * ComplexType::generateUnion(unsigned long long int a_minOccurs, unsigned long long int a_maxOccurs)
{
  FieldType * new_union = new FieldType(this);

  fields.push_back(new_union);

  new_union->setTypeOfField(Mstring("union"));
  new_union->setNameOfField(Mstring("choice"), true);
  new_union->addVariant(V_untagged, empty_string, true);
  new_union->setLevel(actualLevel++);
  new_union->setElementFormAs(module->getElementFormDefault());
  new_union->applyMinMaxOccursAttribute(a_minOccurs, a_maxOccurs, true);

  fieldGenInfo.push_back(GenerationType(new_union, parser->getActualDepth()));
  recGenInfo.push_back(GenerationType(new_union, parser->getActualDepth()));

  return new_union;
}

bool ComplexType::hasUnresolvedReference()
{
  for (List<FieldType*>::iterator field = fields.begin(); field; field = field->Next)
  {
    if (!field->Data->getReference().empty() && !field->Data->getReference().is_resolved()) return true;

    for (List<FieldType*>::iterator attr = field->Data->getAttributes().begin(); attr; attr = attr->Next)
    {
      if (!attr->Data->getReference().empty() && !attr->Data->getReference().is_resolved()) return true;
    }
  }
  return false;
}

void ComplexType::referenceResolving()
{
  if (resolved != No) return; // nothing to do
  resolved = InProgress;
  for (List<FieldType*>::iterator field = fields.begin(); field; field = field->Next) {
    reference_resolving_funtion(field->Data->getAttributes());
  }
  reference_resolving_funtion(fields);
  for (List<FieldType*>::iterator field = fields.begin(); field; field = field->Next) {
    field->Data->sortAttributes();
  }
  resolved = Yes;
}

void ComplexType::reference_resolving_funtion(List<FieldType*> & container)
{
  if (container.empty()) return; // take a quick exit

  List<FieldType*> temp_container;
  List<ImportedField> temp_container_imported;

  for (List<FieldType*>::iterator field = container.begin(), nextField; field; field = nextField)
  {
    nextField = field->Next;
    if (field->Data->getReference().empty() || field->Data->getReference().is_resolved()) {
      temp_container.push_back(field->Data);
      continue;
    }

    SimpleType  * found_ST = static_cast<SimpleType*>(
      TTCN3ModuleInventory::getInstance().lookup(field->Data, want_ST));
    ComplexType * found_CT = static_cast<ComplexType*>(
      TTCN3ModuleInventory::getInstance().lookup(field->Data, want_CT));
    // It _is_ possible to find both, in which case the simpleType is selected
    // for no apparent reason.

    if (found_ST != NULL) {
      field->Data->referenceForST(found_ST, temp_container); // does not affect temp_container_imported
      found_ST->addToNameDepList(field->Data);
    }
    else if (found_CT != NULL) {
      // HACK the function returns 0 if the field has been added to one of the containers
      //        returns the field if a copy has been added to to one of the containers
      //        in this case the field can be safely deleted
      FieldType* ft = field->Data->referenceForCT(*found_CT, temp_container, temp_container_imported);
      if (!ft) {
        found_CT->addToNameDepList(field->Data);
      }
      delete ft;
    }
    else {
      printError(module->getSchemaname(), name.convertedValue,
        "Reference for a non-defined type: " + field->Data->getReference().repr());
      TTCN3ModuleInventory::getInstance().incrNumErrors();

      delete field->Data; // this field will not be restored
    }
  }

  container.clear();

  for (List<FieldType*>::iterator field = temp_container.begin(), nextField; field; field = nextField)
  {
    nextField = field->Next;

    for (List<ImportedField>::iterator field_imp = temp_container_imported.begin(),
      nextField_imp; field_imp; field_imp = nextField_imp)
    {
      nextField_imp = field_imp->Next;

      if (field_imp->Data.field->getType().convertedValue == "record" ||
        field_imp->Data.field->getType().convertedValue == "union")
        continue;

      if (field->Data->getName().convertedValue == field_imp->Data.field->getName().convertedValue)
      {
        field->Data->applyReference(*(field_imp->Data.field));
        if (field->Data->getName().convertedValue == "onlyForStoringAttributes") {
          container.push_back(field->Data);
          temp_container.remove(field); // backwards remove cheap
        }
        delete field_imp->Data.field;
        temp_container_imported.remove(field_imp); // backwards remove cheap
        break;
      }
    }
  }

  // Restore the fields

  List<ImportedField>::iterator it_imp = temp_container_imported.begin();
  if (!container.empty()) {
    // Restore the imported fields which we pretend to belong
    // to "onlyForStoringAttributes" or any other field transferred above.
    const FieldType * const last = container.back();
    while (it_imp != NULL && it_imp->Data.orig == last) {
      container.push_back(it_imp->Data.field);
      it_imp = it_imp->Next;
    }
  }

  for (List<FieldType*>::iterator it = temp_container.begin(); it; it = it->Next) {
    // Restore an original field
    container.push_back(it->Data);
    // Merge in all imported fields which "belong" to the original field
    while (it_imp != NULL && it_imp->Data.orig == it->Data) {
      container.push_back(it_imp->Data.field);
      it_imp = it_imp->Next;
    }
  }

  // Handle leftovers
  for (; it_imp; it_imp = it_imp->Next) {
    container.push_back(it_imp->Data.field);
  }
}

void ComplexType::everything_into_fields_final() // putting everything into 'fields_for_final' container
{
  for (List<FieldType*>::iterator field = fields.begin(); field; field = field->Next)
  {
    if (field->Data->getType().convertedValue == "record" || field->Data->getType().convertedValue == "union")
    {
      fields_final.push_back(field->Data);
      for (List<FieldType*>::iterator attr = field->Data->getAttributes().begin(); attr; attr = attr->Next)
      {
        fields_final.push_back(attr->Data);
      }
    }
    else {
      for (List<FieldType*>::iterator attr = field->Data->getAttributes().begin(); attr; attr = attr->Next)
      {
        fields_final.push_back(attr->Data);
      }
      fields_final.push_back(field->Data);
    }
  }
}

void ComplexType::nameConversion(NameConversionMode conversion_mode, const List<NamespaceType> & ns)
{
  switch (conversion_mode)
  {
  case nameMode:
    nameConversion_names(ns);
    break;
  case typeMode:
    nameConversion_types(ns);
    break;
  case fieldMode:
    nameConversion_fields(ns);
    break;
  }
}

void ComplexType::nameConversion_names(const List<NamespaceType> & )
{
  Mstring res, var(module->getTargetNamespace());
  XSDName2TTCN3Name(name.convertedValue, TTCN3ModuleInventory::getInstance().getTypenames(), type_name, res, var);
  name.convertedValue = res;
  bool found = false;
  for (List<Mstring>::iterator vari = variant.begin(); vari; vari = vari->Next)
  {
    if (vari->Data == "\"untagged\"") {
      found = true;
      break;
    }
  }
  if (!found) {
    addVariant(V_onlyValue, var);
  }
  for (List<SimpleType*>::iterator st = nameDepList.begin(); st; st = st->Next) {
    st->Data->setTypeValue(res); // FIXME: is this still needed ?
  }
}

void ComplexType::nameConversion_types(const List<NamespaceType> & ns)
{
  Mstring prefix, uri, value;

  if (type.convertedValue == "record" ||
    type.convertedValue == "set" ||
    type.convertedValue == "union" ||
    type.convertedValue == "enumerated")
    return;

  prefix = type.convertedValue.getPrefix(':');
  value  = type.convertedValue.getValueWithoutPrefix(':');

  for (List<NamespaceType>::iterator namesp = ns.begin(); namesp; namesp = namesp->Next)
  {
    if (prefix == namesp->Data.prefix) {
      uri = namesp->Data.uri;
      break;
    }
  }

  QualifiedName in(uri, value); // ns uri + original name

  // Check all known types
  QualifiedNames::iterator origTN = TTCN3ModuleInventory::getInstance().getTypenames().begin();
  for ( ; origTN; origTN = origTN->Next)
  {
    if (origTN->Data == in) {
      QualifiedName tmp_name(module->getTargetNamespace(), name.convertedValue);
      if (origTN->Data == tmp_name)
        continue;
      else
        break;
    }
  }

  if (origTN != NULL) {
    setTypeValue(origTN->Data.name);
  }
  else {
    Mstring res, var;
    XSDName2TTCN3Name(value, TTCN3ModuleInventory::getInstance().getTypenames(), type_reference_name, res, var);
    setTypeValue(res);
  }
}

void ComplexType::nameConversion_fields(const List<NamespaceType> & ns)
{
  List< QualifiedNames > used_field_names_in_different_level;

  for (List<FieldType*>::iterator field = fields_final.begin(); field; field = field->Next)
  {
    Mstring prefix = field->Data->getType().convertedValue.getPrefix(':');
    Mstring value = field->Data->getType().convertedValue.getValueWithoutPrefix(':');
    Mstring uri;
    for (List<NamespaceType>::iterator namesp = ns.begin(); namesp; namesp = namesp->Next)
    {
      if (prefix == namesp->Data.prefix) {
        uri = namesp->Data.uri;
        break;
      }
    }

    if (field == fields_final.begin()) {
      QualifiedNames first_set_of_used_field_names;
      used_field_names_in_different_level.push_back(first_set_of_used_field_names);
    }
    else {
      if (field->Data->getLevel() < field->Prev->Data->getLevel()) {
        for (int k = 0; k != field->Prev->Data->getLevel() - field->Data->getLevel(); ++k) {
          used_field_names_in_different_level.pop_back();
        }
      }
      else if (field->Data->getLevel() > field->Prev->Data->getLevel()) {
        QualifiedNames set_of_used_field_names;
        used_field_names_in_different_level.push_back(set_of_used_field_names);
      }
    }

    if (field->Data->getMinOccurs() == 0 && field->Data->getMaxOccurs() == 0)
      continue;
    if (!field->Data->isVisible())
      continue;

    Mstring in, res, var;

    XSDName2TTCN3Name(value, TTCN3ModuleInventory::getInstance().getTypenames(), type_reference_name, res, var);
    if (field->Data->getType().convertedValue != "enumerated") {
      field->Data->setTypeValue(res);
    }
    field->Data->addVariant(V_onlyValue, var);

    if (field->Data->getName().list_extension)
    {
      field->Data->useNameListProperty();

      if (!used_field_names_in_different_level.empty()) {
        XSDName2TTCN3Name(field->Data->getName().convertedValue,
          used_field_names_in_different_level.back(), field_name, res, var);
        field->Data->setNameValue(res);
        if (!field->Data->getName().originalValueWoPrefix.empty() &&
          field->Data->getName().originalValueWoPrefix != "sequence" &&
          field->Data->getName().originalValueWoPrefix != "choice" &&
          field->Data->getName().originalValueWoPrefix != "elem")
        {
          field->Data->addVariant(V_nameAs, field->Data->getName().originalValueWoPrefix);
        }
      }

      bool found_in_variant = false;
      for (List<Mstring>::iterator vari = field->Data->getVariant().begin(); vari; vari = vari->Next)
      {
        if (vari->Data == "untagged") {
          found_in_variant = true;
          break;
        }
      }
      if (!found_in_variant) {
        field->Data->addVariant(V_untagged, empty_string, true);
      }
    }
    else {
      if (!used_field_names_in_different_level.empty()) {
        XSDName2TTCN3Name(field->Data->getName().convertedValue,
          used_field_names_in_different_level.back(), field_name, res, var);
        field->Data->setNameValue(res);
        field->Data->addVariant(V_onlyValue, var);
      }
    }

    for (List<FieldType*>::iterator inner = fields_final.begin(); inner; inner = inner->Next)
    {
      if (field->Data->getName().list_extension) {
        inner->Data->variantIdReplaceInPath(field->Data->getVariantId(),
          field->Data->getName().convertedValue + "[-]");
      }
      else {
        inner->Data->variantIdReplaceInPath(field->Data->getVariantId(),
          field->Data->getName().convertedValue);
      }
    }
  }
}

void ComplexType::finalModification()
{
  for (List<FieldType*>::iterator field = fields_final.begin(), nextField; field; field = nextField)
  {
    nextField = field->Next;
    if (!field->Data->isVisible()) {
      fields_final.remove(field); // backwards remove cheap
      continue;
    }
    if (field->Data->getMinOccurs() == 0 && field->Data->getMaxOccurs() == 0) {
      fields_final.remove(field); // backwards remove cheap
      continue;
    }

    field->Data->finalModification();

    if (module->getElementFormDefault() == qualified &&
      field->Data->getElementFormAs() == unqualified)
    {
      field->Data->addVariant(V_formAs, Mstring("unqualified"));
    }
    if (module->getAttributeFormDefault() == qualified &&
      field->Data->getAttributeFormAs() == unqualified)
    {
      field->Data->addVariant(V_formAs, Mstring("unqualified"));
    }
  }
}

void ComplexType::printToFile(FILE * file)
{
  if (!isVisible()) return;
  const bool ct_is_union  = type.convertedValue == "union";

  List<Mstring> rec_names;
  List<FieldType*> choices;

  printComment(file);

  fprintf(file, "type ");
  printMinOccursMaxOccurs(file, false);
  fprintf(file, "%s ", type.convertedValue.c_str());
  if (minOccurs == 1 && maxOccurs == 1) { // if it is not a record of
    fprintf(file, "%s", name.convertedValue.c_str());
  }
  fprintf(file, "\n{\n");

  for (List<FieldType*>::iterator fit = fields_final.begin(); fit; fit = fit->Next)
  {
    FieldType& field = *(fit->Data); // alas, cannot be const
    const bool field_is_record = field.getType().convertedValue == "record";
    const bool field_is_union  = field.getType().convertedValue == "union";
    if (field_is_union) {
      choices.push_back(&field);
    }

    bool last = false;
    if (fit->Next == NULL)
    {
      /* A field of type record or union is normally followed by its subfields
       * at level N+1. Therefore if a record or union is the last field,
       * it is an empty record/union. */
      if (field_is_record)
      {
        indent(file, field.getLevel());
        field.printMinOccursMaxOccurs(file, ct_is_union);
        fprintf(file, "%s {\n", field.getType().convertedValue.c_str());
        indent(file, field.getLevel());
        fprintf(file, "} %s", field.getName().convertedValue.c_str());
        if (field.isOptional()) fprintf(file, " optional");
        break;
      }
      else if (field_is_union)
      {
        indent(file, field.getLevel());
        field.printMinOccursMaxOccurs(file, ct_is_union);
        fprintf(file, "%s {\n", field.getType().convertedValue.c_str());
        indent(file, field.getLevel()+1);
        fprintf(file, "record length(0 .. 1) of enumerated { NULL_ } choice\n");
        indent(file, field.getLevel());
        fprintf(file, "} %s", field.getName().convertedValue.c_str());
        if (field.isOptional()) fprintf(file, " optional");
        break;
      }
      last = true;
    }

    bool empty_allowed = true;

    for (List<FieldType*>::iterator p = fit->Prev; p; p = p->Prev) {
      const int prevlevel = p->Data->getLevel();
      if (prevlevel < field.getLevel()) break; // probably our parent, quit
      if (prevlevel == field.getLevel()) { // same level
        // If the earlier field can be empty, and our parent is a choice,
        // then this field's minOccurs needs to be adjusted to 1
        // so it's not a candidate for the empty list.
        if (p->Data->getMinOccurs() == 0ULL
          && !choices.empty()
          &&  choices.back()->getLevel()+1 == field.getLevel()) {
          empty_allowed = false;
          break;
        }
      }
      // Do not quit the loop if finding a prev. field with minOccurs > 0
      // There may be a field with minOccurs == 0 before it.
    }

    indent(file, field.getLevel());

    field.printMinOccursMaxOccurs(file, ct_is_union, empty_allowed);

    if (field_is_record)
    {
      fprintf(file, "%s {\n", field.getType().convertedValue.c_str());
      if (field.getLevel() >= fit->Next->Data->getLevel()) // the record is empty
      {
        indent(file, field.getLevel());
        fprintf(file, "} %s", field.getName().convertedValue.c_str());
        if (field.isOptional()) fprintf(file, " optional");
      }
      else // the record is not empty
      {
        if (field.isOptional()) {
          rec_names.push_back(field.getName().convertedValue + " optional");
        }
        else {
          rec_names.push_back(field.getName().convertedValue);
        }
      }
    }
    else if (field_is_union)
    {
      fprintf(file, "%s {\n", field.getType().convertedValue.c_str());
      if (field.getLevel() >= fit->Next->Data->getLevel()) // the union is empty
      {
        indent(file, field.getLevel()+1);
        fprintf(file, "record length(0 .. 1) of enumerated { NULL_ } choice\n");
        indent(file, field.getLevel());
        fprintf(file, "} %s", field.getName().convertedValue.c_str());
        if (field.isOptional()) fprintf(file, " optional");
      }
      else // the union is not empty
      {
        if (field.isOptional()) {
          rec_names.push_back(field.getName().convertedValue + " optional");
        }
        else {
          rec_names.push_back(field.getName().convertedValue);
        }
      }

    }
    else
    {
      if (field.getEnumeration().modified)
      {
        if (isFloatType(field.getBuiltInBase())) {
          fprintf(file, "%s (", type.convertedValue.c_str());
          field.getEnumeration().sortFacets();
          field.getEnumeration().printToFile(file);
          fprintf(file, ")");
        }
        else {
          fprintf(file, "enumerated {\n");
          if (!rec_names.empty() && rec_names.back() != "order") {
            field.getEnumeration().sortFacets();
          }
          field.getEnumeration().printToFile(file, field.getLevel());
          fprintf(file, "\n");
          indent(file, field.getLevel());
          fprintf(file, "} ");
        }
      }
      else
      {
        int multiplicity = multi(module, field.getReference(), &field);
        if ((multiplicity > 1) && field.getReference().get_ref()) {
          fprintf(file, "%s.", field.getReference().get_ref()->getModule()->getModulename().c_str());
        }
        fprintf(file, "%s ", field.getType().convertedValue.c_str());
      }
      fprintf(file, "%s", field.getName().convertedValue.c_str());
      field.getValue().printToFile(file);
      field.getLength().printToFile(file);
      if (!ct_is_union && field.isOptional()) fprintf(file, " optional");
    }
    /* *******************************************************
     * FINAL
     * *******************************************************/
    if (last) break;
    /* *******************************************************/
    if (field.getLevel() > fit->Next->Data->getLevel()) {
      int diff = field.getLevel() - fit->Next->Data->getLevel();
      fprintf(file, "\n");
      for (int k = 0; k != diff; ++k)
      {
        indent(file, field.getLevel() - (k+1));
        if (!rec_names.empty()) {
          fprintf(file, "} %s", rec_names.back().c_str());
          rec_names.pop_back();
        }
        if (k + 1 == diff) {
          fprintf(file, ",");
        }
        fprintf(file, "\n");
      }
    }
    else if (field.getLevel() == fit->Next->Data->getLevel()) {
      fprintf(file, ",\n");
    }
  }
  /* *******************************************************/
  for (size_t j = 0; j < rec_names.size(); ) {
    fprintf(file, "\n");
    indent(file, rec_names.size());
    fprintf(file, "} %s", rec_names.back().c_str());
    rec_names.pop_back();
  }
  /* *******************************************************/
  fprintf(file, "\n}");
  if (minOccurs != 1 || maxOccurs != 1) { // if it is a record of
    fprintf(file, " %s", name.convertedValue.c_str());
  }

  printVariant(file);

  fprintf(file, ";\n\n\n");
}

void ComplexType::printVariant(FILE * file)
{
  if (e_flag_used) return;

  bool foundAtLeastOneVariant = false;
  if (!variant.empty())
    foundAtLeastOneVariant = true;
  for (List<FieldType*>::iterator field = fields_final.begin(); field; field = field->Next)
  {
    if (!field->Data->getVariant().empty()) {
      foundAtLeastOneVariant = true;
      break;
    }
  }
  if (!foundAtLeastOneVariant) return;


  fprintf(file, "\nwith {\n");

  bool useUnionVariantWhenMainTypeIsRecordOf = false;
  for (List<Mstring>::iterator var = variant.end(); var; var = var->Prev)
  {
    if ((minOccurs != 1 || maxOccurs != 1) && (var->Data == "\"useUnion\"")) { // main type is a record of
      useUnionVariantWhenMainTypeIsRecordOf = true;          // TR HL15893
    }
    else {
      fprintf(file, "variant %s;\n", var->Data.c_str());
    }
  }
  if (useUnionVariantWhenMainTypeIsRecordOf) {
    fprintf(file, "variant ([-]) \"useUnion\";\n");
  }

  for (List<FieldType*>::iterator field = fields_final.begin(); field; field = field->Next)
  {
    if (field->Data->getVariant().empty()) continue;

    expstring_t localPath = mcopystr(field->Data->getPath().c_str());
    localPath = mtruncstr(localPath, mstrlen(localPath)-1);
    size_t loclen = mstrlen(localPath);
    if (loclen >= 3) {
      char * pointer = localPath + loclen - 3;
      if (pointer[0]=='[' && pointer[1]=='-' && pointer[2]==']') {
        localPath = mtruncstr(localPath, loclen - 3);
      }
    }
    if (minOccurs != 1 || maxOccurs != 1) { // main type is a record of
      expstring_t temp = mcopystr(localPath);
      Free(localPath);
      localPath = mprintf("[-].%s", temp);
      Free(temp);
    }

    bool already_used = false;

    for (List<Mstring>::iterator var2 = field->Data->getVariant().end(); var2; var2 = var2->Prev)
    {
      if (var2->Data == "\"untagged\"" && !already_used)
      {
        fprintf(file, "variant (%s) %s;\n", localPath, var2->Data.c_str());
        already_used = true;
      }
      else
      {
        if ((field->Data->getMinOccurs() != 1 || field->Data->getMaxOccurs() != 1) &&
          field->Data->getName().list_extension) {
          fprintf(file, "variant (%s[-]) %s;\n", localPath, var2->Data.c_str());
        }
        else {
          fprintf(file, "variant (%s) %s;\n", localPath, var2->Data.c_str());
        }
      }
    }
    Free(localPath);
  }
  fprintf(file, "}");
}

void ComplexType::dump(unsigned int depth) const
{
  fprintf(stderr, "%*s %sComplexType at %p\n", depth * 2, "", isVisible() ? "" : "(hidden)", (const void*)this);
  fprintf(stderr, "%*s name='%s' -> '%s', %d fields\n", depth * 2, "",
    name.originalValueWoPrefix.c_str(), name.convertedValue.c_str(), (int)fields.size());
  for (List<FieldType*>::iterator field = fields.begin(); field; field = field->Next) {
    field->Data->dump(depth+1);
  }
  fprintf(stderr, "%*s %d final fields\n", depth * 2, "", (int)fields_final.size());
  for (List<FieldType*>::iterator field = fields_final.begin(); field; field = field->Next) {
    field->Data->dump(depth+1);
  }
}
