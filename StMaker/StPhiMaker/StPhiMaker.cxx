#include "StPhiMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "kinematics.h"
#include "cuts/EventCutConfig.h"
#include "cuts/TrackCutConfig.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "cuts/MixingConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "CentralityHelper.h"
#include "StPhiKKReconstruction.h"
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
#include "TRandom.h"
#include <iostream>
#include <vector>
#include <utility>

namespace {
  /** Set to 1 to print which cut rejects each event (first kDebugPhiMakerMaxEvents failures only). */
  const Int_t kDebugPhiMaker = 0;
  const Int_t kDebugPhiMakerMaxEvents = 200;
}

//-----------------------------------------------------------------------------
StPhiMaker::StPhiMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName)
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

//-----------------------------------------------------------------------------
StPhiMaker::~StPhiMaker() {
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
}

//-----------------------------------------------------------------------------
StPhiMaker* createStPhiMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StPhiMaker(name, picoMaker, outName);
}

extern "C" void* createStPhiMakerC(const char* name, void* picoMaker, const char* outName) {
  return (void*)createStPhiMaker(name, (StPicoDstMaker*)picoMaker, outName);
}

//-----------------------------------------------------------------------------
Int_t StPhiMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StPhiMaker] GetHistConfigPath() returned empty; no histograms will be filled." << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StPhiMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStOK;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StPhiMaker] CentralityHelper init failed" << std::endl;
  }

  if (!ConfigManager::GetInstance().GetPhiCuts().FinalizeRapidityFrame(
          ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StPhiMaker] FinalizeRapidityFrame failed; fix maker/centrality YAML." << std::endl;
    return kStErr;
  }

  return kStOK;
}

//-----------------------------------------------------------------------------
void StPhiMaker::Clear(Option_t* opt) {}

//-----------------------------------------------------------------------------
Int_t StPhiMaker::Make() {
  if (!mPicoDstMaker) {
    return kStWarn;
  }
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) {
    return kStWarn;
  }

  StPicoEvent* event = mPicoDst->event();
  if (!event) {
    return kStWarn;
  }

  mEventCounter++;

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
    if (mode == "fxtmult") {
      rawMult = event->fxtMult();
    }
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
  static Int_t s_centDebugCount = 0;

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(runId, centReason)) {
      if (kDebugPhiMaker && s_centDebugCount++ < kDebugPhiMakerMaxEvents) {
        std::cout << "[StPhiMaker] event=" << mEventCounter << " CENT: " << CentralityHelper::RejectReasonString(centReason) << std::endl;
      }
      return kStOK;
    }
  }

  // Event-level fills
  if (m_histManager) {
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

  if (!PassEventCuts(pVtx.Z(), vr, refMult, vzVpd)) {
    return kStOK;
  }

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
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) {
      if (kDebugPhiMaker && s_centDebugCount++ < kDebugPhiMakerMaxEvents) {
        std::cout << "[StPhiMaker] event=" << mEventCounter << " CENT: " << CentralityHelper::RejectReasonString(centReason) << std::endl;
      }
      return kStOK;
    }

    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight, centReason)) {
      if (kDebugPhiMaker && s_centDebugCount++ < kDebugPhiMakerMaxEvents) {
        std::cout << "[StPhiMaker] event=" << mEventCounter << " CENT: " << CentralityHelper::RejectReasonString(centReason) << std::endl;
      }
      return kStOK;
    }

    if (m_histManager) {
      m_histManager->Fill("hCentralityRaw", (Double_t)m_cent9);
      m_histManager->Fill("hRefMultCorr", m_refMultCorr);
      m_histManager->Fill("hCentralityVsVz", vz, (Double_t)m_cent9);
      m_histManager->Fill("hRefMultWeight", m_centWeight);
      m_histManager->Fill("hRefMultVsNTOFMatchAfter", (Double_t)nBTOFMatch, (Double_t)rawMult);
    }

    if (!m_centrality->AcceptCentBin(m_cent9, m_refMultCorr, centReason)) {
      if (kDebugPhiMaker && s_centDebugCount++ < kDebugPhiMakerMaxEvents) {
        std::cout << "[StPhiMaker] event=" << mEventCounter << " CENT: " << CentralityHelper::RejectReasonString(centReason)
                  << " cent9=" << m_cent9 << std::endl;
      }
      return kStOK;
    }

    m_centralityPercent = CentralityHelper::Cent9ToPercentile(m_cent9);
    if (m_histManager) {
      const Double_t w = centCfg.useWeight ? m_centWeight : 1.0;
      TH1* hCent = m_histManager->Get("hCentrality");
      if (hCent) hCent->Fill((Double_t)m_cent9, w);
      TH1* hCent16 = m_histManager->Get("hCentrality16");
      if (hCent16) hCent16->Fill((Double_t)m_cent16, w);
    }
  }

  const Int_t kMaxKaons = 2000;
  std::vector<Track_t> kaonsPlus;
  std::vector<Track_t> kaonsMinus;
  kaonsPlus.reserve(kMaxKaons / 2);
  kaonsMinus.reserve(kMaxKaons / 2);

  Int_t nTracks = mPicoDst->numberOfTracks();
  if (m_histManager) m_histManager->Fill("hNTracks", (Double_t)nTracks);
  PhiCutConfig& phiCfg = ConfigManager::GetInstance().GetPhiCuts();
  if (phiCfg.maxNTr > 0 && nTracks > phiCfg.maxNTr) {
    if (kDebugPhiMaker && mEventCounter <= kDebugPhiMakerMaxEvents) {
      std::cout << "[StPhiMaker] event=" << mEventCounter << " CUT: maxNTr nTracks=" << nTracks << " max=" << phiCfg.maxNTr << std::endl;
    }
    return kStOK;
  }

  Double_t Qx = 0.0, Qy = 0.0;

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

    if (!PassKaonCuts(trk, pVtx)) continue;

    Track_t track;
    BuildTrack(track, trk, event, pVtx);
    FillTofInfo(track, trk, pMom, btofIndex);

    if (m_histManager && track.tofMatch) {
      m_histManager->Fill("hMass2VsP_TpcKaon", pMom.Mag(), track.mass2);
      m_histManager->Fill("hMass2VsPt_TpcKaon", pt, track.mass2);
    }

    if (IsKaon(track)) {
      if (m_histManager) {
        m_histManager->Fill("hK_Pt", track.pT);
        m_histManager->Fill("hK_Eta", track.eta);
        m_histManager->Fill("hK_NSigma", track.nSigmaKaon);
        if (track.tofMatch) {
          m_histManager->Fill("hMass2VsP_IsKaon", pMom.Mag(), track.mass2);
          m_histManager->Fill("hMass2VsPt_IsKaon", pt, track.mass2);
        }
      }
      if (track.charge > 0 && (Int_t)kaonsPlus.size() < kMaxKaons) {
        kaonsPlus.push_back(track);
      } else if (track.charge < 0 && (Int_t)kaonsMinus.size() < kMaxKaons) {
        kaonsMinus.push_back(track);
      }
    }
  }

  if (m_histManager) m_histManager->Fill("hTofMatchMult", (Double_t)nBTOFMatch);

  Int_t nKaonPlus = (Int_t)kaonsPlus.size();
  Int_t nKaonMinus = (Int_t)kaonsMinus.size();
  if (m_histManager) {
    m_histManager->Fill("hNKaonPlusVsNKaonMinus", (Double_t)nKaonPlus, (Double_t)nKaonMinus);
    m_histManager->Fill("hNKaonPlus", (Double_t)nKaonPlus);
    m_histManager->Fill("hNKaonMinus", (Double_t)nKaonMinus);
  }

  // Phi reconstruction: ReconstructPhi pairs
  Int_t nPhiPairs = 0;
  Int_t nPhiPairStage0 = 0;
  Int_t nPhiPairTofStrict = 0;
  const TrackCutConfig& trkCfg = ConfigManager::GetInstance().GetTrackCuts();
  const PhiCutConfig& phiCfgRef = ConfigManager::GetInstance().GetPhiCuts();
  for (size_t iPlus = 0; iPlus < kaonsPlus.size(); iPlus++) {
    for (size_t iMinus = 0; iMinus < kaonsMinus.size(); iMinus++) {
      TVector3 dcaMeasPlus, dcaMeasMinus;
      Double_t dcaKK = CalculateDCA(kaonsPlus[iPlus], kaonsMinus[iMinus], dcaMeasPlus, dcaMeasMinus);
      if (m_histManager) {
        m_histManager->Fill("hDCAKK_All", dcaKK);
      }

      Double_t invMass;
      TVector3 phiMom, dcaPosPlus, dcaPosMinus;
      if (!ReconstructPhi(kaonsPlus[iPlus], kaonsMinus[iMinus], invMass, phiMom, dcaPosPlus, dcaPosMinus)) continue;

      nPhiPairs++;
      if (m_histManager) {
        m_histManager->Fill("hDCAKK_Pass", dcaKK);
      }

      Double_t openingAngle = CalculateOpeningAngle(kaonsPlus[iPlus], kaonsMinus[iMinus]);
      Double_t yLab = CalculatePairRapidity(invMass, phiMom);
      Double_t pairRapidity = ApplyRapidityFrame(yLab);

      const Bool_t passStage0 =
          (TMath::Abs(kaonsPlus[iPlus].DCA) <= phiCfgRef.maxDCAKaon) &&
          (TMath::Abs(kaonsMinus[iMinus].DCA) <= phiCfgRef.maxDCAKaon) &&
          (kaonsPlus[iPlus].pT >= trkCfg.minPt && kaonsPlus[iPlus].pT <= trkCfg.maxPt) &&
          (kaonsMinus[iMinus].pT >= trkCfg.minPt && kaonsMinus[iMinus].pT <= trkCfg.maxPt) &&
          (kaonsPlus[iPlus].eta >= trkCfg.minEta && kaonsPlus[iPlus].eta <= trkCfg.maxEta) &&
          (kaonsMinus[iMinus].eta >= trkCfg.minEta && kaonsMinus[iMinus].eta <= trkCfg.maxEta) &&
          (TMath::Abs(kaonsPlus[iPlus].nSigmaKaon) <= phiCfgRef.nSigmaKaon) &&
          (TMath::Abs(kaonsMinus[iMinus].nSigmaKaon) <= phiCfgRef.nSigmaKaon) &&
          (kaonsPlus[iPlus].nHitsFit >= trkCfg.minNHitsFit) &&
          (kaonsMinus[iMinus].nHitsFit >= trkCfg.minNHitsFit) &&
          (pairRapidity >= phiCfgRef.minPairRapidity && pairRapidity <= phiCfgRef.maxPairRapidity);
      if (passStage0 && m_histManager) {
        nPhiPairStage0++;
        m_histManager->Fill("hPhiPair_Mass_stage0", invMass);
        m_histManager->Fill("hPhiPair_Pt_stage0", phiMom.Pt());
        m_histManager->Fill("hPhiPair_Rapidity_stage0", pairRapidity);
        m_histManager->Fill("hPhiPair_Eta_stage0", phiMom.Eta());
        m_histManager->Fill("hPhiPair_P_stage0", phiMom.Mag());
        m_histManager->Fill("hPhiPair_OpeningAngle_stage0", openingAngle);
      }

      if (!PassPairTofCut(kaonsPlus[iPlus], kaonsMinus[iMinus])) continue;

      if (passStage0 && m_histManager) {
        nPhiPairTofStrict++;
        m_histManager->Fill("hPhiPair_Mass_tofStrict", invMass);
        m_histManager->Fill("hPhiPair_Pt_tofStrict", phiMom.Pt());
        m_histManager->Fill("hPhiPair_Rapidity_tofStrict", pairRapidity);
        m_histManager->Fill("hPhiPair_Eta_tofStrict", phiMom.Eta());
        m_histManager->Fill("hPhiPair_P_tofStrict", phiMom.Mag());
        m_histManager->Fill("hPhiPair_OpeningAngle_tofStrict", openingAngle);
        m_histManager->Fill("hPhiPair_MassVsPt_tofStrict", phiMom.Pt(), invMass);
        if (m_cent9 >= 0 && m_cent9 <= 8) {
          m_histManager->Fill("hPhiPair_MassVsCent_tofStrict", (Double_t)m_cent9, invMass);
        }
      }

      FillPhiPairHistograms(invMass, phiMom, openingAngle, pairRapidity, nKaonPlus, nKaonMinus, kTRUE);
    }
  }
  if (m_histManager) {
    m_histManager->Fill("hPhiPair_NPairs_stage0", (Double_t)nPhiPairStage0);
    m_histManager->Fill("hPhiPair_NPairs_tofStrict", (Double_t)nPhiPairTofStrict);
  }

  if (m_histManager && centCfg.fillCentralityQA && m_cent9 >= 0) {
    FillCentralityEventQA(m_cent9, rawMult, m_refMultCorr, nTracks, nBTOFMatch, nKaonPlus, nKaonMinus, nPhiPairs);
  }

  FillMixedEventPairs(kaonsPlus, kaonsMinus, pVtx.Z());
  StoreEventForMixing(kaonsPlus, kaonsMinus, pVtx.Z());

  // All combinations (simple invariant mass, strict TOF pair cut applied)
  if (m_histManager) {
    for (size_t iPlus = 0; iPlus < kaonsPlus.size(); iPlus++) {
      for (size_t iMinus = 0; iMinus < kaonsMinus.size(); iMinus++) {
        if (!PassPairTofCut(kaonsPlus[iPlus], kaonsMinus[iMinus])) continue;
        Double_t invMass = CalculateInvariantMass(kaonsPlus[iPlus], kaonsMinus[iMinus],
                                                  StPhiKKReconstruction::KaonMass(),
                                                  StPhiKKReconstruction::KaonMass());
        m_histManager->Fill("hMKK_AllCombinations", invMass);
      }
    }
  }

  // Event plane
  TVector2 Q(Qx, Qy);
  if (m_histManager) {
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

//-----------------------------------------------------------------------------
Int_t StPhiMaker::Finish() {
  FinalizeBackgroundSubtractedHistogram();
  if (mOutName != "") {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    if (m_histManager) m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StPhiMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    std::cout << "[StPhiMaker] centrality summary: ok=" << m_centrality->CountOk()
              << " badRun=" << m_centrality->CountBadRun()
              << " pileup=" << m_centrality->CountPileup()
              << " invalidCent=" << m_centrality->CountInvalidCent()
              << " binRejected=" << m_centrality->CountBinRejected() << std::endl;
    m_centrality->Finish();
  }
  return kStOK;
}

//-----------------------------------------------------------------------------
void StPhiMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::PassEventCuts(Float_t vz, Float_t vr, Int_t refMult, Float_t vzVpd) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  static Int_t s_debugFailCount = 0;

  if (vz < ev.minVz || vz > ev.maxVz) {
    if (kDebugPhiMaker && s_debugFailCount++ < kDebugPhiMakerMaxEvents)
      std::cout << "[StPhiMaker] CUT: Vz range vz=" << vz << " min=" << ev.minVz << " max=" << ev.maxVz << std::endl;
    return kFALSE;
  }
  if (vr > ev.maxVr) {
    if (kDebugPhiMaker && s_debugFailCount++ < kDebugPhiMakerMaxEvents)
      std::cout << "[StPhiMaker] CUT: maxVr vr=" << vr << " max=" << ev.maxVr << std::endl;
    return kFALSE;
  }
  if (refMult < ev.minRefMult) {
    if (kDebugPhiMaker && s_debugFailCount++ < kDebugPhiMakerMaxEvents)
      std::cout << "[StPhiMaker] CUT: minRefMult refMult=" << refMult << " min=" << ev.minRefMult << std::endl;
    return kFALSE;
  }
  if (refMult > ev.maxRefMult) {
    if (kDebugPhiMaker && s_debugFailCount++ < kDebugPhiMakerMaxEvents)
      std::cout << "[StPhiMaker] CUT: maxRefMult refMult=" << refMult << " max=" << ev.maxRefMult << std::endl;
    return kFALSE;
  }
  if (TMath::Abs(vz - vzVpd) > ev.maxVzDiff && TMath::Abs(vzVpd) < ev.maxAbsVzVpd) {
    if (kDebugPhiMaker && s_debugFailCount++ < kDebugPhiMakerMaxEvents)
      std::cout << "[StPhiMaker] CUT: maxVzDiff |vz-vzVpd|=" << TMath::Abs(vz - vzVpd) << " max=" << ev.maxVzDiff
                << " vzVpd=" << vzVpd << std::endl;
    return kFALSE;
  }
  return kTRUE;
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::PassTrackCuts(StPicoTrack* trk, TVector3& pVtx) {
  TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
  if (trk->nHitsFit() < tr.minNHitsFit) return kFALSE;
  if ((Float_t)trk->nHitsFit() / (Float_t)trk->nHitsMax() < tr.minNHitsRatio) return kFALSE;
  if (trk->nHitsDedx() < tr.minNHitsDedx) return kFALSE;
  if (trk->chi2() > tr.maxChi2) return kFALSE;
  TVector3 pMom = trk->pMom();
  if (pMom.Mag() < 1e-4) return kFALSE;
  Float_t pt = pMom.Perp();
  Float_t eta = pMom.PseudoRapidity();
  if (pt < tr.minPt || pt > tr.maxPt) return kFALSE;
  if (eta < tr.minEta || eta > tr.maxEta) return kFALSE;
  if (trk->gDCA(pVtx).Mag() > tr.maxDCA) return kFALSE;
  if (tr.requirePrimaryTrack && !trk->isPrimary()) return kFALSE;
  return kTRUE;
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::PassKaonCuts(StPicoTrack* trk, TVector3& pVtx) {
  if (!PassTrackCuts(trk, pVtx)) return kFALSE;
  PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
  if (trk->gDCA(pVtx).Mag() > phi.maxDCAKaon) return kFALSE;
  if (TMath::Abs(trk->nSigmaKaon()) > phi.nSigmaKaon) return kFALSE;
  return kTRUE;
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::PassTrackCuts(const Track_t& trk) {
  TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
  if (trk.nHitsFit < tr.minNHitsFit) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < tr.minNHitsRatio) return kFALSE;
  if (trk.nHitsDedx < tr.minNHitsDedx) return kFALSE;
  if (trk.DCA > tr.maxDCA) return kFALSE;
  if (trk.eta < tr.minEta || trk.eta > tr.maxEta) return kFALSE;
  if (trk.pT < tr.minPt || trk.pT > tr.maxPt) return kFALSE;
  if (trk.chi2 > tr.maxChi2) return kFALSE;
  return kTRUE;
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::IsKaon(const Track_t& trk) {
  if (!PassTrackCuts(trk)) return kFALSE;
  PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
  if (trk.DCA > phi.maxDCAKaon) return kFALSE;
  if (TMath::Abs(trk.nSigmaKaon) > phi.nSigmaKaon) return kFALSE;
  return PassTofKaonPid(trk);
}

//-----------------------------------------------------------------------------
void StPhiMaker::BuildTrack(Track_t& track, StPicoTrack* pico, StPicoEvent* event, TVector3& pVtx) {
  // Use primary momentum consistently for prompt phi -> K+K- kinematics.
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
}

//-----------------------------------------------------------------------------
PhiKkTrackState StPhiMaker::ToPhiKkTrack(const Track_t& trk) {
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

//-----------------------------------------------------------------------------
TVector3 StPhiMaker::TrackMomentum(const Track_t& trk) const {
  return StPhiKKReconstruction::TrackMomentum(ToPhiKkTrack(trk));
}

//-----------------------------------------------------------------------------
StPhysicalHelixD StPhiMaker::BuildHelix(const Track_t& trk) {
  return StPhiKKReconstruction::BuildHelix(ToPhiKkTrack(trk));
}

//-----------------------------------------------------------------------------
Double_t StPhiMaker::CalculateDCA(const Track_t& trk1, const Track_t& trk2, TVector3& dcaPos1, TVector3& dcaPos2) {
  return StPhiKKReconstruction::CalculateDCA(ToPhiKkTrack(trk1), ToPhiKkTrack(trk2), dcaPos1, dcaPos2);
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::ReconstructPhi(const Track_t& kPlus, const Track_t& kMinus, Double_t& invMass, TVector3& phiMom,
                                  TVector3& dcaPosPlus, TVector3& dcaPosMinus) {
  return StPhiKKReconstruction::ReconstructPhi(ToPhiKkTrack(kPlus), ToPhiKkTrack(kMinus), invMass, phiMom, dcaPosPlus,
                                               dcaPosMinus);
}

//-----------------------------------------------------------------------------
Double_t StPhiMaker::CalculateInvariantMass(const Track_t& trk1, const Track_t& trk2, Double_t mass1, Double_t mass2) {
  return StPhiKKReconstruction::CalculateInvariantMass(ToPhiKkTrack(trk1), ToPhiKkTrack(trk2), mass1, mass2);
}

//-----------------------------------------------------------------------------
Double_t StPhiMaker::CalculateOpeningAngle(const Track_t& trk1, const Track_t& trk2) {
  return StPhiKKReconstruction::CalculateOpeningAngle(ToPhiKkTrack(trk1), ToPhiKkTrack(trk2));
}

//-----------------------------------------------------------------------------
Double_t StPhiMaker::CalculatePairRapidity(Double_t invMass, const TVector3& phiMom) {
  return StPhiKKReconstruction::CalculatePairRapidity(invMass, phiMom);
}

//-----------------------------------------------------------------------------
Double_t StPhiMaker::ApplyRapidityFrame(Double_t yLab) const {
  return StPhiKKReconstruction::ApplyRapidityFrame(yLab);
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::PassTofKaonPid(const Track_t& trk) const {
  return StPhiKKReconstruction::PassTofKaonPid(ToPhiKkTrack(trk));
}

//-----------------------------------------------------------------------------
Bool_t StPhiMaker::PassPairTofCut(const Track_t& kPlus, const Track_t& kMinus) const {
  return StPhiKKReconstruction::PassPairTofCut(ToPhiKkTrack(kPlus), ToPhiKkTrack(kMinus));
}

//-----------------------------------------------------------------------------
void StPhiMaker::FillTofInfo(Track_t& track, StPicoTrack* trk, const TVector3& pMom, Int_t btofIndex) {
  (void)trk;
  StPicoBTofPidTraits* tof = 0;
  if (btofIndex >= 0) tof = mPicoDst->btofPidTraits(btofIndex);
  StPhiKKReconstruction::FillTofInfo(track.mass2, track.deltaOneOverBeta, track.tofMatch, tof, pMom);
}

//-----------------------------------------------------------------------------
Int_t StPhiMaker::GetMixingVzBin(Float_t vz) const {
  const EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  if (mix.nVzBins <= 0) return 0;

  Double_t vzSpan = ev.maxVz - ev.minVz;
  if (vzSpan <= 0.0) return 0;

  Int_t vzBin = (Int_t)((vz - ev.minVz) / vzSpan * mix.nVzBins);
  if (vzBin < 0) vzBin = 0;
  if (vzBin >= mix.nVzBins) vzBin = mix.nVzBins - 1;
  return vzBin;
}

//-----------------------------------------------------------------------------
void StPhiMaker::FillMixedEventPairs(const std::vector<Track_t>& kaonsPlus, const std::vector<Track_t>& kaonsMinus, Float_t vz) {
  if (!m_histManager || kaonsPlus.empty() || kaonsMinus.empty()) return;

  Int_t vzBin = GetMixingVzBin(vz);
  std::map<Int_t, std::deque<PhiMixingEvent> >::const_iterator poolIt = m_phiMixingPool.find(vzBin);
  if (poolIt == m_phiMixingPool.end() || poolIt->second.empty()) return;

  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  Int_t nPairs = (Int_t)kaonsPlus.size() * (Int_t)kaonsMinus.size();
  if (nPairs > 1000) nPairs = 1000;
  if (nPairs < 1) nPairs = 1;

  for (Int_t iPair = 0; iPair < nPairs; iPair++) {
    const PhiMixingEvent& mixEvt = poolIt->second[(Int_t)gRandom->Uniform(0, poolIt->second.size())];
    if (mixEvt.kaonsPlus.empty() || mixEvt.kaonsMinus.empty()) continue;

    const Track_t& kPlus = kaonsPlus[(Int_t)gRandom->Uniform(0, kaonsPlus.size())];
    const Track_t& kMinus = mixEvt.kaonsMinus[(Int_t)gRandom->Uniform(0, mixEvt.kaonsMinus.size())];

    Double_t invMass;
    TVector3 phiMom, dcaPosPlus, dcaPosMinus;
    if (!ReconstructPhi(kPlus, kMinus, invMass, phiMom, dcaPosPlus, dcaPosMinus)) continue;
    if (!PassPairTofCut(kPlus, kMinus)) continue;

    Double_t openingAngle = CalculateOpeningAngle(kPlus, kMinus);
    Double_t yLab = CalculatePairRapidity(invMass, phiMom);
    Double_t pairRapidity = ApplyRapidityFrame(yLab);
    FillPhiPairHistograms(invMass, phiMom, openingAngle, pairRapidity, 0, 0, kFALSE);
  }

  (void)mix;
}

//-----------------------------------------------------------------------------
void StPhiMaker::StoreEventForMixing(const std::vector<Track_t>& kaonsPlus, const std::vector<Track_t>& kaonsMinus, Float_t vz) {
  if (kaonsPlus.empty() && kaonsMinus.empty()) return;

  Int_t vzBin = GetMixingVzBin(vz);
  PhiMixingEvent mixEvt;
  mixEvt.kaonsPlus = kaonsPlus;
  mixEvt.kaonsMinus = kaonsMinus;

  std::deque<PhiMixingEvent>& pool = m_phiMixingPool[vzBin];
  pool.push_back(mixEvt);

  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  while ((Int_t)pool.size() > mix.bufferSize) {
    pool.pop_front();
  }
}

//-----------------------------------------------------------------------------
void StPhiMaker::FillCentralityEventQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks, Int_t nBTOFMatch,
                                       Int_t nKaonPlus, Int_t nKaonMinus, Int_t nPhiPairs) {
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
  m_histManager->Fill("hNPhiPairs_vs_Cent9", centX, (Double_t)nPhiPairs);
}

//-----------------------------------------------------------------------------
void StPhiMaker::FillPhiPairHistograms(Double_t invMass, const TVector3& phiMom, Double_t openingAngle, Double_t pairRapidity,
                                      Int_t nKaonPlus, Int_t nKaonMinus, Bool_t applySignalCuts) {
  if (!m_histManager) return;

  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  PhiCutConfig& phiCut = ConfigManager::GetInstance().GetPhiCuts();
  Bool_t passInvMass = kTRUE;
  if (phiCut.useInvMassCut) {
    passInvMass = (invMass >= phiCut.minInvMass && invMass <= phiCut.maxInvMass);
  }
  Bool_t passAngle = (openingAngle >= phiCut.minOpeningAngle && openingAngle <= phiCut.maxOpeningAngle);
  Bool_t passRapidity = (pairRapidity >= phiCut.minPairRapidity && pairRapidity <= phiCut.maxPairRapidity);
  Bool_t passRapidity_0p4 = (pairRapidity >= -0.4 && pairRapidity <= 0.4);
  Bool_t passRapidity_0p3 = (pairRapidity >= -0.3 && pairRapidity <= 0.3);
  Bool_t passRapidity_0p2 = (pairRapidity >= -0.2 && pairRapidity <= 0.2);
  Bool_t passRapidity_0p1 = (pairRapidity >= -0.1 && pairRapidity <= 0.1);

  if (!applySignalCuts) {
    if (!passInvMass) return;
    m_histManager->Fill("hMKK_MixedEvent", invMass);
    return;
  }

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
  m_histManager->Fill("hMKK_SameEvent", invMass);

  if (centCfg.fillCentralityQA && m_cent9 >= 0 && m_cent9 <= 8) {
    m_histManager->Fill("hMKK_vs_Cent9", (Double_t)m_cent9, invMass);
    if (m_refMultCorr >= 0.0) {
      m_histManager->Fill("hMKK_vs_RefMultCorr", m_refMultCorr, invMass);
    }
  }

  if (passAngle) {
    m_histManager->Fill("hMKK_OpeningAngleCut", invMass);
    if (passRapidity) m_histManager->Fill("hMKK_RapidityCut", invMass);
    if (passRapidity_0p4) m_histManager->Fill("hMKK_RapidityCut_0p4", invMass);
    if (passRapidity_0p3) m_histManager->Fill("hMKK_RapidityCut_0p3", invMass);
    if (passRapidity_0p2) m_histManager->Fill("hMKK_RapidityCut_0p2", invMass);
    if (passRapidity_0p1) m_histManager->Fill("hMKK_RapidityCut_0p1", invMass);
  }
  if (passAngle && passRapidity) {
    m_histManager->Fill("hMKK_BothCuts", invMass);
    m_histManager->Fill("hOpeningAngle_AfterCuts", openingAngle);
    m_histManager->Fill("hPairRapidity_AfterCuts", pairRapidity);
    m_histManager->Fill("hPairPt_AfterCuts", phiMom.Pt());
    if (centCfg.fillCentralityQA && m_cent9 >= 0 && m_cent9 <= 8) {
      static const char* kMKKCentBin[] = {
          "hMKK_CentBin0", "hMKK_CentBin1", "hMKK_CentBin2", "hMKK_CentBin3", "hMKK_CentBin4",
          "hMKK_CentBin5", "hMKK_CentBin6", "hMKK_CentBin7", "hMKK_CentBin8"};
      m_histManager->Fill(kMKKCentBin[m_cent9], invMass);
    }
    if (nKaonPlus < 5) m_histManager->Fill("hMKK_Mult0to5_KPlus", invMass);
    else if (nKaonPlus < 10) m_histManager->Fill("hMKK_Mult5to10_KPlus", invMass);
    else if (nKaonPlus < 20) m_histManager->Fill("hMKK_Mult10to20_KPlus", invMass);
    else m_histManager->Fill("hMKK_Mult20up_KPlus", invMass);
    if (nKaonMinus < 5) m_histManager->Fill("hMKK_Mult0to5_KMinus", invMass);
    else if (nKaonMinus < 10) m_histManager->Fill("hMKK_Mult5to10_KMinus", invMass);
    else if (nKaonMinus < 20) m_histManager->Fill("hMKK_Mult10to20_KMinus", invMass);
    else m_histManager->Fill("hMKK_Mult20up_KMinus", invMass);
  }
}

//-----------------------------------------------------------------------------
void StPhiMaker::FinalizeBackgroundSubtractedHistogram() {
  if (!m_histManager) return;

  TH1* signal = m_histManager->Get("hMKK_BothCuts");
  TH1* mixed = m_histManager->Get("hMKK_MixedEvent");
  TH1* subtracted = m_histManager->Get("hMKK_BackgroundSubtracted");
  if (!signal || !mixed || !subtracted) return;

  subtracted->Reset();
  subtracted->Add(signal);
  subtracted->Add(mixed, -1.0);
}
