#include "StK0shortFxtMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "cuts/EventCutConfig.h"
#include "cuts/LambdaCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "CentralityHelper.h"
#include "../StXiFxtMaker/StXiFxtMaker.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/SystemOfUnits.h"

#include "TFile.h"
#include "TH1.h"
#include "TString.h"
#include "TMath.h"
#include "TVector3.h"
#include "TLorentzVector.h"

#include <iostream>
#include <set>
#include <vector>

namespace {
const Double_t kPionMass = 0.139570;
}

StK0shortFxtMaker::StK0shortFxtMaker(const char* name, StPicoDstMaker* picoMaker,
                                     StXiFxtMaker* xiMaker, const char* outName)
    : StMaker(name),
      mPicoDstMaker(picoMaker),
      mPicoDst(0),
      mXiMaker(xiMaker),
      mOutName(outName),
      mEventCounter(0),
      m_histManager(0),
      m_centrality(0),
      m_cent9(-1),
      m_cent16(-1),
      m_refMultCorr(-1.0),
      m_centWeight(1.0),
      m_centralityPercent(-1.0) {}

StK0shortFxtMaker::~StK0shortFxtMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

StK0shortFxtMaker* createStK0shortFxtMaker(const char* name, StPicoDstMaker* picoMaker,
                                           StXiFxtMaker* xiMaker, const char* outName) {
  return new StK0shortFxtMaker(name, picoMaker, xiMaker, outName);
}

extern "C" void* createStK0shortFxtMakerC(const char* name, void* picoMaker, void* xiMaker,
                                          const char* outName) {
  return (void*)createStK0shortFxtMaker(name, (StPicoDstMaker*)picoMaker, (StXiFxtMaker*)xiMaker,
                                        outName);
}

Int_t StK0shortFxtMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StK0shortFxtMaker] GetHistConfigPath() returned empty; no histograms will be filled."
              << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StK0shortFxtMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStOK;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StK0shortFxtMaker] CentralityHelper init failed" << std::endl;
  }
  return kStOK;
}

void StK0shortFxtMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mK0shortMom.clear();
  mK0shortInvMass.clear();
  mK0shortPionPosId.clear();
  mK0shortPionNegId.clear();
}

Bool_t StK0shortFxtMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t nTracks, Int_t refMult,
                                        Float_t vzVpd) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (ev.maxNTr > 0 && nTracks > ev.maxNTr) return kFALSE;
  if (vz < ev.minVz || vz > ev.maxVz) return kFALSE;
  if (vr > ev.maxVr) return kFALSE;
  if (refMult < ev.minRefMult) return kFALSE;
  if (refMult > ev.maxRefMult) return kFALSE;
  if (TMath::Abs(vz - vzVpd) > ev.maxVzDiff && TMath::Abs(vzVpd) < ev.maxAbsVzVpd) return kFALSE;
  return kTRUE;
}

Bool_t StK0shortFxtMaker::PassPionPosCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() <= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (trk->gMom().Pt() < lam.minPtDaughter) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > lam.nSigmaPionPos) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCAPionPos) return kFALSE;
  return kTRUE;
}

Bool_t StK0shortFxtMaker::PassPionNegCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() >= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (trk->gMom().Pt() < lam.minPtDaughter) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > lam.nSigmaPionNeg) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCAPionNeg) return kFALSE;
  return kTRUE;
}

Bool_t StK0shortFxtMaker::TrackUsedByXi(Int_t trackIndex) const {
  if (!mXiMaker || trackIndex < 0) return kFALSE;
  const std::vector<Int_t>& pIds = mXiMaker->GetXiProtonIdList();
  const std::vector<Int_t>& lamPiIds = mXiMaker->GetXiLambdaPionIdList();
  const std::vector<Int_t>& bachIds = mXiMaker->GetXiBachelorPionIdList();
  for (size_t i = 0; i < pIds.size(); ++i) {
    if (pIds[i] == trackIndex) return kTRUE;
  }
  for (size_t i = 0; i < lamPiIds.size(); ++i) {
    if (lamPiIds[i] == trackIndex) return kTRUE;
  }
  for (size_t i = 0; i < bachIds.size(); ++i) {
    if (bachIds[i] == trackIndex) return kTRUE;
  }
  return kFALSE;
}

StPhysicalHelixD StK0shortFxtMaker::MakeHelix(StPicoTrack* trk, Double_t bField) {
  StThreeVectorF p(trk->gMom().X(), trk->gMom().Y(), trk->gMom().Z());
  StThreeVectorF o(trk->origin().X(), trk->origin().Y(), trk->origin().Z());
  return StPhysicalHelixD(p, o, bField * units::kilogauss, (Float_t)trk->charge());
}

Bool_t StK0shortFxtMaker::MakeK0shortHelix(StPicoTrack* pip, StPicoTrack* pim, Double_t bField,
                                           TVector3& v0, TVector3& momPip, TVector3& momPim,
                                           Double_t& dca12) {
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  StPhysicalHelixD hpip = MakeHelix(pip, bField);
  StPhysicalHelixD hpim = MakeHelix(pim, bField);

  std::pair<Double_t, Double_t> s = hpip.pathLengths(hpim);
  if (TMath::Abs(s.first) > lam.maxPathLength || TMath::Abs(s.second) > lam.maxPathLength)
    return kFALSE;

  StThreeVectorD dcaA = hpip.at(s.first);
  StThreeVectorD dcaB = hpim.at(s.second);
  StThreeVectorD v0_((dcaA.x() + dcaB.x()) * 0.5, (dcaA.y() + dcaB.y()) * 0.5,
                     (dcaA.z() + dcaB.z()) * 0.5);
  dca12 = (dcaA - dcaB).mag();
  const Double_t maxDca =
      (lam.maxDcaLambdaDaughters > 0) ? lam.maxDcaLambdaDaughters : lam.maxDaughterDCA;
  if (dca12 < 0 || dca12 > maxDca) return kFALSE;

  StThreeVectorD pp = hpip.momentumAt(s.first, bField * units::kilogauss);
  StThreeVectorD ppi = hpim.momentumAt(s.second, bField * units::kilogauss);
  momPip.SetXYZ(pp.x(), pp.y(), pp.z());
  momPim.SetXYZ(ppi.x(), ppi.y(), ppi.z());
  v0.SetXYZ(v0_.x(), v0_.y(), v0_.z());
  return kTRUE;
}

void StK0shortFxtMaker::FillK0shortCentralityQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr,
                                                Int_t nTracks, Int_t nBTOFMatch, Int_t nPionPosCand,
                                                Int_t nPionNegCand, Int_t nK0shortPairs) {
  if (!m_histManager || cent9 < 0) return;
  const Double_t centX = (Double_t)cent9;
  m_histManager->Fill("hRawMult_vs_Cent9", centX, (Double_t)rawMult);
  if (refMultCorr >= 0.0) {
    m_histManager->Fill("hRefMultCorr_vs_Cent9", centX, refMultCorr);
    m_histManager->Fill("hRawMult_vs_RefMultCorr", refMultCorr, (Double_t)rawMult);
  }
  m_histManager->Fill("hNTracks_vs_Cent9", centX, (Double_t)nTracks);
  m_histManager->Fill("hTofMatchMult_vs_Cent9", centX, (Double_t)nBTOFMatch);
  m_histManager->Fill("hNPionPosCand_vs_Cent9", centX, (Double_t)nPionPosCand);
  m_histManager->Fill("hNPionNegCand_vs_Cent9", centX, (Double_t)nPionNegCand);
  m_histManager->Fill("hNK0shortPairs_vs_Cent9", centX, (Double_t)nK0shortPairs);
}

void StK0shortFxtMaker::FillK0shortInvMassCentrality(Double_t invMass) {
  if (!m_histManager || m_cent9 < 0 || m_cent9 > 8) return;
  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  if (!centCfg.fillCentralityQA) return;
  m_histManager->Fill("hK0short_InvMass_vs_Cent9", (Double_t)m_cent9, invMass);
  if (m_refMultCorr >= 0.0) {
    m_histManager->Fill("hK0short_InvMass_vs_RefMultCorr", m_refMultCorr, invMass);
  }
}

Int_t StK0shortFxtMaker::Make() {
  if (!mPicoDstMaker) return kStWarn;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;

  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  mEventCounter++;

  TVector3 pVtx = event->primaryVertex();
  Int_t refMult = event->refMult();
  const Int_t runId = event->runId();
  const Int_t nBTOFMatch = event->nBTOFMatch();
  const Double_t vz = pVtx.Z();
  const Float_t vr = ConfigManager::GetInstance().GetEventCuts().ComputeVr(pVtx.X(), pVtx.Y());
  const Float_t vzVpd = event->vzVpd();

  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  Int_t rawMult = refMult;
  if (centCfg.enabled) {
    TString mode(centCfg.mode.c_str());
    mode.ToLower();
    if (mode == "fxtmult") rawMult = event->fxtMult();
  }

  m_cent9 = -1;
  m_cent16 = -1;
  m_refMultCorr = -1.0;
  m_centWeight = 1.0;
  m_centralityPercent = -1.0;

  if (m_histManager) {
    m_histManager->Fill("hRefMultVsNTOFMatch", (Double_t)nBTOFMatch, (Double_t)rawMult);
  }

  CentralityRejectReason centReason = kCentralityOk;
  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(runId, centReason)) return kStOK;
  }

  Int_t nTr = mPicoDst->numberOfTracks();
  if (!PassEventCuts(pVtx.Z(), vr, nTr, refMult, vzVpd)) return kStOK;

  if (m_histManager) {
    m_histManager->Fill("hVz", pVtx.Z());
    m_histManager->Fill("hRefMult", refMult);
  }

  if (m_histManager && centCfg.fillCentralityQA) {
    m_histManager->Fill("hRawMult", (Double_t)rawMult);
  }

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) return kStOK;
    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight,
                                   centReason)) {
      return kStOK;
    }
    if (m_histManager) {
      m_histManager->Fill("hCentralityRaw", (Double_t)m_cent9);
      m_histManager->Fill("hRefMultCorr", m_refMultCorr);
      m_histManager->Fill("hCentralityVsVz", vz, (Double_t)m_cent9);
      m_histManager->Fill("hRefMultWeight", m_centWeight);
      m_histManager->Fill("hRefMultVsNTOFMatchAfter", (Double_t)nBTOFMatch, (Double_t)rawMult);
    }
    if (!m_centrality->AcceptCentBin(m_cent9, m_refMultCorr, centReason)) return kStOK;
    m_centralityPercent = CentralityHelper::Cent9ToPercentile(m_cent9);
    if (m_histManager) {
      const Double_t w = centCfg.useWeight ? m_centWeight : 1.0;
      TH1* hCent = m_histManager->Get("hCentrality");
      if (hCent) hCent->Fill((Double_t)m_cent9, w);
      TH1* hCent16 = m_histManager->Get("hCentrality16");
      if (hCent16) hCent16->Fill((Double_t)m_cent16, w);
    }
  }

  // Same-event tracks already used by accepted Xi candidates (pico array index).
  std::set<Int_t> xiUsed;
  if (mXiMaker) {
    const std::vector<Int_t>& pIds = mXiMaker->GetXiProtonIdList();
    const std::vector<Int_t>& lamPiIds = mXiMaker->GetXiLambdaPionIdList();
    const std::vector<Int_t>& bachIds = mXiMaker->GetXiBachelorPionIdList();
    for (size_t i = 0; i < pIds.size(); ++i) xiUsed.insert(pIds[i]);
    for (size_t i = 0; i < lamPiIds.size(); ++i) xiUsed.insert(lamPiIds[i]);
    for (size_t i = 0; i < bachIds.size(); ++i) xiUsed.insert(bachIds[i]);
  }

  Double_t bField = event->bField();
  Int_t nK0shortPairs = 0;
  Int_t nSkippedShared = 0;

  // One pass: build π⁺ / π⁻ candidate index lists (same pattern as StXiFxtMaker).
  std::vector<Int_t> pionPosIdx;
  std::vector<Int_t> pionNegIdx;
  pionPosIdx.reserve(nTr / 4);
  pionNegIdx.reserve(nTr / 4);
  for (Int_t i = 0; i < nTr; i++) {
    StPicoTrack* trk = mPicoDst->track(i);
    if (!trk) continue;
    if (PassPionPosCuts(trk, pVtx)) {
      if (!xiUsed.empty() && xiUsed.count(i)) {
        nSkippedShared++;
      } else {
        pionPosIdx.push_back(i);
      }
    }
    if (PassPionNegCuts(trk, pVtx)) {
      if (!xiUsed.empty() && xiUsed.count(i)) {
        nSkippedShared++;
      } else {
        pionNegIdx.push_back(i);
      }
    }
  }
  const Int_t nPionPosCand = (Int_t)pionPosIdx.size();
  const Int_t nPionNegCand = (Int_t)pionNegIdx.size();

  for (size_t ipos = 0; ipos < pionPosIdx.size(); ipos++) {
    const Int_t ip = pionPosIdx[ipos];
    StPicoTrack* p = mPicoDst->track(ip);
    if (!p) continue;

    for (size_t ineg = 0; ineg < pionNegIdx.size(); ineg++) {
      const Int_t ii = pionNegIdx[ineg];
      if (ip == ii) continue;
      StPicoTrack* pi = mPicoDst->track(ii);
      if (!pi) continue;

      TVector3 v0, momPip, momPim;
      Double_t dca12 = 0;
      if (!MakeK0shortHelix(p, pi, bField, v0, momPip, momPim, dca12)) continue;

      nK0shortPairs++;

      TVector3 pK0 = momPip + momPim;
      Double_t pK0Mag = pK0.Mag();
      if (pK0Mag < 1e-5) continue;

      TVector3 pK0Unit = pK0 * (1.0 / pK0Mag);
      TVector3 diff = pVtx - v0;
      Double_t dcaV0 = (diff.Cross(pK0Unit)).Mag();

      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      if (dcaV0 > lam.maxDCAV0) continue;

      TVector3 flight = v0 - pVtx;
      Double_t cosPoint = flight.Dot(pK0) / (flight.Mag() * pK0.Mag() + 1e-10);
      if (cosPoint < lam.minCosPointing) continue;

      TLorentzVector lpip, lpim;
      lpip.SetVectM(momPip, kPionMass);
      lpim.SetVectM(momPim, kPionMass);
      TLorentzVector lK0short = lpip + lpim;
      Double_t invMass = lK0short.M();
      Double_t rapidity = lK0short.Rapidity();
      Double_t decayLength = flight.Mag();

      // Store pico array indices (aligned with StXiFxtMaker daughter IDs).
      mK0shortMom.push_back(pK0);
      mK0shortInvMass.push_back(invMass);
      mK0shortPionPosId.push_back(ip);
      mK0shortPionNegId.push_back(ii);

      if (m_histManager) {
        m_histManager->Fill("hK0short_InvMass", invMass);
        m_histManager->Fill("hK0short_Pt", pK0.Pt());
        m_histManager->Fill("hK0short_Eta", pK0.PseudoRapidity());
        m_histManager->Fill("hK0short_Phi", pK0.Phi());
        m_histManager->Fill("hDCA12", dca12);
        m_histManager->Fill("hDCAV0", dcaV0);
        m_histManager->Fill("hCosPointing", cosPoint);
        m_histManager->Fill("hNSigmaPionPos", p->nSigmaPion());
        m_histManager->Fill("hNSigmaPionNeg", pi->nSigmaPion());
        m_histManager->Fill("hK0short_InvMass_vs_Pt", pK0.Pt(), invMass);
        m_histManager->Fill("hK0short_InvMass_vs_DecayLength", decayLength, invMass);
        m_histManager->Fill("hK0short_InvMass_vs_Y", rapidity, invMass);
        m_histManager->Fill("hDCAV0_vs_InvMass", invMass, dcaV0);
        m_histManager->Fill("hCosPointing_vs_InvMass", invMass, cosPoint);
        FillK0shortInvMassCentrality(invMass);
      }
    }
  }

  if (m_histManager && nSkippedShared > 0) {
    m_histManager->Fill("hK0_SkippedSharedWithXi", (Double_t)nSkippedShared);
  }

  if (m_histManager && m_cent9 >= 0) {
    FillK0shortCentralityQA(m_cent9, rawMult, m_refMultCorr, nTr, nBTOFMatch, nPionPosCand,
                            nPionNegCand, nK0shortPairs);
  }

  if (m_histManager) m_histManager->Fill("hN", 0);
  return kStOK;
}

Int_t StK0shortFxtMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StK0shortFxtMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    std::cout << "[StK0shortFxtMaker] centrality summary: ok=" << m_centrality->CountOk()
              << " badRun=" << m_centrality->CountBadRun()
              << " pileup=" << m_centrality->CountPileup()
              << " invalidCent=" << m_centrality->CountInvalidCent()
              << " binRejected=" << m_centrality->CountBinRejected() << std::endl;
    m_centrality->Finish();
  }
  return kStOK;
}

void StK0shortFxtMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}
