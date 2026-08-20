#include "cuts/PhiMesicNucleusConfig.h"
#include "YamlParser.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>

namespace {
double Get(const std::map<std::string, std::string>& values, const std::string& key, double fallback) {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end()) return fallback;
  return YamlParser::ToDouble(it->second, fallback);
}

int GetInt(const std::map<std::string, std::string>& values, const std::string& key, int fallback) {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end()) return fallback;
  return YamlParser::ToInt(it->second, fallback);
}

bool GetBool(const std::map<std::string, std::string>& values, const std::string& key, bool fallback) {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end()) return fallback;
  return YamlParser::ToBool(it->second, fallback);
}
}  // namespace

PhiMesicNucleusConfig& PhiMesicNucleusConfig::GetInstance() {
  static PhiMesicNucleusConfig instance;
  return instance;
}

PhiMesicNucleusConfig::PhiMesicNucleusConfig() { SetDefaults(); }
PhiMesicNucleusConfig::~PhiMesicNucleusConfig() {}

void PhiMesicNucleusConfig::SetDefaults() {
  kplusNSigmaMax = 3.0;
  kplusPtMin = 0.2;
  kplusPtMax = 10.0;
  protonNSigmaMax = 2.0;
  pionNSigmaMax = 2.0;
  requireTofForKplus = kFALSE;

  lambdaMassWindow.rawMin = 1.08;
  lambdaMassWindow.rawMax = 1.16;
  lambdaMassWindow.signalMin = 1.108;
  lambdaMassWindow.signalMax = 1.124;
  lambdaMassWindow.leftSbMin = 1.090;
  lambdaMassWindow.leftSbMax = 1.102;
  lambdaMassWindow.rightSbMin = 1.130;
  lambdaMassWindow.rightSbMax = 1.145;

  hypertriton2BodyMassWindow.rawMin = 2.94;
  hypertriton2BodyMassWindow.rawMax = 3.04;
  hypertriton2BodyMassWindow.signalMin = 2.985;
  hypertriton2BodyMassWindow.signalMax = 2.999;
  hypertriton2BodyMassWindow.leftSbMin = 2.965;
  hypertriton2BodyMassWindow.leftSbMax = 2.980;
  hypertriton2BodyMassWindow.rightSbMin = 3.002;
  hypertriton2BodyMassWindow.rightSbMax = 3.020;

  hypertriton3BodyMassWindow = hypertriton2BodyMassWindow;
  hyperhydrogen4MassWindow.rawMin = 3.87;
  hyperhydrogen4MassWindow.rawMax = 3.98;
  hyperhydrogen4MassWindow.signalMin = 3.910;
  hyperhydrogen4MassWindow.signalMax = 3.935;
  hyperhydrogen4MassWindow.leftSbMin = 3.885;
  hyperhydrogen4MassWindow.leftSbMax = 3.905;
  hyperhydrogen4MassWindow.rightSbMin = 3.940;
  hyperhydrogen4MassWindow.rightSbMax = 3.965;

  lambdaTopology.minDaughterDca = 0.0;
  lambdaTopology.maxDaughterDca = 0.5;
  lambdaTopology.maxDcaToPv = 0.5;
  lambdaTopology.minDecayLength = 0.0;
  lambdaTopology.minTransverseDecayLength = 0.0;
  lambdaTopology.minCosPointing = 0.998;
  lambdaTopology.maxAbsRapidity = 2.0;
  lambdaTopology.maxPathLength = 100.0;

  // 2-body hyper: Lambda-like topology defaults
  hypertriton2BodyTopology = lambdaTopology;
  hyperhydrogen4Topology = lambdaTopology;

  // 3-body hyper: no Lambda-style DCA; rapidity only
  hypertriton3BodyTopology = lambdaTopology;
  hypertriton3BodyTopology.minDaughterDca = 0.0;
  hypertriton3BodyTopology.maxDaughterDca = 1.0e9;
  hypertriton3BodyTopology.maxDcaToPv = 1.0e9;
  hypertriton3BodyTopology.minDecayLength = 0.0;
  hypertriton3BodyTopology.minTransverseDecayLength = 0.0;
  hypertriton3BodyTopology.minCosPointing = -1.0;
  hypertriton3BodyTopology.maxPathLength = 1.0e9;

  hyperTwoBodyTrackCuts.nSigmaPion = 3.0;
  hyperTwoBodyTrackCuts.minDcaPion = 1.0;
  hyperTwoBodyTrackCuts.minDcaNuclear = 0.7;
  hyperTwoBodyTrackCuts.minNHitsFit = 15;
  hyperTwoBodyTrackCuts.minNHitsRatio = 0.52;

  parentPvOriginMaxBachelorDca = 1.0;

  prioritizeRawSpectraOnly = kTRUE;
  enableWrongSignDefault = kTRUE;
  enableRotationDefault = kFALSE;
  enableMixedEventDefault = kTRUE;

  mixingVzBins = 10;
  mixingCent9Bins = 9;
  mixingEventPlaneBins = 1;
  mixingDepth = 50;
  mixingRequireDifferentEventId = kTRUE;

  maxLambdaCandidates = 500;
  maxHyperCandidates = 500;
  maxParentCombinations = 50000;

  bindingEnergyMinMeV = 0.0;
  bindingEnergyMaxMeV = 200.0;
  absorptionWidthMinMeV = 10.0;
  absorptionWidthMaxMeV = 50.0;

  BuildDefaultChannels();
}

void PhiMesicNucleusConfig::BuildDefaultChannels() {
  channels.clear();
  ChannelDef ch;

  ch.key = "phi3He_direct";
  ch.intermediateType = kIntermediateLambda;
  ch.enabled = kTRUE;
  ch.enableWrongSign = enableWrongSignDefault;
  ch.enableRotation = enableRotationDefault;
  ch.enableMixedEvent = enableMixedEventDefault;
  ch.expectedChargeSum = 2;
  ch.parentRange.rawMin = 3.35;
  ch.parentRange.rawMax = 4.10;
  ch.parentRange.deltaMMin = -0.50;
  ch.parentRange.deltaMMax = 0.30;
  ch.parentRange.binWidthMeV = 2.0;
  ch.parentKin.ptMin = 0.0;
  ch.parentKin.ptMax = 10.0;
  ch.parentKin.yMin = -2.0;
  ch.parentKin.yMax = 2.0;
  ch.thresholdMass = 3.82785;
  channels.push_back(ch);

  ch.key = "phi4He_direct";
  ch.parentRange.rawMin = 4.25;
  ch.parentRange.rawMax = 5.05;
  ch.thresholdMass = 4.74684;
  channels.push_back(ch);

  ch.key = "phi3He_hypertriton_2body";
  ch.intermediateType = kIntermediateHypertriton2Body;
  ch.parentRange.rawMin = 3.35;
  ch.parentRange.rawMax = 4.10;
  ch.thresholdMass = 3.82785;
  channels.push_back(ch);

  ch.key = "phi3He_hypertriton_3body";
  ch.intermediateType = kIntermediateHypertriton3Body;
  channels.push_back(ch);

  ch.key = "phi4He_hyperhydrogen";
  ch.intermediateType = kIntermediateHyperhydrogen4;
  ch.parentRange.rawMin = 4.25;
  ch.parentRange.rawMax = 5.05;
  ch.thresholdMass = 4.74684;
  channels.push_back(ch);

  ch.key = "phiP_direct";
  ch.intermediateType = kIntermediateLambda;
  ch.expectedChargeSum = 1;
  ch.parentRange.rawMin = 1.45;
  ch.parentRange.rawMax = 2.30;
  ch.thresholdMass = 1.95795;
  channels.push_back(ch);
}

Bool_t PhiMesicNucleusConfig::LoadFromFile(const Char_t* filename) { return ParseYamlFile(filename); }

Bool_t PhiMesicNucleusConfig::ParseYamlFile(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "WARNING: failed to parse " << filename << ", use PhiMesicNucleus defaults." << std::endl;
    return kFALSE;
  }

  SetDefaults();
  kplusNSigmaMax = Get(values, "kplusPid.nSigmaMax", kplusNSigmaMax);
  kplusPtMin = Get(values, "kplusPid.ptMin", kplusPtMin);
  kplusPtMax = Get(values, "kplusPid.ptMax", kplusPtMax);
  protonNSigmaMax = Get(values, "protonPid.nSigmaMax", protonNSigmaMax);
  pionNSigmaMax = Get(values, "pionPid.nSigmaMax", pionNSigmaMax);
  requireTofForKplus = GetBool(values, "kplusPid.requireTof", requireTofForKplus);

  lambdaMassWindow.signalMin = Get(values, "lambdaMassWindow.signalMin", lambdaMassWindow.signalMin);
  lambdaMassWindow.signalMax = Get(values, "lambdaMassWindow.signalMax", lambdaMassWindow.signalMax);
  hypertriton2BodyMassWindow.signalMin =
      Get(values, "hypertriton2BodyMassWindow.signalMin", hypertriton2BodyMassWindow.signalMin);
  hypertriton2BodyMassWindow.signalMax =
      Get(values, "hypertriton2BodyMassWindow.signalMax", hypertriton2BodyMassWindow.signalMax);
  hypertriton3BodyMassWindow.signalMin =
      Get(values, "hypertriton3BodyMassWindow.signalMin", hypertriton3BodyMassWindow.signalMin);
  hypertriton3BodyMassWindow.signalMax =
      Get(values, "hypertriton3BodyMassWindow.signalMax", hypertriton3BodyMassWindow.signalMax);
  hyperhydrogen4MassWindow.signalMin =
      Get(values, "hyperhydrogen4MassWindow.signalMin", hyperhydrogen4MassWindow.signalMin);
  hyperhydrogen4MassWindow.signalMax =
      Get(values, "hyperhydrogen4MassWindow.signalMax", hyperhydrogen4MassWindow.signalMax);

  // 2-body hyper topology (Lambda-like)
  hypertriton2BodyTopology.maxDaughterDca =
      Get(values, "hyperTwoBody.maxDaughterDCA", hypertriton2BodyTopology.maxDaughterDca);
  hypertriton2BodyTopology.maxDcaToPv =
      Get(values, "hyperTwoBody.maxDCAV0", hypertriton2BodyTopology.maxDcaToPv);
  hypertriton2BodyTopology.minCosPointing =
      Get(values, "hyperTwoBody.minCosPointing", hypertriton2BodyTopology.minCosPointing);
  hypertriton2BodyTopology.maxPathLength =
      Get(values, "hyperTwoBody.maxPathLength", hypertriton2BodyTopology.maxPathLength);
  hypertriton2BodyTopology.maxAbsRapidity =
      Get(values, "hyperTwoBody.maxAbsRapidity", hypertriton2BodyTopology.maxAbsRapidity);
  hyperhydrogen4Topology = hypertriton2BodyTopology;

  hyperTwoBodyTrackCuts.nSigmaPion =
      Get(values, "hyperTwoBody.nSigmaPion", hyperTwoBodyTrackCuts.nSigmaPion);
  hyperTwoBodyTrackCuts.minDcaPion =
      Get(values, "hyperTwoBody.minDCAPion", hyperTwoBodyTrackCuts.minDcaPion);
  hyperTwoBodyTrackCuts.minDcaNuclear =
      Get(values, "hyperTwoBody.minDCANuclear", hyperTwoBodyTrackCuts.minDcaNuclear);
  hyperTwoBodyTrackCuts.minNHitsFit =
      GetInt(values, "hyperTwoBody.minNHitsFit", hyperTwoBodyTrackCuts.minNHitsFit);
  hyperTwoBodyTrackCuts.minNHitsRatio =
      Get(values, "hyperTwoBody.minNHitsRatio", hyperTwoBodyTrackCuts.minNHitsRatio);

  hypertriton3BodyTopology.maxAbsRapidity =
      Get(values, "hyperThreeBody.maxAbsRapidity", hypertriton3BodyTopology.maxAbsRapidity);

  parentPvOriginMaxBachelorDca =
      Get(values, "parentPvOrigin.maxBachelorDcaToPv", parentPvOriginMaxBachelorDca);

  prioritizeRawSpectraOnly = GetBool(values, "background.prioritizeRawSpectraOnly", prioritizeRawSpectraOnly);
  enableWrongSignDefault = GetBool(values, "background.enableWrongSign", enableWrongSignDefault);
  enableRotationDefault = GetBool(values, "background.enableRotation", enableRotationDefault);
  enableMixedEventDefault = GetBool(values, "background.enableMixedEvent", enableMixedEventDefault);

  mixingVzBins = GetInt(values, "mixing.vzBins", mixingVzBins);
  mixingCent9Bins = GetInt(values, "mixing.cent9Bins", mixingCent9Bins);
  mixingEventPlaneBins = GetInt(values, "mixing.eventPlaneBins", mixingEventPlaneBins);
  mixingDepth = GetInt(values, "mixing.depth", mixingDepth);
  mixingRequireDifferentEventId = GetBool(values, "mixing.requireDifferentEventId", mixingRequireDifferentEventId);

  maxLambdaCandidates = GetInt(values, "safetyCaps.maxLambdaCandidates", maxLambdaCandidates);
  maxHyperCandidates = GetInt(values, "safetyCaps.maxHyperCandidates", maxHyperCandidates);
  maxParentCombinations = GetInt(values, "safetyCaps.maxParentCombinations", maxParentCombinations);

  ParseChannelOverrides(values);
  return Validate();
}

void PhiMesicNucleusConfig::ParseChannelOverrides(const std::map<std::string, std::string>& values) {
  for (size_t i = 0; i < channels.size(); ++i) {
    ChannelDef& ch = channels[i];
    const std::string base = ch.key;
    ch.enabled = GetBool(values, "channelEnable." + base, ch.enabled);
    ch.enableWrongSign = GetBool(values, "background.channel." + base + ".enableWrongSign", ch.enableWrongSign);
    ch.enableRotation = GetBool(values, "background.channel." + base + ".enableRotation", ch.enableRotation);
    ch.enableMixedEvent = GetBool(values, "background.channel." + base + ".enableMixedEvent", ch.enableMixedEvent);
    ch.parentRange.rawMin = Get(values, "parentMassRange." + base + ".rawMin", ch.parentRange.rawMin);
    ch.parentRange.rawMax = Get(values, "parentMassRange." + base + ".rawMax", ch.parentRange.rawMax);
    ch.parentRange.deltaMMin = Get(values, "parentMassRange." + base + ".deltaMMin", ch.parentRange.deltaMMin);
    ch.parentRange.deltaMMax = Get(values, "parentMassRange." + base + ".deltaMMax", ch.parentRange.deltaMMax);
    ch.parentRange.binWidthMeV = Get(values, "parentMassRange." + base + ".binWidthMeV", ch.parentRange.binWidthMeV);
    ch.parentKin.ptMin = Get(values, "parentKinematics." + base + ".ptMin", ch.parentKin.ptMin);
    ch.parentKin.ptMax = Get(values, "parentKinematics." + base + ".ptMax", ch.parentKin.ptMax);
    ch.parentKin.yMin = Get(values, "parentKinematics." + base + ".yMin", ch.parentKin.yMin);
    ch.parentKin.yMax = Get(values, "parentKinematics." + base + ".yMax", ch.parentKin.yMax);
    ch.thresholdMass = Get(values, "thresholdMass." + base, ch.thresholdMass);
  }
}

Bool_t PhiMesicNucleusConfig::Validate() const {
  Bool_t ok = kTRUE;
  if (!(kplusPtMin < kplusPtMax)) {
    std::cerr << "ERROR: kplusPtMin must be < kplusPtMax" << std::endl;
    ok = kFALSE;
  }
  if (mixingVzBins < 1 || mixingCent9Bins < 1 || mixingEventPlaneBins < 1 || mixingDepth < 1) {
    std::cerr << "ERROR: mixing bins/depth must be >= 1" << std::endl;
    ok = kFALSE;
  }
  for (size_t i = 0; i < channels.size(); ++i) {
    const ChannelDef& ch = channels[i];
    if (!(ch.parentRange.rawMin < ch.parentRange.rawMax)) {
      std::cerr << "ERROR: channel " << ch.key << " rawMin/rawMax invalid" << std::endl;
      ok = kFALSE;
    }
    if (!(ch.parentRange.deltaMMin < ch.parentRange.deltaMMax)) {
      std::cerr << "ERROR: channel " << ch.key << " deltaM range invalid" << std::endl;
      ok = kFALSE;
    }
  }
  return ok;
}

const PhiMesicNucleusConfig::ChannelDef* PhiMesicNucleusConfig::FindChannel(const std::string& key) const {
  for (size_t i = 0; i < channels.size(); ++i) {
    if (channels[i].key == key) return &channels[i];
  }
  return 0;
}

PhiMesicNucleusConfig::IntermediateType PhiMesicNucleusConfig::ParseIntermediateType(const std::string& text) {
  if (text == "Lambda") return kIntermediateLambda;
  if (text == "Hypertriton2Body") return kIntermediateHypertriton2Body;
  if (text == "Hypertriton3Body") return kIntermediateHypertriton3Body;
  if (text == "Hyperhydrogen4") return kIntermediateHyperhydrogen4;
  return kIntermediateLambda;
}

std::string PhiMesicNucleusConfig::IntermediateTypeName(IntermediateType type) {
  switch (type) {
    case kIntermediateLambda:
      return "Lambda";
    case kIntermediateHypertriton2Body:
      return "Hypertriton2Body";
    case kIntermediateHypertriton3Body:
      return "Hypertriton3Body";
    case kIntermediateHyperhydrogen4:
      return "Hyperhydrogen4";
    default:
      return "Unknown";
  }
}
