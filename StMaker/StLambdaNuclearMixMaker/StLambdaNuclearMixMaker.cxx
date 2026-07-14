#include "StLambdaNuclearMixMaker.h"
#include "../StLambdaMaker/StLambdaMaker.h"
#include "../StNuclearIdMaker/StNuclearIdMaker.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "CentralityHelper.h"
#include "cuts/MixingConfig.h"
#include "cuts/NuclearIdCutConfig.h"
#include "cuts/EventCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "TLorentzVector.h"
#include "TMath.h"
#include <iostream>

namespace {
  const Double_t kLambdaMass = 1.115683;
  const Double_t M_d   = 1.87561;
  const Double_t M_t   = 2.80892;
  const Double_t M_3He = 2.80839;
  const Double_t M_4He = 3.72742;
}

//-----------------------------------------------------------------------------
StLambdaNuclearMixMaker::StLambdaNuclearMixMaker(const char* name, const char* outName)
  : StMaker(name),
    mOutName(outName),
    m_histManager(0),
    m_centrality(0),
    m_maxMixEvents(100),
    m_bufferSize(150),
    m_cent9(-1),
    m_cent16(-1),
    m_refMultCorr(-1.0),
    m_centWeight(1.0) {}

//-----------------------------------------------------------------------------
StLambdaNuclearMixMaker::~StLambdaNuclearMixMaker() {
  if (m_centrality) delete m_centrality;
  if (m_histManager) delete m_histManager;
}

//-----------------------------------------------------------------------------
StLambdaNuclearMixMaker* createStLambdaNuclearMixMaker(const char* name, const char* outName) {
  return new StLambdaNuclearMixMaker(name, outName);
}

extern "C" void* createStLambdaNuclearMixMakerC(const char* name, const char* outName) {
  return (void*)createStLambdaNuclearMixMaker(name, outName);
}

//-----------------------------------------------------------------------------
Int_t StLambdaNuclearMixMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath("nuclearid");
  if (histPath.empty()) {
    std::cerr << "[StLambdaNuclearMixMaker] Error: could not find 'nuclearid' config path." << std::endl;
    return kStErr;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StLambdaNuclearMixMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStErr;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StLambdaNuclearMixMaker] CentralityHelper init failed" << std::endl;
  }

  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  if (mix.bufferSize > 0) m_bufferSize = mix.bufferSize;

  return kStOK;
}

//-----------------------------------------------------------------------------
Int_t StLambdaNuclearMixMaker::GetMixingBin(Float_t vz, Int_t cent9) const {
  const MixingConfig& mix = ConfigManager::GetInstance().GetMixingConfig();
  Int_t vzBin = 0, centBin = 0;
  
  if (mix.nVzBins > 0) {
    const EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
    Double_t vzSpan = ev.maxVz - ev.minVz;
    if (vzSpan > 0) {
      vzBin = (Int_t)((vz - ev.minVz) / vzSpan * mix.nVzBins);
      if (vzBin < 0) vzBin = 0;
      if (vzBin >= mix.nVzBins) vzBin = mix.nVzBins - 1;
    }
  }
  
  if (mix.nCentralityBins > 0 && cent9 >= 0) {
    centBin = cent9;
    if (centBin >= mix.nCentralityBins) centBin = mix.nCentralityBins - 1;
  }
  
  return vzBin + mix.nVzBins * centBin;
}

//-----------------------------------------------------------------------------
void StLambdaNuclearMixMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
}

//-----------------------------------------------------------------------------
Int_t StLambdaNuclearMixMaker::Make() {
  StLambdaMaker* lambdaMaker = (StLambdaMaker*)GetMaker("lambda");
  StNuclearIdMaker* nucMaker = (StNuclearIdMaker*)GetMaker("nuclearid");
  if (!lambdaMaker || !nucMaker) return kStOK;

  StPicoDstMaker* picoMaker = (StPicoDstMaker*)GetMaker("picoDst");
  if (!picoMaker || !picoMaker->picoDst() || !picoMaker->picoDst()->event()) return kStOK;
  
  StPicoEvent* event = picoMaker->picoDst()->event();
  TVector3 pVtx = event->primaryVertex();
  Float_t vz = pVtx.Z();
  Int_t rawMult = event->refMult();
  
  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  if (centCfg.enabled) {
    TString mode(centCfg.mode.c_str());
    mode.ToLower();
    if (mode == "fxtmult") rawMult = event->fxtMult();
  }

  CentralityRejectReason centReason = kCentralityOk;
  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(event->runId(), centReason)) return kStOK;
    if (!m_centrality->CheckPileup(rawMult, event->nBTOFMatch(), vz, centReason)) return kStOK;
    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight, centReason)) return kStOK;
    if (!m_centrality->AcceptCentBin(m_cent9, m_refMultCorr, centReason)) return kStOK;
  } else {
    m_cent9 = -1;
  }

  // Get current event candidates
  const std::vector<TVector3>& lamMoms = lambdaMaker->GetLambdaMomList();
  const std::vector<Double_t>& lamMasses = lambdaMaker->GetLambdaInvMassList();
  const std::vector<TVector3>& nucMoms = nucMaker->GetNuclearMomList();
  const std::vector<Int_t>& nucTypes = nucMaker->GetNuclearTypeList();

  // Create current event structure
  MixedEvent currentEvt;
  for (size_t i = 0; i < lamMoms.size(); ++i) {
    MixLambdaCand lam;
    lam.momentum = lamMoms[i];
    lam.invMass = lamMasses[i];
    currentEvt.lambdas.push_back(lam);
  }
  for (size_t i = 0; i < nucMoms.size(); ++i) {
    MixNuclearCand nuc;
    nuc.momentum = nucMoms[i];
    nuc.type = nucTypes[i];
    currentEvt.nuclei.push_back(nuc);
  }

  NuclearIdCutConfig& nucCuts = ConfigManager::GetInstance().GetNuclearIdCuts();

  // Perform mixing
  Int_t mixBin = GetMixingBin(vz, m_cent9);
  std::deque<MixedEvent>& pool = m_mixingPool[mixBin];
  
  Int_t mixCount = 0;
  // Iterate from the back (most recent) to front
  for (auto it = pool.rbegin(); it != pool.rend(); ++it) {
    if (mixCount >= m_maxMixEvents) break;
    const MixedEvent& mixEvt = *it;
    
    // Mix current Lambda with pool Nuclei
    for (size_t il = 0; il < currentEvt.lambdas.size(); ++il) {
      double invMass = currentEvt.lambdas[il].invMass;
      double delta   = invMass - nucCuts.MeanLambda;
      double window  = nucCuts.nsigmaLambda * nucCuts.sigmaLambda;

      bool isSig   = (TMath::Abs(delta) <= window);
      bool isSBPos = (!isSig && delta > window  && delta <= 4.0 * window);
      bool isSBNeg = (!isSig && delta < -window && delta >= -4.0 * window);

      TLorentzVector lambdaMom;
      lambdaMom.SetVectM(currentEvt.lambdas[il].momentum, kLambdaMass);

      for (size_t in = 0; in < mixEvt.nuclei.size(); ++in) {
        double mass = 0;
        int nucType = mixEvt.nuclei[in].type;
        if (nucType == 0) mass = M_d;
        else if (nucType == 1) mass = M_t;
        else if (nucType == 2) mass = M_3He;
        else if (nucType == 3) mass = M_4He;

        TLorentzVector nMom;
        nMom.SetVectM(mixEvt.nuclei[in].momentum, mass);

        TVector3 p3_lam = lambdaMom.Vect();
        TVector3 p3_nuc = nMom.Vect();
        double q_lab = (p3_lam - p3_nuc).Mag();

        TLorentzVector pair = lambdaMom + nMom;
        TVector3 boostVec = -pair.BoostVector();
        TLorentzVector lamRest = lambdaMom;
        lamRest.Boost(boostVec);
        double k_star = lamRest.P();

        FillKstarMassMixed(k_star, invMass, nucType, m_cent9);

        if (!isSig && !isSBPos && !isSBNeg) continue;

        if (isSig) FillKstarMixed(k_star, q_lab, nucType, m_cent9);
        else if (isSBPos) FillKstarMixedSideband(k_star, q_lab, nucType, +1, m_cent9);
        else if (isSBNeg) FillKstarMixedSideband(k_star, q_lab, nucType, -1, m_cent9);
      }
    }

    // Mix pool Lambda with current Nuclei
    for (size_t il = 0; il < mixEvt.lambdas.size(); ++il) {
      double invMass = mixEvt.lambdas[il].invMass;
      double delta   = invMass - nucCuts.MeanLambda;
      double window  = nucCuts.nsigmaLambda * nucCuts.sigmaLambda;

      bool isSig   = (TMath::Abs(delta) <= window);
      bool isSBPos = (!isSig && delta > window  && delta <= 4.0 * window);
      bool isSBNeg = (!isSig && delta < -window && delta >= -4.0 * window);

      TLorentzVector lambdaMom;
      lambdaMom.SetVectM(mixEvt.lambdas[il].momentum, kLambdaMass);

      for (size_t in = 0; in < currentEvt.nuclei.size(); ++in) {
        double mass = 0;
        int nucType = currentEvt.nuclei[in].type;
        if (nucType == 0) mass = M_d;
        else if (nucType == 1) mass = M_t;
        else if (nucType == 2) mass = M_3He;
        else if (nucType == 3) mass = M_4He;

        TLorentzVector nMom;
        nMom.SetVectM(currentEvt.nuclei[in].momentum, mass);

        TVector3 p3_lam = lambdaMom.Vect();
        TVector3 p3_nuc = nMom.Vect();
        double q_lab = (p3_lam - p3_nuc).Mag();

        TLorentzVector pair = lambdaMom + nMom;
        TVector3 boostVec = -pair.BoostVector();
        TLorentzVector lamRest = lambdaMom;
        lamRest.Boost(boostVec);
        double k_star = lamRest.P();

        FillKstarMassMixed(k_star, invMass, nucType, m_cent9);

        if (!isSig && !isSBPos && !isSBNeg) continue;

        if (isSig) FillKstarMixed(k_star, q_lab, nucType, m_cent9);
        else if (isSBPos) FillKstarMixedSideband(k_star, q_lab, nucType, +1, m_cent9);
        else if (isSBNeg) FillKstarMixedSideband(k_star, q_lab, nucType, -1, m_cent9);
      }
    }

    mixCount++;
  }

  // Push current event into pool
  if (currentEvt.lambdas.size() > 0 || currentEvt.nuclei.size() > 0) {
    pool.push_back(currentEvt);
    if ((Int_t)pool.size() > m_bufferSize) {
      pool.pop_front();
    }
  }

  return kStOK;
}

//-----------------------------------------------------------------------------
void StLambdaNuclearMixMaker::FillKstarMixed(Double_t k_star, Double_t q_lab, Int_t type, Int_t cent9) {
  if (!m_histManager) return;
  const char* typeName = "";
  if (type == 0) typeName = "d";
  else if (type == 1) typeName = "t";
  else if (type == 2) typeName = "3He";
  else if (type == 3) typeName = "4He";
  else return;

  if (m_histManager->HasHistogram(Form("hKstar_Mixed_%s", typeName))) m_histManager->Fill(Form("hKstar_Mixed_%s", typeName), k_star);
  if (m_histManager->HasHistogram(Form("hQlab_Mixed_%s", typeName))) m_histManager->Fill(Form("hQlab_Mixed_%s", typeName), q_lab);

  if (cent9 >= 0 && cent9 <= 8) {
    if (m_histManager->HasHistogram(Form("hKstar_Mixed_%s_CentBin%d", typeName, cent9))) {
      m_histManager->Fill(Form("hKstar_Mixed_%s_CentBin%d", typeName, cent9), k_star);
    }
  }
}

//-----------------------------------------------------------------------------
void StLambdaNuclearMixMaker::FillKstarMixedSideband(Double_t k_star, Double_t q_lab, Int_t type, Int_t sbSign, Int_t cent9) {
  if (!m_histManager) return;
  const char* typeName = "";
  if (type == 0) typeName = "d";
  else if (type == 1) typeName = "t";
  else if (type == 2) typeName = "3He";
  else if (type == 3) typeName = "4He";
  else return;

  const char* sbName = (sbSign > 0) ? "SBPos" : "SBNeg";

  if (m_histManager->HasHistogram(Form("hKstar_Mixed_%s_%s", typeName, sbName))) m_histManager->Fill(Form("hKstar_Mixed_%s_%s", typeName, sbName), k_star);
  if (m_histManager->HasHistogram(Form("hQlab_Mixed_%s_%s", typeName, sbName))) m_histManager->Fill(Form("hQlab_Mixed_%s_%s", typeName, sbName), q_lab);

  if (cent9 >= 0 && cent9 <= 8) {
    if (m_histManager->HasHistogram(Form("hKstar_Mixed_%s_%s_CentBin%d", typeName, sbName, cent9))) {
      m_histManager->Fill(Form("hKstar_Mixed_%s_%s_CentBin%d", typeName, sbName, cent9), k_star);
    }
  }
}

void StLambdaNuclearMixMaker::FillKstarMassMixed(Double_t k_star, Double_t invMass, Int_t type, Int_t cent9) {
  if (!m_histManager) return;
  const char* typeStr = "d";
  if (type == 1) typeStr = "t";
  else if (type == 2) typeStr = "3He";
  else if (type == 3) typeStr = "4He";

  if (cent9 >= 0 && cent9 <= 8) {
    m_histManager->Fill(Form("hKstarMass_Mixed_%s_CentBin%d", typeStr, cent9), k_star, invMass);
  }
}

//-----------------------------------------------------------------------------
Int_t StLambdaNuclearMixMaker::Finish() {
  return kStOK;
}

//-----------------------------------------------------------------------------
void StLambdaNuclearMixMaker::WriteHistograms() {
  if (m_histManager) {
    // Write only Mixed-event histograms (names contain "Mixed").
    // The HistManager is loaded from the same YAML as StNuclearIdMaker,
    // so it also holds True-event histogram definitions; we must NOT
    // write those here to avoid polluting the "mix" TDirectory.
    m_histManager->WriteContaining("Mixed");
  }
}
