#include "cuts/PhiCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "kinematics.h"
#include "YamlParser.h"
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

PhiCutConfig& PhiCutConfig::GetInstance() {
  static PhiCutConfig instance;
  return instance;
}

PhiCutConfig::PhiCutConfig() {
  SetDefaults();
}

PhiCutConfig::~PhiCutConfig() {}

std::string PhiCutConfig::ToLower(const std::string& s) {
  std::string out = s;
  for (size_t i = 0; i < out.size(); i++) {
    if (out[i] >= 'A' && out[i] <= 'Z') out[i] = static_cast<char>(out[i] - 'A' + 'a');
  }
  return out;
}

void PhiCutConfig::SetDefaults() {
  nSigmaKaon = 2.0;
  minMass2Kaon = 0.15;
  maxMass2Kaon = 0.35;
  maxDCAKaon = 2.0;
  maxDCAKK = 999.0;
  useInvMassCut = kFALSE;
  minInvMass = 0.99;
  maxInvMass = 1.05;
  minOpeningAngle = 0.0;
  maxOpeningAngle = 0.5;
  minPairRapidity = -0.8;
  maxPairRapidity = 0.8;
  minPtEp = 0.15;
  maxPtEp = 2.0;
  maxEtaEp = 1.0;
  maxNTr = 0;

  rapidityFrameRequested = "auto";
  sqrtSNNGeV = 0.0;
  nucleonMassGeV = 0.938272;
  hasManualRapidityShift = kFALSE;
  rapidityShiftManual = 0.0;
  rapidityFrameEffective = "lab";
  rapidityShiftEffective = 0.0;
  rapidityShiftFromManual = kFALSE;
}

Bool_t PhiCutConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t PhiCutConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename << ", using default values" << std::endl;
    return kFALSE;
  }

  if (values.find("nSigmaKaon") != values.end()) {
    nSigmaKaon = YamlParser::ToDouble(values["nSigmaKaon"], nSigmaKaon);
  }
  if (values.find("minMass2Kaon") != values.end()) {
    minMass2Kaon = YamlParser::ToDouble(values["minMass2Kaon"], minMass2Kaon);
  }
  if (values.find("maxMass2Kaon") != values.end()) {
    maxMass2Kaon = YamlParser::ToDouble(values["maxMass2Kaon"], maxMass2Kaon);
  }
  if (values.find("maxDCAKaon") != values.end()) {
    maxDCAKaon = YamlParser::ToDouble(values["maxDCAKaon"], maxDCAKaon);
  }
  if (values.find("maxDCAKK") != values.end()) {
    maxDCAKK = YamlParser::ToDouble(values["maxDCAKK"], maxDCAKK);
  }
  if (values.find("useInvMassCut") != values.end()) {
    useInvMassCut = YamlParser::ToBool(values["useInvMassCut"], useInvMassCut);
  }
  if (values.find("minInvMass") != values.end()) {
    minInvMass = YamlParser::ToDouble(values["minInvMass"], minInvMass);
  }
  if (values.find("maxInvMass") != values.end()) {
    maxInvMass = YamlParser::ToDouble(values["maxInvMass"], maxInvMass);
  }
  if (values.find("minOpeningAngle") != values.end()) {
    minOpeningAngle = YamlParser::ToDouble(values["minOpeningAngle"], minOpeningAngle);
  }
  if (values.find("maxOpeningAngle") != values.end()) {
    maxOpeningAngle = YamlParser::ToDouble(values["maxOpeningAngle"], maxOpeningAngle);
  }
  if (values.find("minPairRapidity") != values.end()) {
    minPairRapidity = YamlParser::ToDouble(values["minPairRapidity"], minPairRapidity);
  }
  if (values.find("maxPairRapidity") != values.end()) {
    maxPairRapidity = YamlParser::ToDouble(values["maxPairRapidity"], maxPairRapidity);
  }
  if (values.find("minPtEp") != values.end()) {
    minPtEp = YamlParser::ToDouble(values["minPtEp"], minPtEp);
  }
  if (values.find("maxPtEp") != values.end()) {
    maxPtEp = YamlParser::ToDouble(values["maxPtEp"], maxPtEp);
  }
  if (values.find("maxEtaEp") != values.end()) {
    maxEtaEp = YamlParser::ToDouble(values["maxEtaEp"], maxEtaEp);
  }
  if (values.find("maxNTr") != values.end()) {
    maxNTr = YamlParser::ToInt(values["maxNTr"], maxNTr);
  }
  if (values.find("rapidityFrame") != values.end()) {
    rapidityFrameRequested = values["rapidityFrame"];
  }
  if (values.find("sqrtSNNGeV") != values.end()) {
    sqrtSNNGeV = YamlParser::ToDouble(values["sqrtSNNGeV"], sqrtSNNGeV);
  }
  if (values.find("nucleonMassGeV") != values.end()) {
    nucleonMassGeV = YamlParser::ToDouble(values["nucleonMassGeV"], nucleonMassGeV);
  }
  if (values.find("rapidityShift") != values.end()) {
    hasManualRapidityShift = kTRUE;
    rapidityShiftManual = YamlParser::ToDouble(values["rapidityShift"], rapidityShiftManual);
  }

  return kTRUE;
}

Bool_t PhiCutConfig::FinalizeRapidityFrame(const CentralityCutConfig& centrality) {
  const std::string frameReq = ToLower(rapidityFrameRequested);
  std::string frameUse = frameReq;

  if (frameReq == "auto") {
    const std::string centMode = ToLower(centrality.mode);
    if (centMode == "fxtmult") {
      frameUse = "cm";
    } else {
      frameUse = "lab";
    }
  } else if (frameReq != "lab" && frameReq != "cm") {
    std::cerr << "[PhiCutConfig] ERROR: rapidityFrame must be auto, lab, or cm (got '" << rapidityFrameRequested
              << "')" << std::endl;
    return kFALSE;
  }

  rapidityFrameEffective = frameUse;
  rapidityShiftEffective = 0.0;
  rapidityShiftFromManual = kFALSE;

  if (frameUse == "lab") {
    std::cout << "[PhiCutConfig] Pair rapidity frame: lab (y_ana = y_lab)" << std::endl;
    return kTRUE;
  }

  if (hasManualRapidityShift) {
    rapidityShiftEffective = rapidityShiftManual;
    rapidityShiftFromManual = kTRUE;
    std::cout << "[PhiCutConfig] Pair rapidity frame: cm (y_ana = y_lab - y_shift)" << std::endl;
    std::cout << "[PhiCutConfig]   y_shift = " << rapidityShiftEffective << " (manual override)" << std::endl;
    return kTRUE;
  }

  if (sqrtSNNGeV <= 0.) {
    std::cerr << "[PhiCutConfig] ERROR: cm/auto(FXT) rapidity requires sqrtSNNGeV > 0 or manual rapidityShift"
              << std::endl;
    return kFALSE;
  }

  double yShift = 0.;
  if (!BeamRapidityShiftFromSqrtSNN(sqrtSNNGeV, nucleonMassGeV, &yShift)) {
    std::cerr << "[PhiCutConfig] ERROR: invalid kinematics for sqrtSNNGeV=" << sqrtSNNGeV
              << " nucleonMassGeV=" << nucleonMassGeV << std::endl;
    return kFALSE;
  }

  rapidityShiftEffective = yShift;
  std::cout << "[PhiCutConfig] Pair rapidity frame: cm (y_ana = y_lab - y_shift)" << std::endl;
  std::cout << "[PhiCutConfig]   sqrtSNNGeV = " << sqrtSNNGeV << ", nucleonMassGeV = " << nucleonMassGeV
            << std::endl;
  std::cout << "[PhiCutConfig]   y_shift = " << rapidityShiftEffective << " (from sqrt(s_NN), fixed target)"
            << std::endl;
  return kTRUE;
}

Double_t PhiCutConfig::ApplyAnalysisRapidity(Double_t yLab) const {
  if (rapidityFrameEffective == "cm") {
    return yLab - rapidityShiftEffective;
  }
  return yLab;
}

std::string PhiCutConfig::GetRapidityFrameSummary() const {
  std::ostringstream oss;
  oss << "Pair rapidity: " << rapidityFrameEffective;
  if (rapidityFrameEffective == "cm") {
    oss << " (y_ana = y_lab - " << rapidityShiftEffective << ")";
    if (rapidityShiftFromManual) {
      oss << ", shift manual";
    } else if (sqrtSNNGeV > 0.) {
      oss << ", sqrtSNN=" << sqrtSNNGeV << " GeV";
    }
  } else {
    oss << " (y_ana = y_lab)";
  }
  return oss.str();
}
