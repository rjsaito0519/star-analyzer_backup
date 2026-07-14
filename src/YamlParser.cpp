#include "YamlParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

Bool_t YamlParser::ParseFile(const Char_t* filename, std::map<std::string, std::string>& values) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "ERROR: Cannot open config file: " << filename << std::endl;
    return kFALSE;
  }
  
  values.clear();
  std::string line;
  Int_t lineNumber = 0;
  
  while (std::getline(file, line)) {
    lineNumber++;
    
    // Trim the line
    line = Trim(line);
    
    // Skip comments and empty lines
    if (IsCommentOrEmpty(line)) {
      continue;
    }
    
    // Find colon separator
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
      std::cerr << "WARNING: Line " << lineNumber << " in " << filename 
                << " does not contain ':' separator, skipping" << std::endl;
      continue;
    }
    
    // Extract key and value
    std::string key = Trim(line.substr(0, colonPos));
    std::string value = Trim(line.substr(colonPos + 1));
    
    // Remove inline comments (everything after # that is not inside quotes)
    size_t commentPos = value.find('#');
    if (commentPos != std::string::npos) {
      value = Trim(value.substr(0, commentPos));
    }
    
    if (key.empty()) {
      std::cerr << "WARNING: Line " << lineNumber << " in " << filename 
                << " has empty key, skipping" << std::endl;
      continue;
    }
    
    values[key] = value;
  }
  
  file.close();
  return kTRUE;
}

Double_t YamlParser::ToDouble(const std::string& value, Double_t defaultValue) {
  if (value.empty()) return defaultValue;
  
  try {
    return std::stod(value);
  } catch (const std::exception& e) {
    std::cerr << "WARNING: Cannot convert '" << value << "' to Double, using default: " 
              << defaultValue << std::endl;
    return defaultValue;
  }
}

Int_t YamlParser::ToInt(const std::string& value, Int_t defaultValue) {
  if (value.empty()) return defaultValue;
  
  try {
    return std::stoi(value);
  } catch (const std::exception& e) {
    std::cerr << "WARNING: Cannot convert '" << value << "' to Int, using default: " 
              << defaultValue << std::endl;
    return defaultValue;
  }
}

Bool_t YamlParser::ToBool(const std::string& value, Bool_t defaultValue) {
  if (value.empty()) return defaultValue;
  
  std::string lowerValue = value;
  std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
  
  if (lowerValue == "true" || lowerValue == "1" || lowerValue == "yes") {
    return kTRUE;
  } else if (lowerValue == "false" || lowerValue == "0" || lowerValue == "no") {
    return kFALSE;
  } else {
    std::cerr << "WARNING: Cannot convert '" << value << "' to Bool, using default: " 
              << (defaultValue ? "true" : "false") << std::endl;
    return defaultValue;
  }
}

std::string YamlParser::Trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return "";
  }
  size_t last = str.find_last_not_of(" \t");
  return str.substr(first, (last - first + 1));
}

Bool_t YamlParser::IsCommentOrEmpty(const std::string& line) {
  std::string trimmed = Trim(line);
  return trimmed.empty() || trimmed[0] == '#';
}

