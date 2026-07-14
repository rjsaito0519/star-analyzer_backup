#include "cuts/LambdaCutConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <iostream>

LambdaCutConfig& LambdaCutConfig::GetInstance() {
  static LambdaCutConfig instance;
  return instance;
}

LambdaCutConfig::LambdaCutConfig() {
  SetDefaults();
}

LambdaCutConfig::~LambdaCutConfig() {
}

void LambdaCutConfig::SetDefaults() {
  nSigmaProton = 2.0;
  nSigmaPion = 2.0;
  minDCAProton = 0.5;
  minDCAPion = 0.8;
  maxDaughterDCA = 1.0;
  maxDCAV0 = 1.0;
  minCosPointing = 0.995;
  maxPathLength = 100.0;
  minNHitsFit = 15;
  minNHitsRatio = 0.52;
}

Bool_t LambdaCutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t LambdaCutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename
              << ", using default values" << std::endl;
    return kFALSE;
  }

  if (values.find("nSigmaProton") != values.end()) {
    nSigmaProton = YamlParser::ToDouble(values["nSigmaProton"], nSigmaProton);
  }
  if (values.find("nSigmaPion") != values.end()) {
    nSigmaPion = YamlParser::ToDouble(values["nSigmaPion"], nSigmaPion);
  }
  if (values.find("minDCAProton") != values.end()) {
    minDCAProton = YamlParser::ToDouble(values["minDCAProton"], minDCAProton);
  }
  if (values.find("minDCAPion") != values.end()) {
    minDCAPion = YamlParser::ToDouble(values["minDCAPion"], minDCAPion);
  }
  if (values.find("maxDaughterDCA") != values.end()) {
    maxDaughterDCA = YamlParser::ToDouble(values["maxDaughterDCA"], maxDaughterDCA);
  }
  if (values.find("maxDCAV0") != values.end()) {
    maxDCAV0 = YamlParser::ToDouble(values["maxDCAV0"], maxDCAV0);
  }
  if (values.find("minCosPointing") != values.end()) {
    minCosPointing = YamlParser::ToDouble(values["minCosPointing"], minCosPointing);
  }
  if (values.find("maxPathLength") != values.end()) {
    maxPathLength = YamlParser::ToDouble(values["maxPathLength"], maxPathLength);
  }
  if (values.find("minNHitsFit") != values.end()) {
    minNHitsFit = (Int_t)YamlParser::ToDouble(values["minNHitsFit"], (Double_t)minNHitsFit);
  }
  if (values.find("minNHitsRatio") != values.end()) {
    minNHitsRatio = YamlParser::ToDouble(values["minNHitsRatio"], minNHitsRatio);
  }

  return kTRUE;
}
