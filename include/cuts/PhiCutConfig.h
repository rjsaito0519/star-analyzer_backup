#ifndef PHI_CUT_CONFIG_H
#define PHI_CUT_CONFIG_H

#include "Rtypes.h"
#include <string>

class CentralityCutConfig;

class PhiCutConfig {
public:
  static PhiCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);

  // Cut values (public member variables)
  Double_t nSigmaKaon;
  Double_t minMass2Kaon;
  Double_t maxMass2Kaon;
  Double_t maxDCAKaon;
  Double_t maxDCAKK;
  Bool_t useInvMassCut;
  Double_t minInvMass;
  Double_t maxInvMass;
  Double_t minOpeningAngle;
  Double_t maxOpeningAngle;
  Double_t minPairRapidity;
  Double_t maxPairRapidity;
  // Event-plane (Q-vector) track selection
  Double_t minPtEp;
  Double_t maxPtEp;
  Double_t maxEtaEp;
  // Event-level: skip event if nTracks > maxNTr (0 = no limit)
  Int_t maxNTr;

  // Pair rapidity frame (lab / CM)
  std::string rapidityFrameRequested;  // auto | lab | cm
  Double_t sqrtSNNGeV;                 // 0 = unset (required for auto/cm without manual shift)
  Double_t nucleonMassGeV;
  Bool_t hasManualRapidityShift;
  Double_t rapidityShiftManual;

  std::string rapidityFrameEffective;  // lab | cm (set by FinalizeRapidityFrame)
  Double_t rapidityShiftEffective;
  Bool_t rapidityShiftFromManual;

  void SetDefaults();
  Bool_t FinalizeRapidityFrame(const CentralityCutConfig& centrality);
  Double_t ApplyAnalysisRapidity(Double_t yLab) const;
  std::string GetRapidityFrameSummary() const;

private:
  PhiCutConfig();
  ~PhiCutConfig();
  PhiCutConfig(const PhiCutConfig&);
  PhiCutConfig& operator=(const PhiCutConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
  static std::string ToLower(const std::string& s);
};

#endif
