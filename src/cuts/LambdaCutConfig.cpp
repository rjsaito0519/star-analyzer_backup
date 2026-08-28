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
  nSigmaPionPos = 2.0;
  nSigmaPionNeg = 2.0;
  minDCAPionPos = 0.5;
  minDCAPionNeg = 0.8;
  minDCABachelor = 0.3;
  maxDaughterDCA = 1.0;
  maxDcaLambdaDaughters = -1.0;
  maxDcaLambdaBachelor = -1.0;
  maxDCAV0 = 1.0;
  minCosPointing = 0.995;
  maxPathLength = 100.0;
  minPtDaughter = 0.15;
  minDecayLengthXi = 0.0;
  lambdaMassWindow = 0.006;
  xiMassWindow = -1.0;
  requireDecayLengthOrder = kFALSE;
  fakeLambdaMean = 1.115683;
  fakeLambdaWindow = -1.0;
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

  Bool_t hasNSigmaPionPos = kFALSE;
  Bool_t hasNSigmaPionNeg = kFALSE;
  Bool_t hasMinDCAPionPos = kFALSE;
  Bool_t hasMinDCAPionNeg = kFALSE;
  if (values.find("nSigmaPionPos") != values.end()) {
    nSigmaPionPos = YamlParser::ToDouble(values["nSigmaPionPos"], nSigmaPionPos);
    hasNSigmaPionPos = kTRUE;
  }
  if (values.find("nSigmaPionNeg") != values.end()) {
    nSigmaPionNeg = YamlParser::ToDouble(values["nSigmaPionNeg"], nSigmaPionNeg);
    hasNSigmaPionNeg = kTRUE;
  }
  if (values.find("minDCAPionPos") != values.end()) {
    minDCAPionPos = YamlParser::ToDouble(values["minDCAPionPos"], minDCAPionPos);
    hasMinDCAPionPos = kTRUE;
  }
  if (values.find("minDCAPionNeg") != values.end()) {
    minDCAPionNeg = YamlParser::ToDouble(values["minDCAPionNeg"], minDCAPionNeg);
    hasMinDCAPionNeg = kTRUE;
  }
  // Legacy K0 configs: pi+ borrowed proton keys; pi- used nSigmaPion/minDCAPion.
  if (!hasNSigmaPionPos) nSigmaPionPos = nSigmaProton;
  if (!hasNSigmaPionNeg) nSigmaPionNeg = nSigmaPion;
  if (!hasMinDCAPionPos) minDCAPionPos = minDCAProton;
  if (!hasMinDCAPionNeg) minDCAPionNeg = minDCAPion;

  if (values.find("minDCABachelor") != values.end()) {
    minDCABachelor = YamlParser::ToDouble(values["minDCABachelor"], minDCABachelor);
  }
  if (values.find("maxDaughterDCA") != values.end()) {
    maxDaughterDCA = YamlParser::ToDouble(values["maxDaughterDCA"], maxDaughterDCA);
  }
  if (values.find("maxDcaLambdaDaughters") != values.end()) {
    maxDcaLambdaDaughters = YamlParser::ToDouble(values["maxDcaLambdaDaughters"], maxDcaLambdaDaughters);
  }
  if (values.find("maxDcaLambdaBachelor") != values.end()) {
    maxDcaLambdaBachelor = YamlParser::ToDouble(values["maxDcaLambdaBachelor"], maxDcaLambdaBachelor);
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
  if (values.find("minPtDaughter") != values.end()) {
    minPtDaughter = YamlParser::ToDouble(values["minPtDaughter"], minPtDaughter);
  }
  if (values.find("minDecayLengthXi") != values.end()) {
    minDecayLengthXi = YamlParser::ToDouble(values["minDecayLengthXi"], minDecayLengthXi);
  }
  if (values.find("lambdaMassWindow") != values.end()) {
    lambdaMassWindow = YamlParser::ToDouble(values["lambdaMassWindow"], lambdaMassWindow);
  }
  if (values.find("xiMassWindow") != values.end()) {
    xiMassWindow = YamlParser::ToDouble(values["xiMassWindow"], xiMassWindow);
  }
  if (values.find("requireDecayLengthOrder") != values.end()) {
    requireDecayLengthOrder = YamlParser::ToBool(values["requireDecayLengthOrder"], requireDecayLengthOrder);
  }
  if (values.find("fakeLambdaMean") != values.end()) {
    fakeLambdaMean = YamlParser::ToDouble(values["fakeLambdaMean"], fakeLambdaMean);
  }
  if (values.find("fakeLambdaWindow") != values.end()) {
    fakeLambdaWindow = YamlParser::ToDouble(values["fakeLambdaWindow"], fakeLambdaWindow);
  }
  if (values.find("minNHitsFit") != values.end()) {
    minNHitsFit = (Int_t)YamlParser::ToDouble(values["minNHitsFit"], (Double_t)minNHitsFit);
  }
  if (values.find("minNHitsRatio") != values.end()) {
    minNHitsRatio = YamlParser::ToDouble(values["minNHitsRatio"], minNHitsRatio);
  }

  return kTRUE;
}
