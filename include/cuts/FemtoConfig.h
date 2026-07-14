#ifndef FEMTO_CONFIG_H
#define FEMTO_CONFIG_H

// Species/channel key naming rules: StMaker/StFemtoMaker/README.md

#include "Rtypes.h"
#include <map>
#include <string>
#include <vector>

class FemtoConfig {
 public:
  static FemtoConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);

  struct SpeciesDef {
    std::string key;
    std::string builderType;  // track | resonance
    std::string particleKey;  // proton | phi | phi_rotation | ...
    std::string cutsRef;
  };

  struct ChannelDef {
    std::string name;
    std::string partA;
    std::string partB;
    Bool_t enabled;
    Bool_t doMixing;
    Double_t signalMin;
    Double_t signalMax;
    Double_t normQMin;
    Double_t normQMax;
  };

  std::map<std::string, SpeciesDef> species;
  std::vector<ChannelDef> channels;

  // Zhangwei-like proton bachelor cuts for femto pairing (see 4ReadTree/analysis.cxx).
  std::string protonChargeMode;
  Double_t protonMaxDca;
  Double_t protonMinPtPre;
  Double_t protonMinPtPair;
  Double_t protonMaxPtPair;
  Double_t protonMaxAbsEta;
  Double_t protonMaxAbsNSigma;
  Short_t protonMinNHitsFit;
  Double_t protonMinNHitsRatio;
  Double_t protonTofMomentumThreshold;
  Double_t protonMinMass2;
  Double_t protonMaxMass2;
  Double_t protonMinRapidityCm;
  Double_t protonMaxRapidityCm;

  // K- bachelor cuts for femto pairing (anaFemtoKaon).
  Double_t kaonMinusMaxDca;
  Double_t kaonMinusMinPtPre;
  Double_t kaonMinusMinPtPair;
  Double_t kaonMinusMaxPtPair;
  Double_t kaonMinusMaxAbsEta;
  Double_t kaonMinusMaxAbsNSigma;
  Short_t kaonMinusMinNHitsFit;
  Double_t kaonMinusMinNHitsRatio;
  Double_t kaonMinusTofMomentumThreshold;
  Double_t kaonMinusMinMass2;
  Double_t kaonMinusMaxMass2;
  Double_t kaonMinusMinRapidityCm;
  Double_t kaonMinusMaxRapidityCm;

  // 4He bachelor cuts for femto pairing.
  Double_t he4MaxDca;
  Double_t he4MinPMom;
  Double_t he4MaxPMom;
  Double_t he4MinPtPre;
  Double_t he4MaxPtPre;
  Double_t he4MinPtPair;
  Double_t he4MaxPtPair;
  Double_t he4MaxAbsEta;
  Double_t he4MaxAbsNSigma;
  Short_t he4MinNHitsFit;
  Double_t he4MinNHitsRatio;
  Double_t he4TofMomentumThreshold;
  Double_t he4MinMass2;
  Double_t he4MaxMass2;
  Double_t he4MinRapidityCm;
  Double_t he4MaxRapidityCm;

  // Deuteron bachelor cuts for femto pairing.
  Double_t deuteronMaxDca;
  Double_t deuteronMinPMom;
  Double_t deuteronMaxPMom;
  Double_t deuteronMinPtPre;
  Double_t deuteronMaxPtPre;
  Double_t deuteronMinPtPair;
  Double_t deuteronMaxPtPair;
  Double_t deuteronMaxAbsEta;
  Double_t deuteronMaxAbsNSigma;
  Short_t deuteronMinNHitsFit;
  Double_t deuteronMinNHitsRatio;
  Double_t deuteronTofMomentumThreshold;
  Double_t deuteronMinMass2;
  Double_t deuteronMaxMass2;
  Double_t deuteronMinRapidityCm;
  Double_t deuteronMaxRapidityCm;

  // Triton bachelor cuts for femto pairing.
  Double_t tritonMaxDca;
  Double_t tritonMinPMom;
  Double_t tritonMaxPMom;
  Double_t tritonMinPtPre;
  Double_t tritonMaxPtPre;
  Double_t tritonMinPtPair;
  Double_t tritonMaxPtPair;
  Double_t tritonMaxAbsEta;
  Double_t tritonMaxAbsNSigma;
  Short_t tritonMinNHitsFit;
  Double_t tritonMinNHitsRatio;
  Double_t tritonTofMomentumThreshold;
  Double_t tritonMinMass2;
  Double_t tritonMaxMass2;
  Double_t tritonMinRapidityCm;
  Double_t tritonMaxRapidityCm;

  // 3He bachelor cuts for femto pairing.
  Double_t he3MaxDca;
  Double_t he3MinPMom;
  Double_t he3MaxPMom;
  Double_t he3MinPtPre;
  Double_t he3MaxPtPre;
  Double_t he3MinPtPair;
  Double_t he3MaxPtPair;
  Double_t he3MaxAbsEta;
  Double_t he3MaxAbsNSigma;
  Short_t he3MinNHitsFit;
  Double_t he3MinNHitsRatio;
  Double_t he3TofMomentumThreshold;
  Double_t he3MinMass2;
  Double_t he3MaxMass2;
  Double_t he3MinRapidityCm;
  Double_t he3MaxRapidityCm;

  // Rotation background (phi_rot species).
  Bool_t rotationEnabled;
  std::string rotationSpeciesKey;
  std::string rotationParticleKey;
  Int_t rotationN;
  Double_t rotationMinAngle;
  Double_t rotationMaxAngle;
  Int_t rotationSeed;

  // checkHist CF: merge this many adjacent k* bins after merge (1 = no rebin).
  Int_t cfRebinFactor;

  // checkHist CF cent slice: project hKstar*VsCent over cent9 in [cfCent9Min, cfCent9Max].
  Int_t cfCent9Min;
  Int_t cfCent9Max;

  struct CfCentSlice {
    std::string id;
    Int_t cent9Min;
    Int_t cent9Max;
  };

  // checkHist: per-slice cent9 projection ranges (default 15: cent9_0..8 + pct_0_10..60).
  std::vector<CfCentSlice> cfCentSlices;

  // Slice ids printed in QA PDF (default pct_0_10, pct_0_20, pct_0_30).
  std::vector<std::string> cfCentSlicesQaPdfInclude;

  // When true, slices in cfCentSlicesQaPdfInclude are omitted from CF PDF.
  Bool_t cfPdfExcludeQaSlices;

  // Sideband-subtracted CF (checkHist Phase 3).
  Double_t sidebandSubtractAlpha;
  std::string sidebandAlphaMode;  // fixed | massYieldRatio (future)
  std::string negativeBinPolicy;  // zero | skip

  // k*-binned purity / CF_genuine (checkHist Topic 3).
  Bool_t purityFitUseConstantBkg;
  Double_t purityFitGaussSigmaMin;
  Double_t purityFitGaussSigmaMax;
  Double_t purityMinKstar;
  Double_t purityMaxKstar;
  Int_t purityMinEntriesPerBin;
  Double_t purityClampMin;
  Double_t purityClampMax;
  std::string cfBkgMode;  // me_mass

  Bool_t Validate() const;
  const CfCentSlice* FindCfCentSlice(const std::string& id) const;
  Bool_t IsCfCentSliceInQaPdf(const std::string& id) const;
  const SpeciesDef* FindSpecies(const std::string& key) const;
  const ChannelDef* FindChannel(const std::string& name) const;

  void SetDefaults();

 private:
  FemtoConfig();
  ~FemtoConfig();
  FemtoConfig(const FemtoConfig&);
  FemtoConfig& operator=(const FemtoConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
  void ApplyYamlValues(const std::map<std::string, std::string>& values);
};

#endif
