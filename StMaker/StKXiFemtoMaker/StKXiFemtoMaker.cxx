#include "StKXiFemtoMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "CentralityHelper.h"
#include "../StK0shortMaker/StK0shortMaker.h"
#include "../StXiMaker/StXiMaker.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "cuts/EventCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/MixingConfig.h"

#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TString.h"
#include "TMath.h"
#include "TRandom.h"
#include <iostream>
#include <vector>

namespace {
const Double_t kK0shortMass = 0.497611;
const Double_t kXiMass = 1.32171;
}

StKXiFemtoMaker::StKXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                 StK0shortMaker* k0Maker, StXiMaker* xiMaker,
                                 const char* outName)
    : StMaker(name),
      mPicoDstMaker(picoMaker),
      mPicoDst(0),
      mK0shortMaker(k0Maker),
      mXiMaker(xiMaker),
      mOutName(outName),
      mEventCounter(0),
      m_histManager(0),
      m_centrality(0),
      m_cent9(-1),
      m_cent16(-1),
      m_refMultCorr(-1.0),
      m_centWeight(1.0),
      m_centralityPercent(-1.0) {
  mK0MassMin = 0.482;
  mK0MassMax = 0.513;
  mXiMassMin = 1.312;
  mXiMassMax = 1.332;
}

StKXiFemtoMaker::~StKXiFemtoMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

StKXiFemtoMaker* createStKXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                       StK0shortMaker* k0Maker, StXiMaker* xiMaker,
                                       const char* outName) {
  return new StKXiFemtoMaker(name, picoMaker, k0Maker, xiMaker, outName);
}

extern "C" void* createStKXiFemtoMakerC(const char* name, void* picoMaker,
                                        void* k0Maker, void* xiMaker,
                                        const char* outName) {
  return (void*)createStKXiFemtoMaker(name, (StPicoDstMaker*)picoMaker,
                                      (StK0shortMaker*)k0Maker, (StXiMaker*)xiMaker, outName);
}

Int_t StKXiFemtoMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StKXiFemtoMaker] GetHistConfigPath() returned empty; no histograms will be filled." << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StKXiFemtoMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStErr;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StKXiFemtoMaker] CentralityHelper init failed" << std::endl;
  }

  return kStOK;
}

void StKXiFemtoMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mK0Candidates.clear();
  mXiCandidates.clear();
}

Int_t StKXiFemtoMaker::Make() {
  if (!mPicoDstMaker) return kStWarn;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;

  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  mEventCounter++;
  mK0Candidates.clear();
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

  // Retrieve Candidates from K0shortMaker and XiMaker
  if (!mK0shortMaker || !mXiMaker) {
    std::cerr << "[StKXiFemtoMaker] Required K0short or Xi Maker is missing!" << std::endl;
    return kStWarn;
  }

  const std::vector<TVector3>& k0moms = mK0shortMaker->GetK0shortMomList();
  const std::vector<Double_t>& k0masses = mK0shortMaker->GetK0shortInvMassList();
  const std::vector<Int_t>& k0posIds = mK0shortMaker->GetK0shortPionPosIdList();
  const std::vector<Int_t>& k0negIds = mK0shortMaker->GetK0shortPionNegIdList();

  for (size_t i = 0; i < k0moms.size(); i++) {
    K0Candidate cand;
    cand.mom = k0moms[i];
    cand.invMass = k0masses[i];
    cand.pionPosId = k0posIds[i];
    cand.pionNegId = k0negIds[i];
    mK0Candidates.push_back(cand);

    if (m_histManager) {
      m_histManager->Fill("hK0short_InvMass", cand.invMass);
      m_histManager->Fill("hK0short_Pt", cand.mom.Pt());
      m_histManager->Fill("hK0short_Eta", cand.mom.Eta());
    }
  }

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

  // Fill Same Event Pairs
  FillSameEventPairs();

  // Fill Mixed Event Pairs
  FillMixedEventPairs(vz, m_cent9);

  // Store Event for Mixing Pool
  StoreEventForMixing(vz, m_cent9);

  return kStOK;
}

Int_t StKXiFemtoMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StKXiFemtoMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    m_centrality->Finish();
  }
  return kStOK;
}

void StKXiFemtoMaker::WriteHistograms() {
  if (m_histManager) {
    m_histManager->Write();
  }
}

Bool_t StKXiFemtoMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t refMult) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (ev.maxNTr > 0 && refMult > ev.maxNTr) return kFALSE;
  if (TMath::Abs(vz) < ev.minVz || TMath::Abs(vz) > ev.maxVz) return kFALSE;
  if (vr > ev.maxVr) return kFALSE;
  return kTRUE;
}

Double_t StKXiFemtoMaker::ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const {
  TLorentzVector q = pA - pB;
  TLorentzVector pair = pA + pB;
  q.Boost(-pair.BoostVector());
  return 0.5 * q.Vect().Mag();
}

TLorentzVector StKXiFemtoMaker::K0shortP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kK0shortMass);
  return p4;
}

TLorentzVector StKXiFemtoMaker::XiP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kXiMass);
  return p4;
}

Bool_t StKXiFemtoMaker::ShareTracks(const K0Candidate& k0, const XiCandidate& xi) const {
  if (k0.pionPosId == xi.protonId || k0.pionPosId == xi.lambdaPionId || k0.pionPosId == xi.bachelorPionId) return kTRUE;
  if (k0.pionNegId == xi.protonId || k0.pionNegId == xi.lambdaPionId || k0.pionNegId == xi.bachelorPionId) return kTRUE;
  return kFALSE;
}

Int_t StKXiFemtoMaker::GetMixingBin(Float_t vz, Int_t cent9) const {
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

void StKXiFemtoMaker::FillSameEventPairs() {
  if (mK0Candidates.empty() || mXiCandidates.empty()) return;
  const Double_t w = (m_centrality && m_centrality->IsEnabled() && ConfigManager::GetInstance().GetCentralityCuts().useWeight) ? m_centWeight : 1.0;

  for (size_t ik0 = 0; ik0 < mK0Candidates.size(); ik0++) {
    const K0Candidate& k0 = mK0Candidates[ik0];
    if (k0.invMass < mK0MassMin || k0.invMass > mK0MassMax) continue;
    
    for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
      const XiCandidate& xi = mXiCandidates[ixi];
      if (xi.invMass < mXiMassMin || xi.invMass > mXiMassMax) continue;
      
      if (ShareTracks(k0, xi)) continue;

      TLorentzVector p4k0 = K0shortP4(k0.mom);
      TLorentzVector p4xi = XiP4(xi.mom);
      Double_t kstar = ComputeKStar(p4k0, p4xi);

      if (m_histManager) {
        m_histManager->Fill("hKstarSE_k0_xi", kstar, w);
        if (m_cent9 >= 0) {
          TH2* h2 = (TH2*)m_histManager->Get("hKstarSEVsCent_k0_xi");
          if (h2) h2->Fill(kstar, (Double_t)m_cent9, w);
        }
      }
    }
  }
}

void StKXiFemtoMaker::FillMixedEventPairs(Float_t vz, Int_t cent9) {
  if (cent9 < 0) return;
  Int_t mixBin = GetMixingBin(vz, cent9);
  std::map<Int_t, std::deque<FemtoMixingEvent> >::const_iterator poolIt = m_mixingPool.find(mixBin);
  if (poolIt == m_mixingPool.end() || poolIt->second.empty()) return;

  const Double_t w = (m_centrality && m_centrality->IsEnabled() && ConfigManager::GetInstance().GetCentralityCuts().useWeight) ? m_centWeight : 1.0;
  const std::deque<FemtoMixingEvent>& pool = poolIt->second;

  // Mix current K0s with pool Xis
  if (!mK0Candidates.empty()) {
    for (size_t ie = 0; ie < pool.size(); ie++) {
      const std::vector<XiCandidate>& poolXis = pool[ie].xis;
      for (size_t ik0 = 0; ik0 < mK0Candidates.size(); ik0++) {
        const K0Candidate& k0 = mK0Candidates[ik0];
        if (k0.invMass < mK0MassMin || k0.invMass > mK0MassMax) continue;
        for (size_t ixi = 0; ixi < poolXis.size(); ixi++) {
          const XiCandidate& xi = poolXis[ixi];
          if (xi.invMass < mXiMassMin || xi.invMass > mXiMassMax) continue;
          if (ShareTracks(k0, xi)) continue;

          TLorentzVector p4k0 = K0shortP4(k0.mom);
          TLorentzVector p4xi = XiP4(xi.mom);
          Double_t kstar = ComputeKStar(p4k0, p4xi);

          if (m_histManager) {
            m_histManager->Fill("hKstarME_k0_xi", kstar, w);
            if (m_cent9 >= 0) {
              TH2* h2 = (TH2*)m_histManager->Get("hKstarMEVsCent_k0_xi");
              if (h2) h2->Fill(kstar, (Double_t)m_cent9, w);
            }
          }
        }
      }
    }
  }

  // Mix current Xis with pool K0s
  if (!mXiCandidates.empty()) {
    for (size_t ie = 0; ie < pool.size(); ie++) {
      const std::vector<K0Candidate>& poolK0s = pool[ie].k0shorts;
      for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
        const XiCandidate& xi = mXiCandidates[ixi];
        if (xi.invMass < mXiMassMin || xi.invMass > mXiMassMax) continue;
        for (size_t ik0 = 0; ik0 < poolK0s.size(); ik0++) {
          const K0Candidate& k0 = poolK0s[ik0];
          if (k0.invMass < mK0MassMin || k0.invMass > mK0MassMax) continue;
          if (ShareTracks(k0, xi)) continue;

          TLorentzVector p4k0 = K0shortP4(k0.mom);
          TLorentzVector p4xi = XiP4(xi.mom);
          Double_t kstar = ComputeKStar(p4k0, p4xi);

          if (m_histManager) {
            m_histManager->Fill("hKstarME_k0_xi", kstar, w);
            if (m_cent9 >= 0) {
              TH2* h2 = (TH2*)m_histManager->Get("hKstarMEVsCent_k0_xi");
              if (h2) h2->Fill(kstar, (Double_t)m_cent9, w);
            }
          }
        }
      }
    }
  }
}

void StKXiFemtoMaker::StoreEventForMixing(Float_t vz, Int_t cent9) {
  if (cent9 < 0) return;
  Int_t mixBin = GetMixingBin(vz, cent9);
  FemtoMixingEvent evt;
  evt.k0shorts = mK0Candidates;
  evt.xis = mXiCandidates;

  std::deque<FemtoMixingEvent>& pool = m_mixingPool[mixBin];
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  pool.push_back(evt);
  while ((Int_t)pool.size() > mix.bufferSize) {
    pool.pop_front();
  }
}


