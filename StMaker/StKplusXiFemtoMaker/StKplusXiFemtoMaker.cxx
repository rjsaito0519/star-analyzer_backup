#include "StKplusXiFemtoMaker.h"
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
const Double_t kKaonPlusMass = 0.493677;
const Double_t kXiMass = 1.32171;
const char* kChannel = "kp_xim";

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

StKplusXiFemtoMaker::StKplusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StXiMaker* xiMaker,
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

StKplusXiFemtoMaker::~StKplusXiFemtoMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

StKplusXiFemtoMaker* createStKplusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StXiMaker* xiMaker,
                                               const char* outName) {
  return new StKplusXiFemtoMaker(name, picoMaker, xiMaker, outName);
}

extern "C" void* createStKplusXiFemtoMakerC(const char* name, void* picoMaker, void* xiMaker, const char* outName) {
  return (void*)createStKplusXiFemtoMaker(name, (StPicoDstMaker*)picoMaker, (StXiMaker*)xiMaker, outName);
}

Int_t StKplusXiFemtoMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StKplusXiFemtoMaker] GetHistConfigPath() returned empty; no histograms will be filled."
              << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StKplusXiFemtoMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStErr;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StKplusXiFemtoMaker] CentralityHelper init failed" << std::endl;
  }

  return kStOK;
}

void StKplusXiFemtoMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mKaonPlusCandidates.clear();
  mXiCandidates.clear();
}

Bool_t StKplusXiFemtoMaker::PassKaonPlusCuts(StPicoTrack* trk, const TVector3& pVtx, Float_t& mass2,
                                             Bool_t& tofMatch) const {
  mass2 = -999.0f;
  tofMatch = kFALSE;
  if (!trk || trk->charge() <= 0) return kFALSE;

  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk->nHitsFit() < cfg.kaonPlusMinNHitsFit) return kFALSE;
  if (trk->nHitsMax() <= 0) return kFALSE;
  if ((Float_t)trk->nHitsFit() / (Float_t)trk->nHitsMax() < cfg.kaonPlusMinNHitsRatio) return kFALSE;

  TVector3 pMom = trk->gMom();
  const Float_t pt = pMom.Perp();
  const Float_t eta = pMom.PseudoRapidity();
  if (pt < cfg.kaonPlusMinPt || pt > cfg.kaonPlusMaxPt) return kFALSE;
  if (TMath::Abs(eta) >= cfg.kaonPlusMaxAbsEta) return kFALSE;

  const Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca >= cfg.kaonPlusMaxDca) return kFALSE;
  if (TMath::Abs(trk->nSigmaKaon()) >= cfg.kaonPlusMaxAbsNSigma) return kFALSE;

  Float_t deltaOneOverBeta = -999.0f;
  Int_t btofIndex = trk->bTofPidTraitsIndex();
  StPicoBTofPidTraits* tof = 0;
  if (btofIndex >= 0 && mPicoDst) tof = mPicoDst->btofPidTraits(btofIndex);
  StPhiKKReconstruction::FillTofInfo(mass2, deltaOneOverBeta, tofMatch, tof, pMom);

  const Double_t pmom = pMom.Mag();
  const Bool_t passTofRule =
      (pmom < cfg.kaonPlusTofMomentumThreshold) ||
      (pmom >= cfg.kaonPlusTofMomentumThreshold && tofMatch && mass2 >= cfg.kaonPlusMinMass2 &&
       mass2 <= cfg.kaonPlusMaxMass2);
  if (!passTofRule) return kFALSE;
  return kTRUE;
}

void StKplusXiFemtoMaker::CollectKaonPlusCandidates(const TVector3& pVtx) {
  if (!mPicoDst) return;
  const Int_t nTracks = mPicoDst->numberOfTracks();
  for (Int_t itrk = 0; itrk < nTracks; itrk++) {
    StPicoTrack* trk = mPicoDst->track(itrk);
    if (!trk) continue;

    Float_t mass2 = -999.0f;
    Bool_t tofMatch = kFALSE;
    if (!PassKaonPlusCuts(trk, pVtx, mass2, tofMatch)) continue;

    KaonPlusCandidate cand;
    cand.mom = trk->gMom();
    cand.trackId = itrk;
    cand.mass2 = mass2;
    cand.tofMatch = tofMatch;
    mKaonPlusCandidates.push_back(cand);

    if (m_histManager) {
      m_histManager->Fill("hKp_Pt", cand.mom.Pt());
      m_histManager->Fill("hKp_Eta", cand.mom.Eta());
      m_histManager->Fill("hKp_NSigma", trk->nSigmaKaon());
      if (tofMatch) {
        m_histManager->Fill("hKp_Mass2VsP", cand.mom.Mag(), mass2);
        m_histManager->Fill("hKp_Mass2VsPt", cand.mom.Pt(), mass2);
      }
    }
  }
  if (m_histManager) {
    m_histManager->Fill("hNKp", (Double_t)mKaonPlusCandidates.size());
  }
}

void StKplusXiFemtoMaker::CollectXiCandidates() {
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

Int_t StKplusXiFemtoMaker::Make() {
  if (!mPicoDstMaker) return kStWarn;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;

  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  mEventCounter++;
  mKaonPlusCandidates.clear();
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
    std::cerr << "[StKplusXiFemtoMaker] Required Xi Maker is missing!" << std::endl;
    return kStWarn;
  }

  CollectKaonPlusCandidates(pVtx);
  CollectXiCandidates();

  FillSameEventPairs();
  FillMixedEventPairs(vz, m_cent9);
  StoreEventForMixing(vz, m_cent9);

  return kStOK;
}

Int_t StKplusXiFemtoMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StKplusXiFemtoMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    m_centrality->Finish();
  }
  return kStOK;
}

void StKplusXiFemtoMaker::WriteHistograms() {
  if (m_histManager) {
    m_histManager->Write();
  }
}

Bool_t StKplusXiFemtoMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t refMult) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (ev.maxNTr > 0 && refMult > ev.maxNTr) return kFALSE;
  if (TMath::Abs(vz) < ev.minVz || TMath::Abs(vz) > ev.maxVz) return kFALSE;
  if (vr > ev.maxVr) return kFALSE;
  return kTRUE;
}

Double_t StKplusXiFemtoMaker::ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const {
  TLorentzVector q = pA - pB;
  TLorentzVector pair = pA + pB;
  q.Boost(-pair.BoostVector());
  return 0.5 * q.Vect().Mag();
}

TLorentzVector StKplusXiFemtoMaker::KaonPlusP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kKaonPlusMass);
  return p4;
}

TLorentzVector StKplusXiFemtoMaker::XiP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kXiMass);
  return p4;
}

Bool_t StKplusXiFemtoMaker::ShareTracks(const KaonPlusCandidate& kp, const XiCandidate& xi) const {
  if (kp.trackId == xi.protonId || kp.trackId == xi.lambdaPionId || kp.trackId == xi.bachelorPionId) return kTRUE;
  return kFALSE;
}

Int_t StKplusXiFemtoMaker::GetMixingBin(Float_t vz, Int_t cent9) const {
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

void StKplusXiFemtoMaker::FillSameEventPairs() {
  if (mKaonPlusCandidates.empty() || mXiCandidates.empty()) return;
  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t w =
      (m_centrality && m_centrality->IsEnabled() && ConfigManager::GetInstance().GetCentralityCuts().useWeight)
          ? m_centWeight
          : 1.0;

  for (size_t ik = 0; ik < mKaonPlusCandidates.size(); ik++) {
    const KaonPlusCandidate& kp = mKaonPlusCandidates[ik];
    for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
      const XiCandidate& xi = mXiCandidates[ixi];
      const char* suf = XiMassHistSuffix(xi.invMass, cfg);
      if (!suf) continue;
      if (ShareTracks(kp, xi)) continue;

      TLorentzVector p4k = KaonPlusP4(kp.mom);
      TLorentzVector p4xi = XiP4(xi.mom);
      Double_t kstar = ComputeKStar(p4k, p4xi);
      FillKstarPairHists(m_histManager, kTRUE, suf, kstar, m_cent9, w);
    }
  }
}

void StKplusXiFemtoMaker::FillMixedEventPairs(Float_t vz, Int_t cent9) {
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

  if (!mKaonPlusCandidates.empty()) {
    for (size_t ie = 0; ie < pool.size(); ie++) {
      const std::vector<XiCandidate>& poolXis = pool[ie].xis;
      for (size_t ik = 0; ik < mKaonPlusCandidates.size(); ik++) {
        const KaonPlusCandidate& kp = mKaonPlusCandidates[ik];
        for (size_t ixi = 0; ixi < poolXis.size(); ixi++) {
          const XiCandidate& xi = poolXis[ixi];
          const char* suf = XiMassHistSuffix(xi.invMass, cfg);
          if (!suf) continue;
          if (ShareTracks(kp, xi)) continue;

          TLorentzVector p4k = KaonPlusP4(kp.mom);
          TLorentzVector p4xi = XiP4(xi.mom);
          Double_t kstar = ComputeKStar(p4k, p4xi);
          FillKstarPairHists(m_histManager, kFALSE, suf, kstar, m_cent9, w);
        }
      }
    }
  }

  if (!mXiCandidates.empty()) {
    for (size_t ie = 0; ie < pool.size(); ie++) {
      const std::vector<KaonPlusCandidate>& poolKps = pool[ie].kaonsPlus;
      for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
        const XiCandidate& xi = mXiCandidates[ixi];
        const char* suf = XiMassHistSuffix(xi.invMass, cfg);
        if (!suf) continue;
        for (size_t ik = 0; ik < poolKps.size(); ik++) {
          const KaonPlusCandidate& kp = poolKps[ik];
          if (ShareTracks(kp, xi)) continue;

          TLorentzVector p4k = KaonPlusP4(kp.mom);
          TLorentzVector p4xi = XiP4(xi.mom);
          Double_t kstar = ComputeKStar(p4k, p4xi);
          FillKstarPairHists(m_histManager, kFALSE, suf, kstar, m_cent9, w);
        }
      }
    }
  }
}

void StKplusXiFemtoMaker::StoreEventForMixing(Float_t vz, Int_t cent9) {
  if (cent9 < 0) return;
  Int_t mixBin = GetMixingBin(vz, cent9);
  FemtoMixingEvent evt;
  evt.kaonsPlus = mKaonPlusCandidates;
  evt.xis = mXiCandidates;

  std::deque<FemtoMixingEvent>& pool = m_mixingPool[mixBin];
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  pool.push_back(evt);
  while ((Int_t)pool.size() > mix.bufferSize) {
    pool.pop_front();
  }
}
