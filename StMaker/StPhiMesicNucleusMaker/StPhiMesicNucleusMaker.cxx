#include "StPhiMesicNucleusMaker.h"

#include "CentralityHelper.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/EventCutConfig.h"
#include "cuts/LambdaCutConfig.h"
#include "cuts/NuclearIdCutConfig.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/PhiMesicNucleusConfig.h"
#include "cuts/TrackCutConfig.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/SystemOfUnits.h"

#include "TFile.h"
#include "TMath.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>

namespace {
const Double_t kPionMass = 0.139570;
const Double_t kProtonMass = 0.938272;
const Double_t kKaonMass = 0.493677;

StHypernucleusReconstructor::TopologyCuts ToTopo(const PhiMesicNucleusConfig::TopologyCuts& in) {
  StHypernucleusReconstructor::TopologyCuts out;
  out.minDaughterDca = in.minDaughterDca;
  out.maxDaughterDca = in.maxDaughterDca;
  out.maxDcaToPv = in.maxDcaToPv;
  out.minDecayLength = in.minDecayLength;
  out.minTransverseDecayLength = in.minTransverseDecayLength;
  out.minCosPointing = in.minCosPointing;
  out.maxAbsRapidity = in.maxAbsRapidity;
  out.maxPathLength = in.maxPathLength;
  return out;
}

StHypernucleusReconstructor::MassWindow ToWindow(const PhiMesicNucleusConfig::MassWindow& in) {
  StHypernucleusReconstructor::MassWindow out;
  out.signalMin = in.signalMin;
  out.signalMax = in.signalMax;
  out.leftSbMin = in.leftSbMin;
  out.leftSbMax = in.leftSbMax;
  out.rightSbMin = in.rightSbMin;
  out.rightSbMax = in.rightSbMax;
  return out;
}
}  // namespace

StPhiMesicNucleusMaker::StPhiMesicNucleusMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName)
    : StMaker(name),
      mPicoDstMaker(picoMaker),
      mPicoDst(0),
      mOutName(outName),
      mEventCounter(0),
      m_histManager(0),
      m_centrality(0),
      m_currentCent9(-1),
      m_currentCent16(-1),
      m_currentRefMultCorr(-1.0),
      m_currentCentWeight(1.0) {}

StPhiMesicNucleusMaker::~StPhiMesicNucleusMaker() {
  delete m_centrality;
  m_centrality = 0;
  delete m_histManager;
  m_histManager = 0;
}

Int_t StPhiMesicNucleusMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (!histPath.empty()) {
    m_histManager = new HistManager();
    if (!m_histManager->LoadFromFile(histPath.c_str())) {
      std::cerr << "[StPhiMesicNucleusMaker] failed to load hist config " << histPath << std::endl;
      delete m_histManager;
      m_histManager = 0;
    }
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StPhiMesicNucleusMaker] centrality helper init failed." << std::endl;
  }
  return kStOK;
}

void StPhiMesicNucleusMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  m_kplusTracks.clear();
  m_protonTracks.clear();
  m_pionMinusTracks.clear();
  m_protonLooseTracks.clear();
  m_pionLooseTracks.clear();
  m_deuteronTracks.clear();
  m_deuteronLooseTracks.clear();
  m_tritonTracks.clear();
  m_he3Tracks.clear();
  m_he4Tracks.clear();
  m_lambdas.clear();
  m_hypertriton2Body.clear();
  m_hypertriton3Body.clear();
  m_hyperhydrogen4.clear();
  m_parents.clear();
}

Int_t StPhiMesicNucleusMaker::Make() {
  EventContext ctx;
  if (!BuildEventContext(ctx)) return kStOK;
  if (!ctx.accepted) return kStOK;

  SelectTracksAndPid(ctx);
  SelectNuclearTracks(ctx);
  BuildLambdaCandidates(ctx);
  BuildHyperCandidates(ctx);
  BuildParentCandidates(ctx);
  FillMixedEventParents(ctx);
  FillQaAndPhysicsHists(ctx);
  StoreEventForMixing(ctx);
  return kStOK;
}

Int_t StPhiMesicNucleusMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    m_histManager->Write();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  return kStOK;
}

Bool_t StPhiMesicNucleusMaker::BuildEventContext(EventContext& ctx) {
  if (!mPicoDstMaker) return kFALSE;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kFALSE;

  StPicoEvent* event = mPicoDst->event();
  if (!event) return kFALSE;
  ++mEventCounter;

  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  CentralityCutConfig& cent = ConfigManager::GetInstance().GetCentralityCuts();

  ctx.accepted = kFALSE;
  ctx.eventId = event->eventId();
  ctx.runId = event->runId();
  ctx.pv = event->primaryVertex();
  ctx.bField = event->bField();
  ctx.nTracks = mPicoDst->numberOfTracks();
  ctx.rawMult = (cent.enabled && cent.mode == "fxtmult") ? event->fxtMult() : event->refMult();
  ctx.refMultCorr = -1.0;
  ctx.centWeight = 1.0;
  ctx.cent9 = -1;
  ctx.cent16 = -1;

  if (ctx.pv.Z() < ev.minVz || ctx.pv.Z() > ev.maxVz) return kTRUE;
  if (ev.maxNTr > 0 && ctx.nTracks > ev.maxNTr) return kTRUE;
  if (ctx.rawMult < ev.minRefMult || ctx.rawMult > ev.maxRefMult) return kTRUE;
  if (ev.ComputeVr(ctx.pv.X(), ctx.pv.Y()) > ev.maxVr) return kTRUE;

  CentralityRejectReason reason = kCentralityOk;
  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(ctx.runId, reason)) return kTRUE;
    if (!m_centrality->CheckPileup(ctx.rawMult, event->nBTOFMatch(), ctx.pv.Z(), reason)) return kTRUE;
    if (!m_centrality->ComputeBins(event, ctx.rawMult, ctx.pv.Z(), ctx.cent9, ctx.cent16, ctx.refMultCorr,
                                   ctx.centWeight, reason)) {
      return kTRUE;
    }
    if (!m_centrality->AcceptCentBin(ctx.cent9, ctx.refMultCorr, reason)) return kTRUE;
  }

  m_currentCent9 = ctx.cent9;
  m_currentCent16 = ctx.cent16;
  m_currentRefMultCorr = ctx.refMultCorr;
  m_currentCentWeight = ctx.centWeight;
  ctx.accepted = kTRUE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassTrackQuality(const StPicoTrack* trk, const TVector3& pv) const {
  if (!trk) return kFALSE;
  TrackCutConfig& tc = ConfigManager::GetInstance().GetTrackCuts();
  if (trk->nHitsFit() < tc.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 && (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < tc.minNHitsRatio) return kFALSE;
  if (trk->nHitsDedx() < tc.minNHitsDedx) return kFALSE;
  if (tc.requirePrimaryTrack && !trk->isPrimary()) return kFALSE;
  TVector3 p = trk->pMom();
  if (p.Pt() < tc.minPt || p.Pt() > tc.maxPt) return kFALSE;
  if (p.Eta() < tc.minEta || p.Eta() > tc.maxEta) return kFALSE;
  if (trk->gDCA(pv.X(), pv.Y(), pv.Z()) > tc.maxDCA) return kFALSE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassKplusPid(const StPicoTrack* trk) const {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  TVector3 p = trk->pMom();
  if (p.Pt() < pmn.kplusPtMin || p.Pt() > pmn.kplusPtMax) return kFALSE;
  if (trk->charge() <= 0) return kFALSE;
  if (TMath::Abs(trk->nSigmaKaon()) > pmn.kplusNSigmaMax) return kFALSE;
  if (pmn.requireTofForKplus && !trk->isTofTrack()) return kFALSE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassLambdaProtonCuts(const StPicoTrack* trk, const TVector3& pv) const {
  if (!trk || trk->charge() <= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio) return kFALSE;
  if (TMath::Abs(trk->nSigmaProton()) > lam.nSigmaProton) return kFALSE;
  const Double_t dca = trk->gDCA(pv.X(), pv.Y(), pv.Z());
  if (dca < lam.minDCAProton) return kFALSE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassLambdaPionCuts(const StPicoTrack* trk, const TVector3& pv) const {
  if (!trk || trk->charge() >= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > lam.nSigmaPion) return kFALSE;
  const Double_t dca = trk->gDCA(pv.X(), pv.Y(), pv.Z());
  if (dca < lam.minDCAPion) return kFALSE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassHyperTwoBodyPionCuts(const StPicoTrack* trk, const TVector3& pv) const {
  if (!trk || trk->charge() >= 0) return kFALSE;
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  const PhiMesicNucleusConfig::HyperTwoBodyTrackCuts& hc = pmn.hyperTwoBodyTrackCuts;
  if (trk->nHitsFit() < hc.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < hc.minNHitsRatio) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > hc.nSigmaPion) return kFALSE;
  if (trk->gDCA(pv.X(), pv.Y(), pv.Z()) < hc.minDcaPion) return kFALSE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassHyperThreeBodyLightCuts(const StPicoTrack* trk, Int_t expectedChargeSign) const {
  // 3-body hyper daughters: nHits + nSigma only (no Lambda-style min DCA).
  if (!trk) return kFALSE;
  if (expectedChargeSign > 0 && trk->charge() <= 0) return kFALSE;
  if (expectedChargeSign < 0 && trk->charge() >= 0) return kFALSE;
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  const PhiMesicNucleusConfig::HyperTwoBodyTrackCuts& hc = pmn.hyperTwoBodyTrackCuts;
  if (trk->nHitsFit() < hc.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < hc.minNHitsRatio) return kFALSE;
  if (expectedChargeSign > 0) {
    if (TMath::Abs(trk->nSigmaProton()) > pmn.protonNSigmaMax) return kFALSE;
  } else {
    if (TMath::Abs(trk->nSigmaPion()) > hc.nSigmaPion) return kFALSE;
  }
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassHyperTwoBodyNuclearCuts(const StPicoTrack* trk, const TVector3& pv) const {
  if (!trk || trk->charge() <= 0) return kFALSE;
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  const PhiMesicNucleusConfig::HyperTwoBodyTrackCuts& hc = pmn.hyperTwoBodyTrackCuts;
  if (trk->nHitsFit() < hc.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < hc.minNHitsRatio) return kFALSE;
  if (trk->gDCA(pv.X(), pv.Y(), pv.Z()) < hc.minDcaNuclear) return kFALSE;
  return kTRUE;
}

Bool_t StPhiMesicNucleusMaker::PassPrimaryLikeBachelor(const StPicoTrack* trk, const TVector3& pv) const {
  if (!trk) return kFALSE;
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  const Double_t dca = trk->gDCA(pv.X(), pv.Y(), pv.Z());
  return (dca <= pmn.parentPvOriginMaxBachelorDca);
}

Bool_t StPhiMesicNucleusMaker::PassParentPvOrigin(const TrackRef& bachelor) const {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  return (bachelor.dcaToPv <= pmn.parentPvOriginMaxBachelorDca);
}

Bool_t StPhiMesicNucleusMaker::PassNuclearDedxPid(NuclearSpecies species, const StPicoTrack* trk) const {
  if (!trk || trk->charge() <= 0) return kFALSE;
  const Double_t nSigma = StNuclearIdHelper::GetNSigma(species, trk->pMom().Mag(), trk->dEdx());
  return (TMath::Abs(nSigma) <= ConfigManager::GetInstance().GetNuclearIdCuts().maxNSigmaNuclear);
}

void StPhiMesicNucleusMaker::SelectTracksAndPid(const EventContext& ctx) {
  for (Int_t i = 0; i < ctx.nTracks; ++i) {
    StPicoTrack* trk = mPicoDst->track(i);
    if (!trk) continue;

    TrackRef ref;
    ref.trackId = trk->id();
    ref.trackIndex = i;
    ref.charge = trk->charge();
    ref.dcaToPv = trk->gDCA(ctx.pv.X(), ctx.pv.Y(), ctx.pv.Z());
    ref.m2 = -999.0;
    ref.nSigma = 0.0;
    ref.hasTof = kFALSE;

    const TVector3 pMom = trk->pMom();
    if (trk->isTofTrack()) {
      const Int_t idx = trk->bTofPidTraitsIndex();
      if (idx >= 0) {
        const StPicoBTofPidTraits* btof = mPicoDst->btofPidTraits(idx);
        if (btof && btof->btofBeta() > 0.0) {
          const Double_t pMag = pMom.Mag();
          ref.m2 = pMag * pMag * (1.0 / (btof->btofBeta() * btof->btofBeta()) - 1.0);
          ref.hasTof = kTRUE;
        }
      }
    }

    // K+: primary-style track quality + dEdx PID + PV-origin (bachelor)
    if (PassTrackQuality(trk, ctx.pv) && PassKplusPid(trk) && PassPrimaryLikeBachelor(trk, ctx.pv)) {
      ref.speciesKey = "kplus";
      ref.p4.SetVectM(pMom, kKaonMass);
      ref.nSigma = trk->nSigmaKaon();
      m_kplusTracks.push_back(ref);
    }

    // Lambda daughters (min DCA)
    if (PassLambdaProtonCuts(trk, ctx.pv)) {
      TVector3 mom = trk->gMom();
      if (mom.Mag() < 1e-6) mom = pMom;
      ref.speciesKey = "proton";
      ref.p4.SetVectM(mom, kProtonMass);
      ref.nSigma = trk->nSigmaProton();
      m_protonTracks.push_back(ref);
    }
    // Hyper 2-body pi uses Lambda-like min DCA (same pool as Lambda pi, or hyper-specific)
    if (PassLambdaPionCuts(trk, ctx.pv) || PassHyperTwoBodyPionCuts(trk, ctx.pv)) {
      TVector3 mom = trk->gMom();
      if (mom.Mag() < 1e-6) mom = pMom;
      ref.speciesKey = "piminus";
      ref.p4.SetVectM(mom, kPionMass);
      ref.nSigma = trk->nSigmaPion();
      // Avoid duplicate entries for the same track
      Bool_t already = kFALSE;
      for (size_t j = 0; j < m_pionMinusTracks.size(); ++j) {
        if (m_pionMinusTracks[j].trackId == ref.trackId) {
          already = kTRUE;
          break;
        }
      }
      if (!already) m_pionMinusTracks.push_back(ref);
    }

    // Hyper 3-body light daughters: no min DCA
    if (PassHyperThreeBodyLightCuts(trk, +1)) {
      TVector3 mom = trk->gMom();
      if (mom.Mag() < 1e-6) mom = pMom;
      ref.speciesKey = "proton_loose";
      ref.p4.SetVectM(mom, kProtonMass);
      ref.nSigma = trk->nSigmaProton();
      m_protonLooseTracks.push_back(ref);
    }
    if (PassHyperThreeBodyLightCuts(trk, -1)) {
      TVector3 mom = trk->gMom();
      if (mom.Mag() < 1e-6) mom = pMom;
      ref.speciesKey = "piminus_loose";
      ref.p4.SetVectM(mom, kPionMass);
      ref.nSigma = trk->nSigmaPion();
      m_pionLooseTracks.push_back(ref);
    }
  }
}

void StPhiMesicNucleusMaker::SelectNuclearTracks(const EventContext& ctx) {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  for (Int_t i = 0; i < ctx.nTracks; ++i) {
    StPicoTrack* trk = mPicoDst->track(i);
    if (!trk || trk->charge() <= 0) continue;

    NuclearTrackState state;
    StNuclearIdHelper::FillFromPico(state, trk, mPicoDst);

    TrackRef ref;
    ref.trackId = trk->id();
    ref.trackIndex = i;
    ref.charge = trk->charge();
    ref.dcaToPv = trk->gDCA(ctx.pv.X(), ctx.pv.Y(), ctx.pv.Z());
    ref.m2 = state.mass2;
    ref.hasTof = state.tofMatch;
    ref.nSigma = 0.0;

    // Direct-channel nucleus (d/t): primary-like bachelor for parent PV origin
    if (PassTrackQuality(trk, ctx.pv) && PassPrimaryLikeBachelor(trk, ctx.pv)) {
      if (PassNuclearDedxPid(kNucDeuteron, trk)) {
        ref.speciesKey = "deuteron";
        ref.p4 = StNuclearIdHelper::NuclearP4(trk->pMom(), kNucDeuteron);
        ref.nSigma = StNuclearIdHelper::GetNSigma(kNucDeuteron, trk->pMom().Mag(), trk->dEdx());
        m_deuteronTracks.push_back(ref);
      }
      if (PassNuclearDedxPid(kNucTriton, trk)) {
        ref.speciesKey = "triton";
        ref.p4 = StNuclearIdHelper::NuclearP4(trk->pMom(), kNucTriton);
        ref.nSigma = StNuclearIdHelper::GetNSigma(kNucTriton, trk->pMom().Mag(), trk->dEdx());
        m_tritonTracks.push_back(ref);
      }
    }

    // Hyper 3-body deuteron: nHits + nSigma only (no DCA)
    if (PassNuclearDedxPid(kNucDeuteron, trk) &&
        trk->nHitsFit() >= pmn.hyperTwoBodyTrackCuts.minNHitsFit &&
        (trk->nHitsMax() <= 0 ||
         (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() >= pmn.hyperTwoBodyTrackCuts.minNHitsRatio)) {
      ref.speciesKey = "deuteron_loose";
      ref.p4 = StNuclearIdHelper::NuclearP4(trk->pMom(), kNucDeuteron);
      ref.nSigma = StNuclearIdHelper::GetNSigma(kNucDeuteron, trk->pMom().Mag(), trk->dEdx());
      m_deuteronLooseTracks.push_back(ref);
    }

    // Hyper 2-body nucleus (He3/He4): secondary min DCA, no requirePrimary
    if (PassHyperTwoBodyNuclearCuts(trk, ctx.pv)) {
      if (PassNuclearDedxPid(kNucHe3, trk)) {
        ref.speciesKey = "he3";
        ref.p4 = StNuclearIdHelper::NuclearP4(trk->pMom(), kNucHe3);
        ref.nSigma = StNuclearIdHelper::GetNSigma(kNucHe3, trk->pMom().Mag(), trk->dEdx());
        m_he3Tracks.push_back(ref);
      }
      if (PassNuclearDedxPid(kNucHe4, trk)) {
        ref.speciesKey = "he4";
        ref.p4 = StNuclearIdHelper::NuclearP4(trk->pMom(), kNucHe4);
        ref.nSigma = StNuclearIdHelper::GetNSigma(kNucHe4, trk->pMom().Mag(), trk->dEdx());
        m_he4Tracks.push_back(ref);
      }
    }
  }
}

Bool_t StPhiMesicNucleusMaker::ReconstructLambda(const TrackRef& proton,
                                                const TrackRef& pion,
                                                const EventContext& ctx,
                                                LambdaCandidate& out) {
  StPicoTrack* trkP = mPicoDst->track(proton.trackIndex);
  StPicoTrack* trkPi = mPicoDst->track(pion.trackIndex);
  if (!trkP || !trkPi) return kFALSE;

  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();

  // Helix from global momentum (same as StLambdaMaker)
  StPhysicalHelixD hp(StThreeVectorF(trkP->gMom().X(), trkP->gMom().Y(), trkP->gMom().Z()),
                      StThreeVectorF(trkP->origin().X(), trkP->origin().Y(), trkP->origin().Z()),
                      ctx.bField * units::kilogauss, (Float_t)trkP->charge());
  StPhysicalHelixD hpi(StThreeVectorF(trkPi->gMom().X(), trkPi->gMom().Y(), trkPi->gMom().Z()),
                       StThreeVectorF(trkPi->origin().X(), trkPi->origin().Y(), trkPi->origin().Z()),
                       ctx.bField * units::kilogauss, (Float_t)trkPi->charge());

  std::pair<Double_t, Double_t> s = hp.pathLengths(hpi);
  // maxPathLength cut
  if (TMath::Abs(s.first) > lam.maxPathLength || TMath::Abs(s.second) > lam.maxPathLength) return kFALSE;

  StThreeVectorD dcaA = hp.at(s.first);
  StThreeVectorD dcaB = hpi.at(s.second);
  TVector3 v0((dcaA.x() + dcaB.x()) * 0.5, (dcaA.y() + dcaB.y()) * 0.5, (dcaA.z() + dcaB.z()) * 0.5);
  Double_t dca12 = (dcaA - dcaB).mag();
  // maxDaughterDCA cut
  if (dca12 < 0.0 || dca12 > lam.maxDaughterDCA) return kFALSE;

  StThreeVectorD pPAt = hp.momentumAt(s.first, ctx.bField * units::kilogauss);
  StThreeVectorD pPiAt = hpi.momentumAt(s.second, ctx.bField * units::kilogauss);
  TLorentzVector lp, lpi;
  lp.SetXYZM(pPAt.x(), pPAt.y(), pPAt.z(), kProtonMass);
  lpi.SetXYZM(pPiAt.x(), pPiAt.y(), pPiAt.z(), kPionMass);
  TLorentzVector lLambda = lp + lpi;

  TVector3 pLambda = lLambda.Vect();
  if (pLambda.Mag() < 1e-5) return kFALSE;
  TVector3 flight = v0 - ctx.pv;
  Double_t dcaV0 = (ctx.pv - v0).Cross(pLambda.Unit()).Mag();
  // maxDCAV0 cut
  if (dcaV0 > lam.maxDCAV0) return kFALSE;

  Double_t cosPointing = (flight.Mag() > 0.0)
                             ? flight.Dot(pLambda) / (flight.Mag() * pLambda.Mag())
                             : -2.0;
  // minCosPointing cut
  if (cosPointing < lam.minCosPointing) return kFALSE;

  out.p4 = lLambda;
  out.invMass = lLambda.M();
  out.decayVertex = v0;
  out.protonTrackId = proton.trackId;
  out.protonTrackIndex = proton.trackIndex;
  out.pionTrackId = pion.trackId;
  out.pionTrackIndex = pion.trackIndex;
  out.daughterDca = dca12;
  out.v0DcaToPv = dcaV0;
  out.decayLength = flight.Mag();
  out.cosPointing = cosPointing;
  out.rapidity = lLambda.Rapidity();
  out.pt = lLambda.Pt();
  // All LambdaCutConfig topology cuts already applied above
  out.passTopology = kTRUE;
  out.passSignal = (out.invMass >= pmn.lambdaMassWindow.signalMin && out.invMass <= pmn.lambdaMassWindow.signalMax);
  out.passLeftSideband =
      (out.invMass >= pmn.lambdaMassWindow.leftSbMin && out.invMass <= pmn.lambdaMassWindow.leftSbMax);
  out.passRightSideband =
      (out.invMass >= pmn.lambdaMassWindow.rightSbMin && out.invMass <= pmn.lambdaMassWindow.rightSbMax);
  return kTRUE;
}

void StPhiMesicNucleusMaker::BuildLambdaCandidates(const EventContext& ctx) {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  for (size_t ip = 0; ip < m_protonTracks.size(); ++ip) {
    for (size_t ii = 0; ii < m_pionMinusTracks.size(); ++ii) {
      if (m_protonTracks[ip].trackId == m_pionMinusTracks[ii].trackId) continue;
      LambdaCandidate lc;
      if (!ReconstructLambda(m_protonTracks[ip], m_pionMinusTracks[ii], ctx, lc)) continue;
      if (lc.invMass < pmn.lambdaMassWindow.rawMin || lc.invMass > pmn.lambdaMassWindow.rawMax) continue;
      m_lambdas.push_back(lc);
      if ((Int_t)m_lambdas.size() >= pmn.maxLambdaCandidates) return;
    }
  }
}

void StPhiMesicNucleusMaker::BuildHyperCandidates(const EventContext& ctx) {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  for (size_t in = 0; in < m_he3Tracks.size(); ++in) {
    for (size_t ipi = 0; ipi < m_pionMinusTracks.size(); ++ipi) {
      if (m_he3Tracks[in].trackId == m_pionMinusTracks[ipi].trackId) continue;
      HyperCandidate hc;
      if (!StHypernucleusReconstructor::BuildTwoBody(
              mPicoDst->track(m_he3Tracks[in].trackIndex), kNucHe3, m_he3Tracks[in].trackIndex,
              mPicoDst->track(m_pionMinusTracks[ipi].trackIndex), m_pionMinusTracks[ipi].trackIndex, ctx.pv, ctx.bField,
              ToTopo(pmn.hypertriton2BodyTopology), ToWindow(pmn.hypertriton2BodyMassWindow), hc)) {
        continue;
      }
      if (!hc.passTopology) continue;
      if (hc.rawMass >= pmn.hypertriton2BodyMassWindow.rawMin && hc.rawMass <= pmn.hypertriton2BodyMassWindow.rawMax) {
        m_hypertriton2Body.push_back(hc);
        if ((Int_t)m_hypertriton2Body.size() >= pmn.maxHyperCandidates) return;
      }
    }
  }

  for (size_t in = 0; in < m_he4Tracks.size(); ++in) {
    for (size_t ipi = 0; ipi < m_pionMinusTracks.size(); ++ipi) {
      if (m_he4Tracks[in].trackId == m_pionMinusTracks[ipi].trackId) continue;
      HyperCandidate hc;
      if (!StHypernucleusReconstructor::BuildTwoBody(
              mPicoDst->track(m_he4Tracks[in].trackIndex), kNucHe4, m_he4Tracks[in].trackIndex,
              mPicoDst->track(m_pionMinusTracks[ipi].trackIndex), m_pionMinusTracks[ipi].trackIndex, ctx.pv, ctx.bField,
              ToTopo(pmn.hyperhydrogen4Topology), ToWindow(pmn.hyperhydrogen4MassWindow), hc)) {
        continue;
      }
      if (!hc.passTopology) continue;
      if (hc.rawMass >= pmn.hyperhydrogen4MassWindow.rawMin && hc.rawMass <= pmn.hyperhydrogen4MassWindow.rawMax) {
        m_hyperhydrogen4.push_back(hc);
        if ((Int_t)m_hyperhydrogen4.size() >= pmn.maxHyperCandidates) return;
      }
    }
  }

  // 3-body hyper: no Lambda-style DCA; use loose d/p/pi pools
  for (size_t id = 0; id < m_deuteronLooseTracks.size(); ++id) {
    for (size_t ip = 0; ip < m_protonLooseTracks.size(); ++ip) {
      if (m_deuteronLooseTracks[id].trackId == m_protonLooseTracks[ip].trackId) continue;
      for (size_t ipi = 0; ipi < m_pionLooseTracks.size(); ++ipi) {
        if (m_pionLooseTracks[ipi].trackId == m_deuteronLooseTracks[id].trackId ||
            m_pionLooseTracks[ipi].trackId == m_protonLooseTracks[ip].trackId)
          continue;
        HyperCandidate hc;
        if (!StHypernucleusReconstructor::BuildThreeBodyDaughter(
                m_deuteronLooseTracks[id].p4, m_deuteronLooseTracks[id].trackId, m_deuteronLooseTracks[id].trackIndex,
                m_protonLooseTracks[ip].p4, m_protonLooseTracks[ip].trackId, m_protonLooseTracks[ip].trackIndex,
                m_pionLooseTracks[ipi].p4, m_pionLooseTracks[ipi].trackId, m_pionLooseTracks[ipi].trackIndex, ctx.pv,
                ToTopo(pmn.hypertriton3BodyTopology), ToWindow(pmn.hypertriton3BodyMassWindow), hc)) {
          continue;
        }
        if (!hc.passTopology) continue;
        if (hc.rawMass >= pmn.hypertriton3BodyMassWindow.rawMin &&
            hc.rawMass <= pmn.hypertriton3BodyMassWindow.rawMax) {
          m_hypertriton3Body.push_back(hc);
          if ((Int_t)m_hypertriton3Body.size() >= pmn.maxHyperCandidates) return;
        }
      }
    }
  }
}

Bool_t StPhiMesicNucleusMaker::RejectSharedTracks(const std::vector<Int_t>& ids) const {
  std::set<Int_t> uniq;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (uniq.count(ids[i])) return kTRUE;
    uniq.insert(ids[i]);
  }
  return kFALSE;
}

void StPhiMesicNucleusMaker::BuildParentCandidates(const EventContext& ctx) {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  Int_t nComb = 0;
  for (size_t ic = 0; ic < pmn.Channels().size(); ++ic) {
    const PhiMesicNucleusConfig::ChannelDef& ch = pmn.Channels()[ic];
    if (!ch.enabled) continue;

    if (ch.key == "phiP_direct") {
      for (size_t il = 0; il < m_lambdas.size(); ++il) {
        for (size_t ik = 0; ik < m_kplusTracks.size(); ++ik) {
          if (!PassParentPvOrigin(m_kplusTracks[ik])) continue;
          ParentCandidate pc;
          pc.channelKey = ch.key;
          pc.p4 = m_lambdas[il].p4 + m_kplusTracks[ik].p4;
          pc.mass = pc.p4.M();
          pc.deltaM = pc.mass - ch.thresholdMass;
          pc.pt = pc.p4.Pt();
          pc.rapidity = pc.p4.Rapidity();
          pc.intermediateMass = m_lambdas[il].invMass;
          pc.daughterTrackIds.push_back(m_lambdas[il].protonTrackId);
          pc.daughterTrackIds.push_back(m_lambdas[il].pionTrackId);
          pc.daughterTrackIds.push_back(m_kplusTracks[ik].trackId);
          if (RejectSharedTracks(pc.daughterTrackIds)) continue;
          if (pc.mass < ch.parentRange.rawMin || pc.mass > ch.parentRange.rawMax) continue;
          pc.passSignal = m_lambdas[il].passSignal;
          pc.passLeftSideband = m_lambdas[il].passLeftSideband;
          pc.passRightSideband = m_lambdas[il].passRightSideband;
          m_parents.push_back(pc);
          if (++nComb >= pmn.maxParentCombinations) return;
        }
      }
      continue;
    }

    // phi3He_direct = Lambda + K+ + d ; phi4He_direct = Lambda + K+ + t
    // Parent PV origin: K+ and nucleus are primary-like (no Lambda min DCA on them).
    const std::vector<TrackRef>* nuc = 0;
    if (ch.key == "phi3He_direct") nuc = &m_deuteronTracks;
    if (ch.key == "phi4He_direct") nuc = &m_tritonTracks;
    if (nuc) {
      for (size_t il = 0; il < m_lambdas.size(); ++il) {
        for (size_t ik = 0; ik < m_kplusTracks.size(); ++ik) {
          if (!PassParentPvOrigin(m_kplusTracks[ik])) continue;
          for (size_t in = 0; in < nuc->size(); ++in) {
            if (!PassParentPvOrigin((*nuc)[in])) continue;
            ParentCandidate pc;
            pc.channelKey = ch.key;
            pc.p4 = m_lambdas[il].p4 + m_kplusTracks[ik].p4 + (*nuc)[in].p4;
            pc.mass = pc.p4.M();
            pc.deltaM = pc.mass - ch.thresholdMass;
            pc.pt = pc.p4.Pt();
            pc.rapidity = pc.p4.Rapidity();
            pc.intermediateMass = m_lambdas[il].invMass;
            pc.daughterTrackIds.push_back(m_lambdas[il].protonTrackId);
            pc.daughterTrackIds.push_back(m_lambdas[il].pionTrackId);
            pc.daughterTrackIds.push_back(m_kplusTracks[ik].trackId);
            pc.daughterTrackIds.push_back((*nuc)[in].trackId);
            if (RejectSharedTracks(pc.daughterTrackIds)) continue;
            if (pc.mass < ch.parentRange.rawMin || pc.mass > ch.parentRange.rawMax) continue;
            pc.passSignal = m_lambdas[il].passSignal;
            pc.passLeftSideband = m_lambdas[il].passLeftSideband;
            pc.passRightSideband = m_lambdas[il].passRightSideband;
            m_parents.push_back(pc);
            if (++nComb >= pmn.maxParentCombinations) return;
          }
        }
      }
      continue;
    }

    const std::vector<HyperCandidate>* hypers = 0;
    if (ch.key == "phi3He_hypertriton_2body") hypers = &m_hypertriton2Body;
    if (ch.key == "phi3He_hypertriton_3body") hypers = &m_hypertriton3Body;
    if (ch.key == "phi4He_hyperhydrogen") hypers = &m_hyperhydrogen4;
    if (!hypers) continue;

    for (size_t ih = 0; ih < hypers->size(); ++ih) {
      for (size_t ik = 0; ik < m_kplusTracks.size(); ++ik) {
        // Parent = hyper + K+: require K+ (bachelor) PV origin
        if (!PassParentPvOrigin(m_kplusTracks[ik])) continue;
        ParentCandidate pc;
        pc.channelKey = ch.key;
        pc.p4 = (*hypers)[ih].motherP4 + m_kplusTracks[ik].p4;
        pc.mass = pc.p4.M();
        pc.deltaM = pc.mass - ch.thresholdMass;
        pc.pt = pc.p4.Pt();
        pc.rapidity = pc.p4.Rapidity();
        pc.intermediateMass = (*hypers)[ih].rawMass;
        for (size_t id = 0; id < (*hypers)[ih].daughters.size(); ++id) {
          pc.daughterTrackIds.push_back((*hypers)[ih].daughters[id].trackId);
        }
        pc.daughterTrackIds.push_back(m_kplusTracks[ik].trackId);
        if (RejectSharedTracks(pc.daughterTrackIds)) continue;
        if (pc.mass < ch.parentRange.rawMin || pc.mass > ch.parentRange.rawMax) continue;
        pc.passSignal = (*hypers)[ih].passSignal;
        pc.passLeftSideband = (*hypers)[ih].passLeftSideband;
        pc.passRightSideband = (*hypers)[ih].passRightSideband;
        m_parents.push_back(pc);
        if (++nComb >= pmn.maxParentCombinations) return;
      }
    }
  }
}

Int_t StPhiMesicNucleusMaker::ComputeVzBin(Double_t vz, Double_t minVz, Double_t maxVz, Int_t nBins) {
  if (nBins <= 1) return 0;
  if (vz < minVz || vz > maxVz) return -1;
  const Double_t width = (maxVz - minVz) / nBins;
  Int_t idx = (Int_t)((vz - minVz) / width);
  if (idx < 0) idx = 0;
  if (idx >= nBins) idx = nBins - 1;
  return idx;
}

std::string StPhiMesicNucleusMaker::BuildMixingKey(Int_t cent9, Int_t vzBin) {
  std::ostringstream oss;
  oss << cent9 << "_" << vzBin;
  return oss.str();
}

void StPhiMesicNucleusMaker::StoreEventForMixing(const EventContext& ctx) {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  const Int_t vzBin = ComputeVzBin(ctx.pv.Z(), ev.minVz, ev.maxVz, pmn.mixingVzBins);
  if (vzBin < 0) return;
  std::string key = BuildMixingKey(ctx.cent9, vzBin);

  EventSnapshot snap;
  snap.eventId = ctx.eventId;
  snap.cent9 = ctx.cent9;
  snap.vzBin = vzBin;
  snap.kplus = m_kplusTracks;
  snap.lambdas = m_lambdas;
  snap.hyper2Body = m_hypertriton2Body;
  snap.hyper3Body = m_hypertriton3Body;
  snap.hyper4 = m_hyperhydrogen4;

  std::deque<EventSnapshot>& q = m_mixingPool[key];
  q.push_back(snap);
  while ((Int_t)q.size() > pmn.mixingDepth) q.pop_front();
}

void StPhiMesicNucleusMaker::FillMixedEventParents(const EventContext& ctx) {
  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  const Int_t vzBin = ComputeVzBin(ctx.pv.Z(), ev.minVz, ev.maxVz, pmn.mixingVzBins);
  if (vzBin < 0) return;
  std::string key = BuildMixingKey(ctx.cent9, vzBin);
  std::map<std::string, std::deque<EventSnapshot> >::iterator itPool = m_mixingPool.find(key);
  if (itPool == m_mixingPool.end()) return;

  const std::deque<EventSnapshot>& pool = itPool->second;
  for (size_t ie = 0; ie < pool.size(); ++ie) {
    if (pmn.mixingRequireDifferentEventId && pool[ie].eventId == ctx.eventId) continue;
    for (size_t ic = 0; ic < pmn.Channels().size(); ++ic) {
      const PhiMesicNucleusConfig::ChannelDef& ch = pmn.Channels()[ic];
      if (!ch.enabled || !ch.enableMixedEvent) continue;

      if (ch.key == "phiP_direct") {
        for (size_t il = 0; il < m_lambdas.size(); ++il) {
          for (size_t ik = 0; ik < pool[ie].kplus.size(); ++ik) {
            TLorentzVector parent = m_lambdas[il].p4 + pool[ie].kplus[ik].p4;
            if (m_histManager) {
              m_histManager->Fill("hParentMass_ME_phiP_direct", parent.M());
              m_histManager->Fill("hParentDeltaM_ME_phiP_direct", parent.M() - ch.thresholdMass);
            }
          }
        }
      }
    }
  }
}

void StPhiMesicNucleusMaker::FillQaAndPhysicsHists(const EventContext& ctx) {
  if (!m_histManager) return;

  PhiMesicNucleusConfig& pmn = PhiMesicNucleusConfig::GetInstance();
  Bool_t enPhiP = kFALSE;
  Bool_t enPhi3He3 = kFALSE;
  Bool_t enPhi4He3 = kFALSE;
  Bool_t enPhi3He2 = kFALSE;
  Bool_t enPhi4He2 = kFALSE;
  for (size_t ic = 0; ic < pmn.Channels().size(); ++ic) {
    const PhiMesicNucleusConfig::ChannelDef& ch = pmn.Channels()[ic];
    if (!ch.enabled) continue;
    if (ch.key == "phiP_direct") enPhiP = kTRUE;
    if (ch.key == "phi3He_direct") enPhi3He3 = kTRUE;
    if (ch.key == "phi4He_direct") enPhi4He3 = kTRUE;
    if (ch.key == "phi3He_hypertriton_2body") enPhi3He2 = kTRUE;
    if (ch.key == "phi4He_hyperhydrogen") enPhi4He2 = kTRUE;
  }

  m_histManager->Fill("hNEvents", 0.5);
  m_histManager->Fill("hNTracks", (Double_t)ctx.nTracks);
  if (ctx.cent9 >= 0) m_histManager->Fill("hCent9", (Double_t)ctx.cent9);
  m_histManager->Fill("hVz", ctx.pv.Z());

  m_histManager->Fill("hNKplus_PrePid", (Double_t)m_kplusTracks.size());
  m_histManager->Fill("hNProton_PrePid", (Double_t)m_protonTracks.size());
  m_histManager->Fill("hNPionMinus_PrePid", (Double_t)m_pionMinusTracks.size());

  for (size_t i = 0; i < m_lambdas.size(); ++i) {
    m_histManager->Fill("hLambdaMass_Raw", m_lambdas[i].invMass);
    if (m_lambdas[i].passTopology) m_histManager->Fill("hLambdaMass_AfterTopology", m_lambdas[i].invMass);
    if (m_lambdas[i].passSignal) m_histManager->Fill("hLambdaMass_SignalWindow", m_lambdas[i].invMass);
    if (m_lambdas[i].passLeftSideband) m_histManager->Fill("hLambdaMass_LeftSideband", m_lambdas[i].invMass);
    if (m_lambdas[i].passRightSideband) m_histManager->Fill("hLambdaMass_RightSideband", m_lambdas[i].invMass);
    // Channel PID QA: Lambda mass independent of parent acceptance
    if (enPhiP) m_histManager->Fill("hPhiP_LambdaMass", m_lambdas[i].invMass);
    if (enPhi3He3) m_histManager->Fill("hPhi3He3Body_LambdaMass", m_lambdas[i].invMass);
    if (enPhi4He3) m_histManager->Fill("hPhi4He3Body_LambdaMass", m_lambdas[i].invMass);
  }
  for (size_t i = 0; i < m_hypertriton2Body.size(); ++i) {
    m_histManager->Fill("hHypertriton2BodyMass_Raw", m_hypertriton2Body[i].rawMass);
    if (enPhi3He2) m_histManager->Fill("hPhi3He2Body_HypertritonMass", m_hypertriton2Body[i].rawMass);
  }
  for (size_t i = 0; i < m_hypertriton3Body.size(); ++i) {
    m_histManager->Fill("hHypertriton3BodyMass_Raw", m_hypertriton3Body[i].rawMass);
  }
  for (size_t i = 0; i < m_hyperhydrogen4.size(); ++i) {
    m_histManager->Fill("hHyperhydrogen4Mass_Raw", m_hyperhydrogen4[i].rawMass);
    if (enPhi4He2) m_histManager->Fill("hPhi4He2Body_HyperhydrogenMass", m_hyperhydrogen4[i].rawMass);
  }

  // Channel PID QA: dEdx-selected species with TOF hit (m2), independent of parent mass cut
  for (size_t i = 0; i < m_kplusTracks.size(); ++i) {
    if (!m_kplusTracks[i].hasTof) continue;
    if (enPhiP) m_histManager->Fill("hPhiP_KplusM2", m_kplusTracks[i].m2);
    if (enPhi3He3) m_histManager->Fill("hPhi3He3Body_KplusM2", m_kplusTracks[i].m2);
    if (enPhi3He2) m_histManager->Fill("hPhi3He2Body_KplusM2", m_kplusTracks[i].m2);
    if (enPhi4He3) m_histManager->Fill("hPhi4He3Body_KplusM2", m_kplusTracks[i].m2);
    if (enPhi4He2) m_histManager->Fill("hPhi4He2Body_KplusM2", m_kplusTracks[i].m2);
  }
  if (enPhi3He3) {
    for (size_t i = 0; i < m_deuteronTracks.size(); ++i) {
      if (m_deuteronTracks[i].hasTof) m_histManager->Fill("hPhi3He3Body_DeuteronM2", m_deuteronTracks[i].m2);
    }
  }
  if (enPhi4He3) {
    for (size_t i = 0; i < m_tritonTracks.size(); ++i) {
      if (m_tritonTracks[i].hasTof) m_histManager->Fill("hPhi4He3Body_TritonM2", m_tritonTracks[i].m2);
    }
  }

  for (size_t i = 0; i < m_parents.size(); ++i) {
    const ParentCandidate& pc = m_parents[i];
    m_histManager->Fill(("hParentMass_SE_" + pc.channelKey).c_str(), pc.mass);
    m_histManager->Fill(("hParentDeltaM_SE_" + pc.channelKey).c_str(), pc.deltaM);
    if (pc.passSignal) {
      m_histManager->Fill(("hParentMass_Signal_" + pc.channelKey).c_str(), pc.mass);
      m_histManager->Fill(("hParentDeltaM_Signal_" + pc.channelKey).c_str(), pc.deltaM);
    }
    if (pc.passLeftSideband) {
      m_histManager->Fill(("hParentMass_LeftSB_" + pc.channelKey).c_str(), pc.mass);
      m_histManager->Fill(("hParentDeltaM_LeftSB_" + pc.channelKey).c_str(), pc.deltaM);
    }
    if (pc.passRightSideband) {
      m_histManager->Fill(("hParentMass_RightSB_" + pc.channelKey).c_str(), pc.mass);
      m_histManager->Fill(("hParentDeltaM_RightSB_" + pc.channelKey).c_str(), pc.deltaM);
    }
  }
}
