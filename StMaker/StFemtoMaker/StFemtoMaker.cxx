#include "StFemtoMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "kinematics.h"
#include "cuts/EventCutConfig.h"
#include "cuts/TrackCutConfig.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "cuts/MixingConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/FemtoConfig.h"
#include "cuts/NuclearIdCutConfig.h"
#include "CentralityHelper.h"
#include "StPhiKKReconstruction.h"
#include "StNuclearIdHelper.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/SystemOfUnits.h"

#include "TFile.h"
#include "TH1.h"
#include "TString.h"
#include "TMath.h"
#include "TSystem.h"
#include "TVector3.h"
#include "TVector2.h"
#include "TLorentzVector.h"
#include "TRandom.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace {
const Double_t kProtonMass = 0.938272;
const Double_t kKaonMass = 0.493677;
const Double_t kPhiMass = 1.019461;

Bool_t ComputePhiBetaGamma(Bool_t tofPlus, Float_t betaPlus, Bool_t tofMinus, Float_t betaMinus, Float_t& betaGamma) {
  betaGamma = -1.0f;
  if (!tofPlus || !tofMinus) return kFALSE;
  if (betaPlus <= 1e-4f || betaMinus <= 1e-4f) return kFALSE;
  const Double_t beta = 0.5 * ((Double_t)betaPlus + (Double_t)betaMinus);
  if (beta >= 1.0) return kFALSE;
  const Double_t gamma = 1.0 / TMath::Sqrt(1.0 - beta * beta);
  betaGamma = (Float_t)(beta * gamma);
  return kTRUE;
}
}  // namespace

StFemtoMaker::StFemtoMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName)
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
      m_centralityPercent(-1.0),
      m_psi2(-1.0) {}

StFemtoMaker::~StFemtoMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

StFemtoMaker* createStFemtoMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StFemtoMaker(name, picoMaker, outName);
}

extern "C" void* createStFemtoMakerC(const char* name, void* picoMaker, const char* outName) {
  return (void*)createStFemtoMaker(name, (StPicoDstMaker*)picoMaker, outName);
}

Int_t StFemtoMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StFemtoMaker] GetHistConfigPath() returned empty; no histograms will be filled." << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StFemtoMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStOK;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StFemtoMaker] CentralityHelper init failed" << std::endl;
  }

  if (!ConfigManager::GetInstance().GetPhiCuts().FinalizeRapidityFrame(
          ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StFemtoMaker] FinalizeRapidityFrame failed; fix maker/centrality YAML." << std::endl;
    return kStErr;
  }

  if (!ConfigManager::GetInstance().GetFemtoConfig().Validate()) {
    std::cerr << "[StFemtoMaker] FemtoConfig validation failed." << std::endl;
    return kStErr;
  }

  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  if (femtoCfg.rotationEnabled && femtoCfg.rotationSeed != 0) {
    gRandom->SetSeed(femtoCfg.rotationSeed);
  }

  return kStOK;
}

void StFemtoMaker::Clear(Option_t* opt) {
  (void)opt;
  m_eventCandidates.clear();
  m_phiQaLoose.clear();
  m_phiQaPreMassLoose.clear();
  m_phiQaPreMassTofStrict.clear();
}

Int_t StFemtoMaker::Make() {
  if (!mPicoDstMaker) return kStWarn;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;

  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  mEventCounter++;
  m_eventCandidates.clear();
  m_phiQaLoose.clear();
  m_phiQaPreMassLoose.clear();
  m_phiQaPreMassTofStrict.clear();

  TVector3 pVtx = event->primaryVertex();
  Float_t vzVpd = event->vzVpd();
  Int_t refMult = event->refMult();
  EventCutConfig& evCuts = ConfigManager::GetInstance().GetEventCuts();
  Float_t vr = evCuts.ComputeVr(pVtx.X(), pVtx.Y());
  const Double_t vz = pVtx.Z();
  const Int_t runId = event->runId();
  const Int_t nBTOFMatch = event->nBTOFMatch();

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
  m_psi2 = -1.0;

  if (m_histManager) {
    m_histManager->Fill("hRefMultVsNTOFMatch", (Double_t)nBTOFMatch, (Double_t)rawMult);
    m_histManager->Fill("hVz", pVtx.Z());
    m_histManager->Fill("hVr", vr);
    m_histManager->Fill("hVxVy", pVtx.X(), pVtx.Y());
    m_histManager->Fill("hRefMult", refMult);
    m_histManager->Fill("hVzVsRun", (Double_t)event->runId(), pVtx.Z());
    m_histManager->Fill("hRefMultVsVz", pVtx.Z(), refMult);
    EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
    if (TMath::Abs(vzVpd) < ev.maxAbsVzVpd) {
      m_histManager->Fill("hVzDiff", pVtx.Z() - vzVpd);
    }
    std::vector<unsigned int> triggerIds = event->triggerIds();
    for (size_t i = 0; i < triggerIds.size(); i++) {
      m_histManager->Fill("hTriggerIds", triggerIds[i]);
    }
  }

  CentralityRejectReason centReason = kCentralityOk;
  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(runId, centReason)) return kStOK;
  }

  if (!PassEventCuts(pVtx.Z(), vr, refMult, vzVpd)) return kStOK;

  if (m_histManager) {
    m_histManager->Fill("hVz_After", pVtx.Z());
    m_histManager->Fill("hVr_After", vr);
    m_histManager->Fill("hVxVy_After", pVtx.X(), pVtx.Y());
    m_histManager->Fill("hRefMult_After", refMult);
    EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
    if (TMath::Abs(vzVpd) < ev.maxAbsVzVpd) {
      m_histManager->Fill("hVzDiff_After", pVtx.Z() - vzVpd);
    }
  }

  if (m_histManager && centCfg.fillCentralityQA) {
    m_histManager->Fill("hRawMult", (Double_t)rawMult);
  }

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) return kStOK;
    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight, centReason)) {
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

  PhiCutConfig& phiCfg = ConfigManager::GetInstance().GetPhiCuts();
  const Int_t kMaxTracks = 4000;
  std::vector<TrackState> kaonsPlus;
  std::vector<TrackState> kaonsMinus;
  std::vector<TrackState> kaonMinusTracks;
  std::vector<TrackState> protons;
  std::vector<He4TrackState> he4Tracks;
  std::vector<DeuteronTrackState> deuteronTracks;
  std::vector<TritonTrackState> tritonTracks;
  std::vector<He3TrackState> he3Tracks;
  kaonsPlus.reserve(500);
  kaonsMinus.reserve(500);
  kaonMinusTracks.reserve(500);
  protons.reserve(500);
  he4Tracks.reserve(100);
  deuteronTracks.reserve(100);
  tritonTracks.reserve(100);
  he3Tracks.reserve(100);

  const FemtoConfig& femtoCfgSpecies = ConfigManager::GetInstance().GetFemtoConfig();
  const Bool_t needProton = (femtoCfgSpecies.FindSpecies("proton") != nullptr);
  const Bool_t needKaonMinus = (femtoCfgSpecies.FindSpecies("kaon_minus") != nullptr);
  const Bool_t needHe4 = (femtoCfgSpecies.FindSpecies("he4") != nullptr);
  const Bool_t needDeuteron = (femtoCfgSpecies.FindSpecies("deuteron") != nullptr);
  const Bool_t needTriton = (femtoCfgSpecies.FindSpecies("triton") != nullptr);
  const Bool_t needHe3 = (femtoCfgSpecies.FindSpecies("he3") != nullptr);
  const Bool_t needPhi = (femtoCfgSpecies.FindSpecies("phi") != nullptr) ||
                         (femtoCfgSpecies.FindSpecies("phi_rot") != nullptr) ||
                         femtoCfgSpecies.rotationEnabled;
  const NuclearIdCutConfig& nucIdCfg = ConfigManager::GetInstance().GetNuclearIdCuts();

  Double_t Qx = 0.0, Qy = 0.0;
  Int_t nTracks = mPicoDst->numberOfTracks();
  if (m_histManager) m_histManager->Fill("hNTracks", (Double_t)nTracks);
  if (phiCfg.maxNTr > 0 && nTracks > phiCfg.maxNTr) return kStOK;

  for (Int_t itrk = 0; itrk < nTracks; itrk++) {
    StPicoTrack* trk = mPicoDst->track(itrk);
    if (!trk) continue;
    TVector3 pMom = trk->pMom();
    if (m_histManager) {
      Float_t ptRaw = pMom.Perp();
      Float_t etaRaw = pMom.PseudoRapidity();
      m_histManager->Fill("hPt_Raw", ptRaw);
      m_histManager->Fill("hEta_Raw", etaRaw);
      m_histManager->Fill("hPhi_Raw", pMom.Phi());
      m_histManager->Fill("hNHitsFit_Raw", trk->nHitsFit());
      if (trk->nHitsMax() > 0) {
        m_histManager->Fill("hNHitsRatio_Raw", (Float_t)trk->nHitsFit() / (Float_t)trk->nHitsMax());
      }
      m_histManager->Fill("hNHitsDedx_Raw", trk->nHitsDedx());
      m_histManager->Fill("hChi2_Raw", trk->chi2());
      m_histManager->Fill("hDCA_Raw", trk->gDCA(pVtx).Mag());
    }
    if (!PassTrackCuts(trk, pVtx)) continue;

    Float_t pt = pMom.Perp();
    Float_t eta = pMom.PseudoRapidity();
    Float_t phi = pMom.Phi();
    Int_t btofIndex = trk->bTofPidTraitsIndex();

    if (m_histManager) {
      m_histManager->Fill("hPt", pt);
      m_histManager->Fill("hEta", eta);
      m_histManager->Fill("hPhi", phi);
      m_histManager->Fill("hNHitsFit", trk->nHitsFit());
      m_histManager->Fill("hNHitsRatio", (Float_t)trk->nHitsFit() / (Float_t)trk->nHitsMax());
      m_histManager->Fill("hNHitsDedx", trk->nHitsDedx());
      m_histManager->Fill("hDCA", trk->gDCA(pVtx).Mag());
      m_histManager->Fill("hCharge", trk->charge());
      m_histManager->Fill("hChi2", trk->chi2());
      m_histManager->Fill("hDedxVsP", pMom.Mag(), trk->dEdx());
      m_histManager->Fill("hDedxVsPt", pt, trk->dEdx());
      m_histManager->Fill("hNSigmaPionVsP", pMom.Mag(), trk->nSigmaPion());
      m_histManager->Fill("hNSigmaPionVsPt", pt, trk->nSigmaPion());
      m_histManager->Fill("hNSigmaKaon_Raw", trk->nSigmaKaon());
      m_histManager->Fill("hNSigmaKaonVsP", pMom.Mag(), trk->nSigmaKaon());
      m_histManager->Fill("hNSigmaKaonVsPt", pt, trk->nSigmaKaon());
      m_histManager->Fill("hNSigmaProtonVsP", pMom.Mag(), trk->nSigmaProton());
      m_histManager->Fill("hNSigmaProtonVsPt", pt, trk->nSigmaProton());
      if (trk->charge() > 0) {
        m_histManager->Fill("hNSigmaProtonVsPt_Pos", pt, trk->nSigmaProton());
      }
      if (trk->charge() != 0) {
        Double_t pOverQ = pMom.Mag() / (Double_t)trk->charge();
        m_histManager->Fill("hDedxVsPq", pOverQ, trk->dEdx());
        if (btofIndex >= 0) {
          StPicoBTofPidTraits* tof = mPicoDst->btofPidTraits(btofIndex);
          if (tof) {
            Double_t beta = tof->btofBeta();
            if (beta > 1e-4) {
              Double_t mass2 = pMom.Mag2() * (1.0 / (beta * beta) - 1.0);
              m_histManager->Fill("hM2q2VsPq", pOverQ, mass2);
              Double_t pMag = pMom.Mag();
              m_histManager->Fill("hBetaVsP", pMag, 1.0 / beta);
              m_histManager->Fill("hBetaVsPt", pt, 1.0 / beta);
              m_histManager->Fill("hMass2VsP", pMag, mass2);
              m_histManager->Fill("hMass2VsPt", pt, mass2);
              Double_t deltaInvBeta =
                  DeltaOneOverBeta(beta, StPhiKKReconstruction::KaonMass(), pMom.Mag());
              if (TMath::Abs(deltaInvBeta) < 10.0) {
                m_histManager->Fill("hDeltaOneOverBetaKaon", deltaInvBeta);
                m_histManager->Fill("hDeltaOneOverBetaVsP", pMom.Mag(), deltaInvBeta);
              }
            }
          }
        }
      }
    }

    if (pt >= phiCfg.minPtEp && pt <= phiCfg.maxPtEp && TMath::Abs(eta) < phiCfg.maxEtaEp) {
      Qx += TMath::Cos(2.0 * phi);
      Qy += TMath::Sin(2.0 * phi);
    }

    if (needPhi && PassKaonCuts(trk, pVtx)) {
      TrackState kTrack;
      BuildTrackState(kTrack, trk, event, pVtx, itrk);
      FillTofInfo(kTrack, trk, pMom, btofIndex);
      if (m_histManager && kTrack.tofMatch) {
        m_histManager->Fill("hMass2VsP_TpcKaon", pMom.Mag(), kTrack.mass2);
        m_histManager->Fill("hMass2VsPt_TpcKaon", pt, kTrack.mass2);
      }
      if (IsKaon(kTrack)) {
        if (m_histManager) {
          m_histManager->Fill("hK_Pt", kTrack.pT);
          m_histManager->Fill("hK_Eta", kTrack.eta);
          m_histManager->Fill("hK_NSigma", kTrack.nSigmaKaon);
          if (kTrack.tofMatch) {
            m_histManager->Fill("hMass2VsP_IsKaon", pMom.Mag(), kTrack.mass2);
            m_histManager->Fill("hMass2VsPt_IsKaon", pt, kTrack.mass2);
          }
        }
        if (kTrack.charge > 0 && (Int_t)kaonsPlus.size() < kMaxTracks) {
          kaonsPlus.push_back(kTrack);
        } else if (kTrack.charge < 0 && (Int_t)kaonsMinus.size() < kMaxTracks) {
          kaonsMinus.push_back(kTrack);
        }
      }
    }

    if (needKaonMinus && PassKaonMinusBaseCuts(trk, pVtx)) {
      TrackState kTrack;
      BuildTrackState(kTrack, trk, event, pVtx, itrk);
      if (kTrack.charge >= 0) continue;
      FillTofInfo(kTrack, trk, pMom, btofIndex);
      if (!IsKaon(kTrack)) continue;
      if (kTrack.pT < femtoCfgSpecies.kaonMinusMinPtPre) continue;
      FillKaonMinusPreFemtoQa(kTrack);
      if (PassFemtoKaonMinusCuts(kTrack) && (Int_t)kaonMinusTracks.size() < kMaxTracks) {
        FillKaonMinusFemtoQa(kTrack);
        kaonMinusTracks.push_back(kTrack);
      }
    }

    if (needProton && PassProtonCuts(trk, pVtx)) {
      TrackState pTrack;
      BuildTrackState(pTrack, trk, event, pVtx, itrk);
      FillTofInfo(pTrack, trk, pMom, btofIndex);
      if (!IsProton(pTrack)) continue;
      const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
      Bool_t passCharge = kTRUE;
      if (femtoCfg.protonChargeMode == "positive") {
        passCharge = (pTrack.charge > 0);
      } else if (femtoCfg.protonChargeMode == "negative") {
        passCharge = (pTrack.charge < 0);
      }
      if (!passCharge) continue;
      if (pTrack.pT < femtoCfg.protonMinPtPre) continue;
      FillProtonPreFemtoQa(pTrack);
      if (PassFemtoProtonCuts(pTrack) && (Int_t)protons.size() < kMaxTracks) {
        FillProtonFemtoQa(pTrack);
        protons.push_back(pTrack);
      }
    }

    if (needHe4 && trk->nHitsDedx() >= nucIdCfg.minNHitsDedxNuclear) {
      NuclearTrackState nucState;
      StNuclearIdHelper::FillFromPico(nucState, trk, mPicoDst);
      if (nucState.dedx > 0 && m_histManager) {
        m_histManager->Fill("hNSigmaHe4VsP_All", pMom.Mag(),
                            StNuclearIdHelper::GetNSigma(kNucHe4, nucState.pMag, nucState.dedx));
      }
      if (StNuclearIdHelper::IsHe4(nucState)) {
        TrackState hTrack;
        BuildTrackState(hTrack, trk, event, pVtx, itrk);
        FillTofInfo(hTrack, trk, pMom, btofIndex);
        const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
        He4TrackState h4;
        h4.trk = hTrack;
        h4.nSigmaHe4 = (Float_t)StNuclearIdHelper::GetNSigma(kNucHe4, nucState.pMag, nucState.dedx);
        if (m_histManager) {
          m_histManager->Fill("hNSigmaHe4VsP", pMom.Mag(), h4.nSigmaHe4);
        }
        const Double_t he4Pmom = pMom.Mag();
        if (he4Pmom < femtoCfg.he4MinPMom || he4Pmom > femtoCfg.he4MaxPMom) continue;
        if (h4.trk.pT < femtoCfg.he4MinPtPre || h4.trk.pT > femtoCfg.he4MaxPtPre) continue;
        FillHe4PreFemtoQa(h4.trk, h4.nSigmaHe4);
        if (PassFemtoHe4Cuts(h4) && (Int_t)he4Tracks.size() < kMaxTracks) {
          FillHe4FemtoQa(h4.trk);
          he4Tracks.push_back(h4);
        }
      }
    }

    if (needDeuteron && trk->nHitsDedx() >= nucIdCfg.minNHitsDedxNuclear) {
      NuclearTrackState nucState;
      StNuclearIdHelper::FillFromPico(nucState, trk, mPicoDst);
      if (nucState.dedx > 0 && m_histManager) {
        m_histManager->Fill("hNSigmaDeuteronVsP_All", pMom.Mag(),
                            StNuclearIdHelper::GetNSigma(kNucDeuteron, nucState.pMag, nucState.dedx));
      }
      if (StNuclearIdHelper::IsDeuteron(nucState)) {
        TrackState dTrack;
        BuildTrackState(dTrack, trk, event, pVtx, itrk);
        FillTofInfo(dTrack, trk, pMom, btofIndex);
        const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
        DeuteronTrackState dState;
        dState.trk = dTrack;
        dState.nSigmaDeuteron =
            (Float_t)StNuclearIdHelper::GetNSigma(kNucDeuteron, nucState.pMag, nucState.dedx);
        if (m_histManager) {
          m_histManager->Fill("hNSigmaDeuteronVsP", pMom.Mag(), dState.nSigmaDeuteron);
        }
        const Double_t dPmom = pMom.Mag();
        if (dPmom < femtoCfg.deuteronMinPMom || dPmom > femtoCfg.deuteronMaxPMom) continue;
        if (dState.trk.pT < femtoCfg.deuteronMinPtPre || dState.trk.pT > femtoCfg.deuteronMaxPtPre) continue;
        FillDeuteronPreFemtoQa(dState.trk, dState.nSigmaDeuteron);
        if (PassFemtoDeuteronCuts(dState) && (Int_t)deuteronTracks.size() < kMaxTracks) {
          FillDeuteronFemtoQa(dState.trk);
          deuteronTracks.push_back(dState);
        }
      }
    }

    if (needTriton && trk->nHitsDedx() >= nucIdCfg.minNHitsDedxNuclear) {
      NuclearTrackState nucState;
      StNuclearIdHelper::FillFromPico(nucState, trk, mPicoDst);
      if (nucState.dedx > 0 && m_histManager) {
        m_histManager->Fill("hNSigmaTritonVsP_All", pMom.Mag(),
                            StNuclearIdHelper::GetNSigma(kNucTriton, nucState.pMag, nucState.dedx));
      }
      if (StNuclearIdHelper::IsTriton(nucState)) {
        TrackState tTrack;
        BuildTrackState(tTrack, trk, event, pVtx, itrk);
        FillTofInfo(tTrack, trk, pMom, btofIndex);
        const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
        TritonTrackState tState;
        tState.trk = tTrack;
        tState.nSigmaTriton =
            (Float_t)StNuclearIdHelper::GetNSigma(kNucTriton, nucState.pMag, nucState.dedx);
        if (m_histManager) {
          m_histManager->Fill("hNSigmaTritonVsP", pMom.Mag(), tState.nSigmaTriton);
        }
        const Double_t tPmom = pMom.Mag();
        if (tPmom < femtoCfg.tritonMinPMom || tPmom > femtoCfg.tritonMaxPMom) continue;
        if (tState.trk.pT < femtoCfg.tritonMinPtPre || tState.trk.pT > femtoCfg.tritonMaxPtPre) continue;
        FillTritonPreFemtoQa(tState.trk, tState.nSigmaTriton);
        if (PassFemtoTritonCuts(tState) && (Int_t)tritonTracks.size() < kMaxTracks) {
          FillTritonFemtoQa(tState.trk);
          tritonTracks.push_back(tState);
        }
      }
    }

    if (needHe3 && trk->nHitsDedx() >= nucIdCfg.minNHitsDedxNuclear) {
      NuclearTrackState nucState;
      StNuclearIdHelper::FillFromPico(nucState, trk, mPicoDst);
      if (nucState.dedx > 0 && m_histManager) {
        m_histManager->Fill("hNSigmaHe3VsP_All", pMom.Mag(),
                            StNuclearIdHelper::GetNSigma(kNucHe3, nucState.pMag, nucState.dedx));
      }
      if (StNuclearIdHelper::IsHe3(nucState)) {
        TrackState h3Track;
        BuildTrackState(h3Track, trk, event, pVtx, itrk);
        FillTofInfo(h3Track, trk, pMom, btofIndex);
        const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
        He3TrackState h3State;
        h3State.trk = h3Track;
        h3State.nSigmaHe3 = (Float_t)StNuclearIdHelper::GetNSigma(kNucHe3, nucState.pMag, nucState.dedx);
        if (m_histManager) {
          m_histManager->Fill("hNSigmaHe3VsP", pMom.Mag(), h3State.nSigmaHe3);
        }
        const Double_t h3Pmom = pMom.Mag();
        if (h3Pmom < femtoCfg.he3MinPMom || h3Pmom > femtoCfg.he3MaxPMom) continue;
        if (h3State.trk.pT < femtoCfg.he3MinPtPre || h3State.trk.pT > femtoCfg.he3MaxPtPre) continue;
        FillHe3PreFemtoQa(h3State.trk, h3State.nSigmaHe3);
        if (PassFemtoHe3Cuts(h3State) && (Int_t)he3Tracks.size() < kMaxTracks) {
          FillHe3FemtoQa(h3State.trk);
          he3Tracks.push_back(h3State);
        }
      }
    }
  }

  if (m_histManager) {
    m_histManager->Fill("hTofMatchMult", (Double_t)nBTOFMatch);
    if (needPhi) {
      m_histManager->Fill("hNKaonPlus", (Double_t)kaonsPlus.size());
      m_histManager->Fill("hNKaonMinus", (Double_t)kaonsMinus.size());
      m_histManager->Fill("hNKaonPlusVsNKaonMinus", (Double_t)kaonsPlus.size(), (Double_t)kaonsMinus.size());
    }
    if (needKaonMinus && m_histManager->Get("hNKaonMinusFemto")) {
      m_histManager->Fill("hNKaonMinusFemto", (Double_t)kaonMinusTracks.size());
    }
  }

  TVector2 Q(Qx, Qy);
  if (Q.Mod() > 0) {
    m_psi2 = 0.5 * TMath::ATan2(Qy, Qx);
    if (m_psi2 < 0) m_psi2 += TMath::Pi();
  }

  FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  for (std::map<std::string, FemtoConfig::SpeciesDef>::const_iterator it = femtoCfg.species.begin();
       it != femtoCfg.species.end(); ++it) {
    const FemtoConfig::SpeciesDef& sp = it->second;
    if (sp.builderType == "track") {
      BuildTrackPidCandidates(sp.key, sp.particleKey, protons, kaonMinusTracks, he4Tracks, deuteronTracks,
                              tritonTracks, he3Tracks, mEventCounter);
    } else if (sp.builderType == "resonance") {
      if (sp.particleKey == femtoCfg.rotationParticleKey) {
        BuildRotatedPhiCandidates(sp.key, kaonsPlus, kaonsMinus, mEventCounter);
      } else {
        BuildResonanceCandidates(sp.key, sp.particleKey, kaonsPlus, kaonsMinus, mEventCounter);
      }
    }
  }

  FillCandidateQA();
  FillPhiBachelorPairAngleQa();

  for (size_t ic = 0; ic < femtoCfg.channels.size(); ic++) {
    const FemtoConfig::ChannelDef& ch = femtoCfg.channels[ic];
    if (!ch.enabled) continue;
    FillSameEventPairs(ch);
    if (ch.doMixing) FillMixedEventPairs(ch, vz, m_cent9, m_psi2);
  }

  StoreEventForMixing(vz, m_cent9, m_psi2);

  Int_t nPhiCandidates = 0;
  FemtoCandidateStore::const_iterator itPhi = m_eventCandidates.find("phi");
  if (itPhi != m_eventCandidates.end()) nPhiCandidates = (Int_t)itPhi->second.size();

  if (m_histManager && centCfg.fillCentralityQA && m_cent9 >= 0) {
    const Int_t nKmForCent = needKaonMinus ? (Int_t)kaonMinusTracks.size() : (Int_t)kaonsMinus.size();
    FillCentralityEventQA(m_cent9, rawMult, m_refMultCorr, nTracks, nBTOFMatch, (Int_t)kaonsPlus.size(), nKmForCent,
                          nPhiCandidates, (Int_t)protons.size(), (Int_t)he4Tracks.size(), (Int_t)deuteronTracks.size(),
                          (Int_t)tritonTracks.size(), (Int_t)he3Tracks.size());
  }

  if (m_histManager) {
    TVector2 Q(Qx, Qy);
    m_histManager->Fill("hQxQy", Qx, Qy);
    if (Q.Mod() > 0) {
      Double_t psi2 = 0.5 * TMath::ATan2(Qy, Qx);
      if (psi2 < 0) psi2 += TMath::Pi();
      m_histManager->Fill("hPsi2", psi2);
    }
    m_histManager->Fill("hN", 0);
  }
  return kStOK;
}

Int_t StFemtoMaker::Finish() {
  if (mOutName != "") {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    if (m_histManager) m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StFemtoMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    m_centrality->Finish();
  }
  return kStOK;
}

void StFemtoMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}

Bool_t StFemtoMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t refMult, Float_t vzVpd) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (vz < ev.minVz || vz > ev.maxVz) return kFALSE;
  if (vr > ev.maxVr) return kFALSE;
  if (refMult < ev.minRefMult) return kFALSE;
  if (refMult > ev.maxRefMult) return kFALSE;
  if (TMath::Abs(vz - vzVpd) > ev.maxVzDiff && TMath::Abs(vzVpd) < ev.maxAbsVzVpd) return kFALSE;
  return kTRUE;
}

Bool_t StFemtoMaker::PassTrackCuts(StPicoTrack* trk, TVector3& pVtx) {
  TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
  if (trk->nHitsFit() < tr.minNHitsFit) return kFALSE;
  if ((Float_t)trk->nHitsFit() / (Float_t)trk->nHitsMax() < tr.minNHitsRatio) return kFALSE;
  if (trk->nHitsDedx() < tr.minNHitsDedx) return kFALSE;
  TVector3 pMom = trk->pMom();
  if (pMom.Mag() < 1e-4) return kFALSE;
  Float_t pt = pMom.Perp();
  Float_t eta = pMom.PseudoRapidity();
  if (pt < tr.minPt || pt > tr.maxPt) return kFALSE;
  if (eta < tr.minEta || eta > tr.maxEta) return kFALSE;
  if (trk->gDCA(pVtx).Mag() > tr.maxDCA) return kFALSE;
  if (tr.requirePrimaryTrack && !trk->isPrimary()) return kFALSE;
  if (trk->chi2() > tr.maxChi2) return kFALSE;
  return kTRUE;
}

Bool_t StFemtoMaker::PassKaonCuts(StPicoTrack* trk, TVector3& pVtx) {
  if (!PassTrackCuts(trk, pVtx)) return kFALSE;
  PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
  if (trk->gDCA(pVtx).Mag() > phi.maxDCAKaon) return kFALSE;
  if (TMath::Abs(trk->nSigmaKaon()) > phi.nSigmaKaon) return kFALSE;
  return kTRUE;
}

Bool_t StFemtoMaker::PassKaonMinusBaseCuts(StPicoTrack* trk, TVector3& pVtx) {
  if (!PassTrackCuts(trk, pVtx)) return kFALSE;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk->charge() >= 0) return kFALSE;
  if (trk->gDCA(pVtx).Mag() > fc.kaonMinusMaxDca) return kFALSE;
  if (TMath::Abs(trk->nSigmaKaon()) > fc.kaonMinusMaxAbsNSigma) return kFALSE;
  return kTRUE;
}

Bool_t StFemtoMaker::PassProtonCuts(StPicoTrack* trk, TVector3& pVtx) {
  if (!PassTrackCuts(trk, pVtx)) return kFALSE;
  PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  if (TMath::Abs(trk->nSigmaProton()) > pid.nSigmaProton) return kFALSE;
  return kTRUE;
}

void StFemtoMaker::BuildTrackState(TrackState& track, StPicoTrack* pico, StPicoEvent* event, TVector3& pVtx,
                                   Int_t index) {
  TVector3 pmom = pico->pMom();
  TVector3 org = pico->origin();
  track.originX = org.X();
  track.originY = org.Y();
  track.originZ = org.Z();
  track.momentumX = pmom.X();
  track.momentumY = pmom.Y();
  track.momentumZ = pmom.Z();
  track.BField = event->bField();
  track.pT = pmom.Perp();
  track.eta = pmom.PseudoRapidity();
  track.phi = pmom.Phi();
  track.charge = pico->charge();
  track.nHitsFit = pico->nHitsFit();
  track.nHitsMax = pico->nHitsMax();
  track.nHitsDedx = pico->nHitsDedx();
  track.DCA = pico->gDCA(pVtx).Mag();
  track.chi2 = pico->chi2();
  track.nSigmaKaon = pico->nSigmaKaon();
  track.nSigmaProton = pico->nSigmaProton();
  track.trackIndex = index;
  track.tofMatch = kFALSE;
  track.mass2 = -999.0f;
  track.deltaOneOverBeta = 999.0f;
  track.tofBeta = -1.0f;
}

PhiKkTrackState StFemtoMaker::ToPhiKkTrack(const TrackState& trk) {
  PhiKkTrackState s;
  s.pT = trk.pT;
  s.eta = trk.eta;
  s.phi = trk.phi;
  s.charge = trk.charge;
  s.originX = trk.originX;
  s.originY = trk.originY;
  s.originZ = trk.originZ;
  s.momentumX = trk.momentumX;
  s.momentumY = trk.momentumY;
  s.momentumZ = trk.momentumZ;
  s.BField = trk.BField;
  s.tofMatch = trk.tofMatch;
  s.mass2 = trk.mass2;
  s.deltaOneOverBeta = trk.deltaOneOverBeta;
  return s;
}

void StFemtoMaker::FillTofInfo(TrackState& track, StPicoTrack* trk, const TVector3& pMom, Int_t btofIndex) {
  (void)trk;
  track.tofBeta = -1.0f;
  StPicoBTofPidTraits* tof = 0;
  if (btofIndex >= 0) tof = mPicoDst->btofPidTraits(btofIndex);
  StPhiKKReconstruction::FillTofInfo(track.mass2, track.deltaOneOverBeta, track.tofMatch, tof, pMom);
  if (tof && track.tofMatch) {
    const Double_t beta = tof->btofBeta();
    if (beta > 1e-4) track.tofBeta = (Float_t)beta;
  }
}

Bool_t StFemtoMaker::PassTofKaonPid(const TrackState& trk) const {
  return StPhiKKReconstruction::PassTofKaonPid(ToPhiKkTrack(trk));
}

Bool_t StFemtoMaker::PassTofProtonPid(const TrackState& trk) const {
  const PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  if (!pid.requireTOF) return kTRUE;
  if (!trk.tofMatch) {
    TString fallbackMode(pid.tofFallbackMode.c_str());
    fallbackMode.ToLower();
    if (fallbackMode == "tpconly") return kTRUE;
    return kFALSE;
  }
  if (pid.tofUseMass2Cut) {
    return (trk.mass2 >= pid.minMass2Proton && trk.mass2 <= pid.maxMass2Proton);
  }
  return kTRUE;
}

Bool_t StFemtoMaker::IsKaon(const TrackState& trk) { return PassTofKaonPid(trk); }

Bool_t StFemtoMaker::IsProton(const TrackState& trk) { return PassTofProtonPid(trk); }

Double_t StFemtoMaker::ProtonRapidityCm(const TrackState& trk) const {
  TVector3 p = TrackMomentum(trk);
  TLorentzVector lv(p.X(), p.Y(), p.Z(), TMath::Sqrt(kProtonMass * kProtonMass + p.Mag2()));
  Double_t yLab = lv.Rapidity();
  return ApplyRapidityFrame(yLab);
}

Double_t StFemtoMaker::KaonMinusRapidityCm(const TrackState& trk) const {
  TVector3 p = TrackMomentum(trk);
  TLorentzVector lv(p.X(), p.Y(), p.Z(), TMath::Sqrt(kKaonMass * kKaonMass + p.Mag2()));
  return ApplyRapidityFrame(lv.Rapidity());
}

void StFemtoMaker::FillProtonPreFemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = ProtonRapidityCm(trk);
  m_histManager->Fill("hP_Y_PreFemtoCut", yCm);
  m_histManager->Fill("hP_PtVsY_PreFemtoCut", yCm, trk.pT);
  m_histManager->Fill("hP_Pt_PreFemtoCut", trk.pT);
  m_histManager->Fill("hP_Eta_PreFemtoCut", trk.eta);
  m_histManager->Fill("hP_NSigmaProton_PreFemtoCut", trk.nSigmaProton);
  m_histManager->Fill("hP_DCA_PreFemtoCut", trk.DCA);
  if (trk.tofMatch) m_histManager->Fill("hP_Mass2_PreFemtoCut", trk.mass2);
}

void StFemtoMaker::FillPhiPairKinematicsQa(Double_t invMass, const TVector3& phiMom, Double_t openingAngle,
                                           Double_t pairRapidity) {
  if (!m_histManager) return;

  PhiCutConfig& phiCut = ConfigManager::GetInstance().GetPhiCuts();
  Bool_t passInvMass = kTRUE;
  if (phiCut.useInvMassCut) {
    passInvMass = (invMass >= phiCut.minInvMass && invMass <= phiCut.maxInvMass);
  }
  Bool_t passAngle = (openingAngle >= phiCut.minOpeningAngle && openingAngle <= phiCut.maxOpeningAngle);
  Bool_t passRapidity = (pairRapidity >= phiCut.minPairRapidity && pairRapidity <= phiCut.maxPairRapidity);

  m_histManager->Fill("hOpeningAngle_Raw", openingAngle);
  m_histManager->Fill("hPairRapidity_Raw", pairRapidity);
  m_histManager->Fill("hPairPt_Raw", phiMom.Pt());
  m_histManager->Fill("hOpeningAngle_vs_MKK", openingAngle, invMass);
  m_histManager->Fill("hPairRapidity_vs_MKK", pairRapidity, invMass);
  m_histManager->Fill("hOpeningAngle_vs_Pt", openingAngle, phiMom.Pt());
  m_histManager->Fill("hOpeningAngle_vs_Rapidity", openingAngle, pairRapidity);
  m_histManager->Fill("hPairRapidity_vs_Pt", pairRapidity, phiMom.Pt());
  m_histManager->Fill("hMKK_vs_Pt", phiMom.Pt(), invMass);

  if (!passInvMass) return;

  if (passAngle && passRapidity) {
    m_histManager->Fill("hOpeningAngle_AfterCuts", openingAngle);
    m_histManager->Fill("hPairRapidity_AfterCuts", pairRapidity);
    m_histManager->Fill("hPairPt_AfterCuts", phiMom.Pt());
  }
}

void StFemtoMaker::FillPhiCandidatePreCutQa(Double_t invMass, Double_t pt, Double_t pairRapidity) {
  if (!m_histManager) return;
  m_histManager->Fill("hPhi_MKK_PreCut", invMass);
  m_histManager->Fill("hPhi_Pt_PreCut", pt);
  m_histManager->Fill("hPhi_Rapidity_PreCut", pairRapidity);
  m_histManager->Fill("hPhi_PtVsY_PreCut", pairRapidity, pt);
}

void StFemtoMaker::FillProtonFemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = ProtonRapidityCm(trk);
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  m_histManager->Fill("hP_Y_FemtoCut", yCm);
  m_histManager->Fill("hP_PtVsY_FemtoCut", yCm, trk.pT);
  m_histManager->Fill("hP_Mass2VsP", pmom, trk.mass2);
  m_histManager->Fill("hP_TofMatchVsP", pmom, trk.tofMatch ? 1.0 : 0.0);
  m_histManager->Fill("hP_NHitsFit_FemtoCut", trk.nHitsFit);
  if (trk.nHitsMax > 0) {
    m_histManager->Fill("hP_NHitsRatio_FemtoCut", (Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax);
  }
  if (trk.tofMatch) m_histManager->Fill("hP_Mass2", trk.mass2);
}

void StFemtoMaker::FillKaonMinusPreFemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = KaonMinusRapidityCm(trk);
  m_histManager->Fill("hKm_Y_PreFemtoCut", yCm);
  m_histManager->Fill("hKm_PtVsY_PreFemtoCut", yCm, trk.pT);
  m_histManager->Fill("hKm_Pt_PreFemtoCut", trk.pT);
  m_histManager->Fill("hKm_Eta_PreFemtoCut", trk.eta);
  m_histManager->Fill("hKm_NSigmaKaon_PreFemtoCut", trk.nSigmaKaon);
  m_histManager->Fill("hKm_DCA_PreFemtoCut", trk.DCA);
  if (trk.tofMatch) m_histManager->Fill("hKm_Mass2_PreFemtoCut", trk.mass2);
}

void StFemtoMaker::FillKaonMinusFemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = KaonMinusRapidityCm(trk);
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  m_histManager->Fill("hKm_Y_FemtoCut", yCm);
  m_histManager->Fill("hKm_PtVsY_FemtoCut", yCm, trk.pT);
  m_histManager->Fill("hKm_Mass2VsP", pmom, trk.mass2);
  m_histManager->Fill("hKm_TofMatchVsP", pmom, trk.tofMatch ? 1.0 : 0.0);
  m_histManager->Fill("hKm_NHitsFit_FemtoCut", trk.nHitsFit);
  if (trk.nHitsMax > 0) {
    m_histManager->Fill("hKm_NHitsRatio_FemtoCut", (Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax);
  }
  if (trk.tofMatch) m_histManager->Fill("hKm_Mass2", trk.mass2);
}

Double_t StFemtoMaker::He4RapidityCm(const TrackState& trk) const {
  TVector3 p = TrackMomentum(trk);
  TLorentzVector lv = StNuclearIdHelper::NuclearP4(p, kNucHe4);
  return ApplyRapidityFrame(lv.Rapidity());
}

void StFemtoMaker::FillHe4PreFemtoQa(const TrackState& trk, Float_t nSigmaHe4) {
  if (!m_histManager) return;
  Double_t yCm = He4RapidityCm(trk);
  m_histManager->Fill("hHe4_Y_PreFemtoCut", yCm);
  m_histManager->Fill("hHe4_PtVsY_PreFemtoCut", yCm, trk.pT);
  m_histManager->Fill("hHe4_Pt_PreFemtoCut", trk.pT);
  m_histManager->Fill("hHe4_Eta_PreFemtoCut", trk.eta);
  m_histManager->Fill("hHe4_NSigmaHe4_PreFemtoCut", nSigmaHe4);
  m_histManager->Fill("hHe4_DCA_PreFemtoCut", trk.DCA);
  if (trk.tofMatch) {
    m_histManager->Fill("hHe4_Mass2_PreFemtoCut", trk.mass2);
    TVector3 p = TrackMomentum(trk);
    m_histManager->Fill("hHe4_Mass2VsP_PreFemtoCut_wide", p.Mag(), trk.mass2);
  }
}

void StFemtoMaker::FillHe4FemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = He4RapidityCm(trk);
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  m_histManager->Fill("hHe4_Y_FemtoCut", yCm);
  m_histManager->Fill("hHe4_PtVsY_FemtoCut", yCm, trk.pT);
  m_histManager->Fill("hHe4_Mass2VsP", pmom, trk.mass2);
  if (trk.tofMatch) m_histManager->Fill("hHe4_Mass2VsP_wide", pmom, trk.mass2);
  m_histManager->Fill("hHe4_TofMatchVsP", pmom, trk.tofMatch ? 1.0 : 0.0);
  m_histManager->Fill("hHe4_NHitsFit_FemtoCut", trk.nHitsFit);
  if (trk.nHitsMax > 0) {
    m_histManager->Fill("hHe4_NHitsRatio_FemtoCut", (Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax);
  }
  if (trk.tofMatch) m_histManager->Fill("hHe4_Mass2", trk.mass2);
}

Bool_t StFemtoMaker::PassFemtoHe4Cuts(const He4TrackState& h4) const {
  const TrackState& trk = h4.trk;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk.charge <= 0) return kFALSE;
  if (trk.DCA >= fc.he4MaxDca) return kFALSE;
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  if (pmom < fc.he4MinPMom || pmom > fc.he4MaxPMom) return kFALSE;
  if (trk.pT < fc.he4MinPtPre || trk.pT > fc.he4MaxPtPre) return kFALSE;
  if (TMath::Abs(trk.eta) >= fc.he4MaxAbsEta) return kFALSE;
  if (TMath::Abs(h4.nSigmaHe4) >= fc.he4MaxAbsNSigma) return kFALSE;
  if (trk.nHitsFit < fc.he4MinNHitsFit) return kFALSE;
  if (trk.nHitsMax <= 0) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < fc.he4MinNHitsRatio) return kFALSE;

  const Bool_t passTofRule =
      (pmom < fc.he4TofMomentumThreshold) ||
      (pmom > fc.he4TofMomentumThreshold && trk.tofMatch && trk.mass2 >= fc.he4MinMass2 &&
       trk.mass2 <= fc.he4MaxMass2);
  if (!passTofRule) return kFALSE;

  if (trk.pT < fc.he4MinPtPair || trk.pT > fc.he4MaxPtPair) return kFALSE;
  Double_t yCm = He4RapidityCm(trk);
  if (yCm < fc.he4MinRapidityCm || yCm > fc.he4MaxRapidityCm) return kFALSE;
  return kTRUE;
}

Double_t StFemtoMaker::DeuteronRapidityCm(const TrackState& trk) const {
  TVector3 p = TrackMomentum(trk);
  TLorentzVector lv = StNuclearIdHelper::NuclearP4(p, kNucDeuteron);
  return ApplyRapidityFrame(lv.Rapidity());
}

void StFemtoMaker::FillDeuteronPreFemtoQa(const TrackState& trk, Float_t nSigmaDeuteron) {
  if (!m_histManager) return;
  Double_t yCm = DeuteronRapidityCm(trk);
  m_histManager->Fill("hDeuteron_Y_PreFemtoCut", yCm);
  m_histManager->Fill("hDeuteron_PtVsY_PreFemtoCut", yCm, trk.pT);
  m_histManager->Fill("hDeuteron_Pt_PreFemtoCut", trk.pT);
  m_histManager->Fill("hDeuteron_Eta_PreFemtoCut", trk.eta);
  m_histManager->Fill("hDeuteron_NSigmaDeuteron_PreFemtoCut", nSigmaDeuteron);
  m_histManager->Fill("hDeuteron_DCA_PreFemtoCut", trk.DCA);
  if (trk.tofMatch) {
    m_histManager->Fill("hDeuteron_Mass2_PreFemtoCut", trk.mass2);
    TVector3 p = TrackMomentum(trk);
    m_histManager->Fill("hDeuteron_Mass2VsP_PreFemtoCut_wide", p.Mag(), trk.mass2);
  }
}

void StFemtoMaker::FillDeuteronFemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = DeuteronRapidityCm(trk);
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  m_histManager->Fill("hDeuteron_Y_FemtoCut", yCm);
  m_histManager->Fill("hDeuteron_PtVsY_FemtoCut", yCm, trk.pT);
  m_histManager->Fill("hDeuteron_Mass2VsP", pmom, trk.mass2);
  if (trk.tofMatch) m_histManager->Fill("hDeuteron_Mass2VsP_wide", pmom, trk.mass2);
  m_histManager->Fill("hDeuteron_TofMatchVsP", pmom, trk.tofMatch ? 1.0 : 0.0);
  m_histManager->Fill("hDeuteron_NHitsFit_FemtoCut", trk.nHitsFit);
  if (trk.nHitsMax > 0) {
    m_histManager->Fill("hDeuteron_NHitsRatio_FemtoCut", (Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax);
  }
  if (trk.tofMatch) m_histManager->Fill("hDeuteron_Mass2", trk.mass2);
}

Bool_t StFemtoMaker::PassFemtoDeuteronCuts(const DeuteronTrackState& dState) const {
  const TrackState& trk = dState.trk;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk.charge <= 0) return kFALSE;
  if (trk.DCA >= fc.deuteronMaxDca) return kFALSE;
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  if (pmom < fc.deuteronMinPMom || pmom > fc.deuteronMaxPMom) return kFALSE;
  if (trk.pT < fc.deuteronMinPtPre || trk.pT > fc.deuteronMaxPtPre) return kFALSE;
  if (TMath::Abs(trk.eta) >= fc.deuteronMaxAbsEta) return kFALSE;
  if (TMath::Abs(dState.nSigmaDeuteron) >= fc.deuteronMaxAbsNSigma) return kFALSE;
  if (trk.nHitsFit < fc.deuteronMinNHitsFit) return kFALSE;
  if (trk.nHitsMax <= 0) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < fc.deuteronMinNHitsRatio) return kFALSE;

  const Bool_t passTofRule =
      (pmom < fc.deuteronTofMomentumThreshold) ||
      (pmom > fc.deuteronTofMomentumThreshold && trk.tofMatch && trk.mass2 >= fc.deuteronMinMass2 &&
       trk.mass2 <= fc.deuteronMaxMass2);
  if (!passTofRule) return kFALSE;

  if (trk.pT < fc.deuteronMinPtPair || trk.pT > fc.deuteronMaxPtPair) return kFALSE;
  Double_t yCm = DeuteronRapidityCm(trk);
  if (yCm < fc.deuteronMinRapidityCm || yCm > fc.deuteronMaxRapidityCm) return kFALSE;
  return kTRUE;
}

Double_t StFemtoMaker::TritonRapidityCm(const TrackState& trk) const {
  TVector3 p = TrackMomentum(trk);
  TLorentzVector lv = StNuclearIdHelper::NuclearP4(p, kNucTriton);
  return ApplyRapidityFrame(lv.Rapidity());
}

Double_t StFemtoMaker::He3RapidityCm(const TrackState& trk) const {
  TVector3 p = TrackMomentum(trk);
  TLorentzVector lv = StNuclearIdHelper::NuclearP4(p, kNucHe3);
  return ApplyRapidityFrame(lv.Rapidity());
}

void StFemtoMaker::FillTritonPreFemtoQa(const TrackState& trk, Float_t nSigmaTriton) {
  if (!m_histManager) return;
  Double_t yCm = TritonRapidityCm(trk);
  m_histManager->Fill("hTriton_Y_PreFemtoCut", yCm);
  m_histManager->Fill("hTriton_PtVsY_PreFemtoCut", yCm, trk.pT);
  m_histManager->Fill("hTriton_Pt_PreFemtoCut", trk.pT);
  m_histManager->Fill("hTriton_Eta_PreFemtoCut", trk.eta);
  m_histManager->Fill("hTriton_NSigmaTriton_PreFemtoCut", nSigmaTriton);
  m_histManager->Fill("hTriton_DCA_PreFemtoCut", trk.DCA);
  if (trk.tofMatch) {
    m_histManager->Fill("hTriton_Mass2_PreFemtoCut", trk.mass2);
    TVector3 p = TrackMomentum(trk);
    m_histManager->Fill("hTriton_Mass2VsP_PreFemtoCut_wide", p.Mag(), trk.mass2);
  }
}

void StFemtoMaker::FillTritonFemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = TritonRapidityCm(trk);
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  m_histManager->Fill("hTriton_Y_FemtoCut", yCm);
  m_histManager->Fill("hTriton_PtVsY_FemtoCut", yCm, trk.pT);
  m_histManager->Fill("hTriton_Mass2VsP", pmom, trk.mass2);
  if (trk.tofMatch) m_histManager->Fill("hTriton_Mass2VsP_wide", pmom, trk.mass2);
  m_histManager->Fill("hTriton_TofMatchVsP", pmom, trk.tofMatch ? 1.0 : 0.0);
  m_histManager->Fill("hTriton_NHitsFit_FemtoCut", trk.nHitsFit);
  if (trk.nHitsMax > 0) {
    m_histManager->Fill("hTriton_NHitsRatio_FemtoCut", (Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax);
  }
  if (trk.tofMatch) m_histManager->Fill("hTriton_Mass2", trk.mass2);
}

Bool_t StFemtoMaker::PassFemtoTritonCuts(const TritonTrackState& tState) const {
  const TrackState& trk = tState.trk;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk.charge <= 0) return kFALSE;
  if (trk.DCA >= fc.tritonMaxDca) return kFALSE;
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  if (pmom < fc.tritonMinPMom || pmom > fc.tritonMaxPMom) return kFALSE;
  if (trk.pT < fc.tritonMinPtPre || trk.pT > fc.tritonMaxPtPre) return kFALSE;
  if (TMath::Abs(trk.eta) >= fc.tritonMaxAbsEta) return kFALSE;
  if (TMath::Abs(tState.nSigmaTriton) >= fc.tritonMaxAbsNSigma) return kFALSE;
  if (trk.nHitsFit < fc.tritonMinNHitsFit) return kFALSE;
  if (trk.nHitsMax <= 0) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < fc.tritonMinNHitsRatio) return kFALSE;

  const Bool_t passTofRule =
      (pmom < fc.tritonTofMomentumThreshold) ||
      (pmom > fc.tritonTofMomentumThreshold && trk.tofMatch && trk.mass2 >= fc.tritonMinMass2 &&
       trk.mass2 <= fc.tritonMaxMass2);
  if (!passTofRule) return kFALSE;

  if (trk.pT < fc.tritonMinPtPair || trk.pT > fc.tritonMaxPtPair) return kFALSE;
  Double_t yCm = TritonRapidityCm(trk);
  if (yCm < fc.tritonMinRapidityCm || yCm > fc.tritonMaxRapidityCm) return kFALSE;
  return kTRUE;
}

void StFemtoMaker::FillHe3PreFemtoQa(const TrackState& trk, Float_t nSigmaHe3) {
  if (!m_histManager) return;
  Double_t yCm = He3RapidityCm(trk);
  m_histManager->Fill("hHe3_Y_PreFemtoCut", yCm);
  m_histManager->Fill("hHe3_PtVsY_PreFemtoCut", yCm, trk.pT);
  m_histManager->Fill("hHe3_Pt_PreFemtoCut", trk.pT);
  m_histManager->Fill("hHe3_Eta_PreFemtoCut", trk.eta);
  m_histManager->Fill("hHe3_NSigmaHe3_PreFemtoCut", nSigmaHe3);
  m_histManager->Fill("hHe3_DCA_PreFemtoCut", trk.DCA);
  if (trk.tofMatch) {
    m_histManager->Fill("hHe3_Mass2_PreFemtoCut", trk.mass2);
    TVector3 p = TrackMomentum(trk);
    m_histManager->Fill("hHe3_Mass2VsP_PreFemtoCut_wide", p.Mag(), trk.mass2);
  }
}

void StFemtoMaker::FillHe3FemtoQa(const TrackState& trk) {
  if (!m_histManager) return;
  Double_t yCm = He3RapidityCm(trk);
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  m_histManager->Fill("hHe3_Y_FemtoCut", yCm);
  m_histManager->Fill("hHe3_PtVsY_FemtoCut", yCm, trk.pT);
  m_histManager->Fill("hHe3_Mass2VsP", pmom, trk.mass2);
  if (trk.tofMatch) m_histManager->Fill("hHe3_Mass2VsP_wide", pmom, trk.mass2);
  m_histManager->Fill("hHe3_TofMatchVsP", pmom, trk.tofMatch ? 1.0 : 0.0);
  m_histManager->Fill("hHe3_NHitsFit_FemtoCut", trk.nHitsFit);
  if (trk.nHitsMax > 0) {
    m_histManager->Fill("hHe3_NHitsRatio_FemtoCut", (Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax);
  }
  if (trk.tofMatch) m_histManager->Fill("hHe3_Mass2", trk.mass2);
}

Bool_t StFemtoMaker::PassFemtoHe3Cuts(const He3TrackState& h3State) const {
  const TrackState& trk = h3State.trk;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk.charge <= 0) return kFALSE;
  if (trk.DCA >= fc.he3MaxDca) return kFALSE;
  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  if (pmom < fc.he3MinPMom || pmom > fc.he3MaxPMom) return kFALSE;
  if (trk.pT < fc.he3MinPtPre || trk.pT > fc.he3MaxPtPre) return kFALSE;
  if (TMath::Abs(trk.eta) >= fc.he3MaxAbsEta) return kFALSE;
  if (TMath::Abs(h3State.nSigmaHe3) >= fc.he3MaxAbsNSigma) return kFALSE;
  if (trk.nHitsFit < fc.he3MinNHitsFit) return kFALSE;
  if (trk.nHitsMax <= 0) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < fc.he3MinNHitsRatio) return kFALSE;

  const Bool_t passTofRule =
      (pmom < fc.he3TofMomentumThreshold) ||
      (pmom > fc.he3TofMomentumThreshold && trk.tofMatch && trk.mass2 >= fc.he3MinMass2 &&
       trk.mass2 <= fc.he3MaxMass2);
  if (!passTofRule) return kFALSE;

  if (trk.pT < fc.he3MinPtPair || trk.pT > fc.he3MaxPtPair) return kFALSE;
  Double_t yCm = He3RapidityCm(trk);
  if (yCm < fc.he3MinRapidityCm || yCm > fc.he3MaxRapidityCm) return kFALSE;
  return kTRUE;
}

Bool_t StFemtoMaker::PassFemtoProtonCuts(const TrackState& trk) const {
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (fc.protonChargeMode == "positive" && trk.charge <= 0) return kFALSE;
  if (fc.protonChargeMode == "negative" && trk.charge >= 0) return kFALSE;
  if (trk.DCA >= fc.protonMaxDca) return kFALSE;
  if (trk.pT < fc.protonMinPtPre) return kFALSE;
  if (TMath::Abs(trk.eta) >= fc.protonMaxAbsEta) return kFALSE;
  if (TMath::Abs(trk.nSigmaProton) >= fc.protonMaxAbsNSigma) return kFALSE;
  if (trk.nHitsFit < fc.protonMinNHitsFit) return kFALSE;
  if (trk.nHitsMax <= 0) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < fc.protonMinNHitsRatio) return kFALSE;

  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  const Bool_t passTofRule =
      (pmom < fc.protonTofMomentumThreshold) ||
      (pmom > fc.protonTofMomentumThreshold && trk.tofMatch && trk.mass2 >= fc.protonMinMass2 &&
       trk.mass2 <= fc.protonMaxMass2);
  if (!passTofRule) return kFALSE;

  if (trk.pT < fc.protonMinPtPair || trk.pT > fc.protonMaxPtPair) return kFALSE;
  Double_t yCm = ProtonRapidityCm(trk);
  if (yCm < fc.protonMinRapidityCm || yCm > fc.protonMaxRapidityCm) return kFALSE;
  return kTRUE;
}

Bool_t StFemtoMaker::PassFemtoKaonMinusCuts(const TrackState& trk) const {
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (trk.charge >= 0) return kFALSE;
  if (trk.DCA >= fc.kaonMinusMaxDca) return kFALSE;
  if (trk.pT < fc.kaonMinusMinPtPre) return kFALSE;
  if (TMath::Abs(trk.eta) >= fc.kaonMinusMaxAbsEta) return kFALSE;
  if (TMath::Abs(trk.nSigmaKaon) >= fc.kaonMinusMaxAbsNSigma) return kFALSE;
  if (trk.nHitsFit < fc.kaonMinusMinNHitsFit) return kFALSE;
  if (trk.nHitsMax <= 0) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < fc.kaonMinusMinNHitsRatio) return kFALSE;

  TVector3 p = TrackMomentum(trk);
  Double_t pmom = p.Mag();
  const Bool_t passTofRule =
      (pmom < fc.kaonMinusTofMomentumThreshold) ||
      (pmom >= fc.kaonMinusTofMomentumThreshold && trk.tofMatch && trk.mass2 >= fc.kaonMinusMinMass2 &&
       trk.mass2 <= fc.kaonMinusMaxMass2);
  if (!passTofRule) return kFALSE;

  if (trk.pT < fc.kaonMinusMinPtPair || trk.pT > fc.kaonMinusMaxPtPair) return kFALSE;
  Double_t yCm = KaonMinusRapidityCm(trk);
  if (yCm < fc.kaonMinusMinRapidityCm || yCm > fc.kaonMinusMaxRapidityCm) return kFALSE;
  return kTRUE;
}

TVector3 StFemtoMaker::TrackMomentum(const TrackState& trk) const {
  return StPhiKKReconstruction::TrackMomentum(ToPhiKkTrack(trk));
}

StPhysicalHelixD StFemtoMaker::BuildHelix(const TrackState& trk) {
  return StPhiKKReconstruction::BuildHelix(ToPhiKkTrack(trk));
}

Double_t StFemtoMaker::CalculateDCA(const TrackState& trk1, const TrackState& trk2, TVector3& dcaPos1,
                                    TVector3& dcaPos2) {
  return StPhiKKReconstruction::CalculateDCA(ToPhiKkTrack(trk1), ToPhiKkTrack(trk2), dcaPos1, dcaPos2);
}

Bool_t StFemtoMaker::ReconstructPhi(const TrackState& kPlus, const TrackState& kMinus, Double_t& invMass,
                                    TVector3& phiMom, TVector3& dcaPosPlus, TVector3& dcaPosMinus) {
  return StPhiKKReconstruction::ReconstructPhi(ToPhiKkTrack(kPlus), ToPhiKkTrack(kMinus), invMass, phiMom, dcaPosPlus,
                                               dcaPosMinus);
}

Double_t StFemtoMaker::CalculateOpeningAngle(const TrackState& trk1, const TrackState& trk2) {
  return StPhiKKReconstruction::CalculateOpeningAngle(ToPhiKkTrack(trk1), ToPhiKkTrack(trk2));
}

Double_t StFemtoMaker::CalculatePairRapidity(Double_t invMass, const TVector3& phiMom) {
  return StPhiKKReconstruction::CalculatePairRapidity(invMass, phiMom);
}

Double_t StFemtoMaker::ApplyRapidityFrame(Double_t yLab) const {
  return StPhiKKReconstruction::ApplyRapidityFrame(yLab);
}

Bool_t StFemtoMaker::PassPairTofCut(const TrackState& kPlus, const TrackState& kMinus) const {
  return StPhiKKReconstruction::PassPairTofCut(ToPhiKkTrack(kPlus), ToPhiKkTrack(kMinus));
}

FemtoCandidate StFemtoMaker::MakeProtonCandidate(const TrackState& trk, Int_t eventIndex,
                                                 const std::string& speciesKey) const {
  FemtoCandidate cand;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandTrack;
  cand.speciesKey = speciesKey;
  cand.charge = trk.charge;
  TVector3 p = TrackMomentum(trk);
  TLorentzVector p4 = ProtonP4(p);
  cand.SetP4(p4);
  cand.trk.trackIndex = trk.trackIndex;
  cand.trk.nSigmaProton = trk.nSigmaProton;
  cand.trk.nSigmaKaon = trk.nSigmaKaon;
  cand.trk.mass2 = trk.mass2;
  cand.trk.dca = trk.DCA;
  cand.trk.nHitsFit = trk.nHitsFit;
  return cand;
}

FemtoCandidate StFemtoMaker::MakeKaonMinusCandidate(const TrackState& trk, Int_t eventIndex,
                                                    const std::string& speciesKey) const {
  FemtoCandidate cand;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandTrack;
  cand.speciesKey = speciesKey;
  cand.charge = trk.charge;
  TVector3 p = TrackMomentum(trk);
  TLorentzVector p4 = KaonP4(p);
  cand.SetP4(p4);
  cand.trk.trackIndex = trk.trackIndex;
  cand.trk.nSigmaKaon = trk.nSigmaKaon;
  cand.trk.nSigmaProton = trk.nSigmaProton;
  cand.trk.mass2 = trk.mass2;
  cand.trk.dca = trk.DCA;
  cand.trk.nHitsFit = trk.nHitsFit;
  return cand;
}

FemtoCandidate StFemtoMaker::MakeHe4Candidate(const He4TrackState& h4, Int_t eventIndex,
                                              const std::string& speciesKey) const {
  FemtoCandidate cand;
  const TrackState& trk = h4.trk;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandTrack;
  cand.speciesKey = speciesKey;
  cand.charge = trk.charge;
  TVector3 p = TrackMomentum(trk);
  TLorentzVector p4 = StNuclearIdHelper::NuclearP4(p, kNucHe4);
  cand.SetP4(p4);
  cand.trk.trackIndex = trk.trackIndex;
  cand.trk.nSigmaHe4 = h4.nSigmaHe4;
  cand.trk.nSigmaKaon = trk.nSigmaKaon;
  cand.trk.mass2 = trk.mass2;
  cand.trk.dca = trk.DCA;
  cand.trk.nHitsFit = trk.nHitsFit;
  return cand;
}

FemtoCandidate StFemtoMaker::MakeDeuteronCandidate(const DeuteronTrackState& dState, Int_t eventIndex,
                                                   const std::string& speciesKey) const {
  FemtoCandidate cand;
  const TrackState& trk = dState.trk;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandTrack;
  cand.speciesKey = speciesKey;
  cand.charge = trk.charge;
  TVector3 p = TrackMomentum(trk);
  TLorentzVector p4 = StNuclearIdHelper::NuclearP4(p, kNucDeuteron);
  cand.SetP4(p4);
  cand.trk.trackIndex = trk.trackIndex;
  cand.trk.nSigmaDeuteron = dState.nSigmaDeuteron;
  cand.trk.nSigmaKaon = trk.nSigmaKaon;
  cand.trk.mass2 = trk.mass2;
  cand.trk.dca = trk.DCA;
  cand.trk.nHitsFit = trk.nHitsFit;
  return cand;
}

FemtoCandidate StFemtoMaker::MakeTritonCandidate(const TritonTrackState& tState, Int_t eventIndex,
                                                 const std::string& speciesKey) const {
  FemtoCandidate cand;
  const TrackState& trk = tState.trk;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandTrack;
  cand.speciesKey = speciesKey;
  cand.charge = trk.charge;
  TVector3 p = TrackMomentum(trk);
  TLorentzVector p4 = StNuclearIdHelper::NuclearP4(p, kNucTriton);
  cand.SetP4(p4);
  cand.trk.trackIndex = trk.trackIndex;
  cand.trk.nSigmaTriton = tState.nSigmaTriton;
  cand.trk.nSigmaKaon = trk.nSigmaKaon;
  cand.trk.mass2 = trk.mass2;
  cand.trk.dca = trk.DCA;
  cand.trk.nHitsFit = trk.nHitsFit;
  return cand;
}

FemtoCandidate StFemtoMaker::MakeHe3Candidate(const He3TrackState& h3State, Int_t eventIndex,
                                              const std::string& speciesKey) const {
  FemtoCandidate cand;
  const TrackState& trk = h3State.trk;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandTrack;
  cand.speciesKey = speciesKey;
  cand.charge = trk.charge;
  TVector3 p = TrackMomentum(trk);
  TLorentzVector p4 = StNuclearIdHelper::NuclearP4(p, kNucHe3);
  cand.SetP4(p4);
  cand.trk.trackIndex = trk.trackIndex;
  cand.trk.nSigmaHe3 = h3State.nSigmaHe3;
  cand.trk.nSigmaKaon = trk.nSigmaKaon;
  cand.trk.mass2 = trk.mass2;
  cand.trk.dca = trk.DCA;
  cand.trk.nHitsFit = trk.nHitsFit;
  return cand;
}

FemtoCandidate StFemtoMaker::MakePhiCandidate(const TrackState& kPlus, const TrackState& kMinus, Double_t invMass,
                                              const TVector3& phiMom, Double_t openingAngle, Double_t pairRapidity,
                                              Double_t dcaKK, Int_t eventIndex, const std::string& speciesKey) const {
  FemtoCandidate cand;
  cand.eventIndex = eventIndex;
  cand.source = kFemtoCandResonance;
  cand.speciesKey = speciesKey;
  cand.charge = 0;
  Double_t E = TMath::Sqrt(invMass * invMass + phiMom.Mag2());
  TLorentzVector p4(phiMom.X(), phiMom.Y(), phiMom.Z(), E);
  cand.SetP4(p4);
  cand.y = (Float_t)pairRapidity;
  cand.reso.invMass = (Float_t)invMass;
  cand.reso.dcaDaughters = (Float_t)dcaKK;
  cand.reso.dau1Index = kPlus.trackIndex;
  cand.reso.dau2Index = kMinus.trackIndex;
  ComputePhiBetaGamma(kPlus.tofMatch, kPlus.tofBeta, kMinus.tofMatch, kMinus.tofBeta, cand.reso.betaGamma);
  (void)openingAngle;
  return cand;
}

void StFemtoMaker::BuildTrackPidCandidates(const std::string& speciesKey, const std::string& particleKey,
                                           const std::vector<TrackState>& protonTracks,
                                           const std::vector<TrackState>& kaonMinusTracks,
                                           const std::vector<He4TrackState>& he4Tracks,
                                           const std::vector<DeuteronTrackState>& deuteronTracks,
                                           const std::vector<TritonTrackState>& tritonTracks,
                                           const std::vector<He3TrackState>& he3Tracks, Int_t eventIndex) {
  std::vector<FemtoCandidate>& out = m_eventCandidates[speciesKey];
  if (particleKey == "proton") {
    for (size_t i = 0; i < protonTracks.size(); i++) {
      out.push_back(MakeProtonCandidate(protonTracks[i], eventIndex, speciesKey));
    }
    return;
  }
  if (particleKey == "kaon_minus" || particleKey == "kaon") {
    for (size_t i = 0; i < kaonMinusTracks.size(); i++) {
      out.push_back(MakeKaonMinusCandidate(kaonMinusTracks[i], eventIndex, speciesKey));
    }
    return;
  }
  if (particleKey == "he4") {
    for (size_t i = 0; i < he4Tracks.size(); i++) {
      out.push_back(MakeHe4Candidate(he4Tracks[i], eventIndex, speciesKey));
    }
    return;
  }
  if (particleKey == "deuteron") {
    for (size_t i = 0; i < deuteronTracks.size(); i++) {
      out.push_back(MakeDeuteronCandidate(deuteronTracks[i], eventIndex, speciesKey));
    }
    return;
  }
  if (particleKey == "triton") {
    for (size_t i = 0; i < tritonTracks.size(); i++) {
      out.push_back(MakeTritonCandidate(tritonTracks[i], eventIndex, speciesKey));
    }
    return;
  }
  if (particleKey == "he3") {
    for (size_t i = 0; i < he3Tracks.size(); i++) {
      out.push_back(MakeHe3Candidate(he3Tracks[i], eventIndex, speciesKey));
    }
    return;
  }
  std::cerr << "[StFemtoMaker] TrackPidBuilder: unsupported particleKey '" << particleKey << "'" << std::endl;
}

void StFemtoMaker::BuildResonanceCandidates(const std::string& speciesKey, const std::string& particleKey,
                                            const std::vector<TrackState>& kaonsPlus,
                                            const std::vector<TrackState>& kaonsMinus, Int_t eventIndex) {
  if (particleKey != "phi") {
    std::cerr << "[StFemtoMaker] ResonanceBuilder: unsupported particleKey '" << particleKey << "'" << std::endl;
    return;
  }
  PhiCutConfig& phiCfg = ConfigManager::GetInstance().GetPhiCuts();
  TrackCutConfig& trkCfg = ConfigManager::GetInstance().GetTrackCuts();
  std::vector<FemtoCandidate>& out = m_eventCandidates[speciesKey];
  Int_t nPhiPairStage0 = 0;
  Int_t nPhiPairTofStrict = 0;

  for (size_t iPlus = 0; iPlus < kaonsPlus.size(); iPlus++) {
    for (size_t iMinus = 0; iMinus < kaonsMinus.size(); iMinus++) {
      TVector3 dcaMeasPlus, dcaMeasMinus;
      Double_t dcaKK =
          CalculateDCA(kaonsPlus[iPlus], kaonsMinus[iMinus], dcaMeasPlus, dcaMeasMinus);
      if (m_histManager) {
        m_histManager->Fill("hDCAKK_All", dcaKK);
      }

      TVector3 dcaPosPlus, dcaPosMinus;
      Double_t invMass;
      TVector3 phiMom;
      if (!ReconstructPhi(kaonsPlus[iPlus], kaonsMinus[iMinus], invMass, phiMom, dcaPosPlus, dcaPosMinus)) {
        continue;
      }
      if (m_histManager) {
        m_histManager->Fill("hDCAKK_Pass", dcaKK);
      }

      Double_t openingAngle = CalculateOpeningAngle(kaonsPlus[iPlus], kaonsMinus[iMinus]);
      Double_t yLab = CalculatePairRapidity(invMass, phiMom);
      Double_t pairRapidity = ApplyRapidityFrame(yLab);

      const Bool_t passPreMassStage =
          (TMath::Abs(kaonsPlus[iPlus].DCA) <= phiCfg.maxDCAKaon) &&
          (TMath::Abs(kaonsMinus[iMinus].DCA) <= phiCfg.maxDCAKaon) &&
          (kaonsPlus[iPlus].pT >= trkCfg.minPt && kaonsPlus[iPlus].pT <= trkCfg.maxPt) &&
          (kaonsMinus[iMinus].pT >= trkCfg.minPt && kaonsMinus[iMinus].pT <= trkCfg.maxPt) &&
          (kaonsPlus[iPlus].eta >= trkCfg.minEta && kaonsPlus[iPlus].eta <= trkCfg.maxEta) &&
          (kaonsMinus[iMinus].eta >= trkCfg.minEta && kaonsMinus[iMinus].eta <= trkCfg.maxEta) &&
          (TMath::Abs(kaonsPlus[iPlus].nSigmaKaon) <= phiCfg.nSigmaKaon) &&
          (TMath::Abs(kaonsMinus[iMinus].nSigmaKaon) <= phiCfg.nSigmaKaon);

      const Bool_t passStage = passPreMassStage &&
          (openingAngle >= phiCfg.minOpeningAngle && openingAngle <= phiCfg.maxOpeningAngle) &&
          (pairRapidity >= phiCfg.minPairRapidity && pairRapidity <= phiCfg.maxPairRapidity);

      if (passPreMassStage) {
        FemtoCandidate preMassCand = MakePhiCandidate(kaonsPlus[iPlus], kaonsMinus[iMinus], invMass, phiMom,
                                                      openingAngle, pairRapidity, dcaKK, eventIndex, speciesKey);
        m_phiQaPreMassLoose.push_back(preMassCand);
        if (PassPairTofCut(kaonsPlus[iPlus], kaonsMinus[iMinus])) {
          m_phiQaPreMassTofStrict.push_back(preMassCand);
        }
      }

      if (passStage) {
        m_phiQaLoose.push_back(MakePhiCandidate(kaonsPlus[iPlus], kaonsMinus[iMinus], invMass, phiMom, openingAngle,
                                                pairRapidity, dcaKK, eventIndex, speciesKey));
      }

      if (!PassPairTofCut(kaonsPlus[iPlus], kaonsMinus[iMinus])) continue;

      FillPhiPairKinematicsQa(invMass, phiMom, openingAngle, pairRapidity);

      if (!passStage) continue;
      if (m_histManager) {
        nPhiPairStage0++;
        m_histManager->Fill("hPhiPair_Mass_stage0", invMass);
        m_histManager->Fill("hPhiPair_Pt_stage0", phiMom.Pt());
        m_histManager->Fill("hPhiPair_Rapidity_stage0", pairRapidity);
        m_histManager->Fill("hPhiPair_OpeningAngle_stage0", openingAngle);
      }

      if (m_histManager) {
        nPhiPairTofStrict++;
        m_histManager->Fill("hPhiPair_Mass_tofStrict", invMass);
        m_histManager->Fill("hPhiPair_Pt_tofStrict", phiMom.Pt());
        m_histManager->Fill("hPhiPair_Rapidity_tofStrict", pairRapidity);
        m_histManager->Fill("hPhiPair_OpeningAngle_tofStrict", openingAngle);
        m_histManager->Fill("hPhiPair_MassVsPt_tofStrict", phiMom.Pt(), invMass);
        if (m_cent9 >= 0 && m_cent9 <= 8) {
          m_histManager->Fill("hPhiPair_MassVsCent_tofStrict", (Double_t)m_cent9, invMass);
        }
      }

      FillPhiCandidatePreCutQa(invMass, phiMom.Pt(), pairRapidity);

      out.push_back(MakePhiCandidate(kaonsPlus[iPlus], kaonsMinus[iMinus], invMass, phiMom, openingAngle,
                                     pairRapidity, dcaKK, eventIndex, speciesKey));
    }
  }
  if (m_histManager) {
    m_histManager->Fill("hPhiPair_NPairs_stage0", (Double_t)nPhiPairStage0);
    m_histManager->Fill("hPhiPair_NPairs_tofStrict", (Double_t)nPhiPairTofStrict);
  }
}

void StFemtoMaker::BuildRotatedPhiCandidates(const std::string& speciesKey, const std::vector<TrackState>& kaonsPlus,
                                             const std::vector<TrackState>& kaonsMinus, Int_t eventIndex) {
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (!fc.rotationEnabled) return;

  PhiCutConfig& phiCfg = ConfigManager::GetInstance().GetPhiCuts();
  TrackCutConfig& trkCfg = ConfigManager::GetInstance().GetTrackCuts();
  std::vector<FemtoCandidate>& out = m_eventCandidates[speciesKey];
  const Double_t mK = StPhiKKReconstruction::KaonMass();
  Int_t nRotCand = 0;

  for (size_t iPlus = 0; iPlus < kaonsPlus.size(); iPlus++) {
    for (size_t iMinus = 0; iMinus < kaonsMinus.size(); iMinus++) {
      TVector3 dcaPosPlus, dcaPosMinus;
      Double_t invMass;
      TVector3 phiMom;
      if (!ReconstructPhi(kaonsPlus[iPlus], kaonsMinus[iMinus], invMass, phiMom, dcaPosPlus, dcaPosMinus)) continue;

      Double_t openingAngle = CalculateOpeningAngle(kaonsPlus[iPlus], kaonsMinus[iMinus]);
      Double_t yLab = CalculatePairRapidity(invMass, phiMom);
      Double_t pairRapidity = ApplyRapidityFrame(yLab);
      Double_t dcaKK = CalculateDCA(kaonsPlus[iPlus], kaonsMinus[iMinus], dcaPosPlus, dcaPosMinus);

      const Bool_t passStage =
          (TMath::Abs(kaonsPlus[iPlus].DCA) <= phiCfg.maxDCAKaon) &&
          (TMath::Abs(kaonsMinus[iMinus].DCA) <= phiCfg.maxDCAKaon) &&
          (kaonsPlus[iPlus].pT >= trkCfg.minPt && kaonsPlus[iPlus].pT <= trkCfg.maxPt) &&
          (kaonsMinus[iMinus].pT >= trkCfg.minPt && kaonsMinus[iMinus].pT <= trkCfg.maxPt) &&
          (kaonsPlus[iPlus].eta >= trkCfg.minEta && kaonsPlus[iPlus].eta <= trkCfg.maxEta) &&
          (kaonsMinus[iMinus].eta >= trkCfg.minEta && kaonsMinus[iMinus].eta <= trkCfg.maxEta) &&
          (TMath::Abs(kaonsPlus[iPlus].nSigmaKaon) <= phiCfg.nSigmaKaon) &&
          (TMath::Abs(kaonsMinus[iMinus].nSigmaKaon) <= phiCfg.nSigmaKaon) &&
          (openingAngle >= phiCfg.minOpeningAngle && openingAngle <= phiCfg.maxOpeningAngle) &&
          (pairRapidity >= phiCfg.minPairRapidity && pairRapidity <= phiCfg.maxPairRapidity);
      if (!passStage) continue;
      if (!PassPairTofCut(kaonsPlus[iPlus], kaonsMinus[iMinus])) continue;

      TVector3 pMinus = TrackMomentum(kaonsMinus[iMinus]);
      Double_t eMinus = TMath::Sqrt(mK * mK + pMinus.Mag2());

      for (Int_t ir = 0; ir < fc.rotationN; ++ir) {
        Double_t deltaPhi = gRandom->Uniform(fc.rotationMinAngle, fc.rotationMaxAngle);
        if (m_histManager) m_histManager->Fill("hPhiRot_DeltaPhiApplied", deltaPhi);

        Float_t phiRot = kaonsPlus[iPlus].phi + (Float_t)deltaPhi;
        while (phiRot > TMath::Pi()) phiRot -= (Float_t)(2.0 * TMath::Pi());
        while (phiRot < -TMath::Pi()) phiRot += (Float_t)(2.0 * TMath::Pi());

        TVector3 pPlusRot;
        pPlusRot.SetPtEtaPhi(kaonsPlus[iPlus].pT, kaonsPlus[iPlus].eta, phiRot);
        Double_t ePlus = TMath::Sqrt(mK * mK + pPlusRot.Mag2());
        TVector3 phiMomRot = pPlusRot + pMinus;
        Double_t invMassRot =
            TMath::Sqrt(TMath::Max(0.0, (ePlus + eMinus) * (ePlus + eMinus) - phiMomRot.Mag2()));
        Double_t yLabRot = CalculatePairRapidity(invMassRot, phiMomRot);
        Double_t pairRapidityRot = ApplyRapidityFrame(yLabRot);

        if (m_histManager) {
          m_histManager->Fill("hPhiRot_MKK", invMassRot);
          m_histManager->Fill("hPhiRot_Pt", phiMomRot.Pt());
          m_histManager->Fill("hPhiRot_Rapidity", pairRapidityRot);
        }

        out.push_back(MakePhiCandidate(kaonsPlus[iPlus], kaonsMinus[iMinus], invMassRot, phiMomRot, openingAngle,
                                       pairRapidityRot, dcaKK, eventIndex, speciesKey));
        nRotCand++;
      }
    }
  }
  if (m_histManager) m_histManager->Fill("hPhiRot_NCand", (Double_t)nRotCand);
}

void StFemtoMaker::FillCentralityEventQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
                                         Int_t nBTOFMatch, Int_t nKaonPlus, Int_t nKaonMinus, Int_t nPhiCandidates,
                                         Int_t nProtons, Int_t nHe4, Int_t nDeuteron, Int_t nTriton, Int_t nHe3) {
  if (!m_histManager || cent9 < 0) return;
  const Double_t centX = (Double_t)cent9;
  m_histManager->Fill("hRawMult_vs_Cent9", centX, (Double_t)rawMult);
  if (refMultCorr >= 0.0) {
    m_histManager->Fill("hRefMultCorr_vs_Cent9", centX, refMultCorr);
    m_histManager->Fill("hRawMult_vs_RefMultCorr", refMultCorr, (Double_t)rawMult);
  }
  m_histManager->Fill("hNTracks_vs_Cent9", centX, (Double_t)nTracks);
  m_histManager->Fill("hTofMatchMult_vs_Cent9", centX, (Double_t)nBTOFMatch);
  m_histManager->Fill("hNKaonPlus_vs_Cent9", centX, (Double_t)nKaonPlus);
  m_histManager->Fill("hNKaonMinus_vs_Cent9", centX, (Double_t)nKaonMinus);
  m_histManager->Fill("hNPhiPairs_vs_Cent9", centX, (Double_t)nPhiCandidates);
  if (m_histManager->Get("hNProton_vs_Cent9")) {
    m_histManager->Fill("hNProton_vs_Cent9", centX, (Double_t)nProtons);
  }
  if (m_histManager->Get("hNHe4_vs_Cent9")) {
    m_histManager->Fill("hNHe4_vs_Cent9", centX, (Double_t)nHe4);
  }
  if (m_histManager->Get("hNDeuteron_vs_Cent9")) {
    m_histManager->Fill("hNDeuteron_vs_Cent9", centX, (Double_t)nDeuteron);
  }
  if (m_histManager->Get("hNTriton_vs_Cent9")) {
    m_histManager->Fill("hNTriton_vs_Cent9", centX, (Double_t)nTriton);
  }
  if (m_histManager->Get("hNHe3_vs_Cent9")) {
    m_histManager->Fill("hNHe3_vs_Cent9", centX, (Double_t)nHe3);
  }
}

TLorentzVector StFemtoMaker::ProtonP4(const TVector3& p) const {
  Double_t e = TMath::Sqrt(kProtonMass * kProtonMass + p.Mag2());
  return TLorentzVector(p.X(), p.Y(), p.Z(), e);
}

TLorentzVector StFemtoMaker::KaonP4(const TVector3& p) const {
  Double_t e = TMath::Sqrt(kKaonMass * kKaonMass + p.Mag2());
  return TLorentzVector(p.X(), p.Y(), p.Z(), e);
}

TLorentzVector StFemtoMaker::CandidateP4(const FemtoCandidate& cand) const {
  return cand.P4();
}

Double_t StFemtoMaker::ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const {
  TLorentzVector q = pA - pB;
  TLorentzVector pair = pA + pB;
  q.Boost(-pair.BoostVector());
  return 0.5 * q.Vect().Mag();
}

Bool_t StFemtoMaker::TracksOverlap(const FemtoCandidate& phiCand, const FemtoCandidate& trkCand) const {
  if (phiCand.source != kFemtoCandResonance) return kFALSE;
  Int_t idx = trkCand.trk.trackIndex;
  return (idx == phiCand.reso.dau1Index || idx == phiCand.reso.dau2Index);
}

std::string StFemtoMaker::HistName(const std::string& prefix, const std::string& channelName) const {
  return prefix + "_" + channelName;
}

Double_t StFemtoMaker::ComputeMomentumAngleRad(const TVector3& pA, const TVector3& pB) {
  const Double_t magA = pA.Mag();
  const Double_t magB = pB.Mag();
  if (magA <= 0.0 || magB <= 0.0) return -1.0;
  Double_t cosTheta = pA.Dot(pB) / (magA * magB);
  if (cosTheta > 1.0) cosTheta = 1.0;
  if (cosTheta < -1.0) cosTheta = -1.0;
  return TMath::ACos(cosTheta);
}

std::string StFemtoMaker::PhiPairMomAngleHistKeyWithSuffix(const std::string& channel, Bool_t vsMkk,
                                                                   const std::string& suffix) const {
  std::string key = vsMkk ? "hPhiPairMomAngle_vs_MKK_" : "hPhiPairMomAngle_";
  key += channel;
  key += suffix;
  return key;
}

std::string StFemtoMaker::PhiPairMomAngleHistKey(const std::string& channel, Bool_t vsMkk, Bool_t tofStrict) const {
  return PhiPairMomAngleHistKeyWithSuffix(channel, vsMkk, tofStrict ? "_tofStrict" : "");
}

void StFemtoMaker::FillPhiBachelorPairAngleQa() {
  if (!m_histManager) return;

  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  static const char* kBachelorKeys[] = {"proton", "deuteron", "triton", "he3", "he4", 0};

  for (Int_t ib = 0; kBachelorKeys[ib]; ++ib) {
    const std::string partB(kBachelorKeys[ib]);
    std::string channelName = std::string("phi_") + partB + "_signal";
    const FemtoConfig::ChannelDef* ch = femtoCfg.FindChannel(channelName);
    if (!ch || !ch->enabled) continue;

    FemtoCandidateStore::const_iterator itBach = m_eventCandidates.find(partB);
    if (itBach == m_eventCandidates.end() || itBach->second.empty()) continue;
    const std::vector<FemtoCandidate>& bachCands = itBach->second;

    const std::string h1dLoose = PhiPairMomAngleHistKey(channelName, kFALSE, kFALSE);
    const std::string h2dLoose = PhiPairMomAngleHistKey(channelName, kTRUE, kFALSE);
    const std::string h1dStrict = PhiPairMomAngleHistKey(channelName, kFALSE, kTRUE);
    const std::string h2dStrict = PhiPairMomAngleHistKey(channelName, kTRUE, kTRUE);
    const std::string h2dPreMass = PhiPairMomAngleHistKeyWithSuffix(channelName, kTRUE, "_preMass");
    const std::string h2dPreMassStrict = PhiPairMomAngleHistKeyWithSuffix(channelName, kTRUE, "_preMass_tofStrict");

    for (size_t ip = 0; ip < m_phiQaLoose.size(); ++ip) {
      const FemtoCandidate& phiCand = m_phiQaLoose[ip];
      if (phiCand.reso.invMass < ch->signalMin || phiCand.reso.invMass > ch->signalMax) continue;

      TVector3 pPhi(phiCand.px, phiCand.py, phiCand.pz);
      for (size_t jb = 0; jb < bachCands.size(); ++jb) {
        const FemtoCandidate& bach = bachCands[jb];
        if (TracksOverlap(phiCand, bach)) continue;
        TVector3 pBach(bach.px, bach.py, bach.pz);
        Double_t angle = ComputeMomentumAngleRad(pPhi, pBach);
        if (angle < 0.0) continue;
        if (m_histManager->Get(h1dLoose.c_str())) m_histManager->Fill(h1dLoose.c_str(), angle);
        if (m_histManager->Get(h2dLoose.c_str())) {
          m_histManager->Fill(h2dLoose.c_str(), angle, (Double_t)phiCand.reso.invMass);
        }
      }
    }

    for (size_t ip = 0; ip < m_phiQaPreMassLoose.size(); ++ip) {
      const FemtoCandidate& phiCand = m_phiQaPreMassLoose[ip];
      TVector3 pPhi(phiCand.px, phiCand.py, phiCand.pz);
      for (size_t jb = 0; jb < bachCands.size(); ++jb) {
        const FemtoCandidate& bach = bachCands[jb];
        if (TracksOverlap(phiCand, bach)) continue;
        TVector3 pBach(bach.px, bach.py, bach.pz);
        Double_t angle = ComputeMomentumAngleRad(pPhi, pBach);
        if (angle < 0.0) continue;
        if (m_histManager->Get(h2dPreMass.c_str())) {
          m_histManager->Fill(h2dPreMass.c_str(), angle, (Double_t)phiCand.reso.invMass);
        }
      }
    }

    for (size_t ip = 0; ip < m_phiQaPreMassTofStrict.size(); ++ip) {
      const FemtoCandidate& phiCand = m_phiQaPreMassTofStrict[ip];
      TVector3 pPhi(phiCand.px, phiCand.py, phiCand.pz);
      for (size_t jb = 0; jb < bachCands.size(); ++jb) {
        const FemtoCandidate& bach = bachCands[jb];
        if (TracksOverlap(phiCand, bach)) continue;
        TVector3 pBach(bach.px, bach.py, bach.pz);
        Double_t angle = ComputeMomentumAngleRad(pPhi, pBach);
        if (angle < 0.0) continue;
        if (m_histManager->Get(h2dPreMassStrict.c_str())) {
          m_histManager->Fill(h2dPreMassStrict.c_str(), angle, (Double_t)phiCand.reso.invMass);
        }
      }
    }

    FemtoCandidateStore::const_iterator itPhi = m_eventCandidates.find("phi");
    if (itPhi == m_eventCandidates.end()) continue;
    const std::vector<FemtoCandidate>& phiStore = itPhi->second;
    for (size_t ip = 0; ip < phiStore.size(); ++ip) {
      const FemtoCandidate& phiCand = phiStore[ip];
      if (phiCand.reso.invMass < ch->signalMin || phiCand.reso.invMass > ch->signalMax) continue;
      if (phiCand.reso.betaGamma <= 0.0f) continue;

      TVector3 pPhi(phiCand.px, phiCand.py, phiCand.pz);
      for (size_t jb = 0; jb < bachCands.size(); ++jb) {
        const FemtoCandidate& bach = bachCands[jb];
        if (TracksOverlap(phiCand, bach)) continue;
        TVector3 pBach(bach.px, bach.py, bach.pz);
        Double_t angle = ComputeMomentumAngleRad(pPhi, pBach);
        if (angle < 0.0) continue;
        if (m_histManager->Get(h1dStrict.c_str())) m_histManager->Fill(h1dStrict.c_str(), angle);
        if (m_histManager->Get(h2dStrict.c_str())) {
          m_histManager->Fill(h2dStrict.c_str(), angle, (Double_t)phiCand.reso.invMass);
        }
      }
    }
  }
}

void StFemtoMaker::FillSameEventPairs(const FemtoConfig::ChannelDef& ch) {
  FemtoCandidateStore::const_iterator itA = m_eventCandidates.find(ch.partA);
  FemtoCandidateStore::const_iterator itB = m_eventCandidates.find(ch.partB);
  if (itA == m_eventCandidates.end() || itB == m_eventCandidates.end()) return;

  const std::vector<FemtoCandidate>& candsA = itA->second;
  const std::vector<FemtoCandidate>& candsB = itB->second;
  if (candsA.empty() || candsB.empty()) return;

  std::string hName = HistName("hKstarSE", ch.name);
  std::string h2dName = HistName("hKstarSEVsCent", ch.name);
  const Double_t centX = (m_cent9 >= 0) ? (Double_t)m_cent9 : -0.5;
  for (size_t i = 0; i < candsA.size(); i++) {
    const FemtoCandidate& a = candsA[i];
    if (a.source == kFemtoCandResonance) {
      if (a.reso.invMass < ch.signalMin || a.reso.invMass > ch.signalMax) continue;
    }
    for (size_t j = 0; j < candsB.size(); j++) {
      const FemtoCandidate& b = candsB[j];
      if (TracksOverlap(a, b)) continue;
      Double_t kstar = ComputeKStar(CandidateP4(a), CandidateP4(b));
      if (m_histManager) {
        m_histManager->Fill(hName.c_str(), kstar);
        if (m_histManager->Get(h2dName.c_str())) {
          m_histManager->Fill(h2dName.c_str(), kstar, centX);
        }
        if (a.source == kFemtoCandResonance) {
          std::string hMkkK = HistName("hPhiMKK_vs_KstarSE", ch.name);
          if (m_histManager->Get(hMkkK.c_str())) {
            m_histManager->Fill(hMkkK.c_str(), a.reso.invMass, kstar, centX);
          }
        }
      }
    }
  }
}

Int_t StFemtoMaker::GetMixingBin(Float_t vz, Int_t cent9, Double_t psi2) const {
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
  Int_t centBin = 0;
  if (mix.nCentralityBins > 0 && cent9 >= 0) {
    centBin = cent9;
    if (centBin >= mix.nCentralityBins) centBin = mix.nCentralityBins - 1;
  }
  Int_t epBin = 0;
  if (mix.nEventPlaneBins > 1 && psi2 >= 0) {
    epBin = (Int_t)(psi2 / TMath::Pi() * mix.nEventPlaneBins);
    if (epBin < 0) epBin = 0;
    if (epBin >= mix.nEventPlaneBins) epBin = mix.nEventPlaneBins - 1;
  }
  return vzBin + mix.nVzBins * (centBin + mix.nCentralityBins * epBin);
}

void StFemtoMaker::FillMixedEventPairs(const FemtoConfig::ChannelDef& ch, Float_t vz, Int_t cent9, Double_t psi2) {
  FemtoCandidateStore::const_iterator itA = m_eventCandidates.find(ch.partA);
  FemtoCandidateStore::const_iterator itB = m_eventCandidates.find(ch.partB);
  if (itA == m_eventCandidates.end() || itB == m_eventCandidates.end()) return;
  const std::vector<FemtoCandidate>& candsA = itA->second;
  const std::vector<FemtoCandidate>& candsB = itB->second;
  if (candsA.empty() || candsB.empty()) return;

  Int_t mixBin = GetMixingBin(vz, cent9, psi2);
  std::map<Int_t, std::deque<FemtoMixingEvent> >::const_iterator poolIt = m_mixingPool.find(mixBin);
  if (poolIt == m_mixingPool.end() || poolIt->second.empty()) return;

  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  std::string hName = HistName("hKstarME", ch.name);
  std::string h2dName = HistName("hKstarMEVsCent", ch.name);
  const Double_t centX = (m_cent9 >= 0) ? (Double_t)m_cent9 : -0.5;

  auto fillMixedPair = [&](const FemtoCandidate& a, const FemtoCandidate& b) {
    if (a.source == kFemtoCandResonance) {
      if (a.reso.invMass < ch.signalMin || a.reso.invMass > ch.signalMax) return;
    }
    if (TracksOverlap(a, b)) return;
    Double_t kstar = ComputeKStar(CandidateP4(a), CandidateP4(b));
    if (m_histManager) {
      m_histManager->Fill(hName.c_str(), kstar);
      if (m_histManager->Get(h2dName.c_str())) {
        m_histManager->Fill(h2dName.c_str(), kstar, centX);
      }
      if (a.source == kFemtoCandResonance) {
        std::string hMkkK = HistName("hPhiMKK_vs_KstarME", ch.name);
        if (m_histManager->Get(hMkkK.c_str())) {
          m_histManager->Fill(hMkkK.c_str(), a.reso.invMass, kstar, centX);
        }
      }
    }
  };

  if (mix.IsBufferAllMode()) {
    const std::deque<FemtoMixingEvent>& pool = poolIt->second;
    for (size_t ie = 0; ie < pool.size(); ++ie) {
      const FemtoMixingEvent& mixEvt = pool[ie];
      FemtoCandidateStore::const_iterator mixB = mixEvt.candidates.find(ch.partB);
      if (mixB != mixEvt.candidates.end()) {
        const std::vector<FemtoCandidate>& bufB = mixB->second;
        for (size_t ia = 0; ia < candsA.size(); ++ia) {
          for (size_t ib = 0; ib < bufB.size(); ++ib) {
            fillMixedPair(candsA[ia], bufB[ib]);
          }
        }
      }
      if (mix.mixBothDirections) {
        FemtoCandidateStore::const_iterator mixA = mixEvt.candidates.find(ch.partA);
        if (mixA != mixEvt.candidates.end()) {
          const std::vector<FemtoCandidate>& bufA = mixA->second;
          for (size_t ia = 0; ia < bufA.size(); ++ia) {
            for (size_t ib = 0; ib < candsB.size(); ++ib) {
              fillMixedPair(bufA[ia], candsB[ib]);
            }
          }
        }
      }
    }
    return;
  }

  Int_t nPairs = (Int_t)candsA.size() * (Int_t)candsB.size();
  if (mix.maxMixedPairsPerEvent > 0 && nPairs > mix.maxMixedPairsPerEvent) {
    nPairs = mix.maxMixedPairsPerEvent;
  }
  if (nPairs < 1) nPairs = 1;

  for (Int_t ip = 0; ip < nPairs; ip++) {
    const FemtoMixingEvent& mixEvt = poolIt->second[(Int_t)gRandom->Uniform(0, poolIt->second.size())];
    FemtoCandidateStore::const_iterator mixB = mixEvt.candidates.find(ch.partB);
    if (mixB == mixEvt.candidates.end() || mixB->second.empty()) continue;

    const FemtoCandidate& a = candsA[(Int_t)gRandom->Uniform(0, candsA.size())];
    const FemtoCandidate& b = mixB->second[(Int_t)gRandom->Uniform(0, mixB->second.size())];
    fillMixedPair(a, b);
  }
}

void StFemtoMaker::StoreEventForMixing(Float_t vz, Int_t cent9, Double_t psi2) {
  if (m_eventCandidates.empty()) return;
  Int_t mixBin = GetMixingBin(vz, cent9, psi2);
  FemtoMixingEvent evt;
  evt.candidates = m_eventCandidates;
  std::deque<FemtoMixingEvent>& pool = m_mixingPool[mixBin];
  pool.push_back(evt);
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  while ((Int_t)pool.size() > mix.bufferSize) pool.pop_front();
}

void StFemtoMaker::FillCandidateQA() {
  if (!m_histManager) return;
  FemtoCandidateStore::const_iterator itP = m_eventCandidates.find("proton");
  if (itP != m_eventCandidates.end()) {
    for (size_t i = 0; i < itP->second.size(); i++) {
      const FemtoCandidate& c = itP->second[i];
      m_histManager->Fill("hP_Pt", c.pt);
      m_histManager->Fill("hP_Eta", c.eta);
      m_histManager->Fill("hP_Phi", c.phi);
      m_histManager->Fill("hP_NSigmaProton", c.trk.nSigmaProton);
      m_histManager->Fill("hP_DCA", c.trk.dca);
      if (c.trk.mass2 > -900) m_histManager->Fill("hP_Mass2", c.trk.mass2);
    }
    m_histManager->Fill("hP_NCand", (Double_t)itP->second.size());
  }

  FemtoCandidateStore::const_iterator itKm = m_eventCandidates.find("kaon_minus");
  if (itKm != m_eventCandidates.end()) {
    for (size_t i = 0; i < itKm->second.size(); i++) {
      const FemtoCandidate& c = itKm->second[i];
      m_histManager->Fill("hKm_Pt", c.pt);
      m_histManager->Fill("hKm_Eta", c.eta);
      m_histManager->Fill("hKm_Phi", c.phi);
      m_histManager->Fill("hKm_NSigmaKaon", c.trk.nSigmaKaon);
      m_histManager->Fill("hKm_DCA", c.trk.dca);
      if (c.trk.mass2 > -900) m_histManager->Fill("hKm_Mass2", c.trk.mass2);
    }
    m_histManager->Fill("hKm_NCand", (Double_t)itKm->second.size());
  }

  FemtoCandidateStore::const_iterator itH = m_eventCandidates.find("he4");
  if (itH != m_eventCandidates.end()) {
    for (size_t i = 0; i < itH->second.size(); i++) {
      const FemtoCandidate& c = itH->second[i];
      m_histManager->Fill("hHe4_Pt", c.pt);
      m_histManager->Fill("hHe4_Eta", c.eta);
      m_histManager->Fill("hHe4_Phi", c.phi);
      m_histManager->Fill("hHe4_NSigmaHe4", c.trk.nSigmaHe4);
      m_histManager->Fill("hHe4_DCA", c.trk.dca);
      if (c.trk.mass2 > -900) m_histManager->Fill("hHe4_Mass2", c.trk.mass2);
    }
    m_histManager->Fill("hHe4_NCand", (Double_t)itH->second.size());
  }

  FemtoCandidateStore::const_iterator itD = m_eventCandidates.find("deuteron");
  if (itD != m_eventCandidates.end()) {
    for (size_t i = 0; i < itD->second.size(); i++) {
      const FemtoCandidate& c = itD->second[i];
      m_histManager->Fill("hDeuteron_Pt", c.pt);
      m_histManager->Fill("hDeuteron_Eta", c.eta);
      m_histManager->Fill("hDeuteron_Phi", c.phi);
      m_histManager->Fill("hDeuteron_NSigmaDeuteron", c.trk.nSigmaDeuteron);
      m_histManager->Fill("hDeuteron_DCA", c.trk.dca);
      if (c.trk.mass2 > -900) m_histManager->Fill("hDeuteron_Mass2", c.trk.mass2);
    }
    m_histManager->Fill("hDeuteron_NCand", (Double_t)itD->second.size());
  }

  FemtoCandidateStore::const_iterator itT = m_eventCandidates.find("triton");
  if (itT != m_eventCandidates.end()) {
    for (size_t i = 0; i < itT->second.size(); i++) {
      const FemtoCandidate& c = itT->second[i];
      m_histManager->Fill("hTriton_Pt", c.pt);
      m_histManager->Fill("hTriton_Eta", c.eta);
      m_histManager->Fill("hTriton_Phi", c.phi);
      m_histManager->Fill("hTriton_NSigmaTriton", c.trk.nSigmaTriton);
      m_histManager->Fill("hTriton_DCA", c.trk.dca);
      if (c.trk.mass2 > -900) m_histManager->Fill("hTriton_Mass2", c.trk.mass2);
    }
    m_histManager->Fill("hTriton_NCand", (Double_t)itT->second.size());
  }

  FemtoCandidateStore::const_iterator itH3 = m_eventCandidates.find("he3");
  if (itH3 != m_eventCandidates.end()) {
    for (size_t i = 0; i < itH3->second.size(); i++) {
      const FemtoCandidate& c = itH3->second[i];
      m_histManager->Fill("hHe3_Pt", c.pt);
      m_histManager->Fill("hHe3_Eta", c.eta);
      m_histManager->Fill("hHe3_Phi", c.phi);
      m_histManager->Fill("hHe3_NSigmaHe3", c.trk.nSigmaHe3);
      m_histManager->Fill("hHe3_DCA", c.trk.dca);
      if (c.trk.mass2 > -900) m_histManager->Fill("hHe3_Mass2", c.trk.mass2);
    }
    m_histManager->Fill("hHe3_NCand", (Double_t)itH3->second.size());
  }

  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  auto fillPhiMassWindows = [&](const std::string& speciesKey, Bool_t isRot) {
    FemtoCandidateStore::const_iterator it = m_eventCandidates.find(speciesKey);
    if (it == m_eventCandidates.end()) return;
    for (size_t i = 0; i < it->second.size(); i++) {
      const FemtoCandidate& c = it->second[i];
      const Double_t mass = c.reso.invMass;
      if (!isRot) {
        m_histManager->Fill("hPhi_MKK_allForFemto", mass);
        m_histManager->Fill("hPhi_MKK", mass);
        m_histManager->Fill("hPhi_Pt", c.pt);
        m_histManager->Fill("hPhi_Rapidity", c.y);
        m_histManager->Fill("hPhi_PtVsY_PostCut", c.y, c.pt);
        if (c.reso.betaGamma > 0.0f) {
          m_histManager->Fill("hPhi_MKK_vs_BetaGamma", mass, (Double_t)c.reso.betaGamma);
        }
      }
      for (size_t ic = 0; ic < femtoCfg.channels.size(); ic++) {
        const FemtoConfig::ChannelDef& ch = femtoCfg.channels[ic];
        if (!ch.enabled || ch.partA != speciesKey) continue;
        if (mass >= ch.signalMin && mass <= ch.signalMax) {
          const TString chName(ch.name.c_str());
          if (chName.EndsWith("_signal")) {
            m_histManager->Fill("hPhi_MKK_signal", mass);
            m_histManager->Fill("hPhi_PtVsY_signal", c.y, c.pt);
          } else if (chName.EndsWith("_leftSB")) {
            m_histManager->Fill("hPhi_MKK_leftSB", mass);
          } else if (chName.EndsWith("_rightSB")) {
            m_histManager->Fill("hPhi_MKK_rightSB", mass);
          }
        }
      }
      if (isRot) {
        m_histManager->Fill("hPhi_MKK_rot", mass);
        m_histManager->Fill("hPhi_PtVsY_rot", c.y, c.pt);
      }
    }
    if (!isRot) m_histManager->Fill("hPhi_NCand", (Double_t)it->second.size());
  };

  fillPhiMassWindows("phi", kFALSE);
  fillPhiMassWindows(femtoCfg.rotationSpeciesKey, kTRUE);
}
