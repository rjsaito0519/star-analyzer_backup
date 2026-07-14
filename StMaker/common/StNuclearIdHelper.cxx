#include "StNuclearIdHelper.h"
#include "NuclearIdDeDxVsMom.h"
#include "ConfigManager.h"
#include "cuts/NuclearIdCutConfig.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"
#include <TMath.h>

namespace {
using NuclearIdDeDxVsMom::kDeuteron;
using NuclearIdDeDxVsMom::kHe3;
using NuclearIdDeDxVsMom::kHe4;
using NuclearIdDeDxVsMom::kTriton;
using NuclearIdDeDxVsMom::SpeciesIndex;

SpeciesIndex ToDeDxIndex(NuclearSpecies sp) {
  switch (sp) {
    case kNucDeuteron:
      return kDeuteron;
    case kNucTriton:
      return kTriton;
    case kNucHe3:
      return kHe3;
    case kNucHe4:
      return kHe4;
    default:
      return kHe4;
  }
}

const Double_t kM2_d_mean = 3.48096;
const Double_t kM2_d_sigma = 0.141458;
const Double_t kM2_He3_mean = 1.92385;
const Double_t kM2_He3_sigma = 0.094;
const Double_t kM2_t_mean = 7.76906;
const Double_t kM2_t_sigma = 0.341755;
}  // namespace

Double_t StNuclearIdHelper::SpeciesMass(NuclearSpecies sp) {
  switch (sp) {
    case kNucDeuteron:
      return 1.87561;
    case kNucTriton:
      return 2.80892;
    case kNucHe3:
      return 2.80839;
    case kNucHe4:
      return 3.72742;
    default:
      return 3.72742;
  }
}

Int_t StNuclearIdHelper::ChargeNumber(NuclearSpecies sp) {
  switch (sp) {
    case kNucDeuteron:
      return 1;
    case kNucTriton:
      return 1;
    case kNucHe3:
      return 2;
    case kNucHe4:
      return 2;
    default:
      return 2;
  }
}

Double_t StNuclearIdHelper::EffectiveMomentum(Double_t pTrack, NuclearSpecies sp) {
  return ChargeNumber(sp) * pTrack;
}

TVector3 StNuclearIdHelper::EffectiveMomentum(const TVector3& pTrack, NuclearSpecies sp) {
  return ChargeNumber(sp) * pTrack;
}

Double_t StNuclearIdHelper::GetNSigma(NuclearSpecies sp, Double_t p, Double_t dedx) {
  return NuclearIdDeDxVsMom::GetNSigma(ToDeDxIndex(sp), p, dedx);
}

Bool_t StNuclearIdHelper::PassTpc(NuclearSpecies sp, const NuclearTrackState& state) {
  const NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();
  const Double_t nSigma = GetNSigma(sp, state.pMag, state.dedx);
  return (TMath::Abs(nSigma) < cuts.maxNSigmaNuclear);
}

Bool_t StNuclearIdHelper::PassTof(NuclearSpecies sp, const NuclearTrackState& state) {
  const NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();
  if (!cuts.m2_selection) return kTRUE;
  if (!state.tofMatch || state.mass2 < -900.0f) return kFALSE;
  if (state.pMag <= cuts.minP_M2cut) return kFALSE;

  const Double_t m2 = state.mass2;
  switch (sp) {
    case kNucDeuteron:
      return (m2 > kM2_d_mean - cuts.m2SigmaCut * kM2_d_sigma &&
              m2 < kM2_d_mean + cuts.m2SigmaCut * kM2_d_sigma);
    case kNucTriton:
      return (m2 > kM2_t_mean - cuts.m2SigmaCut * kM2_t_sigma &&
              m2 < kM2_t_mean + cuts.m2SigmaCut * kM2_t_sigma);
    case kNucHe3:
      return (m2 > kM2_He3_mean - cuts.m2SigmaCut * kM2_He3_sigma &&
              m2 < kM2_He3_mean + cuts.m2SigmaCut * kM2_He3_sigma);
    case kNucHe4:
      return (m2 > cuts.minM2_M2cut && m2 < cuts.maxM2_M2cut);
    default:
      return kFALSE;
  }
}

Bool_t StNuclearIdHelper::IsDeuteron(const NuclearTrackState& state) {
  const NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();
  if (state.dedx <= 0) return kFALSE;
  if (state.pMag < 0.1) return kFALSE;

  const Double_t nSigma_d = GetNSigma(kNucDeuteron, state.pMag, state.dedx);
  const Double_t nSigma_t = GetNSigma(kNucTriton, state.pMag, state.dedx);
  const Double_t nSigma_3He = GetNSigma(kNucHe3, state.pMag, state.dedx);
  const Double_t nSigma_4He = GetNSigma(kNucHe4, state.pMag, state.dedx);

  Bool_t is_d = PassTpc(kNucDeuteron, state);
  Bool_t is_t = PassTpc(kNucTriton, state);
  Bool_t is_3He = PassTpc(kNucHe3, state);
  Bool_t is_4He = PassTpc(kNucHe4, state);

  if (cuts.m2_selection) {
    if (!state.tofMatch || state.mass2 < -900.0f) {
      is_d = is_t = is_3He = is_4He = kFALSE;
    } else {
      is_d = is_d && PassTof(kNucDeuteron, state);
      is_t = is_t && PassTof(kNucTriton, state);
      is_3He = is_3He && PassTof(kNucHe3, state);
      is_4He = is_4He && PassTof(kNucHe4, state);
    }
  }

  if (!is_d) return kFALSE;

  if (!cuts.requireBestSpecies) return kTRUE;

  Double_t minNSigma = 999.0;
  Int_t bestType = -1;
  if (is_d && TMath::Abs(nSigma_d) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_d);
    bestType = 0;
  }
  if (is_t && TMath::Abs(nSigma_t) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_t);
    bestType = 1;
  }
  if (is_3He && TMath::Abs(nSigma_3He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_3He);
    bestType = 2;
  }
  if (is_4He && TMath::Abs(nSigma_4He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_4He);
    bestType = 3;
  }
  return (bestType == 0);
}

Bool_t StNuclearIdHelper::IsTriton(const NuclearTrackState& state) {
  const NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();
  if (state.dedx <= 0) return kFALSE;
  if (state.pMag < 0.1) return kFALSE;

  const Double_t nSigma_d = GetNSigma(kNucDeuteron, state.pMag, state.dedx);
  const Double_t nSigma_t = GetNSigma(kNucTriton, state.pMag, state.dedx);
  const Double_t nSigma_3He = GetNSigma(kNucHe3, state.pMag, state.dedx);
  const Double_t nSigma_4He = GetNSigma(kNucHe4, state.pMag, state.dedx);

  Bool_t is_d = PassTpc(kNucDeuteron, state);
  Bool_t is_t = PassTpc(kNucTriton, state);
  Bool_t is_3He = PassTpc(kNucHe3, state);
  Bool_t is_4He = PassTpc(kNucHe4, state);

  if (cuts.m2_selection) {
    if (!state.tofMatch || state.mass2 < -900.0f) {
      is_d = is_t = is_3He = is_4He = kFALSE;
    } else {
      is_d = is_d && PassTof(kNucDeuteron, state);
      is_t = is_t && PassTof(kNucTriton, state);
      is_3He = is_3He && PassTof(kNucHe3, state);
      is_4He = is_4He && PassTof(kNucHe4, state);
    }
  }

  if (!is_t) return kFALSE;

  if (!cuts.requireBestSpecies) return kTRUE;

  Double_t minNSigma = 999.0;
  Int_t bestType = -1;
  if (is_d && TMath::Abs(nSigma_d) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_d);
    bestType = 0;
  }
  if (is_t && TMath::Abs(nSigma_t) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_t);
    bestType = 1;
  }
  if (is_3He && TMath::Abs(nSigma_3He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_3He);
    bestType = 2;
  }
  if (is_4He && TMath::Abs(nSigma_4He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_4He);
    bestType = 3;
  }
  return (bestType == 1);
}

Bool_t StNuclearIdHelper::IsHe3(const NuclearTrackState& state) {
  const NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();
  if (state.dedx <= 0) return kFALSE;
  if (state.pMag < 0.1) return kFALSE;

  const Double_t nSigma_d = GetNSigma(kNucDeuteron, state.pMag, state.dedx);
  const Double_t nSigma_t = GetNSigma(kNucTriton, state.pMag, state.dedx);
  const Double_t nSigma_3He = GetNSigma(kNucHe3, state.pMag, state.dedx);
  const Double_t nSigma_4He = GetNSigma(kNucHe4, state.pMag, state.dedx);

  Bool_t is_d = PassTpc(kNucDeuteron, state);
  Bool_t is_t = PassTpc(kNucTriton, state);
  Bool_t is_3He = PassTpc(kNucHe3, state);
  Bool_t is_4He = PassTpc(kNucHe4, state);

  if (cuts.m2_selection) {
    if (!state.tofMatch || state.mass2 < -900.0f) {
      is_d = is_t = is_3He = is_4He = kFALSE;
    } else {
      is_d = is_d && PassTof(kNucDeuteron, state);
      is_t = is_t && PassTof(kNucTriton, state);
      is_3He = is_3He && PassTof(kNucHe3, state);
      is_4He = is_4He && PassTof(kNucHe4, state);
    }
  }

  if (!is_3He) return kFALSE;

  if (!cuts.requireBestSpecies) return kTRUE;

  Double_t minNSigma = 999.0;
  Int_t bestType = -1;
  if (is_d && TMath::Abs(nSigma_d) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_d);
    bestType = 0;
  }
  if (is_t && TMath::Abs(nSigma_t) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_t);
    bestType = 1;
  }
  if (is_3He && TMath::Abs(nSigma_3He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_3He);
    bestType = 2;
  }
  if (is_4He && TMath::Abs(nSigma_4He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_4He);
    bestType = 3;
  }
  return (bestType == 2);
}

Bool_t StNuclearIdHelper::IsHe4(const NuclearTrackState& state) {
  const NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();
  if (state.dedx <= 0) return kFALSE;
  if (state.pMag < 0.1) return kFALSE;

  const Double_t nSigma_d = GetNSigma(kNucDeuteron, state.pMag, state.dedx);
  const Double_t nSigma_t = GetNSigma(kNucTriton, state.pMag, state.dedx);
  const Double_t nSigma_3He = GetNSigma(kNucHe3, state.pMag, state.dedx);
  const Double_t nSigma_4He = GetNSigma(kNucHe4, state.pMag, state.dedx);

  Bool_t is_d = PassTpc(kNucDeuteron, state);
  Bool_t is_t = PassTpc(kNucTriton, state);
  Bool_t is_3He = PassTpc(kNucHe3, state);
  Bool_t is_4He = PassTpc(kNucHe4, state);

  if (cuts.m2_selection) {
    if (!state.tofMatch || state.mass2 < -900.0f) {
      is_d = is_t = is_3He = is_4He = kFALSE;
    } else {
      is_d = is_d && PassTof(kNucDeuteron, state);
      is_t = is_t && PassTof(kNucTriton, state);
      is_3He = is_3He && PassTof(kNucHe3, state);
      is_4He = is_4He && PassTof(kNucHe4, state);
    }
  }

  if (!is_4He) return kFALSE;

  if (!cuts.requireBestSpecies) return kTRUE;

  Double_t minNSigma = 999.0;
  Int_t bestType = -1;
  if (is_d && TMath::Abs(nSigma_d) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_d);
    bestType = 0;
  }
  if (is_t && TMath::Abs(nSigma_t) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_t);
    bestType = 1;
  }
  if (is_3He && TMath::Abs(nSigma_3He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_3He);
    bestType = 2;
  }
  if (is_4He && TMath::Abs(nSigma_4He) < minNSigma) {
    minNSigma = TMath::Abs(nSigma_4He);
    bestType = 3;
  }
  return (bestType == 3);
}

TLorentzVector StNuclearIdHelper::NuclearP4(const TVector3& pTrack, NuclearSpecies sp) {
  const TVector3 pAct = EffectiveMomentum(pTrack, sp);
  const Double_t mass = SpeciesMass(sp);
  const Double_t e = TMath::Sqrt(mass * mass + pAct.Mag2());
  return TLorentzVector(pAct.X(), pAct.Y(), pAct.Z(), e);
}

void StNuclearIdHelper::FillFromPico(NuclearTrackState& state, StPicoTrack* trk, StPicoDst* picoDst) {
  if (!trk) return;
  TVector3 pMom = trk->pMom();
  state.pT = pMom.Perp();
  state.eta = pMom.PseudoRapidity();
  state.phi = pMom.Phi();
  state.pMag = pMom.Mag();
  state.dedx = trk->dEdx();
  state.trackIndex = trk->id();
  state.tofMatch = kFALSE;
  state.mass2 = -999.0f;
  state.beta = -1.0f;

  if (!trk->isTofTrack() || !picoDst) return;
  const Int_t idx = trk->bTofPidTraitsIndex();
  if (idx < 0) return;
  StPicoBTofPidTraits* btof = picoDst->btofPidTraits(idx);
  if (!btof) return;
  const Double_t beta = btof->btofBeta();
  if (beta <= 0) return;
  state.tofMatch = kTRUE;
  state.beta = beta;
  state.mass2 = state.pMag * state.pMag * (1.0 / (beta * beta) - 1.0);
}
