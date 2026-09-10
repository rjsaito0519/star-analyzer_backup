#include "cuts/KfParticleCutConfig.h"
#include "yaml-cpp/yaml.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>

KfParticleCutConfig& KfParticleCutConfig::GetInstance() {
  static KfParticleCutConfig instance;
  return instance;
}
KfParticleCutConfig::KfParticleCutConfig() { SetDefaults(); }
KfParticleCutConfig::~KfParticleCutConfig() {}

void KfParticleCutConfig::SetDefaults() {
  schemaVersion = 0;
  backend = "";
  selectionProfile = "kf_reference";
  imp5MinDCAProton = -1.;
  imp5MinDCAPion = -1.;
  imp5MaxPathLength = -1.;
  pidProfile = "dedx_pull";
  tofCalibrationProfile = "";
  minNHitsFit = 15;
  minNHitsDedx = 5;
  minNHitsRatio = 0.;
  minPt = 0.15;
  maxPt = 10.;
  minEta = -1.5;
  maxEta = 1.5;
  minDedxError = 0.04;
  maxDedxError = 0.12;
  useHftTracksOnly = kFALSE;
  nSigmaProton = 3.;
  nSigmaPion = 3.;
  nSigmaKaon = 2.;
  useTof = kTRUE;
  strictTofPid = kFALSE;
  cleanKaonsWithTof = kFALSE;
  tofNSigma = 3.;
  tofPMax = 2.;
  tofKaonPMin = 0.5;
  tofKaonPMax = 2.;
  tofMean.clear();
  tofSigma.clear();
  rejectBadCovariance = kTRUE;
  maxPositionVariance = 100.;
  maxMomentumVariance = 1.;
  interfaceChiPrimaryCut = 3.;
  primaryProbCut = 0.0001;
  finderMaxDaughterDistance = 1.5;
  finderLCut = 1.;
  finderChiPrimary2D = 3.;
  finderChi2Ndf2D = 10.;
  finderLdL2D = 3.;
  finderLdLXiOmega = 3.;
  finderChi2TopoXiOmega = 10.;
  finderChi2XiOmega = 10.;
  minMass = 1.05;
  maxMass = 1.25;
  maxMassError = -1.;
  maxChi2Ndf = -1.;
  maxTopoChi2Ndf = -1.;
  maxDaughterDistance = -1.;
  maxDistanceToPv = -1.;
  minDecayLength = -1.;
  minDecayLengthSignificance = -1.;
  minVertexLineSignificance = -1.;
  minCosPointing = -2.;
  reconstructAntiLambda = kTRUE;
  reconstructXi = kFALSE;
  reconstructAntiXi = kFALSE;
}

Bool_t KfParticleCutConfig::LoadFromFile(const Char_t* filename) {
  SetDefaults();
  try {
    // Use the yaml-cpp already linked by libStarAnaConfig. Only this KF config
    // has typed sequences; the existing configuration parsers are unchanged.
    const YAML::Node values = YAML::LoadFile(filename);
    if (!values.IsMap()) throw std::runtime_error("expected a flat mapping");
    std::set<std::string> seen;
    for (YAML::const_iterator it = values.begin(); it != values.end(); ++it) {
      const std::string key = it->first.as<std::string>();
      if (!seen.insert(key).second) throw std::runtime_error("duplicate key: " + key);
      const YAML::Node value = it->second;
      if (key == "schemaVersion") { schemaVersion = value.as<Int_t>(); continue; }
      if (key == "backend") { backend = value.as<std::string>(); continue; }
      if (key == "selectionProfile") { selectionProfile = value.as<std::string>(); continue; }
      if (key == "imp5MinDCAProton") { imp5MinDCAProton = value.as<Double_t>(); continue; }
      if (key == "imp5MinDCAPion") { imp5MinDCAPion = value.as<Double_t>(); continue; }
      if (key == "imp5MaxPathLength") { imp5MaxPathLength = value.as<Double_t>(); continue; }
      if (key == "pidProfile") { pidProfile = value.as<std::string>(); continue; }
      if (key == "tofCalibrationProfile") { tofCalibrationProfile = value.as<std::string>(); continue; }
      if (key == "minNHitsFit") { minNHitsFit = value.as<Int_t>(); continue; }
      if (key == "minNHitsDedx") { minNHitsDedx = value.as<Int_t>(); continue; }
      if (key == "minNHitsRatio") { minNHitsRatio = value.as<Double_t>(); continue; }
      if (key == "minPt") { minPt = value.as<Double_t>(); continue; }
      if (key == "maxPt") { maxPt = value.as<Double_t>(); continue; }
      if (key == "minEta") { minEta = value.as<Double_t>(); continue; }
      if (key == "maxEta") { maxEta = value.as<Double_t>(); continue; }
      if (key == "minDedxError") { minDedxError = value.as<Double_t>(); continue; }
      if (key == "maxDedxError") { maxDedxError = value.as<Double_t>(); continue; }
      if (key == "useHftTracksOnly") { useHftTracksOnly = value.as<bool>(); continue; }
      if (key == "nSigmaProton") { nSigmaProton = value.as<Double_t>(); continue; }
      if (key == "nSigmaPion") { nSigmaPion = value.as<Double_t>(); continue; }
      if (key == "nSigmaKaon") { nSigmaKaon = value.as<Double_t>(); continue; }
      if (key == "useTof") { useTof = value.as<bool>(); continue; }
      if (key == "strictTofPid") { strictTofPid = value.as<bool>(); continue; }
      if (key == "cleanKaonsWithTof") { cleanKaonsWithTof = value.as<bool>(); continue; }
      if (key == "tofNSigma") { tofNSigma = value.as<Double_t>(); continue; }
      if (key == "tofPMax") { tofPMax = value.as<Double_t>(); continue; }
      if (key == "tofKaonPMin") { tofKaonPMin = value.as<Double_t>(); continue; }
      if (key == "tofKaonPMax") { tofKaonPMax = value.as<Double_t>(); continue; }
      if (key == "tofMean") { tofMean = value.as<std::vector<double>>(); continue; }
      if (key == "tofSigma") { tofSigma = value.as<std::vector<double>>(); continue; }
      if (key == "rejectBadCovariance") { rejectBadCovariance = value.as<bool>(); continue; }
      if (key == "maxPositionVariance") { maxPositionVariance = value.as<Double_t>(); continue; }
      if (key == "maxMomentumVariance") { maxMomentumVariance = value.as<Double_t>(); continue; }
      if (key == "interfaceChiPrimaryCut") { interfaceChiPrimaryCut = value.as<Double_t>(); continue; }
      if (key == "primaryProbCut") { primaryProbCut = value.as<Double_t>(); continue; }
      if (key == "finderMaxDaughterDistance") { finderMaxDaughterDistance = value.as<Double_t>(); continue; }
      if (key == "finderLCut") { finderLCut = value.as<Double_t>(); continue; }
      if (key == "finderChiPrimary2D") { finderChiPrimary2D = value.as<Double_t>(); continue; }
      if (key == "finderChi2Ndf2D") { finderChi2Ndf2D = value.as<Double_t>(); continue; }
      if (key == "finderLdL2D") { finderLdL2D = value.as<Double_t>(); continue; }
      if (key == "finderLdLXiOmega") { finderLdLXiOmega = value.as<Double_t>(); continue; }
      if (key == "finderChi2TopoXiOmega") { finderChi2TopoXiOmega = value.as<Double_t>(); continue; }
      if (key == "finderChi2XiOmega") { finderChi2XiOmega = value.as<Double_t>(); continue; }
      if (key == "minMass") { minMass = value.as<Double_t>(); continue; }
      if (key == "maxMass") { maxMass = value.as<Double_t>(); continue; }
      if (key == "maxMassError") { maxMassError = value.as<Double_t>(); continue; }
      if (key == "maxChi2Ndf") { maxChi2Ndf = value.as<Double_t>(); continue; }
      if (key == "maxTopoChi2Ndf") { maxTopoChi2Ndf = value.as<Double_t>(); continue; }
      if (key == "maxDaughterDistance") { maxDaughterDistance = value.as<Double_t>(); continue; }
      if (key == "maxDistanceToPv") { maxDistanceToPv = value.as<Double_t>(); continue; }
      if (key == "minDecayLength") { minDecayLength = value.as<Double_t>(); continue; }
      if (key == "minDecayLengthSignificance") { minDecayLengthSignificance = value.as<Double_t>(); continue; }
      if (key == "minVertexLineSignificance") { minVertexLineSignificance = value.as<Double_t>(); continue; }
      if (key == "minCosPointing") { minCosPointing = value.as<Double_t>(); continue; }
      if (key == "reconstructAntiLambda") { reconstructAntiLambda = value.as<bool>(); continue; }
      if (key == "reconstructXi") { reconstructXi = value.as<bool>(); continue; }
      if (key == "reconstructAntiXi") { reconstructAntiXi = value.as<bool>(); continue; }
      throw std::runtime_error("unknown/obsolete KF key: " + key);
    }
    if (!Validate(std::cerr)) return kFALSE;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: KF config " << filename << ": " << error.what() << std::endl;
    return kFALSE;
  }
  return kTRUE;
}

Bool_t KfParticleCutConfig::Validate(std::ostream& errors) const {
  Bool_t ok = kTRUE;
#define REQUIRE(test, message) do { if (!(test)) { errors << "ERROR: KF config: " << message << std::endl; ok = kFALSE; } } while (0)
  REQUIRE(schemaVersion == 2 && backend == "finder_topo",
          "schemaVersion: 2 and backend: finder_topo are required; scalar configs must be migrated explicitly");
  REQUIRE(selectionProfile == "kf_reference" || selectionProfile == "lambda_imp5",
          "unknown selectionProfile (expected kf_reference or lambda_imp5)");
  REQUIRE(std::isfinite(imp5MinDCAProton) && std::isfinite(imp5MinDCAPion) &&
          std::isfinite(imp5MaxPathLength), "Imp5 cuts must be finite");
  if (selectionProfile == "lambda_imp5") {
    REQUIRE(pidProfile == "pico_nsigma" && !useTof && !strictTofPid &&
            !cleanKaonsWithTof && !useHftTracksOnly,
            "lambda_imp5 requires pico_nsigma with TOF/HFT selections disabled");
    REQUIRE(imp5MinDCAProton >= 0. && imp5MinDCAPion >= 0. && imp5MaxPathLength > 0.,
            "lambda_imp5 requires explicit nonnegative daughter DCA and positive path limits");
  } else {
    REQUIRE(imp5MinDCAProton < 0. && imp5MinDCAPion < 0. && imp5MaxPathLength < 0.,
            "Imp5 cuts require explicit selectionProfile: lambda_imp5");
  }
  REQUIRE(pidProfile == "dedx_pull" || pidProfile == "pico_nsigma", "unknown pidProfile");
  REQUIRE(std::isfinite(minNHitsRatio), "minNHitsRatio must be finite");
  REQUIRE(std::isfinite(minPt), "minPt must be finite");
  REQUIRE(std::isfinite(maxPt), "maxPt must be finite");
  REQUIRE(std::isfinite(minEta), "minEta must be finite");
  REQUIRE(std::isfinite(maxEta), "maxEta must be finite");
  REQUIRE(std::isfinite(minDedxError), "minDedxError must be finite");
  REQUIRE(std::isfinite(maxDedxError), "maxDedxError must be finite");
  REQUIRE(std::isfinite(nSigmaProton), "nSigmaProton must be finite");
  REQUIRE(std::isfinite(nSigmaPion), "nSigmaPion must be finite");
  REQUIRE(std::isfinite(nSigmaKaon), "nSigmaKaon must be finite");
  REQUIRE(std::isfinite(tofNSigma), "tofNSigma must be finite");
  REQUIRE(std::isfinite(tofPMax), "tofPMax must be finite");
  REQUIRE(std::isfinite(tofKaonPMin), "tofKaonPMin must be finite");
  REQUIRE(std::isfinite(tofKaonPMax), "tofKaonPMax must be finite");
  REQUIRE(std::isfinite(maxPositionVariance), "maxPositionVariance must be finite");
  REQUIRE(std::isfinite(maxMomentumVariance), "maxMomentumVariance must be finite");
  REQUIRE(std::isfinite(interfaceChiPrimaryCut), "interfaceChiPrimaryCut must be finite");
  REQUIRE(std::isfinite(primaryProbCut), "primaryProbCut must be finite");
  REQUIRE(std::isfinite(finderMaxDaughterDistance), "finderMaxDaughterDistance must be finite");
  REQUIRE(std::isfinite(finderLCut), "finderLCut must be finite");
  REQUIRE(std::isfinite(finderChiPrimary2D), "finderChiPrimary2D must be finite");
  REQUIRE(std::isfinite(finderChi2Ndf2D), "finderChi2Ndf2D must be finite");
  REQUIRE(std::isfinite(finderLdL2D), "finderLdL2D must be finite");
  REQUIRE(std::isfinite(finderLdLXiOmega), "finderLdLXiOmega must be finite");
  REQUIRE(std::isfinite(finderChi2TopoXiOmega), "finderChi2TopoXiOmega must be finite");
  REQUIRE(std::isfinite(finderChi2XiOmega), "finderChi2XiOmega must be finite");
  REQUIRE(std::isfinite(minMass), "minMass must be finite");
  REQUIRE(std::isfinite(maxMass), "maxMass must be finite");
  REQUIRE(std::isfinite(maxMassError), "maxMassError must be finite");
  REQUIRE(std::isfinite(maxChi2Ndf), "maxChi2Ndf must be finite");
  REQUIRE(std::isfinite(maxTopoChi2Ndf), "maxTopoChi2Ndf must be finite");
  REQUIRE(std::isfinite(maxDaughterDistance), "maxDaughterDistance must be finite");
  REQUIRE(std::isfinite(maxDistanceToPv), "maxDistanceToPv must be finite");
  REQUIRE(std::isfinite(minDecayLength), "minDecayLength must be finite");
  REQUIRE(std::isfinite(minDecayLengthSignificance), "minDecayLengthSignificance must be finite");
  REQUIRE(std::isfinite(minVertexLineSignificance), "minVertexLineSignificance must be finite");
  REQUIRE(std::isfinite(minCosPointing), "minCosPointing must be finite");
  REQUIRE(minNHitsFit >= 0 && minNHitsDedx >= 0, "negative hit threshold");
  REQUIRE(minNHitsRatio >= 0. && minNHitsRatio <= 1., "minNHitsRatio must be in [0,1]");
  REQUIRE(minPt >= 0. && maxPt > minPt && maxEta > minEta, "invalid track kinematic range");
  REQUIRE(minDedxError > 0. && maxDedxError >= minDedxError, "invalid dEdx resolution range");
  REQUIRE(nSigmaPion > 0. && nSigmaKaon > 0. && nSigmaProton > 0., "PID windows must be positive");
  REQUIRE(tofNSigma > 0. && tofPMax > 0. && tofKaonPMin >= 0. && tofKaonPMax > tofKaonPMin, "invalid TOF parameters");
  REQUIRE(maxPositionVariance > 0. && maxMomentumVariance > 0., "covariance limits must be positive");
  REQUIRE(interfaceChiPrimaryCut > 0. && primaryProbCut > 0. && primaryProbCut < 1., "invalid primary classification settings");
  REQUIRE(finderMaxDaughterDistance > 0. && finderLCut >= 0. && finderChiPrimary2D >= 0. &&
          finderChi2Ndf2D > 0. && finderLdL2D >= 0., "invalid Finder settings");
  REQUIRE(std::isfinite(finderLdLXiOmega) && std::isfinite(finderChi2TopoXiOmega) &&
          std::isfinite(finderChi2XiOmega), "Xi/Omega Finder cuts must be finite");
  REQUIRE(finderLdLXiOmega >= 0. && finderChi2TopoXiOmega > 0. && finderChi2XiOmega > 0.,
          "invalid Xi/Omega Finder settings");
  if (reconstructAntiXi)
    REQUIRE(reconstructAntiLambda, "reconstructAntiXi requires reconstructAntiLambda");
  REQUIRE(minMass > 0. && maxMass > minMass, "invalid mass range");
  REQUIRE(minCosPointing <= 1., "minCosPointing > 1");
  if (useTof) {
    REQUIRE(!tofCalibrationProfile.empty(), "TOF calibration provenance name is required");
    REQUIRE(tofMean.size() == 30 && tofSigma.size() == 30,
            "tofMean/tofSigma must each contain 30 coefficients: pi+,K+,p,pi-,K-,pbar, powers 0..4");
  }
  for (size_t i = 0; i < tofMean.size(); ++i) REQUIRE(std::isfinite(tofMean[i]), "non-finite TOF mean coefficient");
  for (size_t i = 0; i < tofSigma.size(); ++i) REQUIRE(std::isfinite(tofSigma[i]), "non-finite TOF sigma coefficient");
#undef REQUIRE
  return ok;
}

void KfParticleCutConfig::Dump(std::ostream& output) const {
  const std::streamsize precision = output.precision();
  const bool imp5 = selectionProfile == "lambda_imp5";
  output << std::setprecision(17);
  output << "# Profile-inactive cut parameters are omitted.\n";
  output << "schemaVersion: " << schemaVersion << "\n";
  output << "backend: " << backend << "\n";
  output << "selectionProfile: " << selectionProfile << "\n";
  if (imp5) {
    output << "imp5MinDCAProton: " << imp5MinDCAProton << "\n";
    output << "imp5MinDCAPion: " << imp5MinDCAPion << "\n";
    output << "imp5MaxPathLength: " << imp5MaxPathLength << "\n";
  }
  output << "pidProfile: " << pidProfile << "\n";
  output << "minNHitsFit: " << minNHitsFit << "\n";
  output << "minNHitsRatio: " << minNHitsRatio << "\n";
  if (!imp5) {
    output << "minNHitsDedx: " << minNHitsDedx << "\n";
    output << "minPt: " << minPt << "\n";
    output << "maxPt: " << maxPt << "\n";
    output << "minEta: " << minEta << "\n";
    output << "maxEta: " << maxEta << "\n";
    output << "minDedxError: " << minDedxError << "\n";
    output << "maxDedxError: " << maxDedxError << "\n";
  }
  output << "useHftTracksOnly: " << useHftTracksOnly << "\n";
  output << "nSigmaProton: " << nSigmaProton << "\n";
  output << "nSigmaPion: " << nSigmaPion << "\n";
  if (!imp5) output << "nSigmaKaon: " << nSigmaKaon << "\n";
  output << "useTof: " << useTof << "\n";
  output << "strictTofPid: " << strictTofPid << "\n";
  output << "cleanKaonsWithTof: " << cleanKaonsWithTof << "\n";
  if (!imp5 && useTof) {
    output << "tofCalibrationProfile: " << tofCalibrationProfile << "\n";
    output << "tofNSigma: " << tofNSigma << "\n";
    output << "tofPMax: " << tofPMax << "\n";
    output << "tofMean: [";
    for (size_t i = 0; i < tofMean.size(); ++i) output << (i ? ", " : "") << tofMean[i];
    output << "]\n";
    output << "tofSigma: [";
    for (size_t i = 0; i < tofSigma.size(); ++i) output << (i ? ", " : "") << tofSigma[i];
    output << "]\n";
  }
  if (!imp5 && cleanKaonsWithTof) {
    output << "tofKaonPMin: " << tofKaonPMin << "\n";
    output << "tofKaonPMax: " << tofKaonPMax << "\n";
  }
  output << "rejectBadCovariance: " << rejectBadCovariance << "\n";
  output << "maxPositionVariance: " << maxPositionVariance << "\n";
  output << "maxMomentumVariance: " << maxMomentumVariance << "\n";
  output << "interfaceChiPrimaryCut: " << interfaceChiPrimaryCut << "\n";
  // The external-PicoPV adapter does not refit a PV; primaryProbCut is a
  // backward-compatible initialization field, not an operative selection cut.
  output << "finderMaxDaughterDistance: " << finderMaxDaughterDistance << "\n";
  output << "finderLCut: " << finderLCut << "\n";
  output << "finderChiPrimary2D: " << finderChiPrimary2D << "\n";
  output << "finderChi2Ndf2D: " << finderChi2Ndf2D << "\n";
  output << "finderLdL2D: " << finderLdL2D << "\n";
  output << "finderLdLXiOmega: " << finderLdLXiOmega << "\n";
  output << "finderChi2TopoXiOmega: " << finderChi2TopoXiOmega << "\n";
  output << "finderChi2XiOmega: " << finderChi2XiOmega << "\n";
  output << "minMass: " << minMass << "\n";
  output << "maxMass: " << maxMass << "\n";
  // Keep explicit disabled-state values for backward-compatible output QA.
  output << "# Negative optional final bounds mean disabled (cosine <= -1).\n";
  output << "maxMassError: " << maxMassError << "\n";
  output << "maxChi2Ndf: " << maxChi2Ndf << "\n";
  output << "maxTopoChi2Ndf: " << maxTopoChi2Ndf << "\n";
  output << "maxDaughterDistance: " << maxDaughterDistance << "\n";
  output << "maxDistanceToPv: " << maxDistanceToPv << "\n";
  output << "minDecayLength: " << minDecayLength << "\n";
  output << "minDecayLengthSignificance: " << minDecayLengthSignificance << "\n";
  output << "minVertexLineSignificance: " << minVertexLineSignificance << "\n";
  output << "minCosPointing: " << minCosPointing << "\n";
  output << "reconstructAntiLambda: " << reconstructAntiLambda << "\n";
  output << "reconstructXi: " << reconstructXi << "\n";
  output << "reconstructAntiXi: " << reconstructAntiXi << "\n";
  output.precision(precision);
}
