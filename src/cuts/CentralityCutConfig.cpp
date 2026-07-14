#include "cuts/CentralityCutConfig.h"
#include "YamlParser.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

CentralityCutConfig& CentralityCutConfig::GetInstance() {
  static CentralityCutConfig instance;
  return instance;
}

CentralityCutConfig::CentralityCutConfig() {
  SetDefaults();
}

CentralityCutConfig::~CentralityCutConfig() {}

void CentralityCutConfig::SetDefaults() {
  enabled = kTRUE;
  mode = "refmult";
  rejectBadRun = kTRUE;
  rejectPileup = kTRUE;
  useWeight = kTRUE;
  requireValidCentrality = kTRUE;
  verbose = kFALSE;
  fillCentralityQA = kTRUE;
  cent9MaxRefMultCorrBin = -1;
  cent9MaxRefMultCorr = -1.0;
  acceptedCentBins.clear();
  for (Int_t i = 0; i < 9; i++) {
    acceptedCentBins.push_back(i);
  }
}

void CentralityCutConfig::ParseAcceptedBins(const std::string& value, std::vector<Int_t>& out) {
  out.clear();
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) continue;
    out.push_back(static_cast<Int_t>(std::atoi(token.c_str())));
  }
}

Bool_t CentralityCutConfig::IsCentBinAccepted(Int_t cent9) const {
  if (acceptedCentBins.empty()) return kTRUE;
  for (size_t i = 0; i < acceptedCentBins.size(); i++) {
    if (acceptedCentBins[i] == cent9) return kTRUE;
  }
  return kFALSE;
}

Bool_t CentralityCutConfig::IsRefMultCorrAccepted(Int_t cent9, Double_t refMultCorr) const {
  if (cent9MaxRefMultCorrBin < 0 || cent9MaxRefMultCorr <= 0.0) return kTRUE;
  if (cent9 != cent9MaxRefMultCorrBin) return kTRUE;
  if (refMultCorr < 0.0) return kTRUE;
  return refMultCorr <= cent9MaxRefMultCorr;
}

Bool_t CentralityCutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t CentralityCutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename << ", using centrality defaults" << std::endl;
    return kFALSE;
  }

  if (values.find("enabled") != values.end()) {
    enabled = YamlParser::ToBool(values["enabled"], enabled);
  }
  if (values.find("mode") != values.end()) {
    mode = values["mode"];
  }
  if (values.find("rejectBadRun") != values.end()) {
    rejectBadRun = YamlParser::ToBool(values["rejectBadRun"], rejectBadRun);
  }
  if (values.find("rejectPileup") != values.end()) {
    rejectPileup = YamlParser::ToBool(values["rejectPileup"], rejectPileup);
  }
  if (values.find("useWeight") != values.end()) {
    useWeight = YamlParser::ToBool(values["useWeight"], useWeight);
  }
  if (values.find("requireValidCentrality") != values.end()) {
    requireValidCentrality = YamlParser::ToBool(values["requireValidCentrality"], requireValidCentrality);
  }
  if (values.find("verbose") != values.end()) {
    verbose = YamlParser::ToBool(values["verbose"], verbose);
  }
  if (values.find("acceptedCentBins") != values.end()) {
    ParseAcceptedBins(values["acceptedCentBins"], acceptedCentBins);
  }
  if (values.find("fillCentralityQA") != values.end()) {
    fillCentralityQA = YamlParser::ToBool(values["fillCentralityQA"], fillCentralityQA);
  }
  if (values.find("fillPhiVsCentrality") != values.end()) {
    fillCentralityQA = YamlParser::ToBool(values["fillPhiVsCentrality"], fillCentralityQA);
  }
  if (values.find("cent9MaxRefMultCorrBin") != values.end()) {
    cent9MaxRefMultCorrBin = YamlParser::ToInt(values["cent9MaxRefMultCorrBin"], cent9MaxRefMultCorrBin);
  }
  if (values.find("cent9MaxRefMultCorr") != values.end()) {
    cent9MaxRefMultCorr = YamlParser::ToDouble(values["cent9MaxRefMultCorr"], cent9MaxRefMultCorr);
  }

  return kTRUE;
}
