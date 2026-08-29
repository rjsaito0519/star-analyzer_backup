#include "StXiFxtMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"
#include "TString.h"
#include "TLorentzVector.h"
#include "TMath.h"
#include "TFile.h"

#include "ConfigManager.h"
#include "HistManager.h"
#include "CentralityHelper.h"
#include "cuts/LambdaCutConfig.h"
#include "cuts/EventCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/StThreeVectorD.hh"
#include "StarClassLibrary/SystemOfUnits.h"

#include <iostream>

namespace {
const Double_t kProtonMass = 0.938272;
const Double_t kPionMass = 0.139570;
const Double_t kLambdaMass = 1.115683;
const Double_t kXiMass = 1.32171;

Double_t MaxLambdaDaughterDca(const LambdaCutConfig& lam) {
  if (lam.maxDcaLambdaDaughters > 0.0) return lam.maxDcaLambdaDaughters;
  return lam.maxDaughterDCA;
}

Double_t MaxLambdaBachelorDca(const LambdaCutConfig& lam) {
  if (lam.maxDcaLambdaBachelor > 0.0) return lam.maxDcaLambdaBachelor;
  return lam.maxDaughterDCA;
}

Double_t MinHelixLineDistance(const StPhysicalHelixD& helix, const TVector3& v2, const TVector3& pLam,
                              Double_t& sHelix, Double_t& sLine) {
  TVector3 dir = pLam.Unit();
  StThreeVectorD dirSt(dir.X(), dir.Y(), dir.Z());
  StThreeVectorD v2St(v2.X(), v2.Y(), v2.Z());

  Double_t min_dist2 = 1e9;
  Double_t best_s = 0;
  for (double s = -50.0; s <= 50.0; s += 2.0) {
    StThreeVectorD pos = helix.at(s);
    StThreeVectorD diff = pos - v2St;
    Double_t proj = diff.dot(dirSt);
    StThreeVectorD perp = diff - proj * dirSt;
    Double_t dist2 = perp.mag2();
    if (dist2 < min_dist2) {
      min_dist2 = dist2;
      best_s = s;
    }
  }

  Double_t s_low = best_s - 2.0;
  Double_t s_high = best_s + 2.0;
  for (int iter = 0; iter < 10; iter++) {
    Double_t s1 = s_low + (s_high - s_low) / 3.0;
    Double_t s2 = s_high - (s_high - s_low) / 3.0;

    StThreeVectorD pos1 = helix.at(s1);
    StThreeVectorD diff1 = pos1 - v2St;
    Double_t dist2_1 = (diff1 - diff1.dot(dirSt) * dirSt).mag2();

    StThreeVectorD pos2 = helix.at(s2);
    StThreeVectorD diff2 = pos2 - v2St;
    Double_t dist2_2 = (diff2 - diff2.dot(dirSt) * dirSt).mag2();

    if (dist2_1 < dist2_2) {
      s_high = s2;
    } else {
      s_low = s1;
    }
  }

  sHelix = 0.5 * (s_low + s_high);
  StThreeVectorD pos = helix.at(sHelix);
  StThreeVectorD diff = pos - v2St;
  sLine = diff.dot(dirSt);
  return (diff - sLine * dirSt).mag();
}
}

StXiFxtMaker* createStXiFxtMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StXiFxtMaker(name, picoMaker, outName);
}

extern "C" void* createStXiFxtMakerC(const char* name, void* picoMaker, const char* outName) {
  return (void*)new StXiFxtMaker(name, (StPicoDstMaker*)picoMaker, outName);
}

StXiFxtMaker::StXiFxtMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName)
    : StMaker(name),
      mPicoDstMaker(picoMaker),
      mPicoDst(0),
      mOutName(outName),
      mEventCounter(0),
      m_histManager(0),
      m_centrality(0),
      m_cent9(-1),
      m_cent16(-1),
      m_refMultCorr(-1.0),
      m_centWeight(1.0),
      m_centralityPercent(-1.0) {}

StXiFxtMaker::~StXiFxtMaker() {
  if (m_histManager) delete m_histManager;
  if (m_centrality) delete m_centrality;
}

Int_t StXiFxtMaker::Init() {
  m_histManager = new HistManager();
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath();
  if (histPath.empty()) {
    std::cerr << "[StXiFxtMaker] GetHistConfigPath() returned empty; no histograms will be filled." << std::endl;
  } else if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StXiFxtMaker] Failed to load hist config from: " << histPath << std::endl;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StXiFxtMaker] CentralityHelper init failed" << std::endl;
  }
  return kStOK;
}

void StXiFxtMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mXiMom.clear();
  mXiInvMass.clear();
  mXiProtonId.clear();
  mXiLambdaPionId.clear();
  mXiBachelorPionId.clear();
}

Bool_t StXiFxtMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t nTracks, Int_t refMult, Float_t vzVpd) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (ev.maxNTr > 0 && nTracks > ev.maxNTr) return kFALSE;
  if (vz < ev.minVz || vz > ev.maxVz) return kFALSE;
  if (vr > ev.maxVr) return kFALSE;
  if (refMult < ev.minRefMult) return kFALSE;
  if (refMult > ev.maxRefMult) return kFALSE;
  if (TMath::Abs(vz - vzVpd) > ev.maxVzDiff && TMath::Abs(vzVpd) < ev.maxAbsVzVpd) return kFALSE;
  return kTRUE;
}

Bool_t StXiFxtMaker::PassProtonCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() <= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (lam.minNHitsRatio > 0.0 &&
      (trk->nHitsMax() <= 0 ||
       (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio)) {
    return kFALSE;
  }
  if (trk->gMom().Pt() < lam.minPtDaughter) return kFALSE;
  if (TMath::Abs(trk->nSigmaProton()) > lam.nSigmaProton) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCAProton) return kFALSE;
  return kTRUE;
}

Bool_t StXiFxtMaker::PassLambdaPionCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() >= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (lam.minNHitsRatio > 0.0 &&
      (trk->nHitsMax() <= 0 ||
       (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio)) {
    return kFALSE;
  }
  if (trk->gMom().Pt() < lam.minPtDaughter) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > lam.nSigmaPion) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCAPion) return kFALSE;
  return kTRUE;
}

Bool_t StXiFxtMaker::PassBachelorPionCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() >= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (lam.minNHitsRatio > 0.0 &&
      (trk->nHitsMax() <= 0 ||
       (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio)) {
    return kFALSE;
  }
  if (trk->gMom().Pt() < lam.minPtDaughter) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > lam.nSigmaPion) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCABachelor) return kFALSE;
  return kTRUE;
}

StPhysicalHelixD StXiFxtMaker::MakeHelix(StPicoTrack* trk, Double_t bField) {
  StThreeVectorF p(trk->gMom().X(), trk->gMom().Y(), trk->gMom().Z());
  StThreeVectorF o(trk->origin().X(), trk->origin().Y(), trk->origin().Z());
  return StPhysicalHelixD(p, o, bField * units::kilogauss, (Float_t)trk->charge());
}

Bool_t StXiFxtMaker::MakeLambdaHelix(StPicoTrack* p, StPicoTrack* pi, Double_t bField, TVector3& v2,
                                    TVector3& momP, TVector3& momPi, Double_t& dca12) {
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  StPhysicalHelixD hp = MakeHelix(p, bField);
  StPhysicalHelixD hpi = MakeHelix(pi, bField);

  std::pair<Double_t, Double_t> s = hp.pathLengths(hpi);
  if (TMath::Abs(s.first) > lam.maxPathLength || TMath::Abs(s.second) > lam.maxPathLength) return kFALSE;

  StThreeVectorD dcaA = hp.at(s.first);
  StThreeVectorD dcaB = hpi.at(s.second);
  StThreeVectorD v2_((dcaA.x() + dcaB.x()) * 0.5, (dcaA.y() + dcaB.y()) * 0.5, (dcaA.z() + dcaB.z()) * 0.5);
  dca12 = (dcaA - dcaB).mag();

  if (dca12 < 0 || dca12 > MaxLambdaDaughterDca(lam)) return kFALSE;

  StThreeVectorD pp = hp.momentumAt(s.first, bField * units::kilogauss);
  StThreeVectorD ppi = hpi.momentumAt(s.second, bField * units::kilogauss);

  momP.SetXYZ(pp.x(), pp.y(), pp.z());
  momPi.SetXYZ(ppi.x(), ppi.y(), ppi.z());
  v2.SetXYZ(v2_.x(), v2_.y(), v2_.z());
  return kTRUE;
}

Bool_t StXiFxtMaker::MakeXiHelix(const TVector3& v2, const TVector3& momLam, StPicoTrack* pi_bach, Double_t bField,
                                TVector3& v1, TVector3& momXi, Double_t& dcaCascade, Double_t& pathLengthLam) {
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  StPhysicalHelixD hBach = MakeHelix(pi_bach, bField);

  Double_t sHelix = 0;
  dcaCascade = MinHelixLineDistance(hBach, v2, momLam, sHelix, pathLengthLam);

  if (TMath::Abs(sHelix) > lam.maxPathLength || TMath::Abs(pathLengthLam) > lam.maxPathLength) return kFALSE;
  if (pathLengthLam >= 0) return kFALSE;
  if (dcaCascade < 0 || dcaCascade > MaxLambdaBachelorDca(lam)) return kFALSE;

  StThreeVectorD dcaA = hBach.at(sHelix);
  StThreeVectorD pLam(momLam.X(), momLam.Y(), momLam.Z());
  StThreeVectorD oLam(v2.X(), v2.Y(), v2.Z());
  StPhysicalHelixD hLambda(pLam, oLam, 0.0, 0.0);
  StThreeVectorD dcaB = hLambda.at(pathLengthLam);
  StThreeVectorD v1_((dcaA.x() + dcaB.x()) * 0.5, (dcaA.y() + dcaB.y()) * 0.5, (dcaA.z() + dcaB.z()) * 0.5);

  StThreeVectorD pBach = hBach.momentumAt(sHelix, bField * units::kilogauss);
  momXi.SetXYZ(momLam.X() + pBach.x(), momLam.Y() + pBach.y(), momLam.Z() + pBach.z());
  v1.SetXYZ(v1_.x(), v1_.y(), v1_.z());
  return kTRUE;
}

Int_t StXiFxtMaker::Make() {
  if (!mPicoDstMaker) return kStWarn;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;

  StPicoEvent* picoEvent = mPicoDst->event();
  if (!picoEvent) return kStWarn;

  mEventCounter++;

  TVector3 pVtx = picoEvent->primaryVertex();
  Int_t nTracks = mPicoDst->numberOfTracks();
  const Int_t refMult = picoEvent->refMult();
  const Float_t vzVpd = picoEvent->vzVpd();
  const Float_t vr = ConfigManager::GetInstance().GetEventCuts().ComputeVr(pVtx.X(), pVtx.Y());

  if (!PassEventCuts(pVtx.Z(), vr, nTracks, refMult, vzVpd)) return kStOK;

  m_cent9 = -1;
  m_cent16 = -1;
  m_refMultCorr = -1.0;
  m_centWeight = 1.0;
  m_centralityPercent = -1.0;

  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  Int_t rawMult = nTracks;
  if (centCfg.enabled) {
    TString mode(centCfg.mode.c_str());
    mode.ToLower();
    if (mode == "fxtmult") {
      rawMult = picoEvent->fxtMult();
    } else {
      rawMult = picoEvent->refMult();
    }
  }

  CentralityRejectReason centReason = kCentralityOk;
  if (m_centrality && m_centrality->IsEnabled()) {
    const Int_t runId = picoEvent->runId();
    const Int_t nBTOFMatch = picoEvent->nBTOFMatch();
    const Double_t vz = pVtx.Z();

    if (!m_centrality->CheckBadRun(runId, centReason)) return kStOK;
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) return kStOK;
    if (!m_centrality->ComputeBins(picoEvent, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight, centReason)) {
      return kStOK;
    }
    if (!m_centrality->AcceptCentBin(m_cent9, m_refMultCorr, centReason)) return kStOK;
    m_centralityPercent = CentralityHelper::Cent9ToPercentile(m_cent9);
  }

  std::vector<Int_t> protonIndices;
  std::vector<Int_t> lambdaPionIndices;
  std::vector<Int_t> bachelorPionIndices;

  for (Int_t i = 0; i < nTracks; i++) {
    StPicoTrack* trk = mPicoDst->track(i);
    if (!trk) continue;
    if (PassProtonCuts(trk, pVtx)) protonIndices.push_back(i);
    if (PassLambdaPionCuts(trk, pVtx)) lambdaPionIndices.push_back(i);
    if (PassBachelorPionCuts(trk, pVtx)) bachelorPionIndices.push_back(i);
  }

  Int_t nXiPairs = 0;
  Double_t bF = picoEvent->bField();
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();

  for (size_t ip = 0; ip < protonIndices.size(); ip++) {
    StPicoTrack* trkP = mPicoDst->track(protonIndices[ip]);

    for (size_t ipi = 0; ipi < lambdaPionIndices.size(); ipi++) {
      Int_t idxPi = lambdaPionIndices[ipi];
      if (protonIndices[ip] == idxPi) continue;
      StPicoTrack* trkPi = mPicoDst->track(idxPi);

      TVector3 v2, momP, momPi;
      Double_t dca12_lam = 0;
      if (!MakeLambdaHelix(trkP, trkPi, bF, v2, momP, momPi, dca12_lam)) continue;

      TLorentzVector lp, lpi;
      lp.SetVectM(momP, kProtonMass);
      lpi.SetVectM(momPi, kPionMass);
      TLorentzVector lLam = lp + lpi;
      Double_t m_lam = lLam.M();

      if (m_histManager) {
        m_histManager->Fill("hLambda_InvMass", m_lam);
      }

      if (TMath::Abs(m_lam - kLambdaMass) > lam.lambdaMassWindow) continue;

      TVector3 pLam = momP + momPi;

      for (size_t ibach = 0; ibach < bachelorPionIndices.size(); ibach++) {
        Int_t idxBach = bachelorPionIndices[ibach];
        if (idxBach == protonIndices[ip] || idxBach == idxPi) continue;
        StPicoTrack* trkBach = mPicoDst->track(idxBach);

        TVector3 v1, momXi;
        Double_t dcaCascade = 0;
        Double_t pathLengthLam = 0;
        if (!MakeXiHelix(v2, pLam, trkBach, bF, v1, momXi, dcaCascade, pathLengthLam)) continue;

        nXiPairs++;

        StThreeVectorD pXi(momXi.X(), momXi.Y(), momXi.Z());
        StThreeVectorD oXi(v1.X(), v1.Y(), v1.Z());
        StPhysicalHelixD hXi(pXi, oXi, bF * units::kilogauss, -1.0);
        StThreeVectorD pVtxThree(pVtx.X(), pVtx.Y(), pVtx.Z());
        Double_t sXi = hXi.pathLength(pVtxThree);
        Double_t dcaXiToPV = (hXi.at(sXi) - pVtxThree).mag();

        TVector3 flightXi = v1 - pVtx;
        Double_t decayLength = flightXi.Mag();
        Double_t cosPointXi = flightXi.Dot(momXi) / (decayLength * momXi.Mag() + 1e-10);

        TVector3 pBach = momXi - pLam;
        TLorentzVector lBach;
        lBach.SetVectM(pBach, kPionMass);
        TLorentzVector lLamFixed;
        lLamFixed.SetVectM(pLam, kLambdaMass);
        TLorentzVector lXi = lLamFixed + lBach;
        Double_t invMass = lXi.M();
        Double_t rapidity = lXi.Rapidity();

        // Pre-topology: fill before DCA / cos / L / L-order / fake-Lambda veto
        if (m_histManager) {
          m_histManager->Fill("hXi_InvMass_preTopo", invMass);
          m_histManager->Fill("hXi_InvMass_vs_DCAV0_preTopo", dcaXiToPV, invMass);
          m_histManager->Fill("hXi_InvMass_vs_CosPointing_preTopo", cosPointXi, invMass);
          m_histManager->Fill("hXi_InvMass_vs_DecayLength_preTopo", decayLength, invMass);
        }

        if (dcaXiToPV > lam.maxDCAV0) continue;
        if (decayLength < lam.minDecayLengthXi) continue;
        if (cosPointXi < lam.minCosPointing) continue;

        if (pathLengthLam >= 0.0) continue;

        const Double_t decayLengthLam = (v2 - v1).Mag();
        if (m_histManager) {
          m_histManager->Fill("hDecayLengthXi_vs_Lam", decayLength, decayLengthLam);
        }
        if (lam.requireDecayLengthOrder && decayLength >= decayLengthLam) continue;

        TLorentzVector lFakeP;
        TLorentzVector lFakePi;
        lFakeP.SetVectM(pBach, kProtonMass);
        lFakePi.SetVectM(momPi, kPionMass);
        const Double_t mFake = (lFakeP + lFakePi).M();
        if (m_histManager) {
          m_histManager->Fill("hFakeLambda_InvMass", mFake);
        }
        if (lam.fakeLambdaWindow > 0.0 &&
            TMath::Abs(mFake - lam.fakeLambdaMean) < lam.fakeLambdaWindow) {
          continue;
        }

        if (m_histManager) {
          m_histManager->Fill("hXi_InvMass", invMass);
          m_histManager->Fill("hXi_Pt", lXi.Pt());
          m_histManager->Fill("hXi_Eta", lXi.Eta());
          m_histManager->Fill("hXi_Phi", lXi.Phi());
          m_histManager->Fill("hDCA12_Lambda", dca12_lam);
          m_histManager->Fill("hDCA_Cascade", dcaCascade);
          m_histManager->Fill("hDCAV0_Xi", dcaXiToPV);
          m_histManager->Fill("hCosPointing_Xi", cosPointXi);
          m_histManager->Fill("hNSigmaProton", trkP->nSigmaProton());
          m_histManager->Fill("hNSigmaPionLambda", trkPi->nSigmaPion());
          m_histManager->Fill("hNSigmaPionBachelor", trkBach->nSigmaPion());
          m_histManager->Fill("hXi_InvMass_vs_Pt", lXi.Pt(), invMass);
          m_histManager->Fill("hXi_InvMass_vs_DecayLength", decayLength, invMass);
          m_histManager->Fill("hXi_InvMass_vs_Y", rapidity, invMass);
          if (m_cent9 >= 0) {
            m_histManager->Fill("hXi_InvMass_vs_Cent9", (Double_t)m_cent9, invMass);
            m_histManager->Fill("hXi_InvMass_vs_RefMultCorr", m_refMultCorr, invMass);
            TString kCentBin = TString::Format("hXi_InvMass_CentBin%d", m_cent9);
            m_histManager->Fill(kCentBin.Data(), invMass);
          }
        }

        if (lam.xiMassWindow > 0.0 && TMath::Abs(invMass - kXiMass) > lam.xiMassWindow) continue;

        mXiMom.push_back(momXi);
        mXiInvMass.push_back(invMass);
        mXiProtonId.push_back(protonIndices[ip]);
        mXiLambdaPionId.push_back(idxPi);
        mXiBachelorPionId.push_back(idxBach);
      }
    }
  }

  if (m_histManager) {
    m_histManager->Fill("hVz", pVtx.Z());
    m_histManager->Fill("hVr", vr);
    m_histManager->Fill("hRefMult", nTracks);
    m_histManager->Fill("hRawMult", (Double_t)nTracks);
    if (m_cent9 >= 0) {
      m_histManager->Fill("hCentrality", (Double_t)m_cent9);
      m_histManager->Fill("hCentralityRaw", (Double_t)m_cent9);
      m_histManager->Fill("hCentrality16", (Double_t)m_cent16);
      m_histManager->Fill("hRefMultCorr", m_refMultCorr);
      m_histManager->Fill("hRefMultWeight", m_centWeight);
      m_histManager->Fill("hCentralityVsVz", pVtx.Z(), (Double_t)m_cent9);
    }
    FillXiCentralityQA(m_cent9, nTracks, m_refMultCorr, nTracks, picoEvent->nBTOFMatch(),
                       protonIndices.size(), lambdaPionIndices.size() + bachelorPionIndices.size(), nXiPairs);
  }

  return kStOK;
}

void StXiFxtMaker::FillXiCentralityQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
                                     Int_t nBTOFMatch, Int_t nProtonCand, Int_t nPionCand, Int_t nXiPairs) {
  if (!m_histManager || cent9 < 0) return;

  const Double_t centX = (Double_t)cent9;
  m_histManager->Fill("hRawMult_vs_Cent9", centX, (Double_t)rawMult);
  if (refMultCorr >= 0.0) {
    m_histManager->Fill("hRefMultCorr_vs_Cent9", centX, refMultCorr);
    m_histManager->Fill("hRawMult_vs_RefMultCorr", refMultCorr, (Double_t)rawMult);
  }
  m_histManager->Fill("hNTracks_vs_Cent9", centX, (Double_t)nTracks);
  m_histManager->Fill("hTofMatchMult_vs_Cent9", centX, (Double_t)nBTOFMatch);
  m_histManager->Fill("hNProtonCand_vs_Cent9", centX, (Double_t)nProtonCand);
  m_histManager->Fill("hNPionCand_vs_Cent9", centX, (Double_t)nPionCand);
  m_histManager->Fill("hNXiPairs_vs_Cent9", centX, (Double_t)nXiPairs);
}

Int_t StXiFxtMaker::Finish() {
  if (m_histManager) {
    m_histManager->Fill("hN", 0);
  }
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StXiFxtMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    std::cout << "[StXiFxtMaker] centrality summary: ok=" << m_centrality->CountOk()
              << " badRun=" << m_centrality->CountBadRun() << " pileup=" << m_centrality->CountPileup()
              << " invalidCent=" << m_centrality->CountInvalidCent()
              << " binRejected=" << m_centrality->CountBinRejected() << std::endl;
    m_centrality->Finish();
  }
  return kStOK;
}

void StXiFxtMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}
