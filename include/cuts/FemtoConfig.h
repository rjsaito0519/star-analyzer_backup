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

  // Fully-mixed KK phi template (phi_mix species): K+ and K- from distinct pool events.
  // kstarMassFitCF background template alternative to ROT (SE/ME wide TH3 via channels).
  // Standard MIX KK (current K x buffer opposite K) for mass BG template (species phi_mix).
  Bool_t fullyMixedEnabled;
  std::string fullyMixedSpeciesKey;
  std::string fullyMixedParticleKey;
  Int_t fullyMixedMaxCandidates; // cap per event on combined fwd+rev; <=0 = uncapped (validation only)
  Int_t fullyMixedSamplingSeed;  // >0 fixed SplitMix64 seed; 0 = time-based (not for production)

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

  // k* loop range for kstarMassFitCF (and deprecated Topic 3 / direct mass-fit).
  Bool_t purityFitUseConstantBkg;  // deprecated: Topic 3 / Method 5 only
  Double_t purityFitGaussSigmaMin;
  Double_t purityFitGaussSigmaMax;
  Double_t purityMinKstar;
  Double_t purityMaxKstar;
  Int_t purityMinEntriesPerBin;
  Double_t purityClampMin;
  Double_t purityClampMax;
  std::string cfBkgMode;  // deprecated Topic 3: me_mass

  // Deprecated Method 5 CF-subtraction. Default none; enable only with legacyCfPagesEnabled.
  std::string cfSubtractionMode;      // none | method5
  std::string cfSubPurityMode;        // fixed | fit_slice
  Double_t cfSubPurityFixed;
  std::string cfSubSidebandCombine;   // sumLR | avgCF_LR
  Bool_t cfSubWriteSidecarRoot;
  Int_t cfSubLowStatsRebinExtra;

  // kstarMassFitCF: per-k* full M_KK, S=F-αB (ROT/MIX template), C = Y_SE/Y_ME.
  Bool_t kstarMassFitCfEnabled;
  std::string kstarMassFitCfTemplate;  // rot | mix
  Bool_t kstarMassFitCfCrossCheck;
  Double_t kstarMassFitCfFitMassMin;
  Double_t kstarMassFitCfFitMassMax;
  Double_t kstarMassFitCfKstarBinWidth;  // [GeV/c]; must be >0 when enabled
  Int_t kstarMassFitCfLowKstarMergeBins;  // merge first N rebinned k* bins (1 = disabled)
  Double_t kstarMassFitCfAlphaMassMin;    // α window; max<=min => leftSB+rightSB
  Double_t kstarMassFitCfAlphaMassMax;
  Bool_t kstarMassFitCfWriteSidecar;

  // Deprecated: Topic 3 / direct mass-fit / Method 5 pages. Default false.
  Bool_t legacyCfPagesEnabled;
  std::string purityDirectFitModel;  // deprecated direct mass-fit: gaus_pol2 | gaus_const

  // h-K correlations extension (off by default; existing paths unchanged when false).
  Bool_t enableHKaonTwoBody;  // fill two-body h-K+/- pairs (needs phikaon_plus/minus species + channels)
  Bool_t enableKuboTriplet;   // fill Kubo-rule 3-body triplet background histograms (p, d)
  Bool_t enableKuboGenuine;   // macro-side: also produce Kubo-subtracted genuine CF (p, d)
  Bool_t kuboStoreFullMass;   // also fill full-M_KK TH3 Kubo topologies (no early signal-window cut)

  // PID QA: all quality tracks near signal-window phi in PRF (k*). Not used in CF.
  Bool_t phiNearTrackQaEnabled;
  std::string phiNearTrackSignalChannel;  // mass window from this channel's signalMin/Max
  Double_t phiNearTrackMaxKstarLoose;     // e.g. 1.0 GeV/c
  Double_t phiNearTrackMaxKstarTight;     // e.g. 0.3 GeV/c (subset of loose)
  std::string phiNearTrackMassHyp;        // pion | proton | kaon (track 4-vector for k*)

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
