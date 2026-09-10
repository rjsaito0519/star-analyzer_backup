#ifndef KF_PARTICLE_CUT_CONFIG_H
#define KF_PARTICLE_CUT_CONFIG_H

#include "Rtypes.h"
#include <iosfwd>
#include <string>
#include <vector>

// Plain configuration only: this header must never depend on the KF backend.
class KfParticleCutConfig {
public:
  static KfParticleCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  void SetDefaults();
  Bool_t Validate(std::ostream& errors) const;
  void Dump(std::ostream& output) const;

  Int_t schemaVersion;
  std::string backend;
  // Explicit, backward-compatible non-KF selection profile.
  std::string selectionProfile;
  Double_t imp5MinDCAProton;
  Double_t imp5MinDCAPion;
  Double_t imp5MaxPathLength;
  std::string pidProfile;
  std::string tofCalibrationProfile;
  Int_t minNHitsFit;
  Int_t minNHitsDedx;
  Double_t minNHitsRatio;
  Double_t minPt;
  Double_t maxPt;
  Double_t minEta;
  Double_t maxEta;
  Double_t minDedxError;
  Double_t maxDedxError;
  Bool_t useHftTracksOnly;
  Double_t nSigmaProton;
  Double_t nSigmaPion;
  Double_t nSigmaKaon;
  Bool_t useTof;
  Bool_t strictTofPid;
  Bool_t cleanKaonsWithTof;
  Double_t tofNSigma;
  Double_t tofPMax;
  Double_t tofKaonPMin;
  Double_t tofKaonPMax;
  std::vector<double> tofMean;
  std::vector<double> tofSigma;
  Bool_t rejectBadCovariance;
  Double_t maxPositionVariance;
  Double_t maxMomentumVariance;
  Double_t interfaceChiPrimaryCut;
  Double_t primaryProbCut;
  Double_t finderMaxDaughterDistance;
  Double_t finderLCut;
  Double_t finderChiPrimary2D;
  Double_t finderChi2Ndf2D;
  Double_t finderLdL2D;
  Double_t finderLdLXiOmega;
  Double_t finderChi2TopoXiOmega;
  Double_t finderChi2XiOmega;
  Double_t minMass;
  Double_t maxMass;
  Double_t maxMassError;
  Double_t maxChi2Ndf;
  Double_t maxTopoChi2Ndf;
  Double_t maxDaughterDistance;
  Double_t maxDistanceToPv;
  Double_t minDecayLength;
  Double_t minDecayLengthSignificance;
  Double_t minVertexLineSignificance;
  Double_t minCosPointing;
  Bool_t reconstructAntiLambda;
  Bool_t reconstructXi;
  Bool_t reconstructAntiXi;

private:
  KfParticleCutConfig();
  ~KfParticleCutConfig();
  KfParticleCutConfig(const KfParticleCutConfig&);
  KfParticleCutConfig& operator=(const KfParticleCutConfig&);
};

#endif
