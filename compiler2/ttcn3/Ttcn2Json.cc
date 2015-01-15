#include "Ttcn2Json.hh"

#include "compiler.h"

#include "../AST.hh"
#include "../../common/JSON_Tokenizer.hh"

// forward declarations
namespace Common {
  class Type;
}

namespace Ttcn {

Ttcn2Json::Ttcn2Json(Common::Modules* p_modules, const char* p_schema_name)
: modules(p_modules)
{
  boolean is_temporary;
  FILE* file = open_output_file(p_schema_name, &is_temporary);
  
  JSON_Tokenizer json(true);
  
  create_schema(json);
  
  fprintf(file, "%s\n", json.get_buffer());
  
  close_output_file(p_schema_name, file, is_temporary, 0);
}

void Ttcn2Json::create_schema(JSON_Tokenizer& json)
{
  // top-level object start
  json.put_next_token(JSON_TOKEN_OBJECT_START, NULL);
  
  // start of type definitions
  json.put_next_token(JSON_TOKEN_NAME, "definitions");
  json.put_next_token(JSON_TOKEN_OBJECT_START, NULL);
  
  // insert module names and schemas for types
  modules->add_types_to_json_schema(json);
  
  // end of type definitions
  json.put_next_token(JSON_TOKEN_OBJECT_END, NULL);
  
  // top-level "anyOf" structure containing references to the types the schema validates
  json.put_next_token(JSON_TOKEN_NAME, "anyOf");
  json.put_next_token(JSON_TOKEN_ARRAY_START, NULL);
  
  // gather type references and JSON encoding/decoding function data
  map<Common::Type*, JSON_Tokenizer> json_refs;
  modules->add_func_to_json_schema(json_refs);
  
  // close schema segments and add them to the main schema
  for (size_t i = 0; i < json_refs.size(); ++i) {
    JSON_Tokenizer* segment = json_refs.get_nth_elem(i);
    segment->put_next_token(JSON_TOKEN_OBJECT_END, NULL);
    insert_schema(json, *segment);
    delete segment;
  }
  json_refs.clear();
  
  // end of the "anyOf" structure
  json.put_next_token(JSON_TOKEN_ARRAY_END, NULL);
  
  // top-level object end
  json.put_next_token(JSON_TOKEN_OBJECT_END, NULL);
}

void Ttcn2Json::insert_schema(JSON_Tokenizer& to, JSON_Tokenizer& from)
{
  json_token_t token = JSON_TOKEN_NONE;
  char* value_str = NULL;
  size_t value_len = 0;
  char temp = 0;
  
  do {
    from.get_next_token(&token, &value_str, &value_len);
    
    if (token == JSON_TOKEN_ERROR) {
      FATAL_ERROR("Ttcn2Json::insert_schema");
    }
    
    if (value_str != NULL) {
      // put_next_token expects a null terminated string, save the next character
      // and set it to null
      temp = value_str[value_len];
      value_str[value_len] = 0;
    }
    
    to.put_next_token(token, value_str);
    
    if (value_str != NULL) {
      // put the original character back to its place
      value_str[value_len] = temp;
    }
  } while (token != JSON_TOKEN_NONE);
}

} // namespace

