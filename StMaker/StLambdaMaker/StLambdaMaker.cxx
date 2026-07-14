#include "StLambdaMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "cuts/EventCutConfig.h"
#include "cuts/LambdaCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "CentralityHelper.h"
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

namespace {
  const Double_t kProtonMass = 0.938272;
  const Double_t kPionMass   = 0.139570;
}

//-----------------------------------------------------------------------------
StLambdaMaker::StLambdaMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName)
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
StLambdaMaker::~StLambdaMaker() {
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
StLambdaMaker* createStLambdaMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StLambdaMaker(name, picoMaker, outName);
}

extern "C" void* createStLambdaMakerC(const char* name, void* picoMaker, const char* outName) {
  return (void*)createStLambdaMaker(name, (StPicoDstMaker*)picoMaker, outName);
}

//-----------------------------------------------------------------------------
Int_t StLambdaMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StLambdaMaker] GetHistConfigPath() returned empty; no histograms will be filled." << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StLambdaMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStOK;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StLambdaMaker] CentralityHelper init failed" << std::endl;
  }
  return kStOK;
}

//-----------------------------------------------------------------------------
void StLambdaMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mLambdaMom.clear();
  mLambdaInvMass.clear();
  mLambdaProtonId.clear();
  mLambdaPionId.clear();
}

//-----------------------------------------------------------------------------
Bool_t StLambdaMaker::PassEventCuts(Int_t nTracks) {
  EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
  if (ev.maxNTr > 0 && nTracks > ev.maxNTr) return kFALSE;
  return kTRUE;
}

//-----------------------------------------------------------------------------
Bool_t StLambdaMaker::PassProtonCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() <= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio) return kFALSE;
  if (TMath::Abs(trk->nSigmaProton()) > lam.nSigmaProton) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCAProton) return kFALSE;
  return kTRUE;
}

//-----------------------------------------------------------------------------
Bool_t StLambdaMaker::PassPionCuts(StPicoTrack* trk, const TVector3& pVtx) {
  if (!trk || trk->charge() >= 0) return kFALSE;
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  if (trk->nHitsFit() < lam.minNHitsFit) return kFALSE;
  if (trk->nHitsMax() > 0 &&
      (Double_t)trk->nHitsFit() / (Double_t)trk->nHitsMax() < lam.minNHitsRatio) return kFALSE;
  if (TMath::Abs(trk->nSigmaPion()) > lam.nSigmaPion) return kFALSE;
  Double_t dca = trk->gDCA(pVtx.X(), pVtx.Y(), pVtx.Z());
  if (dca < lam.minDCAPion) return kFALSE;
  return kTRUE;
}

//-----------------------------------------------------------------------------
StPhysicalHelixD StLambdaMaker::MakeHelix(StPicoTrack* trk, Double_t bField) {
  StThreeVectorF p(trk->gMom().X(), trk->gMom().Y(), trk->gMom().Z());
  StThreeVectorF o(trk->origin().X(), trk->origin().Y(), trk->origin().Z());
  return StPhysicalHelixD(p, o, bField * units::kilogauss, (Float_t)trk->charge());
}

//-----------------------------------------------------------------------------
Bool_t StLambdaMaker::MakeLambdaHelix(StPicoTrack* p, StPicoTrack* pi, Double_t bField,
                                      TVector3& v0, TVector3& momP, TVector3& momPi, Double_t& dca12) {
  LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
  StPhysicalHelixD hp  = MakeHelix(p,  bField);
  StPhysicalHelixD hpi = MakeHelix(pi, bField);

  std::pair<Double_t, Double_t> s = hp.pathLengths(hpi);
  if (TMath::Abs(s.first) > lam.maxPathLength || TMath::Abs(s.second) > lam.maxPathLength)
    return kFALSE;

  StThreeVectorD dcaA = hp.at(s.first);
  StThreeVectorD dcaB = hpi.at(s.second);
  StThreeVectorD v0_( (dcaA.x() + dcaB.x()) * 0.5,
                      (dcaA.y() + dcaB.y()) * 0.5,
                      (dcaA.z() + dcaB.z()) * 0.5 );
  dca12 = (dcaA - dcaB).mag();

  if (dca12 < 0 || dca12 > lam.maxDaughterDCA) return kFALSE;

  StThreeVectorD pp  = hp.momentumAt(s.first,  bField * units::kilogauss);
  StThreeVectorD ppi = hpi.momentumAt(s.second, bField * units::kilogauss);

  momP.SetXYZ(pp.x(), pp.y(), pp.z());
  momPi.SetXYZ(ppi.x(), ppi.y(), ppi.z());
  v0.SetXYZ(v0_.x(), v0_.y(), v0_.z());

  return kTRUE;
}

//-----------------------------------------------------------------------------
void StLambdaMaker::FillLambdaCentralityQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
                                           Int_t nBTOFMatch, Int_t nProtonCand, Int_t nPionCand, Int_t nLambdaPairs) {
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
  m_histManager->Fill("hNLambdaPairs_vs_Cent9", centX, (Double_t)nLambdaPairs);
}

//-----------------------------------------------------------------------------
void StLambdaMaker::FillLambdaInvMassCentrality(Double_t invMass) {
  if (!m_histManager || m_cent9 < 0 || m_cent9 > 8) return;

  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  if (!centCfg.fillCentralityQA) return;

  m_histManager->Fill("hLambda_InvMass_vs_Cent9", (Double_t)m_cent9, invMass);
  if (m_refMultCorr >= 0.0) {
    m_histManager->Fill("hLambda_InvMass_vs_RefMultCorr", m_refMultCorr, invMass);
  }

  static const char* kCentBin[] = {
      "hLambda_InvMass_CentBin0", "hLambda_InvMass_CentBin1", "hLambda_InvMass_CentBin2",
      "hLambda_InvMass_CentBin3", "hLambda_InvMass_CentBin4", "hLambda_InvMass_CentBin5",
      "hLambda_InvMass_CentBin6", "hLambda_InvMass_CentBin7", "hLambda_InvMass_CentBin8"};
  m_histManager->Fill(kCentBin[m_cent9], invMass);
}

//-----------------------------------------------------------------------------
Int_t StLambdaMaker::Make() {
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

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(runId, centReason)) {
      return kStOK;
    }
  }

  if (m_histManager) {
    m_histManager->Fill("hVz", pVtx.Z());
    m_histManager->Fill("hRefMult", refMult);
  }

  Int_t nTr = mPicoDst->numberOfTracks();
  if (!PassEventCuts(nTr)) return kStOK;

  if (m_histManager && centCfg.fillCentralityQA) {
    m_histManager->Fill("hRawMult", (Double_t)rawMult);
  }

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) {
      return kStOK;
    }

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

    if (!m_centrality->AcceptCentBin(m_cent9, m_refMultCorr, centReason)) {
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

  Double_t bField = event->bField();

  Int_t nProtonCand = 0;
  Int_t nPionCand = 0;
  Int_t nLambdaPairs = 0;

  for (Int_t ip = 0; ip < nTr; ip++) {
    StPicoTrack* p = mPicoDst->track(ip);
    if (!p) continue;
    if (PassProtonCuts(p, pVtx)) nProtonCand++;
  }
  for (Int_t ii = 0; ii < nTr; ii++) {
    StPicoTrack* pi = mPicoDst->track(ii);
    if (!pi) continue;
    if (PassPionCuts(pi, pVtx)) nPionCand++;
  }

  for (Int_t ip = 0; ip < nTr; ip++) {
    StPicoTrack* p = mPicoDst->track(ip);
    if (!p) continue;
    if (!PassProtonCuts(p, pVtx)) continue;

    for (Int_t ii = 0; ii < nTr; ii++) {
      if (ii == ip) continue;
      StPicoTrack* pi = mPicoDst->track(ii);
      if (!pi) continue;
      if (!PassPionCuts(pi, pVtx)) continue;

      TVector3 v0, momP, momPi;
      Double_t dca12 = 0;
      if (!MakeLambdaHelix(p, pi, bField, v0, momP, momPi, dca12)) continue;

      nLambdaPairs++;

      TVector3 pLam = momP + momPi;
      Double_t pLamMag = pLam.Mag();
      if (pLamMag < 1e-5) continue;

      TVector3 pLamUnit = pLam * (1.0 / pLamMag);
      TVector3 diff = pVtx - v0;
      Double_t dcaV0 = (diff.Cross(pLamUnit)).Mag();

      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      if (dcaV0 > lam.maxDCAV0) continue;

      TVector3 flight = v0 - pVtx;
      Double_t cosPoint = flight.Dot(pLam) / (flight.Mag() * pLam.Mag() + 1e-10);
      if (cosPoint < lam.minCosPointing) continue;

      TLorentzVector lp, lpi;
      lp.SetVectM(momP,  kProtonMass);
      lpi.SetVectM(momPi, kPionMass);
      TLorentzVector lLambda = lp + lpi;
      Double_t invMass = lLambda.M();
      Double_t rapidity = lLambda.Rapidity();
      Double_t decayLength = flight.Mag();

      mLambdaMom.push_back(pLam);
      mLambdaInvMass.push_back(invMass);
      mLambdaProtonId.push_back(p->id());
      mLambdaPionId.push_back(pi->id());

      if (m_histManager) {
        m_histManager->Fill("hLambda_InvMass", invMass);
        m_histManager->Fill("hLambda_Pt", pLam.Pt());
        m_histManager->Fill("hLambda_Eta", pLam.PseudoRapidity());
        m_histManager->Fill("hLambda_Phi", pLam.Phi());
        m_histManager->Fill("hDCA12", dca12);
        m_histManager->Fill("hDCAV0", dcaV0);
        m_histManager->Fill("hCosPointing", cosPoint);
        m_histManager->Fill("hNSigmaProton", p->nSigmaProton());
        m_histManager->Fill("hNSigmaPion", pi->nSigmaPion());
        m_histManager->Fill("hLambda_InvMass_vs_Pt", pLam.Pt(), invMass);
        m_histManager->Fill("hLambda_InvMass_vs_DecayLength", decayLength, invMass);
        m_histManager->Fill("hLambda_InvMass_vs_Y", rapidity, invMass);
        m_histManager->Fill("hDCAV0_vs_InvMass", invMass, dcaV0);
        m_histManager->Fill("hCosPointing_vs_InvMass", invMass, cosPoint);
        Double_t transDecayLen = flight.Perp();
        m_histManager->Fill("hLambda_InvMass_vs_TransDecayLength", transDecayLen, invMass);
        FillLambdaInvMassCentrality(invMass);
      }
    }
  }

  if (m_histManager && m_cent9 >= 0) {
    FillLambdaCentralityQA(m_cent9, rawMult, m_refMultCorr, nTr, nBTOFMatch,
                           nProtonCand, nPionCand, nLambdaPairs);
  }

  if (m_histManager) m_histManager->Fill("hN", 0);
  return kStOK;
}

//-----------------------------------------------------------------------------
Int_t StLambdaMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StLambdaMaker::Finish() processed " << mEventCounter << " events" << std::endl;
  if (m_centrality && m_centrality->IsEnabled()) {
    std::cout << "[StLambdaMaker] centrality summary: ok=" << m_centrality->CountOk()
              << " badRun=" << m_centrality->CountBadRun()
              << " pileup=" << m_centrality->CountPileup()
              << " invalidCent=" << m_centrality->CountInvalidCent()
              << " binRejected=" << m_centrality->CountBinRejected() << std::endl;
    m_centrality->Finish();
  }
  return kStOK;
}

//-----------------------------------------------------------------------------
void StLambdaMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}
