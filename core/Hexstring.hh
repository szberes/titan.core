///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef HEXSTRING_HH
#define HEXSTRING_HH

#include "Basetype.hh"
#include "Template.hh"
#include "Optional.hh"
#include "Error.hh"

class INTEGER;
class BITSTRING;
class OCTETSTRING;
class CHARSTRING;
class HEXSTRING_ELEMENT;

class Module_Param;

class HEXSTRING : public Base_Type {

  friend class HEXSTRING_ELEMENT;
  friend class HEXSTRING_template;

  friend HEXSTRING bit2hex(const BITSTRING& value);
  friend HEXSTRING int2hex(const INTEGER& value, int length);
  friend HEXSTRING oct2hex(const OCTETSTRING& value);
  friend HEXSTRING str2hex(const CHARSTRING& value);
  friend HEXSTRING substr(const HEXSTRING& value, int index, int returncount);
  friend HEXSTRING replace(const HEXSTRING& value, int index, int len, const HEXSTRING& repl);

  struct hexstring_struct;
  hexstring_struct *val_ptr;

  void init_struct(int n_nibbles);
  unsigned char get_nibble(int nibble_index) const;
  void set_nibble(int nibble_index, unsigned char new_value);
  void copy_value();
  void clear_unused_nibble() const;
  HEXSTRING(int n_nibbles);

public:
  HEXSTRING();
  HEXSTRING(int init_n_nibbles, const unsigned char* init_nibbles);
  HEXSTRING(const HEXSTRING& other_value);
  HEXSTRING(const HEXSTRING_ELEMENT& other_value);
  ~HEXSTRING();
  void clean_up();

  HEXSTRING& operator=(const HEXSTRING& other_value);
  HEXSTRING& operator=(const HEXSTRING_ELEMENT& other_value);

  boolean operator==(const HEXSTRING& other_value) const;
  boolean operator==(const HEXSTRING_ELEMENT& other_value) const;

  inline boolean operator!=(const HEXSTRING& other_value) const
    { return !(*this == other_value); }
  inline boolean operator!=(const HEXSTRING_ELEMENT& other_value) const
    { return !(*this == other_value); }

  HEXSTRING operator+(const HEXSTRING& other_value) const;
  HEXSTRING operator+(const HEXSTRING_ELEMENT& other_value) const;

  HEXSTRING operator~() const;
  HEXSTRING operator&(const HEXSTRING& other_value) const;
  HEXSTRING operator&(const HEXSTRING_ELEMENT& other_value) const;
  HEXSTRING operator|(const HEXSTRING& other_value) const;
  HEXSTRING operator|(const HEXSTRING_ELEMENT& other_value) const;
  HEXSTRING operator^(const HEXSTRING& other_value) const;
  HEXSTRING operator^(const HEXSTRING_ELEMENT& other_value) const;

  HEXSTRING operator<<(int shift_count) const;
  HEXSTRING operator<<(const INTEGER& shift_count) const;
  HEXSTRING operator>>(int shift_count) const;
  HEXSTRING operator>>(const INTEGER& shift_count) const;
  HEXSTRING operator<<=(int rotate_count) const;
  HEXSTRING operator<<=(const INTEGER& rotate_count) const;
  HEXSTRING operator>>=(int rotate_count) const;
  HEXSTRING operator>>=(const INTEGER& rotate_count) const;

  HEXSTRING_ELEMENT operator[](int index_value);
  HEXSTRING_ELEMENT operator[](const INTEGER& index_value);
  const HEXSTRING_ELEMENT operator[](int index_value) const;
  const HEXSTRING_ELEMENT operator[](const INTEGER& index_value) const;

  inline boolean is_bound() const { return val_ptr != NULL; }
  inline boolean is_value() const { return val_ptr != NULL; }
  inline void must_bound(const char *err_msg) const
    { if (val_ptr == NULL) TTCN_error("%s", err_msg); }

  int lengthof() const;

  operator const unsigned char*() const;

#ifdef TITAN_RUNTIME_2
  boolean is_equal(const Base_Type* other_value) const { return *this == *(static_cast<const HEXSTRING*>(other_value)); }
  void set_value(const Base_Type* other_value) { *this = *(static_cast<const HEXSTRING*>(other_value)); }
  Base_Type* clone() const { return new HEXSTRING(*this); }
  const TTCN_Typedescriptor_t* get_descriptor() const { return &HEXSTRING_descr_; }
#else
  inline boolean is_present() const { return is_bound(); }
#endif

  void log() const;
  void set_param(Module_Param& param);
  void encode_text(Text_Buf& text_buf) const;
  void decode_text(Text_Buf& text_buf);
  void encode(const TTCN_Typedescriptor_t& p_td, TTCN_Buffer& p_buf,
              TTCN_EncDec::coding_t p_coding, ...) const;

  void decode(const TTCN_Typedescriptor_t& p_td, TTCN_Buffer& p_buf,
              TTCN_EncDec::coding_t p_coding, ...);
  /** Encodes the value of the variable according to the
    * TTCN_Typedescriptor_t. It must be public because called by
    * another types during encoding. Returns the length of encoded data*/
  int RAW_encode(const TTCN_Typedescriptor_t&, RAW_enc_tree&) const;
  /** Decodes the value of the variable according to the
   * TTCN_Typedescriptor_t. It must be public because called by
   * another types during encoding. Returns the number of decoded
   * bits. */
  int RAW_decode(const TTCN_Typedescriptor_t&, TTCN_Buffer&, int, raw_order_t,
                 boolean no_err=FALSE, int sel_field=-1, boolean first_call=TRUE);
  int XER_encode(const XERdescriptor_t& p_td, TTCN_Buffer& p_buf,
    unsigned int flavor, int indent) const;
  int XER_decode(const XERdescriptor_t& p_td, XmlReaderWrap& reader,
    unsigned int flavor);
  
  /** Encodes accordingly to the JSON encoding rules.
    * Returns the length of the encoded data. */
  int JSON_encode(const TTCN_Typedescriptor_t&, JSON_Tokenizer&) const;
  
  /** Decodes accordingly to the JSON encoding rules.
    * Returns the length of the decoded data. */
  int JSON_decode(const TTCN_Typedescriptor_t&, JSON_Tokenizer&, boolean);
};

class HEXSTRING_ELEMENT {
  boolean bound_flag;
  HEXSTRING& str_val;
  int nibble_pos;

public:
  HEXSTRING_ELEMENT(boolean par_bound_flag, HEXSTRING& par_str_val,
    int par_nibble_pos);

  HEXSTRING_ELEMENT& operator=(const HEXSTRING& other_value);
  HEXSTRING_ELEMENT& operator=(const HEXSTRING_ELEMENT& other_value);

  boolean operator==(const HEXSTRING& other_value) const;
  boolean operator==(const HEXSTRING_ELEMENT& other_value) const;

  inline boolean operator!=(const HEXSTRING& other_value) const
    { return !(*this == other_value); }
  inline boolean operator!=(const HEXSTRING_ELEMENT& other_value) const
    { return !(*this == other_value); }

  HEXSTRING operator+(const HEXSTRING& other_value) const;
  HEXSTRING operator+(const HEXSTRING_ELEMENT& other_value) const;

  HEXSTRING operator~() const;
  HEXSTRING operator&(const HEXSTRING& other_value) const;
  HEXSTRING operator&(const HEXSTRING_ELEMENT& other_value) const;
  HEXSTRING operator|(const HEXSTRING& other_value) const;
  HEXSTRING operator|(const HEXSTRING_ELEMENT& other_value) const;
  HEXSTRING operator^(const HEXSTRING& other_value) const;
  HEXSTRING operator^(const HEXSTRING_ELEMENT& other_value) const;

  inline boolean is_bound() const { return bound_flag; }
  inline boolean is_present() const { return bound_flag; }
  inline boolean is_value() const { return bound_flag; }
  inline void must_bound(const char *err_msg) const
    { if (!bound_flag) TTCN_error("%s", err_msg); }

  unsigned char get_nibble() const;

  void log() const;
};

// hexstring template class

class HEXSTRING_template : public Restricted_Length_Template {
#ifdef __SUNPRO_CC
public:
#endif
  struct hexstring_pattern_struct;
private:
  HEXSTRING single_value;
  union {
    struct {
      unsigned int n_values;
      HEXSTRING_template *list_value;
    } value_list;
    hexstring_pattern_struct *pattern_value;
  };

  void copy_template(const HEXSTRING_template& other_value);
  static boolean match_pattern(const hexstring_pattern_struct *string_pattern,
    const HEXSTRING::hexstring_struct *string_value);

public:
  HEXSTRING_template();
  HEXSTRING_template(template_sel other_value);
  HEXSTRING_template(const HEXSTRING& other_value);
  HEXSTRING_template(const HEXSTRING_ELEMENT& other_value);
  HEXSTRING_template(const OPTIONAL<HEXSTRING>& other_value);
  HEXSTRING_template(unsigned int n_elements,
    const unsigned char *pattern_elements);
  HEXSTRING_template(const HEXSTRING_template& other_value);
  ~HEXSTRING_template();
  void clean_up();

  HEXSTRING_template& operator=(template_sel other_value);
  HEXSTRING_template& operator=(const HEXSTRING& other_value);
  HEXSTRING_template& operator=(const HEXSTRING_ELEMENT& other_value);
  HEXSTRING_template& operator=(const OPTIONAL<HEXSTRING>& other_value);
  HEXSTRING_template& operator=(const HEXSTRING_template& other_value);

  HEXSTRING_ELEMENT operator[](int index_value);
  HEXSTRING_ELEMENT operator[](const INTEGER& index_value);
  const HEXSTRING_ELEMENT operator[](int index_value) const;
  const HEXSTRING_ELEMENT operator[](const INTEGER& index_value) const;

  boolean match(const HEXSTRING& other_value) const;
  const HEXSTRING& valueof() const;

  int lengthof() const;

  void set_type(template_sel template_type, unsigned int list_length);
  HEXSTRING_template& list_item(unsigned int list_index);

  void log() const;
  void log_match(const HEXSTRING& match_value) const;

  void set_param(Module_Param& param);

  void encode_text(Text_Buf& text_buf) const;
  void decode_text(Text_Buf& text_buf);

  boolean is_present() const;
  boolean match_omit() const;
#ifdef TITAN_RUNTIME_2
  void valueofv(Base_Type* value) const { *(static_cast<HEXSTRING*>(value)) = valueof(); }
  void set_value(template_sel other_value) { *this = other_value; }
  void copy_value(const Base_Type* other_value) { *this = *(static_cast<const HEXSTRING*>(other_value)); }
  Base_Template* clone() const { return new HEXSTRING_template(*this); }
  const TTCN_Typedescriptor_t* get_descriptor() const { return &HEXSTRING_descr_; }
  boolean matchv(const Base_Type* other_value) const { return match(*(static_cast<const HEXSTRING*>(other_value))); }
  void log_matchv(const Base_Type* match_value) const  { log_match(*(static_cast<const HEXSTRING*>(match_value))); }
#else
  void check_restriction(template_res t_res, const char* t_name=NULL) const;
#endif
};

#endif
