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
  Double_t minDCAPion;     // min DCA to PV for Lambda pion (pi-)

  // K0s (pi+ pi-) daughter cuts — StK0shortMaker / StK0shortFxtMaker only.
  // YAML may omit these; ParseYamlFile falls back to legacy proton/pion keys above.
  Double_t nSigmaPionPos;
  Double_t nSigmaPionNeg;
  Double_t minDCAPionPos;
  Double_t minDCAPionNeg;
  Double_t maxDaughterDCA;   // max DCA between p and pi- at V0 (legacy / fallback)
  Double_t maxDcaLambdaDaughters;  // max DCA between proton and Lambda pion (cm); <=0 => maxDaughterDCA
  Double_t maxDcaLambdaBachelor;   // max DCA between Lambda and bachelor pion (cm); <=0 => maxDaughterDCA
  Double_t maxDCAV0;         // max DCA of Xi (or V0) to primary vertex
  Double_t minCosPointing;   // min cos(pointing angle)
  Double_t maxPathLength;    // max |path length| for helix (e.g. 100)
  Double_t minDCABachelor;   // min DCA to PV for Xi bachelor pion
  // Min DCA of reconstructed Lambda line (v2, pLam) to PV (cm).
  // Secondary Lambda from Xi should miss PV; <=0 => cut off.
  Double_t minDcaLambdaToPV;
  Double_t minPtDaughter;    // min pT of p / Lambda-pi / bachelor (GeV/c)
  Double_t minDecayLengthXi; // min Xi flight distance from PV (cm)
  Double_t lambdaMassWindow; // |M_pπ - PDG Λ|; fill QA before this cut
  Double_t xiMassWindow;     // |M_Λπ - PDG Ξ| for candidate list only; <=0 => no list cut

  // Step 2 (StXiFxtMaker only): jan1 purity cuts
  Bool_t   requireDecayLengthOrder; // if true: reject when L_Xi >= L_Lambda (cm)
  Double_t fakeLambdaMean;          // fake Λ veto center (GeV)
  Double_t fakeLambdaWindow;        // |M_fake - mean| max; <=0 => veto off

  // Track quality cuts (added for S/N improvement)
  Int_t    minNHitsFit;      // min nHitsFit for daughter tracks
  Double_t minNHitsRatio;    // min nHitsFit/nHitsMax ratio (split track suppression)

  // StK0shortFxtMaker: if true, omit pion candidates already used as Xi daughters.
  // Pair-level ShareTracks in StK0XiFxtFemtoMaker is independent (k* veto).
  Bool_t skipK0DaughtersUsedByXi;

  void SetDefaults();

private:
  LambdaCutConfig();
  ~LambdaCutConfig();
  LambdaCutConfig(const LambdaCutConfig&);
  LambdaCutConfig& operator=(const LambdaCutConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif
