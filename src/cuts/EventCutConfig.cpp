#include "cuts/EventCutConfig.h"
#include "YamlParser.h"
#include "TMath.h"
#include <map>
#include <string>
#include <iostream>

EventCutConfig& EventCutConfig::GetInstance() {
  static EventCutConfig instance;
  return instance;
}

EventCutConfig::EventCutConfig() {
  SetDefaults();
}

EventCutConfig::~EventCutConfig() {
}

void EventCutConfig::SetDefaults() {
  minVz = -100.0;
  maxVz = 100.0;
  maxVr = 2.0;
  vtxCenterX = 0.0;
  vtxCenterY = 0.0;
  minRefMult = 0.0;
  maxRefMult = 1000.0;
  maxVzDiff = 3.0;
  maxAbsVzVpd = 200.0;
  maxNTr = 0;  // no limit
}

Bool_t EventCutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t EventCutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename 
              << ", using default values" << std::endl;
    return kFALSE;
  }
  
  // Load values from map
  if (values.find("minVz") != values.end()) {
    minVz = YamlParser::ToDouble(values["minVz"], minVz);
  }
  if (values.find("maxVz") != values.end()) {
    maxVz = YamlParser::ToDouble(values["maxVz"], maxVz);
    if (values.find("minVz") == values.end()) {
      minVz = -maxVz;  // backward compat: only maxVz given => symmetric window
    }
  }
  if (values.find("maxVr") != values.end()) {
    maxVr = YamlParser::ToDouble(values["maxVr"], maxVr);
  }
  if (values.find("vtxCenterX") != values.end()) {
    vtxCenterX = YamlParser::ToDouble(values["vtxCenterX"], vtxCenterX);
  }
  if (values.find("vtxCenterY") != values.end()) {
    vtxCenterY = YamlParser::ToDouble(values["vtxCenterY"], vtxCenterY);
  }
  if (values.find("minRefMult") != values.end()) {
    minRefMult = YamlParser::ToDouble(values["minRefMult"], minRefMult);
  }
  if (values.find("maxRefMult") != values.end()) {
    maxRefMult = YamlParser::ToDouble(values["maxRefMult"], maxRefMult);
  }
  if (values.find("maxVzDiff") != values.end()) {
    maxVzDiff = YamlParser::ToDouble(values["maxVzDiff"], maxVzDiff);
  }
  if (values.find("maxAbsVzVpd") != values.end()) {
    maxAbsVzVpd = YamlParser::ToDouble(values["maxAbsVzVpd"], maxAbsVzVpd);
  }
  if (values.find("maxNTr") != values.end()) {
    maxNTr = YamlParser::ToInt(values["maxNTr"], maxNTr);
  }

  return kTRUE;
}

Double_t EventCutConfig::ComputeVr(Double_t vx, Double_t vy) const {
  Double_t dx = vx - vtxCenterX;
  Double_t dy = vy - vtxCenterY;
  return TMath::Sqrt(dx * dx + dy * dy);
}

