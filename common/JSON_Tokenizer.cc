#include <cstring>

#include "JSON_Tokenizer.hh"
#include "memory.h"
#include <cstdio>

static const char TABS[] =
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
const size_t MAX_TABS = sizeof(TABS) - 1; // 64

void JSON_Tokenizer::init(const char* p_buf, const size_t p_buf_len)
{
  if (p_buf != 0 && p_buf_len != 0) {
    buf_ptr = mcopystrn(p_buf, p_buf_len);
  } else {
    buf_ptr = 0;
  }
  buf_len = p_buf_len;
  buf_pos = 0;
  depth = 0;
  previous_token = JSON_TOKEN_NONE;
}

JSON_Tokenizer::~JSON_Tokenizer()
{
  Free(buf_ptr);
}

void JSON_Tokenizer::put_c(const char c)
{
  buf_ptr = mputprintf(buf_ptr, "%c", c);
  ++buf_len;
}
  
void JSON_Tokenizer::put_s(const char* s)
{
  buf_ptr = mputstr(buf_ptr, s);
  buf_len += strlen(s);
}

void JSON_Tokenizer::put_depth()
{
  put_s(TABS + ((depth > MAX_TABS) ? 0 : MAX_TABS - depth));
}

bool JSON_Tokenizer::skip_white_spaces()
{
  while(buf_pos < buf_len) {
    switch(buf_ptr[buf_pos]) {
    case ' ':
    case '\r':
    case '\n':
    case '\t':
    case '\f':
      ++buf_pos;
      break;
    default:
      return true;
    }
  }
  return false;
}

bool JSON_Tokenizer::check_for_string() 
{
  if ('\"' == buf_ptr[buf_pos]) {
    ++buf_pos;
  } else {
    return false;
  }
  while (buf_pos < buf_len) {
    if ('\"' == buf_ptr[buf_pos]) {
      return true;
    }
    else if ('\\' == buf_ptr[buf_pos]) {
      // skip escaped character (so escaped quotes (\") are not mistaken for the ending quotes)
      ++buf_pos;
    }
    ++buf_pos;
  }
  return false;
}

bool JSON_Tokenizer::check_for_number()
{
  bool first_digit = false; // first non-zero digit reached
  bool zero = false; // first zero digit reached
  bool decimal_point = false; // decimal point (.) reached
  bool exponent_mark = false; // exponential mark (e or E) reached
  bool exponent_sign = false; // sign of the exponential (- or +) reached
  
  if ('-' == buf_ptr[buf_pos]) {
    ++buf_pos;
  }
  
  while (buf_pos < buf_len) {
    switch(buf_ptr[buf_pos]) {
    case '.':
      if (decimal_point || exponent_mark || (!first_digit && !zero)) {
        return false;
      }
      decimal_point = true;
      first_digit = false;
      zero = false;
      break;
    case 'e':
    case 'E':
      if (exponent_mark || (!first_digit && !zero)) {
        return false;
      }
      exponent_mark = true;
      first_digit = false;
      zero = false;
      break;
    case '0':
      if (!first_digit && (exponent_mark || (!decimal_point && zero))) {
        return false;
      }
      zero = true;
      break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (!first_digit && zero && (!decimal_point || exponent_mark)) {
        return false;
      }
      first_digit = true;
      break;
    case '-':
    case '+':
      if (exponent_sign || !exponent_mark || zero || first_digit) {
        return false;
      }
      exponent_sign = true;
      break;
    default:
      return first_digit || zero;
    }
    
    ++buf_pos; 
  }
  return first_digit || zero;
}

bool JSON_Tokenizer::check_for_separator()
{
  if (buf_pos < buf_len) {
    switch(buf_ptr[buf_pos]) {
    case ',':
      ++buf_pos;
      // no break
    case ':':
    case '{':
    case '}':
    case '[':
    case ']':
      return true;
    default:
      return false;
    }
  }
  return true;
}

bool JSON_Tokenizer::check_for_literal(const char* p_literal)
{
  size_t len = strlen(p_literal);
  size_t start_pos = buf_pos;
  
  if (buf_len - buf_pos >= len && 
      0 == strncmp(buf_ptr + buf_pos, p_literal, len)) {
    buf_pos += len;
    if (!skip_white_spaces() || check_for_separator()) {
      return true;
    } else {
      // must be followed by a separator (or only white spaces until the buffer ends) -> undo buffer action
      buf_pos = start_pos;
    }
  }
  return false;
}

int JSON_Tokenizer::get_next_token(json_token_t* p_token, char** p_token_str, size_t* p_str_len)
{
  size_t start_pos = buf_pos;
  *p_token = JSON_TOKEN_NONE;
  if (0 != p_token_str && 0 != p_str_len) {
    *p_token_str = 0;
    *p_str_len = 0;
  }
  
  if (skip_white_spaces()) {
    char c = buf_ptr[buf_pos];
    switch (c) {
    case '{':
    case '[':
      *p_token = ('{' == c) ? JSON_TOKEN_OBJECT_START : JSON_TOKEN_ARRAY_START;
      ++buf_pos;
      break;
    case '}':
    case ']':
      ++buf_pos;
      if (skip_white_spaces() && !check_for_separator()) {
        // must be followed by a separator (or only white spaces until the buffer ends)
        *p_token = JSON_TOKEN_ERROR;
      } else {
        *p_token = ('}' == c) ? JSON_TOKEN_OBJECT_END : JSON_TOKEN_ARRAY_END;
      }
      break;
    case '\"': {
      // string value or field name
      size_t string_start_pos = buf_pos;
      if(!check_for_string()) {
        // invalid string value
        *p_token = JSON_TOKEN_ERROR;
        break;
      }
      size_t string_end_pos = ++buf_pos; // step over the string's ending quotes
      if (skip_white_spaces() && ':' == buf_ptr[buf_pos]) {
        // name token - don't include the starting and ending quotes
        *p_token = JSON_TOKEN_NAME;
        if (0 != p_token_str && 0 != p_str_len) {
          *p_token_str = buf_ptr + string_start_pos + 1;
          *p_str_len = string_end_pos - string_start_pos - 2;
        }
        ++buf_pos;
      } else if (check_for_separator()) {
        // value token - include the starting and ending quotes
        *p_token = JSON_TOKEN_STRING;
        if (0 != p_token_str && 0 != p_str_len) {
          *p_token_str = buf_ptr + string_start_pos;
          *p_str_len = string_end_pos - string_start_pos;
        }
      } else {
        // value token, but there is no separator after it -> error
        *p_token = JSON_TOKEN_ERROR;
        break;
      }
      break;
    } // case: string value or field name
    default:
      if (('0' <= buf_ptr[buf_pos] && '9' >= buf_ptr[buf_pos]) ||
          '-' == buf_ptr[buf_pos]) {
        // number value
        size_t number_start_pos = buf_pos;
        if (!check_for_number()) {
          // invalid number
          *p_token = JSON_TOKEN_ERROR;
          break;
        }
        size_t number_length = buf_pos - number_start_pos;
        if (skip_white_spaces() && !check_for_separator()) {
          // must be followed by a separator (or only white spaces until the buffer ends)
          *p_token = JSON_TOKEN_ERROR;
          break;
        }
        *p_token = JSON_TOKEN_NUMBER;
        if (0 != p_token_str && 0 != p_str_len) {
          *p_token_str = buf_ptr + number_start_pos;
          *p_str_len = number_length;
        }
        break;
      } // if (number value)
      else if (check_for_literal("true")) {
        *p_token = JSON_TOKEN_LITERAL_TRUE;
        break;
      } 
      else if (check_for_literal("false")) {
        *p_token = JSON_TOKEN_LITERAL_FALSE;
        break;
      } 
      else if (check_for_literal("null")) {
        *p_token = JSON_TOKEN_LITERAL_NULL;
        break;
      }
      else {
        *p_token = JSON_TOKEN_ERROR;
        break;
      }
    } // switch (current char)
  } // if (skip_white_spaces())
  
  return buf_pos - start_pos;
}

void JSON_Tokenizer::put_separator() 
{
  if (JSON_TOKEN_NAME != previous_token && JSON_TOKEN_NONE != previous_token &&
      JSON_TOKEN_ARRAY_START != previous_token && JSON_TOKEN_OBJECT_START != previous_token) {
    put_c(',');
    if (pretty) {
      put_c('\n');
      put_depth();
    }
  }
}

int JSON_Tokenizer::put_next_token(json_token_t p_token, const char* p_token_str)
{
  int start_len = buf_len;
  switch(p_token) {
  case JSON_TOKEN_OBJECT_START: 
  case JSON_TOKEN_ARRAY_START: {
    put_separator();
    put_c( (JSON_TOKEN_OBJECT_START == p_token) ? '{' : '[' );
    if (pretty) {
      put_c('\n');
      ++depth;
      put_depth();
    }
    break;
  }
  case JSON_TOKEN_OBJECT_END: 
  case JSON_TOKEN_ARRAY_END: {
    if (pretty) {
      if (JSON_TOKEN_OBJECT_START != previous_token && JSON_TOKEN_ARRAY_START != previous_token) {
        put_c('\n');
        --depth;
        put_depth();
      } else if (MAX_TABS >= depth) {
        // empty object or array -> remove the extra tab added at the start token
        --depth;
        --buf_len;
        buf_ptr[buf_len] = 0;
      }    
    }
    put_c( (JSON_TOKEN_OBJECT_END == p_token) ? '}' : ']' );
    break;
  }
  case JSON_TOKEN_NUMBER:
  case JSON_TOKEN_STRING:
    put_separator();
    put_s(p_token_str);
    break;
  case JSON_TOKEN_LITERAL_TRUE:
    put_separator();
    put_s("true");
    break;
  case JSON_TOKEN_LITERAL_FALSE:
    put_separator();
    put_s("false");
    break;
  case JSON_TOKEN_LITERAL_NULL:
    put_separator();
    put_s("null");
    break;
  case JSON_TOKEN_NAME:
    put_separator();
    put_c('\"');
    put_s(p_token_str);
    if (pretty) {
      put_s("\" : ");
    } else {
      put_s("\":");
    }
    break;
  default:
    return 0;
  }
  
  previous_token = p_token;
  return buf_len - start_len;
}

