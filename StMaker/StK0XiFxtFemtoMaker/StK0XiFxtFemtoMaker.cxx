#include "StK0XiFxtFemtoMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "CentralityHelper.h"
#include "FemtoMixingSampler.h"
#include "../StK0shortFxtMaker/StK0shortFxtMaker.h"
#include "../StXiFxtMaker/StXiFxtMaker.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "cuts/EventCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/MixingConfig.h"
#include "cuts/FemtoConfig.h"

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
const char* kChannel = "k0_xi";

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

void FillKstarPairHists(HistManager* hm, Bool_t sameEvent, const char* suffix, Double_t kstar,
                        Int_t cent9, Double_t w) {
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
}  // namespace

StK0XiFxtFemtoMaker::StK0XiFxtFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                         StK0shortFxtMaker* k0Maker, StXiFxtMaker* xiMaker,
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
      m_centralityPercent(-1.0) {}

StK0XiFxtFemtoMaker::~StK0XiFxtFemtoMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

StK0XiFxtFemtoMaker* createStK0XiFxtFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                               StK0shortFxtMaker* k0Maker, StXiFxtMaker* xiMaker,
                                               const char* outName) {
  return new StK0XiFxtFemtoMaker(name, picoMaker, k0Maker, xiMaker, outName);
}

extern "C" void* createStK0XiFxtFemtoMakerC(const char* name, void* picoMaker, void* k0Maker,
                                            void* xiMaker, const char* outName) {
  return (void*)createStK0XiFxtFemtoMaker(name, (StPicoDstMaker*)picoMaker,
                                          (StK0shortFxtMaker*)k0Maker, (StXiFxtMaker*)xiMaker,
                                          outName);
}

Int_t StK0XiFxtFemtoMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StK0XiFxtFemtoMaker] GetHistConfigPath() returned empty." << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StK0XiFxtFemtoMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStErr;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StK0XiFxtFemtoMaker] CentralityHelper init failed" << std::endl;
  }
  return kStOK;
}

void StK0XiFxtFemtoMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mK0Candidates.clear();
  mXiCandidates.clear();
}

Int_t StK0XiFxtFemtoMaker::Make() {
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
  const Float_t vzVpd = event->vzVpd();
  const Int_t runId = event->runId();
  const Int_t nBTOFMatch = event->nBTOFMatch();
  const Int_t refMult = event->refMult();
  const Int_t nTracks = mPicoDst->numberOfTracks();

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

  if (!PassEventCuts(pVtx.Z(), vr, nTracks, refMult, vzVpd)) return kStOK;

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) return kStOK;
    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight,
                                   centReason)) {
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

  if (!mK0shortMaker || !mXiMaker) {
    std::cerr << "[StK0XiFxtFemtoMaker] Required K0 or Xi Maker is missing!" << std::endl;
    return kStWarn;
  }

  const Int_t evtIdx = mEventCounter;

  const std::vector<TVector3>& k0moms = mK0shortMaker->GetK0shortMomList();
  const std::vector<Double_t>& k0masses = mK0shortMaker->GetK0shortInvMassList();
  const std::vector<Int_t>& k0posIds = mK0shortMaker->GetK0shortPionPosIdList();
  const std::vector<Int_t>& k0negIds = mK0shortMaker->GetK0shortPionNegIdList();

  for (size_t i = 0; i < k0moms.size(); i++) {
    K0Candidate cand;
    cand.mom = k0moms[i];
    cand.invMass = k0masses[i];
    cand.eventIndex = evtIdx;
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
    cand.eventIndex = evtIdx;
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

  FillSameEventPairs();
  FillMixedEventPairs(vz, m_cent9);
  StoreEventForMixing(vz, m_cent9);
  return kStOK;
}

Int_t StK0XiFxtFemtoMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StK0XiFxtFemtoMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) m_centrality->Finish();
  return kStOK;
}

void StK0XiFxtFemtoMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}

Bool_t StK0XiFxtFemtoMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t nTracks, Int_t refMult,
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

Double_t StK0XiFxtFemtoMaker::ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const {
  TLorentzVector q = pA - pB;
  TLorentzVector pair = pA + pB;
  q.Boost(-pair.BoostVector());
  return 0.5 * q.Vect().Mag();
}

StK0XiFxtFemtoMaker::PairKinematics StK0XiFxtFemtoMaker::ComputePairKinematics(const TVector3& momA,
                                                                               const TVector3& momB) const {
  PairKinematics kin;
  const TLorentzVector pA = K0shortP4(momA);
  const TLorentzVector pB = XiP4(momB);
  kin.kstar = ComputeKStar(pA, pB);

  kin.deltaEta = momA.Eta() - momB.Eta();
  kin.deltaPhiLab = momA.DeltaPhi(momB);
  kin.openingAngle = momA.Angle(momB);

  TLorentzVector qStar = pA - pB;
  TLorentzVector pair = pA + pB;
  qStar.Boost(-pair.BoostVector());
  TLorentzVector pAStar = pA;
  TLorentzVector pBStar = pB;
  pAStar.Boost(-pair.BoostVector());
  pBStar.Boost(-pair.BoostVector());
  const Double_t phiA = TMath::ATan2(pAStar.Vect().Y(), pAStar.Vect().X());
  const Double_t phiB = TMath::ATan2(pBStar.Vect().Y(), pBStar.Vect().X());
  Double_t dPhiStar = phiA - phiB;
  while (dPhiStar > TMath::Pi()) dPhiStar -= 2.0 * TMath::Pi();
  while (dPhiStar < -TMath::Pi()) dPhiStar += 2.0 * TMath::Pi();
  kin.deltaPhiStar = TMath::Abs(dPhiStar);
  return kin;
}

void StK0XiFxtFemtoMaker::FillPairProximityQa(HistManager* hm, Bool_t sameEvent, const char* suffix,
                                              const PairKinematics& kin, Int_t cent9, Double_t w) const {
  if (!hm || !suffix) return;
  const char* seMe = sameEvent ? "SE" : "ME";
  TString hDeltaPhiStar = TString::Format("hDeltaPhiStar%s_%s%s", seMe, kChannel, suffix);
  TString hDeltaEta = TString::Format("hDeltaEta%s_%s%s", seMe, kChannel, suffix);
  TString hDeltaPhiLab = TString::Format("hDeltaPhiLab%s_%s%s", seMe, kChannel, suffix);
  TString hOpeningAngle = TString::Format("hOpeningAngle%s_%s%s", seMe, kChannel, suffix);
  hm->Fill(hDeltaPhiStar.Data(), kin.deltaPhiStar, w);
  hm->Fill(hDeltaEta.Data(), kin.deltaEta, w);
  hm->Fill(hDeltaPhiLab.Data(), kin.deltaPhiLab, w);
  hm->Fill(hOpeningAngle.Data(), kin.openingAngle, w);
  if (sameEvent) {
    TString hEtaPhi = TString::Format("hDeltaEta_vs_DeltaPhiLabSE_%s%s", kChannel, suffix);
    TH2* h2 = (TH2*)hm->Get(hEtaPhi.Data());
    if (h2) h2->Fill(kin.deltaEta, kin.deltaPhiLab, w);
    TString hKstarDphi = TString::Format("hKstar_vs_DeltaPhiStarSE_%s%s", kChannel, suffix);
    TH2* h2k = (TH2*)hm->Get(hKstarDphi.Data());
    if (h2k) h2k->Fill(kin.kstar, kin.deltaPhiStar, w);
  }
  (void)cent9;
}

TLorentzVector StK0XiFxtFemtoMaker::K0shortP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kK0shortMass);
  return p4;
}

TLorentzVector StK0XiFxtFemtoMaker::XiP4(const TVector3& p) const {
  TLorentzVector p4;
  p4.SetVectM(p, kXiMass);
  return p4;
}

Bool_t StK0XiFxtFemtoMaker::ShareTracks(const K0Candidate& k0, const XiCandidate& xi) const {
  if (k0.eventIndex != xi.eventIndex) return kFALSE;
  if (k0.pionPosId == xi.protonId || k0.pionPosId == xi.lambdaPionId ||
      k0.pionPosId == xi.bachelorPionId)
    return kTRUE;
  if (k0.pionNegId == xi.protonId || k0.pionNegId == xi.lambdaPionId ||
      k0.pionNegId == xi.bachelorPionId)
    return kTRUE;
  return kFALSE;
}

Int_t StK0XiFxtFemtoMaker::EffectiveCentBin(Int_t cent9) const {
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  Int_t centBin = cent9;
  if (centBin < 0) centBin = 0;  // centrality disabled → vz-only mixing
  if (mix.nCentralityBins <= 0) return 0;
  if (centBin >= mix.nCentralityBins) centBin = mix.nCentralityBins - 1;
  return centBin;
}

Int_t StK0XiFxtFemtoMaker::GetMixingBin(Float_t vz, Int_t cent9) const {
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
  const Int_t centBin = EffectiveCentBin(cent9);
  const Int_t nCent = (mix.nCentralityBins > 0) ? mix.nCentralityBins : 1;
  return vzBin * nCent + centBin;
}

void StK0XiFxtFemtoMaker::FillSameEventPairs() {
  if (mK0Candidates.empty() || mXiCandidates.empty()) return;
  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t w =
      (m_centrality && m_centrality->IsEnabled() &&
       ConfigManager::GetInstance().GetCentralityCuts().useWeight)
          ? m_centWeight
          : 1.0;

  for (size_t ik0 = 0; ik0 < mK0Candidates.size(); ik0++) {
    const K0Candidate& k0 = mK0Candidates[ik0];
    if (k0.invMass < cfg.k0MassMin || k0.invMass > cfg.k0MassMax) continue;
    for (size_t ixi = 0; ixi < mXiCandidates.size(); ixi++) {
      const XiCandidate& xi = mXiCandidates[ixi];
      const char* suf = XiMassHistSuffix(xi.invMass, cfg);
      if (!suf) continue;
      if (ShareTracks(k0, xi)) {
        if (m_histManager) m_histManager->Fill("hShareRejectSE", 0.5);
        continue;
      }
      const PairKinematics kin = ComputePairKinematics(k0.mom, xi.mom);
      Double_t kstar = kin.kstar;
      FillKstarPairHists(m_histManager, kTRUE, suf, kstar, m_cent9, w);
      FillPairProximityQa(m_histManager, kTRUE, suf, kin, m_cent9, w);
    }
  }
}

void StK0XiFxtFemtoMaker::FillMixedEventPairs(Float_t vz, Int_t cent9) {
  Int_t mixBin = GetMixingBin(vz, cent9);
  std::map<Int_t, std::deque<FemtoMixingEvent> >::const_iterator poolIt = m_mixingPool.find(mixBin);
  if (poolIt == m_mixingPool.end() || poolIt->second.empty()) return;

  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  const Bool_t hasCurrentA = !mK0Candidates.empty();
  const Bool_t hasCurrentB = !mXiCandidates.empty();
  if (!hasCurrentA && (!mix.mixBothDirections || !hasCurrentB)) return;

  const FemtoConfig& cfg = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t w =
      (m_centrality && m_centrality->IsEnabled() &&
       ConfigManager::GetInstance().GetCentralityCuts().useWeight)
          ? m_centWeight
          : 1.0;

  typedef femto_mixing::PairCount PairCount;
  const std::deque<FemtoMixingEvent>& pool = poolIt->second;
  std::vector<femto_mixing::EventCandidateCounts> bufferedCounts;
  bufferedCounts.reserve(pool.size());
  for (size_t ie = 0; ie < pool.size(); ++ie) {
    bufferedCounts.push_back(
        femto_mixing::EventCandidateCounts(pool[ie].k0shorts.size(), pool[ie].xis.size()));
  }

  const size_t nCurrentA = hasCurrentA ? mK0Candidates.size() : 0;
  const size_t nCurrentB = hasCurrentB ? mXiCandidates.size() : 0;
  const femto_mixing::SamplingPlan plan =
      femto_mixing::BuildSamplingPlan(nCurrentA, nCurrentB, bufferedCounts, mix.mixBothDirections);
  const PairCount maxPairs =
      mix.maxMixedPairsPerEvent > 0 ? static_cast<PairCount>(mix.maxMixedPairsPerEvent) : 0;
  const PairCount attempted =
      femto_mixing::PlannedAttemptCount(plan.eligiblePairs, maxPairs, !mix.IsBufferAllMode());

  PairCount filled = 0;
  PairCount skippedOverlap = 0;
  PairCount skippedSignalWindow = 0;

  auto processFlatIndex = [&](PairCount flatIndex) {
    femto_mixing::PairReference ref;
    if (!femto_mixing::ResolvePairReference(plan, flatIndex, ref) || ref.poolEventIndex >= pool.size()) {
      return;
    }
    const FemtoMixingEvent& mixEvt = pool[ref.poolEventIndex];
    const K0Candidate* k0 = 0;
    const XiCandidate* xi = 0;
    if (ref.reverse) {
      if (!hasCurrentB || ref.firstIndex >= mixEvt.k0shorts.size() ||
          ref.secondIndex >= mXiCandidates.size())
        return;
      k0 = &mixEvt.k0shorts[ref.firstIndex];
      xi = &mXiCandidates[ref.secondIndex];
    } else {
      if (!hasCurrentA || ref.firstIndex >= mK0Candidates.size() ||
          ref.secondIndex >= mixEvt.xis.size())
        return;
      k0 = &mK0Candidates[ref.firstIndex];
      xi = &mixEvt.xis[ref.secondIndex];
    }

    if (k0->invMass < cfg.k0MassMin || k0->invMass > cfg.k0MassMax) {
      ++skippedSignalWindow;
      return;
    }
    const char* suf = XiMassHistSuffix(xi->invMass, cfg);
    if (!suf) {
      ++skippedSignalWindow;
      return;
    }
    if (ShareTracks(*k0, *xi)) {
      ++skippedOverlap;
      if (m_histManager) m_histManager->Fill("hShareRejectME", 0.5);
      return;
    }
    const PairKinematics kin = ComputePairKinematics(k0->mom, xi->mom);
    Double_t kstar = kin.kstar;
    FillKstarPairHists(m_histManager, kFALSE, suf, kstar, m_cent9, w);
    FillPairProximityQa(m_histManager, kFALSE, suf, kin, m_cent9, w);
    ++filled;
  };

  if (attempted == plan.eligiblePairs) {
    for (PairCount flatIndex = 0; flatIndex < attempted; ++flatIndex) processFlatIndex(flatIndex);
  } else {
    const std::vector<PairCount> sampled = femto_mixing::SampleWithoutReplacement(
        plan.eligiblePairs, attempted, [&](PairCount maxInclusive) {
          const Double_t upper = static_cast<Double_t>(maxInclusive) + 1.0;
          PairCount selected = static_cast<PairCount>(gRandom->Uniform(0.0, upper));
          if (selected > maxInclusive) selected = maxInclusive;
          return selected;
        });
    for (size_t is = 0; is < sampled.size(); ++is) processFlatIndex(sampled[is]);
  }

  if (m_histManager) {
    if (attempted > 0) m_histManager->Fill("hMixSamplerQA", (Double_t)femto_mixing::kQaAttempted, (Double_t)attempted);
    if (plan.eligiblePairs > 0)
      m_histManager->Fill("hMixSamplerQA", (Double_t)femto_mixing::kQaEligible, (Double_t)plan.eligiblePairs);
    if (filled > 0) m_histManager->Fill("hMixSamplerQA", (Double_t)femto_mixing::kQaFilled, (Double_t)filled);
    if (skippedOverlap > 0)
      m_histManager->Fill("hMixSamplerQA", (Double_t)femto_mixing::kQaSkippedOverlap,
                          (Double_t)skippedOverlap);
    if (skippedSignalWindow > 0)
      m_histManager->Fill("hMixSamplerQA", (Double_t)femto_mixing::kQaSkippedSignalWindow,
                          (Double_t)skippedSignalWindow);
  }
}

void StK0XiFxtFemtoMaker::StoreEventForMixing(Float_t vz, Int_t cent9) {
  if (mK0Candidates.empty() && mXiCandidates.empty()) return;
  Int_t mixBin = GetMixingBin(vz, cent9);
  FemtoMixingEvent evt;
  evt.k0shorts = mK0Candidates;
  evt.xis = mXiCandidates;
  std::deque<FemtoMixingEvent>& pool = m_mixingPool[mixBin];
  pool.push_back(evt);
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  while ((Int_t)pool.size() > mix.bufferSize) pool.pop_front();
}
