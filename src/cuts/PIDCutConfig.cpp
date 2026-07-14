#include "cuts/PIDCutConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <iostream>

PIDCutConfig& PIDCutConfig::GetInstance() {
  static PIDCutConfig instance;
  return instance;
}

PIDCutConfig::PIDCutConfig() {
  SetDefaults();
}

PIDCutConfig::~PIDCutConfig() {
}

void PIDCutConfig::SetDefaults() {
  defaultNSigmaCut = 2.0;
  nSigmaPion = 2.0;
  nSigmaKaon = 2.0;
  nSigmaProton = 2.0;
  minMass2Pion = -0.1;
  maxMass2Pion = 0.1;
  minMass2Kaon = 0.15;
  maxMass2Kaon = 0.35;
  minMass2Proton = 0.7;
  maxMass2Proton = 1.1;
  requireTOF = kFALSE;
  pTofFallbackMax = 0.5;
  pMomKaonPID = 0.5;
  maxAbsDeltaOneOverBetaKaon = 0.03;
  tofUseMass2Cut = kTRUE;
  tofUseDeltaInvBetaCut = kFALSE;
  tofFallbackMode = "acceptLowPt";
}

Bool_t PIDCutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t PIDCutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename 
              << ", using default values" << std::endl;
    return kFALSE;
  }
  
  if (values.find("defaultNSigmaCut") != values.end()) {
    defaultNSigmaCut = YamlParser::ToDouble(values["defaultNSigmaCut"], defaultNSigmaCut);
  }
  if (values.find("nSigmaPion") != values.end()) {
    nSigmaPion = YamlParser::ToDouble(values["nSigmaPion"], nSigmaPion);
  }
  if (values.find("nSigmaKaon") != values.end()) {
    nSigmaKaon = YamlParser::ToDouble(values["nSigmaKaon"], nSigmaKaon);
  }
  if (values.find("nSigmaProton") != values.end()) {
    nSigmaProton = YamlParser::ToDouble(values["nSigmaProton"], nSigmaProton);
  }
  if (values.find("minMass2Pion") != values.end()) {
    minMass2Pion = YamlParser::ToDouble(values["minMass2Pion"], minMass2Pion);
  }
  if (values.find("maxMass2Pion") != values.end()) {
    maxMass2Pion = YamlParser::ToDouble(values["maxMass2Pion"], maxMass2Pion);
  }
  if (values.find("minMass2Kaon") != values.end()) {
    minMass2Kaon = YamlParser::ToDouble(values["minMass2Kaon"], minMass2Kaon);
  }
  if (values.find("maxMass2Kaon") != values.end()) {
    maxMass2Kaon = YamlParser::ToDouble(values["maxMass2Kaon"], maxMass2Kaon);
  }
  if (values.find("minMass2Proton") != values.end()) {
    minMass2Proton = YamlParser::ToDouble(values["minMass2Proton"], minMass2Proton);
  }
  if (values.find("maxMass2Proton") != values.end()) {
    maxMass2Proton = YamlParser::ToDouble(values["maxMass2Proton"], maxMass2Proton);
  }
  if (values.find("requireTOF") != values.end()) {
    requireTOF = YamlParser::ToBool(values["requireTOF"], requireTOF);
  }
  if (values.find("pTofFallbackMax") != values.end()) {
    pTofFallbackMax = YamlParser::ToDouble(values["pTofFallbackMax"], pTofFallbackMax);
  }
  if (values.find("pMomKaonPID") != values.end()) {
    pMomKaonPID = YamlParser::ToDouble(values["pMomKaonPID"], pMomKaonPID);
  }
  if (values.find("maxAbsDeltaOneOverBetaKaon") != values.end()) {
    maxAbsDeltaOneOverBetaKaon = YamlParser::ToDouble(values["maxAbsDeltaOneOverBetaKaon"], maxAbsDeltaOneOverBetaKaon);
  }
  if (values.find("tofUseMass2Cut") != values.end()) {
    tofUseMass2Cut = YamlParser::ToBool(values["tofUseMass2Cut"], tofUseMass2Cut);
  }
  if (values.find("tofUseDeltaInvBetaCut") != values.end()) {
    tofUseDeltaInvBetaCut = YamlParser::ToBool(values["tofUseDeltaInvBetaCut"], tofUseDeltaInvBetaCut);
  }
  if (values.find("tofFallbackMode") != values.end()) {
    tofFallbackMode = values["tofFallbackMode"];
  }
  
  return kTRUE;
}

