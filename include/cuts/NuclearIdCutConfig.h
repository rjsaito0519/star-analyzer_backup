#ifndef NUCLEARID_CUT_CONFIG_H
#define NUCLEARID_CUT_CONFIG_H

#include "Rtypes.h"

class NuclearIdCutConfig {
public:
  static NuclearIdCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);

  // TPC dE/dx PID
  Double_t nSigmaFill1D;
  Double_t nSigmaExclude;
  Double_t maxNSigmaNuclear;

  // TOF m2
  Double_t m2SigmaCut;
  Bool_t m2_selection;

  // General phase space / noise exclusion
  Double_t minP_M2cut;
  Double_t minM2_M2cut;
  Double_t maxM2_M2cut;

  // p/q upper momentum cut (applied to raw TPC rigidity before x2 for He)
  Double_t maxPOverQ;  // GeV/c; cut: p(TPC) < maxPOverQ  for all species

  Bool_t requireBestSpecies;
  Short_t minNHitsDedxNuclear;

  // Lambda selection cuts
  Double_t MeanLambda;
  Double_t sigmaLambda;
  Double_t nsigmaLambda;

  void SetDefaults();

private:
  NuclearIdCutConfig();
  ~NuclearIdCutConfig();
  NuclearIdCutConfig(const NuclearIdCutConfig&);
  NuclearIdCutConfig& operator=(const NuclearIdCutConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif
