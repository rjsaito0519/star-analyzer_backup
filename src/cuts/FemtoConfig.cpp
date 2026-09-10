#include "cuts/FemtoConfig.h"
#include "YamlParser.h"
#include <iostream>
#include <sstream>

namespace {
std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::vector<std::string> SplitComma(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    item = Trim(item);
    if (!item.empty()) out.push_back(item);
  }
  return out;
}

Int_t MaxChannelIndexInYaml(const std::map<std::string, std::string>& values) {
  Int_t maxIdx = -1;
  for (std::map<std::string, std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
    const std::string& key = it->first;
    if (key.size() < 10 || key.compare(0, 8, "channel_") != 0) continue;
    const size_t end = key.find('_', 8);
    if (end == std::string::npos || end <= 8) continue;
    const Int_t idx = YamlParser::ToInt(key.substr(8, end - 8), -1);
    if (idx > maxIdx) maxIdx = idx;
  }
  return maxIdx;
}
}  // namespace

FemtoConfig& FemtoConfig::GetInstance() {
  static FemtoConfig instance;
  return instance;
}

FemtoConfig::FemtoConfig() { SetDefaults(); }

FemtoConfig::~FemtoConfig() {}

void FemtoConfig::SetDefaults() {
  species.clear();
  channels.clear();

  protonChargeMode = "positive";
  protonMaxDca = 1.0;
  protonMinPtPre = 0.2;
  protonMinPtPair = 0.4;
  protonMaxPtPair = 2.0;
  protonMaxAbsEta = 2.0;
  protonMaxAbsNSigma = 2.0;
  protonMinNHitsFit = 15;
  protonMinNHitsRatio = 0.52;
  protonTofMomentumThreshold = 2.0;
  protonMinMass2 = 0.6;
  protonMaxMass2 = 1.2;
  protonMinRapidityCm = -0.5;
  protonMaxRapidityCm = 0.0;

  kaonMinusMaxDca = 2.0;
  kaonMinusMinPtPre = 0.2;
  kaonMinusMinPtPair = 0.4;
  kaonMinusMaxPtPair = 2.0;
  kaonMinusMaxAbsEta = 2.0;
  kaonMinusMaxAbsNSigma = 3.0;
  kaonMinusMinNHitsFit = 15;
  kaonMinusMinNHitsRatio = 0.52;
  kaonMinusTofMomentumThreshold = 2.0;
  kaonMinusMinMass2 = 0.15;
  kaonMinusMaxMass2 = 0.45;
  kaonMinusMinRapidityCm = -1.0;
  kaonMinusMaxRapidityCm = 0.0;

  he4MaxDca = 1.0;
  he4MinPMom = 0.0;
  he4MaxPMom = 99.0;
  he4MinPtPre = 0.2;
  he4MaxPtPre = 99.0;
  he4MinPtPair = 0.4;
  he4MaxPtPair = 2.0;
  he4MaxAbsEta = 2.0;
  he4MaxAbsNSigma = 2.0;
  he4MinNHitsFit = 15;
  he4MinNHitsRatio = 0.52;
  he4TofMomentumThreshold = 2.0;
  he4MinMass2 = 0.01;
  he4MaxMass2 = 10.0;
  he4MinRapidityCm = -1.0;
  he4MaxRapidityCm = 0.0;

  deuteronMaxDca = 1.0;
  deuteronMinPMom = 0.0;
  deuteronMaxPMom = 99.0;
  deuteronMinPtPre = 0.2;
  deuteronMaxPtPre = 99.0;
  deuteronMinPtPair = 0.4;
  deuteronMaxPtPair = 2.0;
  deuteronMaxAbsEta = 2.0;
  deuteronMaxAbsNSigma = 2.0;
  deuteronMinNHitsFit = 15;
  deuteronMinNHitsRatio = 0.52;
  deuteronTofMomentumThreshold = 2.0;
  deuteronMinMass2 = 0.01;
  deuteronMaxMass2 = 10.0;
  deuteronMinRapidityCm = -1.0;
  deuteronMaxRapidityCm = 0.0;

  tritonMaxDca = 1.0;
  tritonMinPMom = 0.0;
  tritonMaxPMom = 99.0;
  tritonMinPtPre = 0.2;
  tritonMaxPtPre = 99.0;
  tritonMinPtPair = 0.4;
  tritonMaxPtPair = 2.0;
  tritonMaxAbsEta = 2.0;
  tritonMaxAbsNSigma = 2.0;
  tritonMinNHitsFit = 15;
  tritonMinNHitsRatio = 0.52;
  tritonTofMomentumThreshold = 99.0;
  tritonMinMass2 = 0.01;
  tritonMaxMass2 = 10.0;
  tritonMinRapidityCm = -1.0;
  tritonMaxRapidityCm = 0.0;

  he3MaxDca = 1.0;
  he3MinPMom = 0.0;
  he3MaxPMom = 99.0;
  he3MinPtPre = 0.2;
  he3MaxPtPre = 99.0;
  he3MinPtPair = 0.4;
  he3MaxPtPair = 2.0;
  he3MaxAbsEta = 2.0;
  he3MaxAbsNSigma = 2.0;
  he3MinNHitsFit = 15;
  he3MinNHitsRatio = 0.52;
  he3TofMomentumThreshold = 99.0;
  he3MinMass2 = 0.01;
  he3MaxMass2 = 10.0;
  he3MinRapidityCm = -1.0;
  he3MaxRapidityCm = 0.0;

  rotationEnabled = kFALSE;
  fullyMixedEnabled = kFALSE;
  fullyMixedSpeciesKey = "phi_mix";
  fullyMixedParticleKey = "phi_fully_mixed";
  fullyMixedMaxCandidates = 2000;
  fullyMixedSamplingSeed = 314159;
  rotationSpeciesKey = "phi_rot";
  rotationParticleKey = "phi_rotation";
  rotationN = 10;
  rotationMinAngle = 0.0;
  rotationMaxAngle = 6.283185307179586;
  rotationSeed = 0;
  cfRebinFactor = 1;
  cfCent9Min = 2;
  cfCent9Max = 8;
  cfCentSlices.clear();
  for (Int_t i = 0; i <= 8; ++i) {
    CfCentSlice sl;
    sl.id = "cent9_" + std::to_string(i);
    sl.cent9Min = i;
    sl.cent9Max = i;
    cfCentSlices.push_back(sl);
  }
  struct PctDef {
    const char* id;
    Int_t cmin;
    Int_t cmax;
  };
  static const PctDef kPctDefs[] = {
      {"pct_0_10", 7, 8}, {"pct_0_20", 6, 8}, {"pct_0_30", 5, 8},
      {"pct_0_40", 4, 8}, {"pct_0_50", 3, 8}, {"pct_0_60", 2, 8},
  };
  for (size_t ip = 0; ip < sizeof(kPctDefs) / sizeof(kPctDefs[0]); ++ip) {
    CfCentSlice sl;
    sl.id = kPctDefs[ip].id;
    sl.cent9Min = kPctDefs[ip].cmin;
    sl.cent9Max = kPctDefs[ip].cmax;
    cfCentSlices.push_back(sl);
  }
  cfCentSlicesQaPdfInclude.clear();
  cfCentSlicesQaPdfInclude.push_back("pct_0_10");
  cfCentSlicesQaPdfInclude.push_back("pct_0_20");
  cfCentSlicesQaPdfInclude.push_back("pct_0_30");
  cfPdfExcludeQaSlices = kTRUE;
  sidebandSubtractAlpha = 1.0;
  sidebandAlphaMode = "fixed";
  negativeBinPolicy = "zero";

  purityFitUseConstantBkg = kTRUE;
  purityFitGaussSigmaMin = 0.002;
  purityFitGaussSigmaMax = 0.020;
  purityMinKstar = 0.0;
  purityMaxKstar = 0.65;
  purityMinEntriesPerBin = 20;
  purityClampMin = 0.05;
  purityClampMax = 1.0;
  cfBkgMode = "me_mass";

  cfSubtractionMode = "none";
  cfSubPurityMode = "fit_slice";
  cfSubPurityFixed = 0.7;
  cfSubSidebandCombine = "sumLR";
  cfSubWriteSidecarRoot = kFALSE;
  cfSubLowStatsRebinExtra = 1;

  kstarMassFitCfEnabled = kTRUE;
  kstarMassFitCfTemplate = "rot";
  kstarMassFitCfCrossCheck = kTRUE;
  kstarMassFitCfFitMassMin = 0.99;
  kstarMassFitCfFitMassMax = 1.06;
  kstarMassFitCfKstarBinWidth = 0.050;
  kstarMassFitCfLowKstarMergeBins = 1;
  kstarMassFitCfAlphaMassMin = 1.04;
  kstarMassFitCfAlphaMassMax = 1.06;
  kstarMassFitCfWriteSidecar = kTRUE;
  legacyCfPagesEnabled = kFALSE;
  purityDirectFitModel = "gaus_pol2";

  enableHKaonTwoBody = kFALSE;
  enableKuboTriplet = kFALSE;
  enableKuboGenuine = kFALSE;
  kuboStoreFullMass = kFALSE;

  phiNearTrackQaEnabled = kFALSE;
  phiNearTrackSignalChannel = "phi_proton_signal";
  phiNearTrackMaxKstarLoose = 1.0;
  phiNearTrackMaxKstarTight = 0.3;
  phiNearTrackMassHyp = "pion";

  SpeciesDef proton;
  proton.key = "proton";
  proton.builderType = "track";
  proton.particleKey = "proton";
  species[proton.key] = proton;

  SpeciesDef phi;
  phi.key = "phi";
  phi.builderType = "resonance";
  phi.particleKey = "phi";
  species[phi.key] = phi;

  ChannelDef ch;
  ch.name = "phi_proton";
  ch.partA = "phi";
  ch.partB = "proton";
  ch.enabled = kTRUE;
  ch.doMixing = kTRUE;
  ch.signalMin = 1.01;
  ch.signalMax = 1.03;
  ch.normQMin = 0.2;
  ch.normQMax = 0.3;
  channels.push_back(ch);
}

void FemtoConfig::ApplyYamlValues(const std::map<std::string, std::string>& values) {
  if (values.find("protonChargeMode") != values.end()) protonChargeMode = values.at("protonChargeMode");
  if (values.find("protonMaxDca") != values.end()) protonMaxDca = YamlParser::ToDouble(values.at("protonMaxDca"), protonMaxDca);
  if (values.find("protonMinPtPre") != values.end()) protonMinPtPre = YamlParser::ToDouble(values.at("protonMinPtPre"), protonMinPtPre);
  if (values.find("protonMinPtPair") != values.end()) protonMinPtPair = YamlParser::ToDouble(values.at("protonMinPtPair"), protonMinPtPair);
  if (values.find("protonMaxPtPair") != values.end()) protonMaxPtPair = YamlParser::ToDouble(values.at("protonMaxPtPair"), protonMaxPtPair);
  if (values.find("protonMaxAbsEta") != values.end()) protonMaxAbsEta = YamlParser::ToDouble(values.at("protonMaxAbsEta"), protonMaxAbsEta);
  if (values.find("protonMaxAbsNSigma") != values.end()) protonMaxAbsNSigma = YamlParser::ToDouble(values.at("protonMaxAbsNSigma"), protonMaxAbsNSigma);
  if (values.find("protonMinNHitsFit") != values.end()) protonMinNHitsFit = YamlParser::ToInt(values.at("protonMinNHitsFit"), protonMinNHitsFit);
  if (values.find("protonMinNHitsRatio") != values.end()) protonMinNHitsRatio = YamlParser::ToDouble(values.at("protonMinNHitsRatio"), protonMinNHitsRatio);
  if (values.find("protonTofMomentumThreshold") != values.end()) {
    protonTofMomentumThreshold = YamlParser::ToDouble(values.at("protonTofMomentumThreshold"), protonTofMomentumThreshold);
  }
  if (values.find("protonMinMass2") != values.end()) protonMinMass2 = YamlParser::ToDouble(values.at("protonMinMass2"), protonMinMass2);
  if (values.find("protonMaxMass2") != values.end()) protonMaxMass2 = YamlParser::ToDouble(values.at("protonMaxMass2"), protonMaxMass2);
  if (values.find("protonMinRapidityCm") != values.end()) protonMinRapidityCm = YamlParser::ToDouble(values.at("protonMinRapidityCm"), protonMinRapidityCm);
  if (values.find("protonMaxRapidityCm") != values.end()) protonMaxRapidityCm = YamlParser::ToDouble(values.at("protonMaxRapidityCm"), protonMaxRapidityCm);

  if (values.find("kaonMinusMaxDca") != values.end()) {
    kaonMinusMaxDca = YamlParser::ToDouble(values.at("kaonMinusMaxDca"), kaonMinusMaxDca);
  }
  if (values.find("kaonMinusMinPtPre") != values.end()) {
    kaonMinusMinPtPre = YamlParser::ToDouble(values.at("kaonMinusMinPtPre"), kaonMinusMinPtPre);
  }
  if (values.find("kaonMinusMinPtPair") != values.end()) {
    kaonMinusMinPtPair = YamlParser::ToDouble(values.at("kaonMinusMinPtPair"), kaonMinusMinPtPair);
  }
  if (values.find("kaonMinusMaxPtPair") != values.end()) {
    kaonMinusMaxPtPair = YamlParser::ToDouble(values.at("kaonMinusMaxPtPair"), kaonMinusMaxPtPair);
  }
  if (values.find("kaonMinusMaxAbsEta") != values.end()) {
    kaonMinusMaxAbsEta = YamlParser::ToDouble(values.at("kaonMinusMaxAbsEta"), kaonMinusMaxAbsEta);
  }
  if (values.find("kaonMinusMaxAbsNSigma") != values.end()) {
    kaonMinusMaxAbsNSigma = YamlParser::ToDouble(values.at("kaonMinusMaxAbsNSigma"), kaonMinusMaxAbsNSigma);
  }
  if (values.find("kaonMinusMinNHitsFit") != values.end()) {
    kaonMinusMinNHitsFit = YamlParser::ToInt(values.at("kaonMinusMinNHitsFit"), kaonMinusMinNHitsFit);
  }
  if (values.find("kaonMinusMinNHitsRatio") != values.end()) {
    kaonMinusMinNHitsRatio = YamlParser::ToDouble(values.at("kaonMinusMinNHitsRatio"), kaonMinusMinNHitsRatio);
  }
  if (values.find("kaonMinusTofMomentumThreshold") != values.end()) {
    kaonMinusTofMomentumThreshold =
        YamlParser::ToDouble(values.at("kaonMinusTofMomentumThreshold"), kaonMinusTofMomentumThreshold);
  }
  if (values.find("kaonMinusMinMass2") != values.end()) {
    kaonMinusMinMass2 = YamlParser::ToDouble(values.at("kaonMinusMinMass2"), kaonMinusMinMass2);
  }
  if (values.find("kaonMinusMaxMass2") != values.end()) {
    kaonMinusMaxMass2 = YamlParser::ToDouble(values.at("kaonMinusMaxMass2"), kaonMinusMaxMass2);
  }
  if (values.find("kaonMinusMinRapidityCm") != values.end()) {
    kaonMinusMinRapidityCm = YamlParser::ToDouble(values.at("kaonMinusMinRapidityCm"), kaonMinusMinRapidityCm);
  }
  if (values.find("kaonMinusMaxRapidityCm") != values.end()) {
    kaonMinusMaxRapidityCm = YamlParser::ToDouble(values.at("kaonMinusMaxRapidityCm"), kaonMinusMaxRapidityCm);
  }

  if (values.find("he4MaxDca") != values.end()) he4MaxDca = YamlParser::ToDouble(values.at("he4MaxDca"), he4MaxDca);
  if (values.find("he4MinPMom") != values.end()) he4MinPMom = YamlParser::ToDouble(values.at("he4MinPMom"), he4MinPMom);
  if (values.find("he4MaxPMom") != values.end()) he4MaxPMom = YamlParser::ToDouble(values.at("he4MaxPMom"), he4MaxPMom);
  if (values.find("he4MinPtPre") != values.end()) he4MinPtPre = YamlParser::ToDouble(values.at("he4MinPtPre"), he4MinPtPre);
  if (values.find("he4MaxPtPre") != values.end()) he4MaxPtPre = YamlParser::ToDouble(values.at("he4MaxPtPre"), he4MaxPtPre);
  if (values.find("he4MinPtPair") != values.end()) he4MinPtPair = YamlParser::ToDouble(values.at("he4MinPtPair"), he4MinPtPair);
  if (values.find("he4MaxPtPair") != values.end()) he4MaxPtPair = YamlParser::ToDouble(values.at("he4MaxPtPair"), he4MaxPtPair);
  if (values.find("he4MaxAbsEta") != values.end()) he4MaxAbsEta = YamlParser::ToDouble(values.at("he4MaxAbsEta"), he4MaxAbsEta);
  if (values.find("he4MaxAbsNSigma") != values.end()) he4MaxAbsNSigma = YamlParser::ToDouble(values.at("he4MaxAbsNSigma"), he4MaxAbsNSigma);
  if (values.find("he4MinNHitsFit") != values.end()) he4MinNHitsFit = YamlParser::ToInt(values.at("he4MinNHitsFit"), he4MinNHitsFit);
  if (values.find("he4MinNHitsRatio") != values.end()) he4MinNHitsRatio = YamlParser::ToDouble(values.at("he4MinNHitsRatio"), he4MinNHitsRatio);
  if (values.find("he4TofMomentumThreshold") != values.end()) {
    he4TofMomentumThreshold = YamlParser::ToDouble(values.at("he4TofMomentumThreshold"), he4TofMomentumThreshold);
  }
  if (values.find("he4MinMass2") != values.end()) he4MinMass2 = YamlParser::ToDouble(values.at("he4MinMass2"), he4MinMass2);
  if (values.find("he4MaxMass2") != values.end()) he4MaxMass2 = YamlParser::ToDouble(values.at("he4MaxMass2"), he4MaxMass2);
  if (values.find("he4MinRapidityCm") != values.end()) he4MinRapidityCm = YamlParser::ToDouble(values.at("he4MinRapidityCm"), he4MinRapidityCm);
  if (values.find("he4MaxRapidityCm") != values.end()) he4MaxRapidityCm = YamlParser::ToDouble(values.at("he4MaxRapidityCm"), he4MaxRapidityCm);

  if (values.find("deuteronMaxDca") != values.end()) {
    deuteronMaxDca = YamlParser::ToDouble(values.at("deuteronMaxDca"), deuteronMaxDca);
  }
  if (values.find("deuteronMinPMom") != values.end()) {
    deuteronMinPMom = YamlParser::ToDouble(values.at("deuteronMinPMom"), deuteronMinPMom);
  }
  if (values.find("deuteronMaxPMom") != values.end()) {
    deuteronMaxPMom = YamlParser::ToDouble(values.at("deuteronMaxPMom"), deuteronMaxPMom);
  }
  if (values.find("deuteronMinPtPre") != values.end()) {
    deuteronMinPtPre = YamlParser::ToDouble(values.at("deuteronMinPtPre"), deuteronMinPtPre);
  }
  if (values.find("deuteronMaxPtPre") != values.end()) {
    deuteronMaxPtPre = YamlParser::ToDouble(values.at("deuteronMaxPtPre"), deuteronMaxPtPre);
  }
  if (values.find("deuteronMinPtPair") != values.end()) {
    deuteronMinPtPair = YamlParser::ToDouble(values.at("deuteronMinPtPair"), deuteronMinPtPair);
  }
  if (values.find("deuteronMaxPtPair") != values.end()) {
    deuteronMaxPtPair = YamlParser::ToDouble(values.at("deuteronMaxPtPair"), deuteronMaxPtPair);
  }
  if (values.find("deuteronMaxAbsEta") != values.end()) {
    deuteronMaxAbsEta = YamlParser::ToDouble(values.at("deuteronMaxAbsEta"), deuteronMaxAbsEta);
  }
  if (values.find("deuteronMaxAbsNSigma") != values.end()) {
    deuteronMaxAbsNSigma = YamlParser::ToDouble(values.at("deuteronMaxAbsNSigma"), deuteronMaxAbsNSigma);
  }
  if (values.find("deuteronMinNHitsFit") != values.end()) {
    deuteronMinNHitsFit = YamlParser::ToInt(values.at("deuteronMinNHitsFit"), deuteronMinNHitsFit);
  }
  if (values.find("deuteronMinNHitsRatio") != values.end()) {
    deuteronMinNHitsRatio = YamlParser::ToDouble(values.at("deuteronMinNHitsRatio"), deuteronMinNHitsRatio);
  }
  if (values.find("deuteronTofMomentumThreshold") != values.end()) {
    deuteronTofMomentumThreshold =
        YamlParser::ToDouble(values.at("deuteronTofMomentumThreshold"), deuteronTofMomentumThreshold);
  }
  if (values.find("deuteronMinMass2") != values.end()) {
    deuteronMinMass2 = YamlParser::ToDouble(values.at("deuteronMinMass2"), deuteronMinMass2);
  }
  if (values.find("deuteronMaxMass2") != values.end()) {
    deuteronMaxMass2 = YamlParser::ToDouble(values.at("deuteronMaxMass2"), deuteronMaxMass2);
  }
  if (values.find("deuteronMinRapidityCm") != values.end()) {
    deuteronMinRapidityCm = YamlParser::ToDouble(values.at("deuteronMinRapidityCm"), deuteronMinRapidityCm);
  }
  if (values.find("deuteronMaxRapidityCm") != values.end()) {
    deuteronMaxRapidityCm = YamlParser::ToDouble(values.at("deuteronMaxRapidityCm"), deuteronMaxRapidityCm);
  }

  if (values.find("tritonMaxDca") != values.end()) {
    tritonMaxDca = YamlParser::ToDouble(values.at("tritonMaxDca"), tritonMaxDca);
  }
  if (values.find("tritonMinPMom") != values.end()) {
    tritonMinPMom = YamlParser::ToDouble(values.at("tritonMinPMom"), tritonMinPMom);
  }
  if (values.find("tritonMaxPMom") != values.end()) {
    tritonMaxPMom = YamlParser::ToDouble(values.at("tritonMaxPMom"), tritonMaxPMom);
  }
  if (values.find("tritonMinPtPre") != values.end()) {
    tritonMinPtPre = YamlParser::ToDouble(values.at("tritonMinPtPre"), tritonMinPtPre);
  }
  if (values.find("tritonMaxPtPre") != values.end()) {
    tritonMaxPtPre = YamlParser::ToDouble(values.at("tritonMaxPtPre"), tritonMaxPtPre);
  }
  if (values.find("tritonMinPtPair") != values.end()) {
    tritonMinPtPair = YamlParser::ToDouble(values.at("tritonMinPtPair"), tritonMinPtPair);
  }
  if (values.find("tritonMaxPtPair") != values.end()) {
    tritonMaxPtPair = YamlParser::ToDouble(values.at("tritonMaxPtPair"), tritonMaxPtPair);
  }
  if (values.find("tritonMaxAbsEta") != values.end()) {
    tritonMaxAbsEta = YamlParser::ToDouble(values.at("tritonMaxAbsEta"), tritonMaxAbsEta);
  }
  if (values.find("tritonMaxAbsNSigma") != values.end()) {
    tritonMaxAbsNSigma = YamlParser::ToDouble(values.at("tritonMaxAbsNSigma"), tritonMaxAbsNSigma);
  }
  if (values.find("tritonMinNHitsFit") != values.end()) {
    tritonMinNHitsFit = YamlParser::ToInt(values.at("tritonMinNHitsFit"), tritonMinNHitsFit);
  }
  if (values.find("tritonMinNHitsRatio") != values.end()) {
    tritonMinNHitsRatio = YamlParser::ToDouble(values.at("tritonMinNHitsRatio"), tritonMinNHitsRatio);
  }
  if (values.find("tritonTofMomentumThreshold") != values.end()) {
    tritonTofMomentumThreshold =
        YamlParser::ToDouble(values.at("tritonTofMomentumThreshold"), tritonTofMomentumThreshold);
  }
  if (values.find("tritonMinMass2") != values.end()) {
    tritonMinMass2 = YamlParser::ToDouble(values.at("tritonMinMass2"), tritonMinMass2);
  }
  if (values.find("tritonMaxMass2") != values.end()) {
    tritonMaxMass2 = YamlParser::ToDouble(values.at("tritonMaxMass2"), tritonMaxMass2);
  }
  if (values.find("tritonMinRapidityCm") != values.end()) {
    tritonMinRapidityCm = YamlParser::ToDouble(values.at("tritonMinRapidityCm"), tritonMinRapidityCm);
  }
  if (values.find("tritonMaxRapidityCm") != values.end()) {
    tritonMaxRapidityCm = YamlParser::ToDouble(values.at("tritonMaxRapidityCm"), tritonMaxRapidityCm);
  }

  if (values.find("he3MaxDca") != values.end()) {
    he3MaxDca = YamlParser::ToDouble(values.at("he3MaxDca"), he3MaxDca);
  }
  if (values.find("he3MinPMom") != values.end()) {
    he3MinPMom = YamlParser::ToDouble(values.at("he3MinPMom"), he3MinPMom);
  }
  if (values.find("he3MaxPMom") != values.end()) {
    he3MaxPMom = YamlParser::ToDouble(values.at("he3MaxPMom"), he3MaxPMom);
  }
  if (values.find("he3MinPtPre") != values.end()) {
    he3MinPtPre = YamlParser::ToDouble(values.at("he3MinPtPre"), he3MinPtPre);
  }
  if (values.find("he3MaxPtPre") != values.end()) {
    he3MaxPtPre = YamlParser::ToDouble(values.at("he3MaxPtPre"), he3MaxPtPre);
  }
  if (values.find("he3MinPtPair") != values.end()) {
    he3MinPtPair = YamlParser::ToDouble(values.at("he3MinPtPair"), he3MinPtPair);
  }
  if (values.find("he3MaxPtPair") != values.end()) {
    he3MaxPtPair = YamlParser::ToDouble(values.at("he3MaxPtPair"), he3MaxPtPair);
  }
  if (values.find("he3MaxAbsEta") != values.end()) {
    he3MaxAbsEta = YamlParser::ToDouble(values.at("he3MaxAbsEta"), he3MaxAbsEta);
  }
  if (values.find("he3MaxAbsNSigma") != values.end()) {
    he3MaxAbsNSigma = YamlParser::ToDouble(values.at("he3MaxAbsNSigma"), he3MaxAbsNSigma);
  }
  if (values.find("he3MinNHitsFit") != values.end()) {
    he3MinNHitsFit = YamlParser::ToInt(values.at("he3MinNHitsFit"), he3MinNHitsFit);
  }
  if (values.find("he3MinNHitsRatio") != values.end()) {
    he3MinNHitsRatio = YamlParser::ToDouble(values.at("he3MinNHitsRatio"), he3MinNHitsRatio);
  }
  if (values.find("he3TofMomentumThreshold") != values.end()) {
    he3TofMomentumThreshold = YamlParser::ToDouble(values.at("he3TofMomentumThreshold"), he3TofMomentumThreshold);
  }
  if (values.find("he3MinMass2") != values.end()) {
    he3MinMass2 = YamlParser::ToDouble(values.at("he3MinMass2"), he3MinMass2);
  }
  if (values.find("he3MaxMass2") != values.end()) {
    he3MaxMass2 = YamlParser::ToDouble(values.at("he3MaxMass2"), he3MaxMass2);
  }
  if (values.find("he3MinRapidityCm") != values.end()) {
    he3MinRapidityCm = YamlParser::ToDouble(values.at("he3MinRapidityCm"), he3MinRapidityCm);
  }
  if (values.find("he3MaxRapidityCm") != values.end()) {
    he3MaxRapidityCm = YamlParser::ToDouble(values.at("he3MaxRapidityCm"), he3MaxRapidityCm);
  }

  if (values.find("rotationEnabled") != values.end()) rotationEnabled = YamlParser::ToBool(values.at("rotationEnabled"), rotationEnabled);
  if (values.find("rotationSpeciesKey") != values.end()) rotationSpeciesKey = values.at("rotationSpeciesKey");
  if (values.find("rotationParticleKey") != values.end()) rotationParticleKey = values.at("rotationParticleKey");
  if (values.find("rotationN") != values.end()) rotationN = YamlParser::ToInt(values.at("rotationN"), rotationN);
  if (values.find("rotationMinAngle") != values.end()) rotationMinAngle = YamlParser::ToDouble(values.at("rotationMinAngle"), rotationMinAngle);
  if (values.find("rotationMaxAngle") != values.end()) rotationMaxAngle = YamlParser::ToDouble(values.at("rotationMaxAngle"), rotationMaxAngle);
  if (values.find("rotationSeed") != values.end()) rotationSeed = YamlParser::ToInt(values.at("rotationSeed"), rotationSeed);
  if (values.find("fullyMixedEnabled") != values.end()) {
    fullyMixedEnabled = YamlParser::ToBool(values.at("fullyMixedEnabled"), fullyMixedEnabled);
  }
  if (values.find("fullyMixedSpeciesKey") != values.end()) fullyMixedSpeciesKey = values.at("fullyMixedSpeciesKey");
  if (values.find("fullyMixedParticleKey") != values.end()) fullyMixedParticleKey = values.at("fullyMixedParticleKey");
  if (values.find("fullyMixedMaxCandidates") != values.end()) {
    fullyMixedMaxCandidates = YamlParser::ToInt(values.at("fullyMixedMaxCandidates"), fullyMixedMaxCandidates);
  }
  if (values.find("fullyMixedSamplingSeed") != values.end()) {
    fullyMixedSamplingSeed = YamlParser::ToInt(values.at("fullyMixedSamplingSeed"), fullyMixedSamplingSeed);
  }
  if (values.find("cfRebinFactor") != values.end()) cfRebinFactor = YamlParser::ToInt(values.at("cfRebinFactor"), cfRebinFactor);
  if (values.find("cfCent9Min") != values.end()) cfCent9Min = YamlParser::ToInt(values.at("cfCent9Min"), cfCent9Min);
  if (values.find("cfCent9Max") != values.end()) cfCent9Max = YamlParser::ToInt(values.at("cfCent9Max"), cfCent9Max);
  if (values.find("cfPdfExcludeQaSlices") != values.end()) {
    cfPdfExcludeQaSlices = YamlParser::ToBool(values.at("cfPdfExcludeQaSlices"), cfPdfExcludeQaSlices);
  }
  if (values.find("sidebandSubtractAlpha") != values.end()) {
    sidebandSubtractAlpha = YamlParser::ToDouble(values.at("sidebandSubtractAlpha"), sidebandSubtractAlpha);
  }
  if (values.find("sidebandAlphaMode") != values.end()) sidebandAlphaMode = values.at("sidebandAlphaMode");
  if (values.find("negativeBinPolicy") != values.end()) negativeBinPolicy = values.at("negativeBinPolicy");
  if (values.find("purityFitUseConstantBkg") != values.end()) {
    purityFitUseConstantBkg = YamlParser::ToBool(values.at("purityFitUseConstantBkg"), purityFitUseConstantBkg);
  }
  if (values.find("purityFitGaussSigmaMin") != values.end()) {
    purityFitGaussSigmaMin = YamlParser::ToDouble(values.at("purityFitGaussSigmaMin"), purityFitGaussSigmaMin);
  }
  if (values.find("purityFitGaussSigmaMax") != values.end()) {
    purityFitGaussSigmaMax = YamlParser::ToDouble(values.at("purityFitGaussSigmaMax"), purityFitGaussSigmaMax);
  }
  if (values.find("purityMinKstar") != values.end()) {
    purityMinKstar = YamlParser::ToDouble(values.at("purityMinKstar"), purityMinKstar);
  }
  if (values.find("purityMaxKstar") != values.end()) {
    purityMaxKstar = YamlParser::ToDouble(values.at("purityMaxKstar"), purityMaxKstar);
  }
  if (values.find("purityMinEntriesPerBin") != values.end()) {
    purityMinEntriesPerBin = YamlParser::ToInt(values.at("purityMinEntriesPerBin"), purityMinEntriesPerBin);
  }
  if (values.find("purityClampMin") != values.end()) {
    purityClampMin = YamlParser::ToDouble(values.at("purityClampMin"), purityClampMin);
  }
  if (values.find("purityClampMax") != values.end()) {
    purityClampMax = YamlParser::ToDouble(values.at("purityClampMax"), purityClampMax);
  }
  if (values.find("cfBkgMode") != values.end()) cfBkgMode = values.at("cfBkgMode");
  if (values.find("cfSubtractionMode") != values.end()) cfSubtractionMode = values.at("cfSubtractionMode");
  if (values.find("cfSubPurityMode") != values.end()) cfSubPurityMode = values.at("cfSubPurityMode");
  if (values.find("cfSubPurityFixed") != values.end()) {
    cfSubPurityFixed = YamlParser::ToDouble(values.at("cfSubPurityFixed"), cfSubPurityFixed);
  }
  if (values.find("cfSubSidebandCombine") != values.end()) {
    cfSubSidebandCombine = values.at("cfSubSidebandCombine");
  }
  if (values.find("cfSubWriteSidecarRoot") != values.end()) {
    cfSubWriteSidecarRoot = YamlParser::ToBool(values.at("cfSubWriteSidecarRoot"), cfSubWriteSidecarRoot);
  }
  if (values.find("cfSubLowStatsRebinExtra") != values.end()) {
    cfSubLowStatsRebinExtra = YamlParser::ToInt(values.at("cfSubLowStatsRebinExtra"), cfSubLowStatsRebinExtra);
  }

  auto warnDeprecated = [](const char* oldKey, const char* newKey) {
    std::cerr << "WARNING: FemtoConfig key '" << oldKey << "' is deprecated; use '" << newKey << "'"
              << std::endl;
  };

  if (values.find("kstarMassFitCfEnabled") != values.end()) {
    kstarMassFitCfEnabled = YamlParser::ToBool(values.at("kstarMassFitCfEnabled"), kstarMassFitCfEnabled);
  }
  if (values.find("kstarMassFitCfTemplate") != values.end()) {
    kstarMassFitCfTemplate = values.at("kstarMassFitCfTemplate");
  }
  if (values.find("kstarMassFitCfCrossCheck") != values.end()) {
    kstarMassFitCfCrossCheck = YamlParser::ToBool(values.at("kstarMassFitCfCrossCheck"), kstarMassFitCfCrossCheck);
  }
  if (values.find("kstarMassFitCfFitMassMin") != values.end()) {
    kstarMassFitCfFitMassMin =
        YamlParser::ToDouble(values.at("kstarMassFitCfFitMassMin"), kstarMassFitCfFitMassMin);
  } else if (values.find("purityDirectFitMassMin") != values.end()) {
    warnDeprecated("purityDirectFitMassMin", "kstarMassFitCfFitMassMin");
    kstarMassFitCfFitMassMin =
        YamlParser::ToDouble(values.at("purityDirectFitMassMin"), kstarMassFitCfFitMassMin);
  }
  if (values.find("kstarMassFitCfFitMassMax") != values.end()) {
    kstarMassFitCfFitMassMax =
        YamlParser::ToDouble(values.at("kstarMassFitCfFitMassMax"), kstarMassFitCfFitMassMax);
  } else if (values.find("purityDirectFitMassMax") != values.end()) {
    warnDeprecated("purityDirectFitMassMax", "kstarMassFitCfFitMassMax");
    kstarMassFitCfFitMassMax =
        YamlParser::ToDouble(values.at("purityDirectFitMassMax"), kstarMassFitCfFitMassMax);
  }
  if (values.find("kstarMassFitCfKstarBinWidth") != values.end()) {
    kstarMassFitCfKstarBinWidth =
        YamlParser::ToDouble(values.at("kstarMassFitCfKstarBinWidth"), kstarMassFitCfKstarBinWidth);
  } else if (values.find("purityDirectKstarBinWidth") != values.end()) {
    warnDeprecated("purityDirectKstarBinWidth", "kstarMassFitCfKstarBinWidth");
    kstarMassFitCfKstarBinWidth =
        YamlParser::ToDouble(values.at("purityDirectKstarBinWidth"), kstarMassFitCfKstarBinWidth);
  }
  if (values.find("kstarMassFitCfLowKstarMergeBins") != values.end()) {
    kstarMassFitCfLowKstarMergeBins =
        YamlParser::ToInt(values.at("kstarMassFitCfLowKstarMergeBins"), kstarMassFitCfLowKstarMergeBins);
  } else if (values.find("method3BkgSubLowKstarMergeBins") != values.end()) {
    warnDeprecated("method3BkgSubLowKstarMergeBins", "kstarMassFitCfLowKstarMergeBins");
    kstarMassFitCfLowKstarMergeBins =
        YamlParser::ToInt(values.at("method3BkgSubLowKstarMergeBins"), kstarMassFitCfLowKstarMergeBins);
  }
  if (values.find("kstarMassFitCfAlphaMassMin") != values.end()) {
    kstarMassFitCfAlphaMassMin =
        YamlParser::ToDouble(values.at("kstarMassFitCfAlphaMassMin"), kstarMassFitCfAlphaMassMin);
  } else if (values.find("method3BkgSubAlphaMassMin") != values.end()) {
    warnDeprecated("method3BkgSubAlphaMassMin", "kstarMassFitCfAlphaMassMin");
    kstarMassFitCfAlphaMassMin =
        YamlParser::ToDouble(values.at("method3BkgSubAlphaMassMin"), kstarMassFitCfAlphaMassMin);
  }
  if (values.find("kstarMassFitCfAlphaMassMax") != values.end()) {
    kstarMassFitCfAlphaMassMax =
        YamlParser::ToDouble(values.at("kstarMassFitCfAlphaMassMax"), kstarMassFitCfAlphaMassMax);
  } else if (values.find("method3BkgSubAlphaMassMax") != values.end()) {
    warnDeprecated("method3BkgSubAlphaMassMax", "kstarMassFitCfAlphaMassMax");
    kstarMassFitCfAlphaMassMax =
        YamlParser::ToDouble(values.at("method3BkgSubAlphaMassMax"), kstarMassFitCfAlphaMassMax);
  }
  if (values.find("kstarMassFitCfWriteSidecar") != values.end()) {
    kstarMassFitCfWriteSidecar =
        YamlParser::ToBool(values.at("kstarMassFitCfWriteSidecar"), kstarMassFitCfWriteSidecar);
  } else if (values.find("cfDirectWriteSidecar") != values.end()) {
    warnDeprecated("cfDirectWriteSidecar", "kstarMassFitCfWriteSidecar");
    kstarMassFitCfWriteSidecar = YamlParser::ToBool(values.at("cfDirectWriteSidecar"), kstarMassFitCfWriteSidecar);
  }
  if (values.find("legacyCfPagesEnabled") != values.end()) {
    legacyCfPagesEnabled = YamlParser::ToBool(values.at("legacyCfPagesEnabled"), legacyCfPagesEnabled);
  }
  if (values.find("purityDirectFitModel") != values.end()) {
    purityDirectFitModel = values.at("purityDirectFitModel");
  }
  if (values.find("cfDirectPurityMode") != values.end()) {
    warnDeprecated("cfDirectPurityMode", "kstarMassFitCfEnabled");
    if (values.at("cfDirectPurityMode") == "method3" && values.find("kstarMassFitCfEnabled") == values.end()) {
      kstarMassFitCfEnabled = kTRUE;
    }
  }
  if (values.find("enableHKaonTwoBody") != values.end()) {
    enableHKaonTwoBody = YamlParser::ToBool(values.at("enableHKaonTwoBody"), enableHKaonTwoBody);
  }
  if (values.find("enableKuboTriplet") != values.end()) {
    enableKuboTriplet = YamlParser::ToBool(values.at("enableKuboTriplet"), enableKuboTriplet);
  }
  if (values.find("enableKuboGenuine") != values.end()) {
    enableKuboGenuine = YamlParser::ToBool(values.at("enableKuboGenuine"), enableKuboGenuine);
  }
  if (values.find("kuboStoreFullMass") != values.end()) {
    kuboStoreFullMass = YamlParser::ToBool(values.at("kuboStoreFullMass"), kuboStoreFullMass);
  }
  if (values.find("phiNearTrackQaEnabled") != values.end()) {
    phiNearTrackQaEnabled = YamlParser::ToBool(values.at("phiNearTrackQaEnabled"), phiNearTrackQaEnabled);
  }
  if (values.find("phiNearTrackSignalChannel") != values.end()) {
    phiNearTrackSignalChannel = values.at("phiNearTrackSignalChannel");
  }
  if (values.find("phiNearTrackMaxKstarLoose") != values.end()) {
    phiNearTrackMaxKstarLoose =
        YamlParser::ToDouble(values.at("phiNearTrackMaxKstarLoose"), phiNearTrackMaxKstarLoose);
  }
  if (values.find("phiNearTrackMaxKstarTight") != values.end()) {
    phiNearTrackMaxKstarTight =
        YamlParser::ToDouble(values.at("phiNearTrackMaxKstarTight"), phiNearTrackMaxKstarTight);
  }
  if (values.find("phiNearTrackMassHyp") != values.end()) {
    phiNearTrackMassHyp = values.at("phiNearTrackMassHyp");
  }
  if (values.find("cfCentSlicesQaPdfInclude") != values.end()) {
    cfCentSlicesQaPdfInclude = SplitComma(values.at("cfCentSlicesQaPdfInclude"));
  }

  Int_t nCfCentSlices = -1;
  if (values.find("nCfCentSlices") != values.end()) {
    nCfCentSlices = YamlParser::ToInt(values.at("nCfCentSlices"), -1);
  }
  if (nCfCentSlices > 0) {
    cfCentSlices.clear();
    for (Int_t is = 0; is < nCfCentSlices; ++is) {
      const std::string prefix = "cfCentSlice_" + std::to_string(is) + "_";
      CfCentSlice sl;
      if (values.find(prefix + "id") != values.end()) sl.id = values.at(prefix + "id");
      if (values.find(prefix + "cent9Min") != values.end()) {
        sl.cent9Min = YamlParser::ToInt(values.at(prefix + "cent9Min"), sl.cent9Min);
      }
      if (values.find(prefix + "cent9Max") != values.end()) {
        sl.cent9Max = YamlParser::ToInt(values.at(prefix + "cent9Max"), sl.cent9Max);
      }
      if (!sl.id.empty()) cfCentSlices.push_back(sl);
    }
  }
}

Bool_t FemtoConfig::LoadFromFile(const Char_t* filename) {
  return ParseYamlFile(filename);
}

Bool_t FemtoConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: Failed to parse " << filename << ", using femto defaults" << std::endl;
    return kFALSE;
  }

  species.clear();
  channels.clear();
  SetDefaults();

  if (values.find("speciesKeys") != values.end()) {
    species.clear();
    std::vector<std::string> keys = SplitComma(values["speciesKeys"]);
    for (size_t i = 0; i < keys.size(); i++) {
      const std::string& key = keys[i];
      SpeciesDef def;
      def.key = key;
      std::string btKey = "species_" + key + "_builderType";
      std::string pkKey = "species_" + key + "_particleKey";
      std::string crKey = "species_" + key + "_cutsRef";
      if (values.find(btKey) != values.end()) def.builderType = values[btKey];
      if (values.find(pkKey) != values.end()) def.particleKey = values[pkKey];
      if (values.find(crKey) != values.end()) def.cutsRef = values[crKey];
      species[key] = def;
    }
  }

  ApplyYamlValues(values);

  Int_t nChannels = 1;
  if (values.find("nChannels") != values.end()) {
    nChannels = YamlParser::ToInt(values["nChannels"], nChannels);
  }
  if (nChannels < 1) nChannels = 1;

  channels.clear();
  for (Int_t ic = 0; ic < nChannels; ic++) {
    std::string prefix = (nChannels == 1) ? "" : ("channel_" + std::to_string(ic) + "_");
    auto getVal = [&](const std::string& k) -> std::string {
      std::string flat = prefix + k;
      if (values.find(flat) != values.end()) return values[flat];
      if (nChannels == 1 && values.find(k) != values.end()) return values[k];
      return "";
    };

    ChannelDef ch;
    ch.name = getVal("channelName");
    if (ch.name.empty()) ch.name = getVal("name");
    ch.partA = getVal("partA");
    ch.partB = getVal("partB");
    ch.enabled = YamlParser::ToBool(getVal("enabled"), kTRUE);
    ch.doMixing = YamlParser::ToBool(getVal("doMixing"), kTRUE);
    ch.signalMin = YamlParser::ToDouble(getVal("signalMin"), 1.01);
    ch.signalMax = YamlParser::ToDouble(getVal("signalMax"), 1.03);
    ch.normQMin = YamlParser::ToDouble(getVal("normQMin"), 0.2);
    ch.normQMax = YamlParser::ToDouble(getVal("normQMax"), 0.3);

    if (ch.name.empty() && !ch.partA.empty() && !ch.partB.empty()) {
      ch.name = ch.partA + "_" + ch.partB;
    }
    if (!ch.partA.empty() && !ch.partB.empty()) {
      channels.push_back(ch);
    }
  }

  if (species.empty()) {
    SetDefaults();
    species.clear();
    channels.clear();
    SpeciesDef proton;
    proton.key = "proton";
    proton.builderType = "track";
    proton.particleKey = "proton";
    species[proton.key] = proton;
    SpeciesDef phi;
    phi.key = "phi";
    phi.builderType = "resonance";
    phi.particleKey = "phi";
    species[phi.key] = phi;
  }
  if (channels.empty()) {
    ChannelDef ch;
    ch.name = "phi_proton";
    ch.partA = "phi";
    ch.partB = "proton";
    ch.enabled = kTRUE;
    ch.doMixing = kTRUE;
    ch.signalMin = 1.01;
    ch.signalMax = 1.03;
    ch.normQMin = 0.2;
    ch.normQMax = 0.3;
    channels.push_back(ch);
  }

  const Int_t maxChannelIdx = MaxChannelIndexInYaml(values);
  if (maxChannelIdx >= nChannels) {
    std::cerr << "ERROR: FemtoConfig nChannels=" << nChannels << " but YAML defines channel_" << maxChannelIdx
              << "_* (set nChannels >= " << (maxChannelIdx + 1) << ")" << std::endl;
    return kFALSE;
  }

  return Validate();
}

Bool_t FemtoConfig::Validate() const {
  Bool_t ok = kTRUE;
  for (size_t i = 0; i < channels.size(); i++) {
    const ChannelDef& ch = channels[i];
    if (species.find(ch.partA) == species.end()) {
      std::cerr << "ERROR: FemtoConfig channel '" << ch.name << "' partA '" << ch.partA
                << "' not found in species map" << std::endl;
      ok = kFALSE;
    }
    if (species.find(ch.partB) == species.end()) {
      std::cerr << "ERROR: FemtoConfig channel '" << ch.name << "' partB '" << ch.partB
                << "' not found in species map" << std::endl;
      ok = kFALSE;
    }
  }
  if (rotationEnabled && species.find(rotationSpeciesKey) == species.end()) {
    std::cerr << "ERROR: rotationEnabled but species '" << rotationSpeciesKey << "' not defined" << std::endl;
    ok = kFALSE;
  }
  if (fullyMixedEnabled && species.find(fullyMixedSpeciesKey) == species.end()) {
    std::cerr << "ERROR: fullyMixedEnabled but species '" << fullyMixedSpeciesKey << "' not defined" << std::endl;
    ok = kFALSE;
  }
  if (fullyMixedEnabled &&
      (species.find("phikaon_plus") == species.end() || species.find("phikaon_minus") == species.end())) {
    std::cerr << "ERROR: FemtoConfig fullyMixedEnabled requires species phikaon_plus and phikaon_minus"
              << std::endl;
    ok = kFALSE;
  }
  if (fullyMixedSamplingSeed < 0) {
    std::cerr << "ERROR: FemtoConfig fullyMixedSamplingSeed must be >= 0 (got " << fullyMixedSamplingSeed
              << ")" << std::endl;
    ok = kFALSE;
  }
  if (fullyMixedEnabled && fullyMixedSamplingSeed == 0) {
    std::cerr << "WARNING: FemtoConfig fullyMixedSamplingSeed=0 uses a time-based RNG seed; "
                 "phi_mix sampling is not reproducible. Production YAML should set a fixed non-zero seed."
              << std::endl;
  }
  if (cfRebinFactor < 1) {
    std::cerr << "ERROR: FemtoConfig cfRebinFactor must be >= 1 (got " << cfRebinFactor << ")" << std::endl;
    ok = kFALSE;
  }
  if (cfCent9Min < 0 || cfCent9Max > 8 || cfCent9Min > cfCent9Max) {
    std::cerr << "ERROR: FemtoConfig cfCent9Min/Max out of range (got [" << cfCent9Min << ", " << cfCent9Max
              << "], valid cent9 is 0-8)" << std::endl;
    ok = kFALSE;
  }
  for (size_t is = 0; is < cfCentSlices.size(); ++is) {
    const CfCentSlice& sl = cfCentSlices[is];
    if (sl.id.empty()) {
      std::cerr << "ERROR: FemtoConfig cfCentSlices[" << is << "] has empty id" << std::endl;
      ok = kFALSE;
    }
    if (sl.cent9Min < 0 || sl.cent9Max > 8 || sl.cent9Min > sl.cent9Max) {
      std::cerr << "ERROR: FemtoConfig cfCentSlices '" << sl.id << "' cent9Min/Max out of range ["
                << sl.cent9Min << ", " << sl.cent9Max << "]" << std::endl;
      ok = kFALSE;
    }
  }
  if (sidebandSubtractAlpha < 0.0) {
    std::cerr << "ERROR: FemtoConfig sidebandSubtractAlpha must be >= 0" << std::endl;
    ok = kFALSE;
  }
  if (purityFitGaussSigmaMin <= 0.0 || purityFitGaussSigmaMax <= purityFitGaussSigmaMin) {
    std::cerr << "ERROR: FemtoConfig purityFitGaussSigmaMin/Max invalid" << std::endl;
    ok = kFALSE;
  }
  if (purityMinKstar < 0.0 || purityMaxKstar <= purityMinKstar) {
    std::cerr << "ERROR: FemtoConfig purityMinKstar/Max invalid" << std::endl;
    ok = kFALSE;
  }
  if (purityMinEntriesPerBin < 1) {
    std::cerr << "ERROR: FemtoConfig purityMinEntriesPerBin must be >= 1" << std::endl;
    ok = kFALSE;
  }
  if (purityClampMin <= 0.0 || purityClampMax > 1.0 || purityClampMin >= purityClampMax) {
    std::cerr << "ERROR: FemtoConfig purityClampMin/Max invalid (expect 0 < min < max <= 1)" << std::endl;
    ok = kFALSE;
  }
  if (cfSubtractionMode != "none" && cfSubtractionMode != "method5") {
    std::cerr << "ERROR: FemtoConfig cfSubtractionMode must be none|method5 (got " << cfSubtractionMode << ")"
              << std::endl;
    ok = kFALSE;
  }
  if (cfSubPurityMode != "fixed" && cfSubPurityMode != "fit_slice") {
    std::cerr << "ERROR: FemtoConfig cfSubPurityMode must be fixed|fit_slice (got " << cfSubPurityMode << ")"
              << std::endl;
    ok = kFALSE;
  }
  if (cfSubPurityFixed <= 0.0 || cfSubPurityFixed > 1.0) {
    std::cerr << "ERROR: FemtoConfig cfSubPurityFixed must be in (0, 1]" << std::endl;
    ok = kFALSE;
  }
  if (cfSubSidebandCombine != "sumLR" && cfSubSidebandCombine != "avgCF_LR") {
    std::cerr << "ERROR: FemtoConfig cfSubSidebandCombine must be sumLR|avgCF_LR (got " << cfSubSidebandCombine
              << ")" << std::endl;
    ok = kFALSE;
  }
  if (cfSubLowStatsRebinExtra < 1) {
    std::cerr << "ERROR: FemtoConfig cfSubLowStatsRebinExtra must be >= 1" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfTemplate != "rot" && kstarMassFitCfTemplate != "mix") {
    std::cerr << "ERROR: FemtoConfig kstarMassFitCfTemplate must be rot|mix (got " << kstarMassFitCfTemplate
              << ")" << std::endl;
    ok = kFALSE;
  }
  if (purityDirectFitModel != "gaus_pol2" && purityDirectFitModel != "gaus_const") {
    std::cerr << "ERROR: FemtoConfig purityDirectFitModel must be gaus_pol2|gaus_const (got "
              << purityDirectFitModel << ")" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfFitMassMin <= 0.0 || kstarMassFitCfFitMassMax <= kstarMassFitCfFitMassMin) {
    std::cerr << "ERROR: FemtoConfig kstarMassFitCfFitMassMin/Max invalid" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfEnabled && kstarMassFitCfKstarBinWidth <= 0.0) {
    std::cerr << "ERROR: FemtoConfig kstarMassFitCfKstarBinWidth must be > 0 when kstarMassFitCfEnabled (got "
              << kstarMassFitCfKstarBinWidth << ")" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfKstarBinWidth < 0.0) {
    std::cerr << "ERROR: FemtoConfig kstarMassFitCfKstarBinWidth must be >= 0 (got " << kstarMassFitCfKstarBinWidth
              << ")" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfLowKstarMergeBins < 1) {
    std::cerr << "ERROR: FemtoConfig kstarMassFitCfLowKstarMergeBins must be >= 1 (got "
              << kstarMassFitCfLowKstarMergeBins << ")" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfAlphaMassMax > kstarMassFitCfAlphaMassMin && kstarMassFitCfAlphaMassMin < 0.0) {
    std::cerr << "ERROR: FemtoConfig kstarMassFitCfAlphaMassMin must be >= 0 when max>min" << std::endl;
    ok = kFALSE;
  }
  if (kstarMassFitCfEnabled && kstarMassFitCfAlphaMassMax > kstarMassFitCfAlphaMassMin) {
    const ChannelDef* chSig = FindChannel("phi_proton_signal");
    if (chSig) {
      const Bool_t overlap =
          !(kstarMassFitCfAlphaMassMax <= chSig->signalMin || kstarMassFitCfAlphaMassMin >= chSig->signalMax);
      if (overlap) {
        std::cerr << "ERROR: FemtoConfig kstarMassFitCfAlphaMass window overlaps phi_proton_signal ["
                  << chSig->signalMin << ", " << chSig->signalMax << "]" << std::endl;
        ok = kFALSE;
      }
    }
    if (kstarMassFitCfAlphaMassMin < kstarMassFitCfFitMassMax &&
        kstarMassFitCfAlphaMassMax > kstarMassFitCfFitMassMin) {
      std::cerr << "WARNING: FemtoConfig kstarMassFitCf alpha window overlaps fit mass range ["
                << kstarMassFitCfFitMassMin << ", " << kstarMassFitCfFitMassMax << "]" << std::endl;
    }
  }
  if (enableKuboTriplet &&
      (species.find("phikaon_plus") == species.end() || species.find("phikaon_minus") == species.end())) {
    std::cerr << "ERROR: FemtoConfig enableKuboTriplet requires species phikaon_plus and phikaon_minus"
              << std::endl;
    ok = kFALSE;
  }
  if (phiNearTrackQaEnabled) {
    if (phiNearTrackSignalChannel.empty() || FindChannel(phiNearTrackSignalChannel) == 0) {
      std::cerr << "ERROR: FemtoConfig phiNearTrackQaEnabled but signal channel '" << phiNearTrackSignalChannel
                << "' not found" << std::endl;
      ok = kFALSE;
    }
    if (phiNearTrackMaxKstarTight <= 0.0 || phiNearTrackMaxKstarLoose <= phiNearTrackMaxKstarTight) {
      std::cerr << "ERROR: FemtoConfig phiNearTrackMaxKstarTight/Loose invalid (need 0 < tight < loose)"
                << std::endl;
      ok = kFALSE;
    }
    if (phiNearTrackMassHyp != "pion" && phiNearTrackMassHyp != "proton" && phiNearTrackMassHyp != "kaon") {
      std::cerr << "ERROR: FemtoConfig phiNearTrackMassHyp must be pion|proton|kaon (got " << phiNearTrackMassHyp
                << ")" << std::endl;
      ok = kFALSE;
    }
  }
  return ok;
}

const FemtoConfig::CfCentSlice* FemtoConfig::FindCfCentSlice(const std::string& id) const {
  for (size_t i = 0; i < cfCentSlices.size(); ++i) {
    if (cfCentSlices[i].id == id) return &cfCentSlices[i];
  }
  return 0;
}

Bool_t FemtoConfig::IsCfCentSliceInQaPdf(const std::string& id) const {
  for (size_t i = 0; i < cfCentSlicesQaPdfInclude.size(); ++i) {
    if (cfCentSlicesQaPdfInclude[i] == id) return kTRUE;
  }
  return kFALSE;
}

const FemtoConfig::SpeciesDef* FemtoConfig::FindSpecies(const std::string& key) const {
  std::map<std::string, SpeciesDef>::const_iterator it = species.find(key);
  if (it == species.end()) return 0;
  return &it->second;
}

const FemtoConfig::ChannelDef* FemtoConfig::FindChannel(const std::string& name) const {
  for (size_t i = 0; i < channels.size(); i++) {
    if (channels[i].name == name) return &channels[i];
  }
  return 0;
}
