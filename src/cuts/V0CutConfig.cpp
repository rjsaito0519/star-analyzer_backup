#include "cuts/V0CutConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <iostream>

V0CutConfig& V0CutConfig::GetInstance() {
  static V0CutConfig instance;
  return instance;
}

V0CutConfig::V0CutConfig() {
  SetDefaults();
}

V0CutConfig::~V0CutConfig() {
}

void V0CutConfig::SetDefaults() {
  minDaughterDCA = 0.0;
  maxDaughterDCA = 0.5;
  minDecayLength = 2.0;
  maxDecayLength = 100.0;
  minPointingAngle = 0.0;
  maxPointingAngle = 0.05;
  maxDCAtoPV = 0.5;
  lambdaMassWindow = 0.010;
  lambdaMass = 1.115683;
}

Bool_t V0CutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t V0CutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename 
              << ", using default values" << std::endl;
    return kFALSE;
  }
  
  if (values.find("minDaughterDCA") != values.end()) {
    minDaughterDCA = YamlParser::ToDouble(values["minDaughterDCA"], minDaughterDCA);
  }
  if (values.find("maxDaughterDCA") != values.end()) {
    maxDaughterDCA = YamlParser::ToDouble(values["maxDaughterDCA"], maxDaughterDCA);
  }
  if (values.find("minDecayLength") != values.end()) {
    minDecayLength = YamlParser::ToDouble(values["minDecayLength"], minDecayLength);
  }
  if (values.find("maxDecayLength") != values.end()) {
    maxDecayLength = YamlParser::ToDouble(values["maxDecayLength"], maxDecayLength);
  }
  if (values.find("minPointingAngle") != values.end()) {
    minPointingAngle = YamlParser::ToDouble(values["minPointingAngle"], minPointingAngle);
  }
  if (values.find("maxPointingAngle") != values.end()) {
    maxPointingAngle = YamlParser::ToDouble(values["maxPointingAngle"], maxPointingAngle);
  }
  if (values.find("maxDCAtoPV") != values.end()) {
    maxDCAtoPV = YamlParser::ToDouble(values["maxDCAtoPV"], maxDCAtoPV);
  }
  if (values.find("lambdaMassWindow") != values.end()) {
    lambdaMassWindow = YamlParser::ToDouble(values["lambdaMassWindow"], lambdaMassWindow);
  }
  if (values.find("lambdaMass") != values.end()) {
    lambdaMass = YamlParser::ToDouble(values["lambdaMass"], lambdaMass);
  }
  
  return kTRUE;
}

