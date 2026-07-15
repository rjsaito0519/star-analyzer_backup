#include "StKminusXiFemtoMaker.h"
#include "CentralityHelper.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "StPhiKKReconstruction.h"
#include "../StXiMaker/StXiMaker.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/EventCutConfig.h"
#include "cuts/FemtoConfig.h"
#include "cuts/MixingConfig.h"

#include "TFile.h"
#include "TH2.h"
#include "TMath.h"
#include "TString.h"
#include <iostream>
#include <vector>

namespace {
const Double_t kKaonMinusMass = 0.493677;
const Double_t kXiMass = 1.32171;
const char* kChannel = "km_xim";

// Returns "" (signal), "_leftSB", "_rightSB", or 0 (skip).
const char* XiMassHistSuffix(Double_t mass, const FemtoConfig& cfg) {
  if (mass >= cfg.xiMassMin && mass <= cfg.xiMassMax) return "";
  if (cfg.xiSidebandLeftMax > cfg.xiSidebandLeftMin && mass >= cfg.xiSidebandLeftMin &&
      mass <= cfg.xiSidebandLeftMax)
    return "_leftSB";
  if (cfg.xiSidebandRightMax > cfg.xiSidebandRightMin && mass >= cfg.xiSidebandRightMin &&
      mass <= cfg.xiSidebandRightMax)
    return "_rightSB";
  return 0;
}

void FillKstarPairHists(HistManager* hm, Bool_t sameEvent, const char* suffix, Double_t kstar, Int_t cent9,
                        Double_t w) {
  if (!hm || !suffix) return;
  const char* seMe = sameEvent ? "SE" : "ME";
  TString h1name = TString::Format("hKstar%s_%s%s", seMe, kChannel, suffix);
  hm->Fill(h1name.Data(), kstar, w);
  if (cent9 >= 0) {
    TString h2name = TString::Format("hKstar%sVsCent_%s%s", seMe, kChannel, suffix);
    TH2* h2 = (TH2*)hm->Get(h2name.Data());
    if (h2) h2->Fill(kstar, (Double_t)cent9, w);
  }
}
}

StKminusXiFemtoMaker::StKminusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StXiMaker* xiMaker,
                                         const char* outName)
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

StKminusXiFemtoMaker::~StKminusXiFemtoMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

StKminusXiFemtoMaker* createStKminusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StXiMaker* xiMaker,
                                               const char* outName) {
  return new StKminusXiFemtoMaker(name, picoMaker, xiMaker, outName);
}

extern "C" void* createStKminusXiFemtoMakerC(const char* name, void* picoMaker, void* xiMaker, const char* outName) {
  return (void*)createStKminusXiFemtoMaker(name, (StPicoDstMaker*)picoMaker, (StXiMaker*)xiMaker, outName);
}

Int_t StKminusXiFemtoMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StKminusXiFemtoMaker] GetHistConfigPath() returned empty; no histograms will be filled."
              << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StKminusXiFemtoMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStErr;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StKminusXiFemtoMaker] CentralityHelper init failed" << std::endl;
  }

  return kStOK;
}

void StKminusXiFemtoMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mKaonMinusCandidates.clear();
  mXiCandidates.clear();
}

Bool_t StKminusXiFemtoMaker::PassKaonMinusCuts(StPicoTrack* trk, const TVector3& pVtx, Float_t& mass2,
                                             Bool_t& tofMatch) const {
  mass2 = -999.0f;
  tofMatch = kFALSE;
  if (!trk || trk->charge() >= 0) return kFALSE;

  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk->nHitsFit() < cfg.kaonMinusMinNHitsFit) return kFALSE;
  if (trk->nHitsMax() <= 0) return kFALSE;
  if ((Float_t)trk->nHitsFit() / (Float_t)trk->nHitsMax() < cfg.kaonMinusMinNHitsRatio) return kFALSE;

  TVector3 pMom = trk->gMom();
  const Float_t pt = pMom.Perp();
  const Float_t eta = pMom.PseudoRapidity();
  if (pt < cfg.kaonMinusMinPtPre || pt > cfg.kaonMinusMaxPtPair) return kFALSE;
  if (TMath::Abs(eta) >= cfg.kaonMinusMaxAbsEta) return kFALSE;

  const Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca >= cfg.kaonMinusMaxDca) return kFALSE;
  if (TMath::Abs(trk->nSigmaKaon()) >= cfg.kaonMinusMaxAbsNSigma) return kFALSE;

  Float_t deltaOneOverBeta = -999.0f;
  Int_t btofIndex = trk->bTofPidTraitsIndex();
  StPicoBTofPidTraits* tof = 0;
  if (btofIndex >= 0 && mPicoDst) tof = mPicoDst->btofPidTraits(btofIndex);
  StPhiKKReconstruction::FillTofInfo(mass2, deltaOneOverBeta, tofMatch, tof, pMom);

  const Double_t pmom = pMom.Mag();
  const Bool_t passTofRule =
      (pmom < cfg.kaonMinusTofMomentumThreshold) ||
      (pmom >= cfg.kaonMinusTofMomentumThreshold && tofMatch && mass2 >= cfg.kaonMinusMinMass2 &&
       mass2 <= cfg.kaonMinusMaxMass2);
  if (!passTofRule) return kFALSE;
  return kTRUE;
}

void StKminusXiFemtoMaker::CollectKaonMinusCandidates(const TVector3& pVtx) {
  if (!mPicoDst) return;
  const Int_t nTracks = mPicoDst->numberOfTracks();
  for (Int_t itrk = 0; itrk < nTracks; itrk++) {
    StPicoTrack* trk = mPicoDst->track(itrk);
    if (!trk) continue;

    Float_t mass2 = -999.0f;
    Bool_t tofMatch = kFALSE;
    if (!PassKaonMinusCuts(trk, pVtx, mass2, tofMatch)) continue;

    KaonMinusCandidate cand;
    cand.mom = trk->gMom();
    cand.trackId = itrk;
    cand.mass2 = mass2;
    cand.tofMatch = tofMatch;
    mKaonMinusCandidates.push_back(cand);

    if (m_histManager) {
      m_histManager->Fill("hKm_Pt", cand.mom.Pt());
      m_histManager->Fill("hKm_Eta", cand.mom.Eta());
      m_histManager->Fill("hKm_NSigma", trk->nSigmaKaon());
      if (tofMatch) {
        m_histManager->Fill("hKm_Mass2VsP", cand.mom.Mag(), mass2);
        m_histManager->Fill("hKm_Mass2VsPt", cand.mom.Pt(), mass2);
      }
    }
  }
  if (m_histManager) {
    m_histManager->Fill("hNKm", (Double_t)mKaonMinusCandidates.size());
  }
}

void StKminusXiFemtoMaker::CollectXiCandidates() {
  if (!mXiMaker) return;
  const std::vector<TVector3>& ximoms = mXiMaker->GetXiMomList();
  const std::vector<Double_t>& ximasses = mXiMaker->GetXiInvMassList();
  const std::vector<Int_t>& xiProtonIds = mXiMaker->GetXiProtonIdList();
  const std::vector<Int_t>& xiLambdaPionIds = mXiMaker->GetXiLambdaPionIdList();
  const std::vector<Int_t>& xiBachelorPionIds = mXiMaker->GetXiBachelorPionIdList();

  for (size_t i = 0; i < ximoms.size(); i++) {
    XiCandidate cand;
    cand.mom = ximoms[i];
    cand.invMass = ximasses[i];
    cand.protonId = xiProtonIds[i];
    cand.lambdaPionId = xiLambdaPionIds[i];
    cand.bachelorPionId = xiBachelorPionIds[i];
    mXiCandidates.push_back(cand);

    if (m_histManager) {
      m_histManager->Fill("hXi_InvMass", cand.invMass);
      m_histManager->Fill("hXi_Pt", cand.mom.Pt());
      m_histManager->Fill("hXi_Eta", cand.mom.Eta());
    }
  }
  if (m_histManager) {
    m_histManager->Fill("hNXi", (Double_t)mXiCandidates.size());
  }
}

Int_t StKminusXiFemtoMaker::Make() {
  if (!mPicoDstMaker) return kStWarn;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;

  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  mEventCounter++;
  mKaonMinusCandidates.clear();
  mXiCandidates.clear();

  TVector3 pVtx = event->primaryVertex();
  Float_t vr = ConfigManager::GetInstance().GetEventCuts().ComputeVr(pVtx.X(), pVtx.Y());
  const Double_t vz = pVtx.Z();
  const Int_t runId = event->runId();
  const Int_t nBTOFMatch = event->nBTOFMatch();
  const Int_t refMult = event->refMult();

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

  CentralityRejectReason centReason = kCentralityOk;
  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(runId, centReason)) return kStOK;
  }

  if (!PassEventCuts(pVtx.Z(), vr, refMult)) return kStOK;

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) return kStOK;
    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight, centReason)) {
      return kStOK;
    }
    if (!m_centrality->AcceptCentBin(m_cent9, m_refMultCorr, centReason)) return kStOK;
    m_centralityPercent = CentralityHelper::Cent9ToPercentile(m_cent9);
  }

  if (m_histManager) {
    m_histManager->Fill("hVz", vz);
    m_histManager->Fill("hRefMult", (Double_t)refMult);
    m_histManager->Fill("hN", 0.5);
    if (m_centrality && m_centrality->IsEnabled()) {
      m_histManager->Fill("hCentrality", (Double_t)m_cent9);
      m_histManager->Fill("hRefMultCorr", m_refMultCorr);
    }
  }

  if (!mXiMaker) {
    std::cerr << "[StKminusXiFemtoMaker] Required Xi Maker is missing!" << std::endl;
    return kStWarn;
  }

  CollectKaonMinusCandidates(pVtx);
  CollectXiCandidates();

  FillSameEventPairs();
  FillMixedEventPairs(vz, m_cent9);
  StoreEventForMixing(vz, m_cent9);

  return kStOK;
}

Int_t StKminusXiFemtoMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StKminusXiFemtoMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    m_centrality->Finish();
  }
  return kStOK;
}

void StKminusXiFemtoMaker::WriteHistograms() {
  if (m_histManager) {
    m_histManager->Write();
  }
}

Bool_t StKminusXiFemtoMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t refMult) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (ev.maxNTr > 0 && refMult > ev.maxNTr) return kFALSE;
  if (TMath::Abs(vz) < ev.minVz || TMath::Abs(vz) > ev.maxVz) return kFALSE;
  if (vr > ev.maxVr) return kFALSE;
  return kTRUE;
}

Double_t StKminusXiFemtoMaker::ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const {
  TLorentzVector q = pA - pB;
  TLorentzVector pair = pA + pB;
  q.Boost(-pair.BoostVector());
  return 0.5 * q.Vect().Mag();
}

TLorentzVector StKminusXiFemtoMaker::KaonMinusP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kKaonMinusMass);
  return p4;
}

TLorentzVector StKminusXiFemtoMaker::XiP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kXiMass);
  return p4;
}

Bool_t StKminusXiFemtoMaker::ShareTracks(const KaonMinusCandidate& kp, const XiCandidate& xi) const {
  if (kp.trackId == xi.protonId || kp.trackId == xi.lambdaPionId || kp.trackId == xi.bachelorPionId) return kTRUE;
  return kFALSE;
}

Int_t StKminusXiFemtoMaker::GetMixingBin(Float_t vz, Int_t cent9) const {
  const EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  Int_t vzBin = 0;
  if (mix.nVzBins > 0) {
    Double_t vzSpan = ev.maxVz - ev.minVz;
    if (vzSpan > 0) {
      vzBin = (Int_t)((vz - ev.minVz) / vzSpan * mix.nVzBins);
      if (vzBin < 0) vzBin = 0;
      if (vzBin >= mix.nVzBins) vzBin = mix.nVzBins - 1;
    }
  }
  if (cent9 < 0) return -1;
  return vzBin * 9 + cent9;
}

void StKminusXiFemtoMaker::FillSameEventPairs() {
  if (mKaonMinusCandidates.empty() || mXiCandidates.empty()) return;
  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t w =
      (m_centrality && m_centrality->IsEnabled() && ConfigManager::GetInstance().GetCentralityCuts().useWeight)
          ? m_centWeight
          : 1.0;

  for (size_t ik = 0; ik < mKaonMinusCandidates.size(); ik++) {
    const KaonMinusCandidate& kp = mKaonMinusCandidates[ik];
    for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
      const XiCandidate& xi = mXiCandidates[ixi];
      const char* suf = XiMassHistSuffix(xi.invMass, cfg);
      if (!suf) continue;
      if (ShareTracks(kp, xi)) continue;

      TLorentzVector p4k = KaonMinusP4(kp.mom);
      TLorentzVector p4xi = XiP4(xi.mom);
      Double_t kstar = ComputeKStar(p4k, p4xi);
      FillKstarPairHists(m_histManager, kTRUE, suf, kstar, m_cent9, w);
    }
  }
}

void StKminusXiFemtoMaker::FillMixedEventPairs(Float_t vz, Int_t cent9) {
  if (cent9 < 0) return;
  Int_t mixBin = GetMixingBin(vz, cent9);
  std::map<Int_t, std::deque<FemtoMixingEvent> >::const_iterator poolIt = m_mixingPool.find(mixBin);
  if (poolIt == m_mixingPool.end() || poolIt->second.empty()) return;

  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t w =
      (m_centrality && m_centrality->IsEnabled() && ConfigManager::GetInstance().GetCentralityCuts().useWeight)
          ? m_centWeight
          : 1.0;
  const std::deque<FemtoMixingEvent>& pool = poolIt->second;

  if (!mKaonMinusCandidates.empty()) {
    for (size_t ie = 0; ie < pool.size(); ie++) {
      const std::vector<XiCandidate>& poolXis = pool[ie].xis;
      for (size_t ik = 0; ik < mKaonMinusCandidates.size(); ik++) {
        const KaonMinusCandidate& kp = mKaonMinusCandidates[ik];
        for (size_t ixi = 0; ixi < poolXis.size(); ixi++) {
          const XiCandidate& xi = poolXis[ixi];
          const char* suf = XiMassHistSuffix(xi.invMass, cfg);
          if (!suf) continue;
          if (ShareTracks(kp, xi)) continue;

          TLorentzVector p4k = KaonMinusP4(kp.mom);
          TLorentzVector p4xi = XiP4(xi.mom);
          Double_t kstar = ComputeKStar(p4k, p4xi);
          FillKstarPairHists(m_histManager, kFALSE, suf, kstar, m_cent9, w);
        }
      }
    }
  }

  if (!mXiCandidates.empty()) {
    for (size_t ie = 0; ie < pool.size(); ie++) {
      const std::vector<KaonMinusCandidate>& poolKps = pool[ie].kaonsMinus;
      for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
        const XiCandidate& xi = mXiCandidates[ixi];
        const char* suf = XiMassHistSuffix(xi.invMass, cfg);
        if (!suf) continue;
        for (size_t ik = 0; ik < poolKps.size(); ik++) {
          const KaonMinusCandidate& kp = poolKps[ik];
          if (ShareTracks(kp, xi)) continue;

          TLorentzVector p4k = KaonMinusP4(kp.mom);
          TLorentzVector p4xi = XiP4(xi.mom);
          Double_t kstar = ComputeKStar(p4k, p4xi);
          FillKstarPairHists(m_histManager, kFALSE, suf, kstar, m_cent9, w);
        }
      }
    }
  }
}

void StKminusXiFemtoMaker::StoreEventForMixing(Float_t vz, Int_t cent9) {
  if (cent9 < 0) return;
  Int_t mixBin = GetMixingBin(vz, cent9);
  FemtoMixingEvent evt;
  evt.kaonsMinus = mKaonMinusCandidates;
  evt.xis = mXiCandidates;

  std::deque<FemtoMixingEvent>& pool = m_mixingPool[mixBin];
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  pool.push_back(evt);
  while ((Int_t)pool.size() > mix.bufferSize) {
    pool.pop_front();
  }
}
