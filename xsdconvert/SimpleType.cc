///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "SimpleType.hh"

#include "GeneralFunctions.hh"

#include "TTCN3ModuleInventory.hh"
#include "TTCN3Module.hh"
#include "ComplexType.hh"
#include "FieldType.hh"


SimpleType::SimpleType(XMLParser * a_parser, TTCN3Module * a_module, ConstructType a_construct)
: RootType(a_parser, a_module, a_construct)
  , builtInBase()
  , length(this)
  , pattern(this)
  , enumeration(this)
  , whitespace(this)
  , value(this)
  , element_form_as(notset)
  , attribute_form_as(notset)
  , mode(noMode)
  , outside_reference()
  , in_name_only(false)
  {}

SimpleType::SimpleType(const SimpleType& other)
: RootType(other)
  , builtInBase(other.builtInBase)
  , length(other.length)
  , pattern(other.pattern)
  , enumeration(other.enumeration)
  , whitespace(other.whitespace)
  , value(other.value)
  , element_form_as(other.element_form_as)
  , attribute_form_as(other.attribute_form_as)
  , mode(other.mode)
  , outside_reference(other.outside_reference)
  , in_name_only(other.in_name_only)
{
  length.parent = this;
  pattern.parent = this;
  enumeration.parent = this;
  whitespace.p_parent = this;
  value.parent = this;
}

void SimpleType::loadWithValues()
{
  const XMLParser::TagAttributes & atts = parser->getActualTagAttributes();

  switch (parser->getActualTagName())
  {
  case XMLParser::n_restriction:
    type.upload(atts.base);
    setReference(atts.base);
    mode = restrictionMode;
    break;
  case XMLParser::n_list:
    type.upload(atts.itemType);
    setReference(atts.itemType);
    minOccurs = 0;
    maxOccurs = ULLONG_MAX;
    addVariant(V_list);
    mode = listMode;
    break;
  case XMLParser::n_union: { // generating complextype from simpletype
    ComplexType * new_complextype = new ComplexType(*this, ComplexType::fromTagUnion);
    new_complextype->loadWithValues();
    break; }
  case XMLParser::n_element:
    name.upload(atts.name);
    type.upload(atts.type);
    setReference(atts.type, true);
    applyDefaultAttribute(atts.default_);
    applyFixedAttribute(atts.fixed);
    applyNillableAttribute(atts.nillable);
    break;
  case XMLParser::n_attribute:
    name.upload(atts.name);
    type.upload(atts.type);
    setReference(atts.type, true);
    applyDefaultAttribute(atts.default_);
    applyFixedAttribute(atts.fixed);
    break;
  case XMLParser::n_simpleType:
    name.upload(atts.name);
    break;
  case XMLParser::n_complexType: { // generating complextype from simpletype
    ComplexType * new_complextype = new ComplexType(*this, ComplexType::fromTagComplexType);
    new_complextype->loadWithValues();
    break; }
  case XMLParser::n_length:
    if (mode == listMode) {
      minOccurs = strtoull(atts.value.c_str(), NULL, 0);
      maxOccurs = strtoull(atts.value.c_str(), NULL, 0);
      break;
    }
    length.facet_minLength = strtoull(atts.value.c_str(), NULL, 0);
    length.facet_maxLength = strtoull(atts.value.c_str(), NULL, 0);
    length.modified = true;
    break;
  case XMLParser::n_minLength:
    if (mode == listMode) {
      minOccurs = strtoull(atts.value.c_str(), NULL, 0);
      break;
    }
    length.facet_minLength = strtoull(atts.value.c_str(), NULL, 0);
    length.modified = true;
    break;
  case XMLParser::n_maxLength:
    if (mode == listMode) {
      maxOccurs = strtoull(atts.value.c_str(), NULL, 0);
      break;
    }
    length.facet_maxLength = strtoull(atts.value.c_str(), NULL, 0);
    length.modified = true;
    break;
  case XMLParser::n_pattern:
    pattern.facet = atts.value;
    pattern.modified = true;
    break;
  case XMLParser::n_enumeration:
    enumeration.facets.push_back(atts.value);
    enumeration.modified = true;
    break;
  case XMLParser::n_whiteSpace:
    whitespace.facet = atts.value;
    whitespace.modified = true;
    break;
  case XMLParser::n_minInclusive:
    if (atts.value == "NaN") {}
    else if (atts.value == "-INF") {
      value.facet_minInclusive = -DBL_MAX;
    }
    else if (atts.value == "INF") {
      value.facet_minInclusive = DBL_MAX;
    }
    else {
      value.facet_minInclusive = stringToLongDouble(atts.value.c_str());
    }
    value.modified = true;
    break;
  case XMLParser::n_maxInclusive:
    if (atts.value == "NaN") {}
    else if (atts.value == "-INF") {
      value.facet_maxInclusive = -DBL_MAX;
    }
    else if (atts.value == "INF") {
      value.facet_maxInclusive = DBL_MAX;
    }
    else {
      value.facet_maxInclusive = stringToLongDouble(atts.value.c_str());
    }
    value.modified = true;
    break;
  case XMLParser::n_minExclusive:
    if (atts.value == "NaN") {}
    else if (atts.value == "-INF") {
      value.facet_minExclusive = -DBL_MAX;
    }
    else if (atts.value == "INF") {
      value.facet_minExclusive = DBL_MAX;
    }
    else {
      value.facet_minExclusive = stringToLongDouble(atts.value.c_str());
    }
    value.modified = true;
    value.lowerExclusive = true;
    break;
  case XMLParser::n_maxExclusive:
    if (atts.value == "NaN") {}
    else if (atts.value == "-INF") {
      value.facet_maxExclusive = -DBL_MAX;
    }
    else if (atts.value == "INF") {
      value.facet_maxExclusive = DBL_MAX;
    }
    else {
      value.facet_maxExclusive = stringToLongDouble(atts.value.c_str());
    }
    value.modified = true;
    value.upperExclusive = true;
    break;
  case XMLParser::n_totalDigits:
    value.facet_totalDigits = strtoul(atts.value.c_str(), NULL, 0);
    value.modified = true;
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

void SimpleType::applyDefaultAttribute(const Mstring& default_value)
{
  if (!default_value.empty()) {
    value.default_value = default_value;
    addVariant(V_defaultForEmpty, default_value);
  }
}

void SimpleType::applyFixedAttribute(const Mstring& fixed_value)
{
  if (!fixed_value.empty()) {
    value.fixed_value = fixed_value;
    addVariant(V_defaultForEmpty, fixed_value);
  }
}

void SimpleType::applyNillableAttribute(bool nillable)
{
  if (nillable)
  {
    ComplexType * new_complextype = new ComplexType(*this, ComplexType::fromTagNillable); // generating complextype from simpletype
    new_complextype->loadWithValues();
  }
}

void SimpleType::setReference(const Mstring& ref, bool only_name_dependency)
{
  if (ref.empty()) {
    return;
  }
  if (isBuiltInType(ref)) {
    builtInBase = ref.getValueWithoutPrefix(':');
    return;
  }

  Mstring refPrefix = ref.getPrefix(':');
  Mstring refValue = ref.getValueWithoutPrefix(':');
  Mstring refUri;
  // Find the URI amongst the known namespace URIs
  List<NamespaceType>::iterator declNS;
  for (declNS = module->getDeclaredNamespaces().begin(); declNS; declNS = declNS->Next)
  {
    if (refPrefix == declNS->Data.prefix) {
      refUri = declNS->Data.uri;
      break;
    }
  }

  // FIXME: can this part be moved above the search ?
  if (refUri.empty()) { // not found
    if (refPrefix == "xml") {
      refUri = "http://www.w3.org/XML/1998/namespace";
    }
    else if (refPrefix == "xmlns") {
      refUri = "http://www.w3.org/2000/xmlns";
    }
    else if (refPrefix.empty() && module->getTargetNamespace() == "NoTargetNamespace") {
      refUri = "NoTargetNamespace";
    }
  }

  if (refUri.empty()) { // something is incorrect - unable to find the uri to the given prefix
    if (refPrefix.empty()) {
      printError(module->getSchemaname(), parser->getActualLineNumber(),
        Mstring("The absent namespace must be imported because "
          "it is not the same as the target namespace of the current schema."));
      parser->incrNumErrors();
      return;
    }
    else {
      printError(module->getSchemaname(), parser->getActualLineNumber(),
        "The value \'" + ref + "\' is incorrect: "
        "A namespace prefix does not denote any URI.");
      parser->incrNumErrors();
      return;
    }
  }

  if (only_name_dependency) {
    //name_dependency   = refUri + "|" + refValue;
    in_name_only = true;
  }

  //reference_for_other = refUri + "|" + refValue;
  outside_reference.load(refUri, refValue, &declNS->Data);
}

void SimpleType::referenceResolving()
{
  if (outside_reference.empty()) return;
  if (outside_reference.is_resolved()) return;

  SimpleType * found_ST = static_cast<SimpleType*>(
    TTCN3ModuleInventory::getInstance().lookup(this, want_ST));
  ComplexType * found_CT = static_cast<ComplexType*>(
    TTCN3ModuleInventory::getInstance().lookup(this, want_CT));
  // It _is_ possible to find both

  if (found_ST != NULL) {
    referenceForST(found_ST);
    found_ST->addToNameDepList(this);
  }
  else if (found_CT != NULL) {
    referenceForCT(found_CT);
    found_CT->addToNameDepList(this);
  }
  else {
    printError(module->getSchemaname(), name.convertedValue,
      "Reference for a non-defined type: " + outside_reference.repr());
    TTCN3ModuleInventory::getInstance().incrNumErrors();
    outside_reference.set_resolved(NULL);
  }
}

void SimpleType::referenceForST(SimpleType const * const found_ST)
{
  outside_reference.set_resolved(found_ST);

  if (in_name_only)
    return;

  if (construct == c_element)
    return;

  if (mode == listMode)
    return;

  builtInBase = found_ST->builtInBase;

  length.applyReference(found_ST->length);
  pattern.applyReference(found_ST->pattern);
  enumeration.applyReference(found_ST->enumeration);
  whitespace.applyReference(found_ST->whitespace);
  value.applyReference(found_ST->value);

  mode = found_ST->mode;
}

void SimpleType::referenceForCT(ComplexType const * const found_CT)
{
  outside_reference.set_resolved(found_CT);

  if (in_name_only)
    return;

  enumeration.modified = false;
  for (List<Mstring>::iterator itemMisc = enumeration.items_misc.begin(); itemMisc; itemMisc = itemMisc->Next)
  {
    if (isdigit(itemMisc->Data[0]))
    {
      for (List<FieldType*>::iterator field = found_CT->getFields().begin(); field; field = field->Next)
      {
        if (isIntegerType(field->Data->getType().convertedValue))
        {
          expstring_t tmp_string = mprintf("{%s:=%d}",
            field->Data->getName().convertedValue.c_str(), atoi(itemMisc->Data.c_str()));
          value.items_with_value.push_back(Mstring(tmp_string));
        }
        else if (isFloatType(field->Data->getType().convertedValue))
        {
          expstring_t tmp_string = mprintf("{%s:=%f}",
            field->Data->getName().convertedValue.c_str(), atof(itemMisc->Data.c_str()));
          value.items_with_value.push_back(Mstring(tmp_string));
        }
        else if (isTimeType(field->Data->getType().convertedValue))
        {
          expstring_t tmp_string = mprintf("{%s;=%s}",
            field->Data->getName().convertedValue.c_str(), itemMisc->Data.c_str());
          value.items_with_value.push_back(Mstring(tmp_string));
        }
      }
    }
    else {
      for (List<FieldType*>::iterator field = found_CT->getFields().begin(); field; field = field->Next)
      {
        if (isStringType(field->Data->getType().convertedValue))
        {
          expstring_t tmp_string = mprintf("{%s:=\"%s\"}",
            field->Data->getName().convertedValue.c_str(), itemMisc->Data.c_str());
          value.items_with_value.push_back(Mstring(tmp_string));
        }
      }
    }
  }
}


void SimpleType::nameConversion(NameConversionMode conversion_mode, const List<NamespaceType> & ns)
{
  switch (conversion_mode)
  {
  case nameMode:
    nameConversion_names();
    break;
  case typeMode:
    nameConversion_types(ns);
    break;
  case fieldMode:
    break;
  }
}

void SimpleType::nameConversion_names()
{
  Mstring res, var(module->getTargetNamespace());
  XSDName2TTCN3Name(name.convertedValue, TTCN3ModuleInventory::getInstance().getTypenames(), type_name, res, var);
  name.convertedValue = res;
  addVariant(V_onlyValue, var);
  for (List<SimpleType*>::iterator st = nameDepList.begin(); st; st = st->Next) {
    st->Data->setTypeValue(res);
  }
}

void SimpleType::nameConversion_types(const List<NamespaceType> & ns)
{
  if (type.convertedValue == "record" || type.convertedValue == "set"
    ||type.convertedValue == "union"  || type.convertedValue == "enumerated") return;

  Mstring prefix    = type.convertedValue.getPrefix(':');
  Mstring value_str = type.convertedValue.getValueWithoutPrefix(':');

  Mstring uri;
  for (List<NamespaceType>::iterator namesp = ns.begin(); namesp; namesp = namesp->Next)
  {
    if (prefix == namesp->Data.prefix) {
      uri = namesp->Data.uri;
      break;
    }
  }

  QualifiedName tmp(uri, value_str);

  QualifiedNames::iterator origTN = TTCN3ModuleInventory::getInstance().getTypenames().begin();
  for ( ; origTN; origTN = origTN->Next)
  {
    if (tmp == origTN->Data) {
      QualifiedName tmp_name(module->getTargetNamespace(), name.convertedValue);
      if (tmp_name == origTN->Data)
        continue; // get a new type name
      else
        break;
    }
  }
  if (origTN != NULL) {
    setTypeValue(origTN->Data.name);
    // This      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ is always value_str
    // The only effect here is to remove the "xs:" prefix from type.convertedValue,
    // otherwise the new value is always the same as the old.
  }
  else {
    Mstring res, var;
    XSDName2TTCN3Name(value_str, TTCN3ModuleInventory::getInstance().getTypenames(), type_reference_name, res, var);
    setTypeValue(res);
  }
}

void SimpleType::finalModification()
{
  value.applyFacets();
  length.applyFacets();
  pattern.applyFacet();
  whitespace.applyFacet();
  enumeration.applyFacets();

  if (module->getElementFormDefault() == qualified &&
    element_form_as == unqualified) {
    addVariant(V_formAs, Mstring("unqualified"));
  }
  if (module->getAttributeFormDefault() == qualified &&
    attribute_form_as == unqualified) {
    addVariant(V_formAs, Mstring("unqualified"));
  }
}

bool SimpleType::hasUnresolvedReference()
{
  if (!outside_reference.empty() && !outside_reference.is_resolved())
    return true;
  else
    return false;
}


void SimpleType::printToFile(FILE * file)
{
  if (!visible) return;

  printComment(file);

  fputs("type ", file);
  if (enumeration.modified) {
    if (isFloatType(builtInBase)) {
      fprintf(file, "%s %s (", type.convertedValue.c_str(), name.convertedValue.c_str());
      enumeration.sortFacets();
      enumeration.printToFile(file);
      fputc(')', file);
    }
    else {
      fprintf(file, "enumerated %s\n{\n", name.convertedValue.c_str());
      enumeration.sortFacets();
      enumeration.printToFile(file);
      fputs("\n}", file);
    }
  }
  else {
    printMinOccursMaxOccurs(file, false);

    int multiplicity = multi(module, outside_reference, this);
    const RootType *type_ref = outside_reference.get_ref();
    if ((multiplicity > 1) && type_ref
      && type_ref->getModule() != module) {
      fprintf(file, "%s.", type_ref->getModule()->getModulename().c_str());
    }

    fprintf(file, "%s %s",
      type.convertedValue.c_str(), name.convertedValue.c_str());
    pattern.printToFile(file);
    value.printToFile(file);
    length.printToFile(file);
  }
  printVariant(file);
  fputs(";\n\n\n", file);
}

void SimpleType::dump(unsigned int depth) const
{
  static const char *modes[] = {
    "", "restriction", "extension", "list"
  };
  fprintf(stderr, "%*s SimpleType '%s' -> '%s' at %p\n", depth * 2, "",
    name.originalValueWoPrefix.c_str(), name.convertedValue.c_str(),
    (const void*)this);
  fprintf(stderr, "%*s   type '%s' -> '%s'\n", depth * 2, "",
    type.originalValueWoPrefix.c_str(), type.convertedValue.c_str());

  if (mode != noMode) {
    fprintf(stderr, "%*s   %s, base='%s'\n", depth * 2, "", modes[mode], builtInBase.c_str());
  }

//  fprintf  (stderr, "%*s   rfo='%s' n_d='%s'\n", depth * 2, "",
//    reference_for_other.c_str(), name_dependency.c_str());
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

LengthType::LengthType(SimpleType * a_simpleType)
: parent(a_simpleType)
  , modified(false)
  , facet_minLength(0)
  , facet_maxLength(ULLONG_MAX)
  , lower(0)
  , upper(ULLONG_MAX)
{}

void LengthType::applyReference(const LengthType & other)
{
  if (!modified) modified = other.modified;
  if (other.facet_minLength > facet_minLength) facet_minLength = other.facet_minLength;
  if (other.facet_maxLength < facet_maxLength) facet_maxLength = other.facet_maxLength;
}

void LengthType::applyFacets() // only for string types and list types without QName
{
  if (!modified) return;

  switch (parent->getMode())
  {
  case SimpleType::restrictionMode: {
    const Mstring & base = parent->getBuiltInBase();
    if ((isStringType(base) || (isSequenceType(base) && base != "QName") || isAnyType(base)) || base.empty() )
    {
      lower = facet_minLength;
      upper = facet_maxLength;
    }
    else
    {
      printWarning(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
        Mstring("Length restriction is not supported on type '") +	base + Mstring("'."));
      TTCN3ModuleInventory::getInstance().incrNumWarnings();
    }
    break; }
  case SimpleType::extensionMode:
    break;
  case SimpleType::listMode:
    lower = facet_minLength;
    upper = facet_maxLength;
    break;
  case SimpleType::noMode:
    break;
  }
  if (lower > upper) {
    printError(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
      Mstring("The upper boundary of length restriction cannot be smaller than the lower boundary."));
    TTCN3ModuleInventory::getInstance().incrNumErrors();
    return;
  }
}

void LengthType::printToFile(FILE * file) const
{
  if (!modified) return;
  if (parent->getEnumeration().modified) return;

  if (lower == upper)
  {
    fprintf(file, " length(%llu)", lower);
  }
  else
  {
    fprintf(file, " length(%llu .. ", lower);

    if (upper == ULLONG_MAX)
    {
      fputs("infinity", file);
    }
    else
    {
      fprintf(file, "%llu", upper);
    }

    fputc(')', file);
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

PatternType::PatternType(SimpleType * a_simpleType)
: parent(a_simpleType)
  , modified(false)
  , facet()
  , value()
  {}

void PatternType::applyReference(const PatternType & other)
{
  if (!modified) modified = other.modified;
  if (facet.empty()) facet = other.facet;
}

void PatternType::applyFacet() // only for time types and string types without hexBinary
{
  if (!modified) return;

  const Mstring & base = parent->getBuiltInBase();
  if (((isStringType(base) && base != "hexBinary") || isTimeType(base) || isAnyType(base)) || base.empty())
  {
    // XSD pattern to TTCN-3 pattern; ETSI ES 201 873-9 clause 6.1.4
    // FIXME a proper scanner is needed, e.g. from flex
    int charclass = 0;
    for (size_t i = 0; i != facet.size(); ++i)
    {
      char c = facet[i];
      switch (c) {
      case '(': case ')': case '/': case '^':
        value += c;
        break;
      case '[':
        value += c;
        ++charclass;
        break;
      case ']':
        value += c;
        --charclass;
        break;
      case '.': // any character
        value += '?';
        break;
      case '*': // 0 or more
        value += '*'; //#(0,)
        break;
      case '+':
        value += '+'; //#(1,)
        break;
      case '?':
        value += "#(0,1)";
        break;
      case '"':
        value += "\"\"";
      case '{': {
        Mstring s;
        int k = 1;
        while (facet[i + k] != '}') {
          s += facet[i + k];
          ++k;
        }
        if (s.isFound(',')) {
          value += "#(";
          value += s;
          value += ')';
        }
        else {
          value += '#';
          value += s;
        }
        i = i + k;
        break; }
      case '\\': {
        // Appendix G1.1 of XML Schema Datatypes: Character class escapes;
        // specifically, http://www.w3.org/TR/xmlschema11-2/#nt-MultiCharEsc
        char cn = facet[i+1];
        switch (cn) {
        case 'c':
          value += charclass ? "\\w\\d.\\-_:"
            :                 "[\\w\\d.\\-_:]";
          break;
        case 'C':
          value += charclass ? "^\\w\\d.\\-_:"
            :                 "[^\\w\\d.\\-_:]";
          break;
        case 'D':
          value += charclass ? "^\\d"
            :                 "[^\\d]";
          break;
        case 'i':
          value += charclass ? "\\w\\d:"
            :                 "[\\w\\d:]";
          break;
        case 'I':
          value += charclass ? "^\\w\\d:"
            :                 "[^\\w\\d:]";
          break;
        case 's':
          value += charclass ? "\\q{0,0,0,20}\\q{0,0,0,10}\\t\\r"
            :                 "[\\q{0,0,0,20}\\q{0,0,0,10}\\t\\r]";
          break;
        case 'S':
          value += charclass ? "^\\q{0,0,0,20}\\q{0,0,0,10}\\t\\r"
            :                 "[^\\q{0,0,0,20}\\q{0,0,0,10}\\t\\r]";
          break;
        case 'W':
          value += charclass ? "^\\w"
            :                 "[^\\w]";
          break;
        case 'p':
          printWarning(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
            Mstring("Character categories and blocks are not supported."));
          TTCN3ModuleInventory::getInstance().incrNumWarnings();
          parent->addComment(
            Mstring("Pattern is not converted due to using character categories and blocks in patterns is not supported."));
          value.clear();
          return;

        case '.':
          value += '.';
          break;
        default:
          // backslash + another: pass unmodified; this also handles \d and \w
          value += c;
          value += cn;
          break;
        }
        ++i;
        break; }
      case '&':
        if (facet[i + 1] == '#') { // &#....;
          Mstring s;
          int k = 2;
          while (facet[i + k] != ';') {
            s += facet[i + k];
            ++k;
          }
          long long int d = atoll(s.c_str());
          if (d < 0 || d > 2147483647) {
            printError(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
              Mstring("Invalid unicode character."));
            TTCN3ModuleInventory::getInstance().incrNumErrors();
          }
          unsigned char group = (d >> 24) & 0xFF;
          unsigned char plane = (d >> 16) & 0xFF;
          unsigned char row = (d >> 8) & 0xFF;
          unsigned char cell = d & 0xFF;

          expstring_t res = mprintf("\\q{%d, %d, %d, %d}", group, plane, row, cell);
          value += res;
          Free(res);
          i = i + k;
        }
        // else fall through
      default: //just_copy:
        value += c;
        break;
      } // switch(c)
    } // next i
  }
  else
  {
    printWarning(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
      Mstring("Pattern restriction is not supported on type '") + base + Mstring("'."));
    TTCN3ModuleInventory::getInstance().incrNumWarnings();
  }
}

void PatternType::printToFile(FILE * file) const
{
  if (!modified || value.empty()) return;
  if (parent->getEnumeration().modified) return;

  fprintf(file, " (pattern \"%s\")", value.c_str());
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

EnumerationType::EnumerationType(SimpleType * a_simpleType)
: parent(a_simpleType)
, modified(false)
, facets()
, items_string()
, items_int()
, items_float()
, items_time()
, items_misc()
{}

void EnumerationType::applyReference(const EnumerationType & other)
{
  if (!modified) modified = other.modified;
  for (List<Mstring>::iterator facet = other.facets.begin(); facet; facet = facet->Next)
  {
    facets.push_back(facet->Data);
  }
}

void EnumerationType::applyFacets() // string types, integer types, float types, time types
{
  if (!modified) return;

  const Mstring & base = parent->getBuiltInBase();

  if (isStringType(base)) // here length restriction is applicable
  {
    List<Mstring> text_variants;
    for (List<Mstring>::iterator facet = facets.begin(); facet; facet = facet->Next)
    {
      const LengthType & length = parent->getLength();
      if (length.lower <= facet->Data.size() && facet->Data.size() <= length.upper)
      {
        Mstring res, var;
        XSDName2TTCN3Name(facet->Data, items_string, enum_id_name, res, var);
        text_variants.push_back(var);
      }
    }
    text_variants.sort();
    for (List<Mstring>::iterator var = text_variants.end(); var; var = var->Prev) {
      parent->addVariant(V_onlyValue, var->Data);
    }
  }
  else if (isIntegerType(base)) // here value restriction is applicable
  {
    for (List<Mstring>::iterator facet = facets.begin(); facet; facet = facet->Next)
    {
      int int_value = atoi(facet->Data.c_str());
      const ValueType & value = parent->getValue();
      if (value.lower <= int_value && int_value <= value.upper)
      {
        bool found = false;
        for (List<int>::iterator itemInt = items_int.begin(); itemInt; itemInt = itemInt->Next)
        {
          if (int_value == itemInt->Data) {
            found = true;
            break;
          }
        }
        if (!found) items_int.push_back(int_value);

        if (parent->getVariant().empty() || parent->getVariant().back() != "\"useNumber\"") {
          parent->addVariant(V_useNumber);
        }
      }
    }
  }
  else if (isFloatType(base)) // here value restriction is applicable
  {
    for (List<Mstring>::iterator facet = facets.begin(); facet; facet = facet->Next)
    {
      double float_value = atof(facet->Data.c_str());
      const ValueType & value = parent->getValue();
      if (value.lower <= float_value && float_value <= value.upper)
      {
        bool found = false;
        for (List<double>::iterator itemFloat = items_float.begin(); itemFloat; itemFloat = itemFloat->Next)
        {
          if (float_value == itemFloat->Data) {
            found = true;
            break;
          }
        }
        if (!found) {
          items_float.push_back(float_value);

        }
      }
    }
  }
  else if (isTimeType(base))
  {
    List<Mstring> text_variants;
    for (List<Mstring>::iterator facet = facets.begin(); facet; facet = facet->Next)
    {
      Mstring res, var;
      XSDName2TTCN3Name(facet->Data, items_time, enum_id_name, res, var);
      text_variants.push_back(var);
    }
    text_variants.sort();
    for (List<Mstring>::iterator var = text_variants.end(); var; var = var->Prev) {
      parent->addVariant(V_onlyValue, var->Data);
    }
  }
  else if (isAnyType(base))
  {}
  else if (base.empty())
  {}
  else
  {
    printWarning(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
      Mstring("Enumeration restriction is not supported on type '") + base + Mstring("'."));
    TTCN3ModuleInventory::getInstance().incrNumWarnings();
    parent->setInvisible();
  }
}

void EnumerationType::sortFacets()
{
  items_string.sort();
  items_int.sort();
  items_float.sort();
  items_time.sort();
}

void EnumerationType::printToFile(FILE * file, unsigned int indent_level) const
{
  if (!modified) return;

  const Mstring & base = parent->getBuiltInBase();
  if (isStringType(base))
  {
    for (QualifiedNames::iterator itemString = items_string.begin(); itemString; itemString = itemString->Next)
    {
      if (itemString != items_string.begin()) fputs(",\n", file);
      for (unsigned int l = 0; l != indent_level; ++l) fputs("\t", file);
      fprintf(file, "\t%s", itemString->Data.name.c_str());
    }
  }
  else if (isIntegerType(base))
  {
    for (List<int>::iterator itemInt = items_int.begin(); itemInt; itemInt = itemInt->Next)
    {
      if (itemInt != items_int.begin()) fputs(",\n", file);
      for (unsigned int l = 0; l != indent_level; ++l) fputs("\t", file);
      if (itemInt->Data < 0) {
        fprintf(file, "\tint_%d(%d)", abs(itemInt->Data), itemInt->Data);
      }
      else {
        fprintf(file, "\tint%d(%d)", itemInt->Data, itemInt->Data);
      }
    }
  }
  else if (isFloatType(base))
  {
    for (List<double>::iterator itemFloat = items_float.begin(); itemFloat; itemFloat = itemFloat->Next)
    {
      if (itemFloat != items_float.begin()) fputs(", ", file);

      double intpart = 0;
      double fracpart = 0;
      fracpart = modf(itemFloat->Data, &intpart);
      if (fracpart == 0) {
        fprintf(file, "%lld.0", (long long int)(itemFloat->Data));
      }
      else {
        fprintf(file, "%g", itemFloat->Data);
      }
    }
  }
  else if (isTimeType(base))
  {
    for (QualifiedNames::iterator itemTime = items_time.begin(); itemTime; itemTime = itemTime->Next)
    {
      if (itemTime != items_time.begin()) fputs(",\n", file);
      for (unsigned int l = 0; l != indent_level; ++l) fputs("\t", file);
      fprintf(file, "\t%s", itemTime->Data.name.c_str());
    }
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

WhitespaceType::WhitespaceType(SimpleType * a_simpleType)
: p_parent(a_simpleType)
, modified(false)
, facet()
, value()
{}

void WhitespaceType::applyReference(const WhitespaceType & other)
{
  if (!modified) modified = other.modified;
  if (facet.empty()) facet = other.facet;
}

void WhitespaceType::applyFacet() // only for string types: string, normalizedString, token, Name, NCName, language
{
  if (!modified) return;

  const Mstring & base = p_parent->getBuiltInBase();
  if (base == "string" || base == "normalizedString" || base == "token" || base == "language" ||
    base == "Name" || base == "NCName" || isAnyType(base) || base.empty())
  {
    p_parent->addVariant(V_whiteSpace, facet);
  }
  else
  {
    printWarning(p_parent->getModule()->getSchemaname(), p_parent->getName().convertedValue,
      Mstring("Facet 'whiteSpace' is not applicable for type '") + base + Mstring("'."));
    TTCN3ModuleInventory::getInstance().incrNumWarnings();
  }
}

ValueType::ValueType(SimpleType * a_simpleType)
: parent(a_simpleType)
, modified(false)
, facet_minInclusive(-DBL_MAX)
, facet_maxInclusive(DBL_MAX)
, facet_minExclusive(-DBL_MAX)
, facet_maxExclusive(DBL_MAX)
, facet_totalDigits(0)
, lower(-DBL_MAX)
, upper(DBL_MAX)
, lowerExclusive(false)
, upperExclusive(false)
, fixed_value()
, default_value()
, items_with_value()
{}

void ValueType::applyReference(const ValueType & other)
{
  if (!modified) modified = other.modified;
  if (other.facet_minInclusive > facet_minInclusive) facet_minInclusive = other.facet_minInclusive;
  if (other.facet_maxInclusive < facet_maxInclusive) facet_maxInclusive = other.facet_maxInclusive;
  if (other.facet_minExclusive > facet_minExclusive) facet_minExclusive = other.facet_minExclusive;
  if (other.facet_maxExclusive < facet_maxExclusive) facet_maxExclusive = other.facet_maxExclusive;
  if (other.facet_totalDigits < facet_totalDigits) facet_totalDigits = other.facet_totalDigits;
}

void ValueType::applyFacets() // only for integer and float types
{
  if (!modified) return;

  const Mstring & base = parent->getBuiltInBase();
  /*
   * Setting of default value range of built-in types
   */
  if (base == "positiveInteger")
  {
    lower = 1;
  }
  else if (base == "nonPositiveInteger")
  {
    upper = 0;
  }
  else if (base == "negativeInteger")
  {
    upper = -1;
  }
  else if (base == "nonNegativeInteger")
  {
    lower = 0;
  }
  else if (base == "unsignedLong")
  {
    lower = 0;
    upper = ULLONG_MAX;
  }
  else if (base == "int")
  {
    lower = INT_MIN;
    upper = INT_MAX;
  }
  else if (base == "unsignedInt")
  {
    lower = 0;
    upper = UINT_MAX;
  }
  else if (base == "short")
  {
    lower = SHRT_MIN;
    upper = SHRT_MAX;
  }
  else if (base == "unsignedShort")
  {
    lower = 0;
    upper = USHRT_MAX;
  }
  else if (base == "byte")
  {
    lower = CHAR_MIN;
    upper = CHAR_MAX;
  }
  else if (base == "unsignedByte")
  {
    lower = 0;
    upper = UCHAR_MAX;
  }

  if (isIntegerType(base))
  {
    if (facet_minInclusive != -DBL_MAX && facet_minInclusive > lower) lower = facet_minInclusive;
    if (facet_maxInclusive != DBL_MAX && upper > facet_maxInclusive) upper = facet_maxInclusive;
    if (facet_minExclusive != -DBL_MAX && lower < facet_minExclusive) lower = facet_minExclusive;
    if (facet_maxExclusive != DBL_MAX && upper > facet_maxExclusive) upper = facet_maxExclusive;
  }
  else if (isFloatType(base))
  {
    if (facet_minInclusive != -DBL_MAX && lower < facet_minInclusive) lower = facet_minInclusive;
    if (facet_maxInclusive != DBL_MAX && upper > facet_maxInclusive) upper = facet_maxInclusive;
    if (facet_minExclusive != -DBL_MAX && lower < facet_minExclusive) lower = facet_minExclusive;
    if (facet_maxExclusive != DBL_MAX && upper > facet_maxExclusive) upper = facet_maxExclusive;
  }
  else if (isAnyType(base))
  {}
  else if (base.empty())
  {}
  else
  {
    printWarning(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
      Mstring("Value restriction is not supported on type '") + base + Mstring("'."));
    TTCN3ModuleInventory::getInstance().incrNumWarnings();
  }

  // totalDigits facet is only for integer types and decimal
  if (facet_totalDigits != 0) // if this facet is used
  {
    double r = pow(10.0, facet_totalDigits);

    if (base == "integer")
    {
      lower = (int) -(r-1);
      upper = (int)  (r-1);
    }
    else if (base == "positiveInteger")
    {
      lower = 1;
      upper = (int)  (r-1);
    }
    else if (base == "nonPositiveInteger")
    {
      lower = (int) -(r-1);
      upper = 0;
    }
    else if (base == "negativeInteger")
    {
      lower = (int) -(r-1);
      upper = -1;
    }
    else if (base == "nonNegativeInteger")
    {
      lower = 0;
      upper = (int)  (r-1);
    }
    else if (base == "long" ||
      base == "unsignedLong" ||
      base == "int" ||
      base == "unsignedInt" ||
      base == "short" ||
      base == "unsignedShort" ||
      base == "byte" ||
      base == "unsignedByte")
    {
      lower = (int) -(r-1);
      upper = (int)  (r-1);
    }
    else if (base == "decimal")
    {
      lower = (int) -(r-1);
      upper = (int) (r-1);
    }
    else {
      printWarning(parent->getModule()->getSchemaname(), parent->getName().convertedValue,
        Mstring("Facet 'totalDigits' is not applicable for type '") + base + Mstring("'."));
      TTCN3ModuleInventory::getInstance().incrNumWarnings();
    }
  }
}

void ValueType::printToFile(FILE * file) const
{
  if (!modified) return;
  if (parent->getEnumeration().modified) return;

  if (!fixed_value.empty())
  {
    fprintf(file, " (\"%s\")", fixed_value.c_str());
    return;
  }
  if (!items_with_value.empty())
  {
    fputs(" (", file);
    for (List<Mstring>::iterator itemWithValue = items_with_value.begin(); itemWithValue; itemWithValue = itemWithValue->Next)
    {
      if (itemWithValue != items_with_value.begin()) fputs(", ", file);
      fprintf(file, "%s", itemWithValue->Data.c_str());
    }
    fputc(')', file);
    return;
  }

  if (lower == -DBL_MAX && upper == DBL_MAX) return;

  fputs(" (", file);

  if (isIntegerType(parent->getBuiltInBase()))
  {
    if (lowerExclusive) {
      fputc('!', file);
    }

    if (lower == -DBL_MAX)
    {
      fputs("-infinity", file);
    }
    else if (lower < 0)
    {
      long double temp_lower = -lower;
      fprintf(file, "-%.0Lf", temp_lower);
    }
    else
    {
      fprintf(file, "%.0Lf", lower);
    }

    fputs(" .. ", file);
    if (upperExclusive) {
      fputc('!', file);
    }

    if (upper == DBL_MAX)
    {
      fputs("infinity", file);
    }
    else if (upper < 0)
    {
      long double temp_upper = -upper;
      fprintf(file, "-%.0Lf", temp_upper);
    }
    else
    {
      fprintf(file, "%.0Lf", upper);
    }
  }
  else if (isFloatType(parent->getBuiltInBase()))
  {
    if (lowerExclusive) {
      fputc('!', file);
    }

    if (lower == -DBL_MAX)
    {
      fputs("-infinity", file);
    }
    else
    {
      double intpart = 0;
      double fracpart = 0;
      fracpart = modf(lower, &intpart);
      if (fracpart == 0) {
        fprintf(file, "%.1Lf", lower);
      }
      else {
        fprintf(file, "%Lg", lower);
      }
    }

    fputs(" .. ", file);
    if (upperExclusive) {
      fputc('!', file);
    }

    if (upper == DBL_MAX)
    {
      fputs("infinity", file);
    }
    else
    {
      double intpart = 0;
      double fracpart = 0;
      fracpart = modf(upper, &intpart);
      if (fracpart == 0) {
        fprintf(file, "%.1Lf", upper);
      }
      else {
        fprintf(file, "%Lg", upper);
      }
    }
  }

  fputc(')', file);
}
