#include "cuts/TrackCutConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <iostream>

TrackCutConfig& TrackCutConfig::GetInstance() {
  static TrackCutConfig instance;
  return instance;
}

TrackCutConfig::TrackCutConfig() {
  SetDefaults();
}

TrackCutConfig::~TrackCutConfig() {
}

void TrackCutConfig::SetDefaults() {
  minNHitsFit = 15;
  minNHitsRatio = 0.52;
  minNHitsDedx = 10;
  maxDCA = 3.0;
  minEta = -2.5;
  maxEta = 2.5;
  minPt = 0.2;
  maxPt = 10.0;
  maxChi2 = 3.0;
  requirePrimaryTrack = kFALSE;
}

Bool_t TrackCutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t TrackCutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename 
              << ", using default values" << std::endl;
    return kFALSE;
  }
  
  if (values.find("minNHitsFit") != values.end()) {
    minNHitsFit = YamlParser::ToInt(values["minNHitsFit"], minNHitsFit);
  }
  if (values.find("minNHitsRatio") != values.end()) {
    minNHitsRatio = YamlParser::ToDouble(values["minNHitsRatio"], minNHitsRatio);
  }
  if (values.find("minNHitsDedx") != values.end()) {
    minNHitsDedx = YamlParser::ToInt(values["minNHitsDedx"], minNHitsDedx);
  }
  if (values.find("maxDCA") != values.end()) {
    maxDCA = YamlParser::ToDouble(values["maxDCA"], maxDCA);
  }
  if (values.find("minEta") != values.end()) {
    minEta = YamlParser::ToDouble(values["minEta"], minEta);
  }
  if (values.find("maxEta") != values.end()) {
    maxEta = YamlParser::ToDouble(values["maxEta"], maxEta);
  }
  if (values.find("minPt") != values.end()) {
    minPt = YamlParser::ToDouble(values["minPt"], minPt);
  }
  if (values.find("maxPt") != values.end()) {
    maxPt = YamlParser::ToDouble(values["maxPt"], maxPt);
  }
  if (values.find("maxChi2") != values.end()) {
    maxChi2 = YamlParser::ToDouble(values["maxChi2"], maxChi2);
  }
  if (values.find("requirePrimaryTrack") != values.end()) {
    requirePrimaryTrack = YamlParser::ToBool(values["requirePrimaryTrack"], requirePrimaryTrack);
  }
  
  return kTRUE;
}

