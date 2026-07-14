#include "cuts/Lambda1520CutConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <iostream>

Lambda1520CutConfig& Lambda1520CutConfig::GetInstance() {
  static Lambda1520CutConfig instance;
  return instance;
}

Lambda1520CutConfig::Lambda1520CutConfig() {
  SetDefaults();
}

Lambda1520CutConfig::~Lambda1520CutConfig() {
}

void Lambda1520CutConfig::SetDefaults() {
  nSigmaProton = 2.0;
  nSigmaKaon = 2.0;
  minInvMass = 1.45;
  maxInvMass = 1.60;
}

Bool_t Lambda1520CutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t Lambda1520CutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename 
              << ", using default values" << std::endl;
    return kFALSE;
  }
  
  if (values.find("nSigmaProton") != values.end()) {
    nSigmaProton = YamlParser::ToDouble(values["nSigmaProton"], nSigmaProton);
  }
  if (values.find("nSigmaKaon") != values.end()) {
    nSigmaKaon = YamlParser::ToDouble(values["nSigmaKaon"], nSigmaKaon);
  }
  if (values.find("minInvMass") != values.end()) {
    minInvMass = YamlParser::ToDouble(values["minInvMass"], minInvMass);
  }
  if (values.find("maxInvMass") != values.end()) {
    maxInvMass = YamlParser::ToDouble(values["maxInvMass"], maxInvMass);
  }
  
  return kTRUE;
}

