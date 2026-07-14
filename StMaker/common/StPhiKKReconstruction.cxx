#include "StPhiKKReconstruction.h"
#include "ConfigManager.h"
#include "kinematics.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/SystemOfUnits.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"

#include "TMath.h"
#include "TString.h"
#include "TVector3.h"

#include <utility>

namespace {
const Double_t kKaonMass = 0.493677;
}  // namespace

namespace StPhiKKReconstruction {

Double_t KaonMass() { return kKaonMass; }

TVector3 TrackMomentum(const PhiKkTrackState& trk) {
  return TVector3(trk.momentumX, trk.momentumY, trk.momentumZ);
}

StPhysicalHelixD BuildHelix(const PhiKkTrackState& trk) {
  StThreeVectorF gmomSt(trk.momentumX, trk.momentumY, trk.momentumZ);
  StThreeVectorF orgSt(trk.originX, trk.originY, trk.originZ);
  return StPhysicalHelixD(gmomSt, orgSt, trk.BField * units::kilogauss, static_cast<float>(trk.charge));
}

Double_t CalculateDCA(const PhiKkTrackState& trk1, const PhiKkTrackState& trk2, TVector3& dcaPos1,
                      TVector3& dcaPos2) {
  StPhysicalHelixD helix1 = BuildHelix(trk1);
  StPhysicalHelixD helix2 = BuildHelix(trk2);
  std::pair<Double_t, Double_t> pathLengths = helix1.pathLengths(helix2);
  StThreeVectorF pos1 = helix1.at(pathLengths.first);
  StThreeVectorF pos2 = helix2.at(pathLengths.second);
  dcaPos1 = TVector3(pos1.x(), pos1.y(), pos1.z());
  dcaPos2 = TVector3(pos2.x(), pos2.y(), pos2.z());
  return (dcaPos1 - dcaPos2).Mag();
}

Bool_t ReconstructPhi(const PhiKkTrackState& kPlus, const PhiKkTrackState& kMinus, Double_t& invMass,
                      TVector3& phiMom, TVector3& dcaPosPlus, TVector3& dcaPosMinus) {
  PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
  Double_t dca = CalculateDCA(kPlus, kMinus, dcaPosPlus, dcaPosMinus);
  if (dca > phi.maxDCAKK) return kFALSE;

  const TVector3 pPlus = TrackMomentum(kPlus);
  const TVector3 pMinus = TrackMomentum(kMinus);
  Double_t EPlus = TMath::Sqrt(kKaonMass * kKaonMass + pPlus.Mag2());
  Double_t EMinus = TMath::Sqrt(kKaonMass * kKaonMass + pMinus.Mag2());
  phiMom = pPlus + pMinus;
  Double_t E = EPlus + EMinus;
  invMass = TMath::Sqrt(E * E - phiMom.Mag2());
  return kTRUE;
}

Double_t CalculateInvariantMass(const PhiKkTrackState& trk1, const PhiKkTrackState& trk2, Double_t mass1,
                                Double_t mass2) {
  TVector3 p1;
  p1.SetPtEtaPhi(trk1.pT, trk1.eta, trk1.phi);
  TVector3 p2;
  p2.SetPtEtaPhi(trk2.pT, trk2.eta, trk2.phi);
  Double_t E1 = TMath::Sqrt(mass1 * mass1 + p1.Mag2());
  Double_t E2 = TMath::Sqrt(mass2 * mass2 + p2.Mag2());
  TVector3 p = p1 + p2;
  return TMath::Sqrt((E1 + E2) * (E1 + E2) - p.Mag2());
}

Double_t CalculateOpeningAngle(const PhiKkTrackState& trk1, const PhiKkTrackState& trk2) {
  TVector3 p1;
  p1.SetPtEtaPhi(trk1.pT, trk1.eta, trk1.phi);
  TVector3 p2;
  p2.SetPtEtaPhi(trk2.pT, trk2.eta, trk2.phi);
  Double_t mag1 = p1.Mag();
  Double_t mag2 = p2.Mag();
  if (mag1 < 1e-10 || mag2 < 1e-10) return TMath::Pi();
  Double_t cosTheta = p1.Dot(p2) / (mag1 * mag2);
  if (cosTheta > 1.0) cosTheta = 1.0;
  if (cosTheta < -1.0) cosTheta = -1.0;
  return TMath::ACos(cosTheta);
}

Double_t CalculatePairRapidity(Double_t invMass, const TVector3& phiMom) {
  Double_t E = TMath::Sqrt(invMass * invMass + phiMom.Mag2());
  Double_t pz = phiMom.Z();
  if (E <= TMath::Abs(pz)) return 0.0;
  return 0.5 * TMath::Log((E + pz) / (E - pz));
}

Double_t ApplyRapidityFrame(Double_t yLab) {
  return ConfigManager::GetInstance().GetPhiCuts().ApplyAnalysisRapidity(yLab);
}

Bool_t InKaonMass2Window(Float_t mass2) {
  const PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  return (mass2 > pid.minMass2Kaon && mass2 < pid.maxMass2Kaon);
}

Bool_t PassTofKaonPid(const PhiKkTrackState& trk) {
  const PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  if (!pid.requireTOF) return kTRUE;
  TString fallbackMode(pid.tofFallbackMode.c_str());
  fallbackMode.ToLower();
  if (fallbackMode.IsNull()) fallbackMode = "acceptlowpt";

  if (fallbackMode == "acceptlowpt" && trk.pT <= pid.pTofFallbackMax) return kTRUE;
  if (!trk.tofMatch) {
    if (fallbackMode == "tpconly") return kTRUE;
    return kFALSE;
  }

  Bool_t pass = kTRUE;
  if (pid.tofUseMass2Cut) {
    pass = pass && (trk.mass2 >= pid.minMass2Kaon && trk.mass2 <= pid.maxMass2Kaon);
  }
  if (pid.tofUseDeltaInvBetaCut) {
    pass = pass && (TMath::Abs(trk.deltaOneOverBeta) <= pid.maxAbsDeltaOneOverBetaKaon);
  }
  return pass;
}

Bool_t PassKplusTofMass2(Float_t pMag, Bool_t tofMatch, Float_t mass2) {
  const PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  const Float_t pLow = pid.pMomKaonPID;
  if (pMag <= pLow) {
    return !tofMatch || (tofMatch && InKaonMass2Window(mass2));
  }
  return tofMatch && InKaonMass2Window(mass2);
}

Bool_t PassKminusTofMass2(Float_t pMag, Bool_t tofMatch, Float_t mass2) {
  const PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  const Float_t pLow = pid.pMomKaonPID;
  if (pMag <= pLow) {
    return tofMatch && InKaonMass2Window(mass2);
  }
  return InKaonMass2Window(mass2);
}

Bool_t PassPairTofCut(const PhiKkTrackState& kPlus, const PhiKkTrackState& kMinus) {
  const Float_t pKplus = TrackMomentum(kPlus).Mag();
  const Float_t pKminus = TrackMomentum(kMinus).Mag();
  return PassKplusTofMass2(pKplus, kPlus.tofMatch, kPlus.mass2) &&
         PassKminusTofMass2(pKminus, kMinus.tofMatch, kMinus.mass2);
}

void FillTofInfo(Float_t& mass2, Float_t& deltaOneOverBeta, Bool_t& tofMatch, StPicoBTofPidTraits* tof,
                 const TVector3& pMom) {
  mass2 = -999.0;
  deltaOneOverBeta = 999.0;
  tofMatch = kFALSE;
  if (!tof) return;

  Double_t beta = tof->btofBeta();
  if (beta <= 1e-4) return;

  Double_t pMag = pMom.Mag();
  mass2 = pMom.Mag2() * (1.0 / (beta * beta) - 1.0);
  deltaOneOverBeta = DeltaOneOverBeta(beta, kKaonMass, pMag);
  tofMatch = kTRUE;
}

}  // namespace StPhiKKReconstruction
