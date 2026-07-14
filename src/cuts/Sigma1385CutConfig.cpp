#include "cuts/Sigma1385CutConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <iostream>

Sigma1385CutConfig& Sigma1385CutConfig::GetInstance() {
  static Sigma1385CutConfig instance;
  return instance;
}

Sigma1385CutConfig::Sigma1385CutConfig() {
  SetDefaults();
}

Sigma1385CutConfig::~Sigma1385CutConfig() {
}

void Sigma1385CutConfig::SetDefaults() {
  nSigmaPion = 2.0;
  nSigmaProton = 2.0;
  nSigmaPionForSigma = 2.0;
  minInvMass = 1.30;
  maxInvMass = 1.45;
}

Bool_t Sigma1385CutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t Sigma1385CutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename 
              << ", using default values" << std::endl;
    return kFALSE;
  }
  
  if (values.find("nSigmaPion") != values.end()) {
    nSigmaPion = YamlParser::ToDouble(values["nSigmaPion"], nSigmaPion);
  }
  if (values.find("nSigmaProton") != values.end()) {
    nSigmaProton = YamlParser::ToDouble(values["nSigmaProton"], nSigmaProton);
  }
  if (values.find("nSigmaPionForSigma") != values.end()) {
    nSigmaPionForSigma = YamlParser::ToDouble(values["nSigmaPionForSigma"], nSigmaPionForSigma);
  }
  if (values.find("minInvMass") != values.end()) {
    minInvMass = YamlParser::ToDouble(values["minInvMass"], minInvMass);
  }
  if (values.find("maxInvMass") != values.end()) {
    maxInvMass = YamlParser::ToDouble(values["maxInvMass"], maxInvMass);
  }
  
  return kTRUE;
}

