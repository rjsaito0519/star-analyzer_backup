#ifndef YAML_PARSER_H
#define YAML_PARSER_H

#include <map>
#include <string>
#include "Rtypes.h"

// Simple YAML-like parser utility
// Supports:
// - key: value format
// - Comments starting with #
// - Empty lines
// - Simple value types (Double, Int, Bool, String)

class YamlParser {
public:
  // Parse a YAML-like file and return key-value pairs
  // Returns true on success, false on error
  static Bool_t ParseFile(const Char_t* filename, std::map<std::string, std::string>& values);
  
  // Convert string value to Double_t
  static Double_t ToDouble(const std::string& value, Double_t defaultValue = 0.0);
  
  // Convert string value to Int_t
  static Int_t ToInt(const std::string& value, Int_t defaultValue = 0);
  
  // Convert string value to Bool_t
  static Bool_t ToBool(const std::string& value, Bool_t defaultValue = kFALSE);
  
private:
  // Trim whitespace from string
  static std::string Trim(const std::string& str);
  
  // Check if line is a comment or empty
  static Bool_t IsCommentOrEmpty(const std::string& line);
};

#endif

