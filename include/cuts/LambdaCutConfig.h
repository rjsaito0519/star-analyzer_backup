#ifndef LAMBDA_CUT_CONFIG_H
#define LAMBDA_CUT_CONFIG_H

#include "Rtypes.h"

class LambdaCutConfig {
public:
  static LambdaCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);

  // V0 Lambda (p+ pi-) reconstruction cuts
  Double_t nSigmaProton;
  Double_t nSigmaPion;
  Double_t minDCAProton;   // min DCA to PV for proton (reject primaries)
  Double_t minDCAPion;     // min DCA to PV for pion
  Double_t maxDaughterDCA;   // max DCA between p and pi- at V0 (dca12)
  Double_t maxDCAV0;         // max DCA of Lambda to primary vertex
  Double_t minCosPointing;   // min cos(pointing angle)
  Double_t maxPathLength;    // max |path length| for helix (e.g. 100)

  // Track quality cuts (added for S/N improvement)
  Int_t    minNHitsFit;      // min nHitsFit for daughter tracks
  Double_t minNHitsRatio;    // min nHitsFit/nHitsMax ratio (split track suppression)

  void SetDefaults();

private:
  LambdaCutConfig();
  ~LambdaCutConfig();
  LambdaCutConfig(const LambdaCutConfig&);
  LambdaCutConfig& operator=(const LambdaCutConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif
