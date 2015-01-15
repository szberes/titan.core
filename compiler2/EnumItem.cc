///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "EnumItem.hh"
#include "Value.hh"

namespace Common {

// =================================
// ===== EnumItem
// =================================

EnumItem::EnumItem(Identifier *p_name, Value *p_value)
: Node(), Location(), name(p_name), value(p_value)
{
  if (!p_name) FATAL_ERROR("NULL parameter: Common::EnumItem::EnumItem()");
}

EnumItem::EnumItem(const EnumItem& p)
: Node(p), Location(p)
{
  name=p.name->clone();
  value=p.value?p.value->clone():0;
}

EnumItem::~EnumItem()
{
  delete name;
  delete value;
}

EnumItem *EnumItem::clone() const
{
  return new EnumItem(*this);
}

void EnumItem::set_fullname(const string& p_fullname)
{
  Node::set_fullname(p_fullname);
  if(value) value->set_fullname(p_fullname);
}

void EnumItem::set_my_scope(Scope *p_scope)
{
  if(value) value->set_my_scope(p_scope);
}

void EnumItem::set_value(Value *p_value)
{
  if(!p_value) FATAL_ERROR("NULL parameter: Common::EnumItem::set_value()");
  if(value) FATAL_ERROR("Common::EnumItem::set_value()");
  value=p_value;
}

void EnumItem::set_text(const string& p_text)
{
  text = p_text;
}

string EnumItem::get_name_hacked(Type *p_type) const
{
  if (p_type->is_asn1()) {
    string hack_asnname(Identifier::name_2_asn(p_type->get_genname_own()));
    hack_asnname += "-enum-";
    hack_asnname += name->get_asnname();
    return Identifier::asn_2_name(hack_asnname);
  } else {
    string hack_ttcnname(Identifier::name_2_ttcn(p_type->get_genname_own()));
    hack_ttcnname += "_enum_";
    hack_ttcnname += name->get_ttcnname();
    return Identifier::ttcn_2_name(hack_ttcnname);
  }
}

void EnumItem::dump(unsigned level) const
{
  name->dump(level);
  if(value) {
    DEBUG(level, "with value:");
    value->dump(level+1);
  }
}

// =================================
// ===== EnumItems
// =================================

EnumItems::EnumItems(const EnumItems& p)
: Node(p), my_scope(0)
{
  for (size_t i = 0; i < p.eis_v.size(); i++) add_ei(p.eis_v[i]->clone());
}

EnumItems::~EnumItems()
{
  for(size_t i = 0; i < eis_v.size(); i++) delete eis_v[i];
  eis_v.clear();
  eis_m.clear();
}

void EnumItems::release_eis()
{
  eis_v.clear();
  eis_m.clear();
}

EnumItems* EnumItems::clone() const
{
  return new EnumItems(*this);
}

void EnumItems::set_fullname(const string& p_fullname)
{
  Node::set_fullname(p_fullname);
  for (size_t i = 0; i < eis_v.size(); i++) {
    EnumItem *ei = eis_v[i];
    ei->set_fullname(p_fullname + "." + ei->get_name().get_dispname());
  }
}

void EnumItems::set_my_scope(Scope *p_scope)
{
  my_scope = p_scope;
  for(size_t i = 0; i < eis_v.size(); i++)
    eis_v[i]->set_my_scope(p_scope);
}

void EnumItems::add_ei(EnumItem *p_ei)
{
  if(!p_ei)
    FATAL_ERROR("NULL parameter: Common::EnumItems::add_ei()");
  eis_v.add(p_ei);
  const Identifier& id = p_ei->get_name();
  const string& name = id.get_name();
  if (!eis_m.has_key(name)) eis_m.add(name, p_ei);
  p_ei->set_fullname(get_fullname()+"."+id.get_dispname());
  p_ei->set_my_scope(my_scope);
}

void EnumItems::dump(unsigned level) const
{
  for (size_t i = 0; i < eis_v.size(); i++) eis_v[i]->dump(level);
}

}
