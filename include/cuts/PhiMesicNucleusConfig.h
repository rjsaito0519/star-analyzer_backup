#ifndef PHI_MESIC_NUCLEUS_CONFIG_H
#define PHI_MESIC_NUCLEUS_CONFIG_H

#include "Rtypes.h"
#include <map>
#include <string>
#include <vector>

class PhiMesicNucleusConfig {
public:
  enum IntermediateType {
    kIntermediateLambda = 0,
    kIntermediateHypertriton2Body,
    kIntermediateHypertriton3Body,
    kIntermediateHyperhydrogen4
  };

  struct MassWindow {
    Double_t rawMin;
    Double_t rawMax;
    Double_t signalMin;
    Double_t signalMax;
    Double_t leftSbMin;
    Double_t leftSbMax;
    Double_t rightSbMin;
    Double_t rightSbMax;
  };

  struct TopologyCuts {
    Double_t minDaughterDca;
    Double_t maxDaughterDca;
    Double_t maxDcaToPv;
    Double_t minDecayLength;
    Double_t minTransverseDecayLength;
    Double_t minCosPointing;
    Double_t maxAbsRapidity;
    Double_t maxPathLength;
  };

  // Hyper 2-body daughter track cuts (Lambda-like)
  struct HyperTwoBodyTrackCuts {
    Double_t nSigmaPion;
    Double_t minDcaPion;
    Double_t minDcaNuclear;
    Int_t minNHitsFit;
    Double_t minNHitsRatio;
  };

  struct ParentRange {
    Double_t rawMin;
    Double_t rawMax;
    Double_t deltaMMin;
    Double_t deltaMMax;
    Double_t binWidthMeV;
  };

  struct ParentKinematics {
    Double_t ptMin;
    Double_t ptMax;
    Double_t yMin;
    Double_t yMax;
  };

  struct ChannelDef {
    std::string key;
    IntermediateType intermediateType;
    Bool_t enabled;
    Bool_t enableWrongSign;
    Bool_t enableRotation;
    Bool_t enableMixedEvent;
    Int_t expectedChargeSum;
    ParentRange parentRange;
    ParentKinematics parentKin;
    Double_t thresholdMass;
  };

  static PhiMesicNucleusConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  void SetDefaults();
  Bool_t Validate() const;

  const ChannelDef* FindChannel(const std::string& key) const;
  const std::vector<ChannelDef>& Channels() const { return channels; }

  // PID / kinematics
  Double_t kplusNSigmaMax;
  Double_t kplusPtMin;
  Double_t kplusPtMax;
  Double_t protonNSigmaMax;
  Double_t pionNSigmaMax;
  Bool_t requireTofForKplus;

  // Intermediate mass windows
  MassWindow lambdaMassWindow;
  MassWindow hypertriton2BodyMassWindow;
  MassWindow hypertriton3BodyMassWindow;
  MassWindow hyperhydrogen4MassWindow;

  // Topology
  TopologyCuts lambdaTopology;
  TopologyCuts hypertriton2BodyTopology;
  TopologyCuts hypertriton3BodyTopology;
  TopologyCuts hyperhydrogen4Topology;
  HyperTwoBodyTrackCuts hyperTwoBodyTrackCuts;

  // Parent (intermediate + K+ [, + nucleus]) must be consistent with PV origin:
  // bachelor K+ / nucleus DCA to PV upper bound (primary-like).
  Double_t parentPvOriginMaxBachelorDca;

  // Background / flow
  Bool_t prioritizeRawSpectraOnly;
  Bool_t enableWrongSignDefault;
  Bool_t enableRotationDefault;
  Bool_t enableMixedEventDefault;

  // Mixing
  Int_t mixingVzBins;
  Int_t mixingCent9Bins;
  Int_t mixingEventPlaneBins;
  Int_t mixingDepth;
  Bool_t mixingRequireDifferentEventId;

  // Safety
  Int_t maxLambdaCandidates;
  Int_t maxHyperCandidates;
  Int_t maxParentCombinations;

  // Scan bookkeeping
  Double_t bindingEnergyMinMeV;
  Double_t bindingEnergyMaxMeV;
  Double_t absorptionWidthMinMeV;
  Double_t absorptionWidthMaxMeV;

private:
  PhiMesicNucleusConfig();
  ~PhiMesicNucleusConfig();
  PhiMesicNucleusConfig(const PhiMesicNucleusConfig&);
  PhiMesicNucleusConfig& operator=(const PhiMesicNucleusConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
  void BuildDefaultChannels();
  void ParseChannelOverrides(const std::map<std::string, std::string>& values);
  static IntermediateType ParseIntermediateType(const std::string& text);
  static std::string IntermediateTypeName(IntermediateType type);

  std::vector<ChannelDef> channels;
};

#endif
