#include "StNuclearIdMaker.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"
#include "ConfigManager.h"
#include "cuts/NuclearIdCutConfig.h"
#include "cuts/EventCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "HistManager.h"
#include "CentralityHelper.h"
#include "TH2D.h"
#include "TFile.h"
#include "TVector3.h"
#include "TMath.h"
#include <iostream>

// Factory functions
StNuclearIdMaker* createStNuclearIdMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StNuclearIdMaker(name, picoMaker, outName);
}
extern "C" void* createStNuclearIdMakerC(const char* name, void* picoMaker, const char* outName) {
  return (void*)createStNuclearIdMaker(name, (StPicoDstMaker*)picoMaker, outName);
}

// NuclearPID struct inside the implementation
struct NuclearPID {
  enum ParticleType { kDeuteron = 0, kTriton, kHe3, kHe4 };
  double fMeanParams[4][4];
  double fSigmaParams[4][4];
  double fRange[4][2];

  NuclearPID() {
    double mean[4][4] = {
      {10.2875, -36.6639, 67.63, -45.2483},
      {19.2177, -70.3198, 113.876, -67.0591},
      {28.8747, -68.8637, 104.455, -56.9342},
      {39.974, -103.365, 139.182, -67.1446}
    };
    double sigma[4][4] = {
      {0.771189, -2.63609, 2.8044, 0.0},
      {1.40996, -4.97147, 4.97608, 0.0},
      {1.636, -4.35588, 4.40594, 0.0},
      {2.24127, -7.08328, 8.07124, 0.0}
    };
    double range[4][2] = {
      {0.2, 6.0}, {0.2, 6.0}, {0.4, 5.0}, {0.4, 5.0}
    };
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        fMeanParams[i][j] = mean[i][j];
        fSigmaParams[i][j] = sigma[i][j];
      }
      fRange[i][0] = range[i][0];
      fRange[i][1] = range[i][1];
    }
  }

  bool IsValid(ParticleType type, double p) const {
    int t = (int)type;
    return (p >= fRange[t][0] && p <= fRange[t][1]);
  }

  double LogPoly(double p, const double* par) const {
    if (p <= 0) return 0;
    double lp = log10(p);
    return par[0] + par[1]*lp + par[2]*lp*lp + par[3]*lp*lp*lp;
  }

  double GetMean(ParticleType type, double p) const {
    if (!IsValid(type, p)) return 0;
    return LogPoly(p, fMeanParams[(int)type]);
  }

  double GetSigma(ParticleType type, double p) const {
    if (!IsValid(type, p)) return 0;
    return LogPoly(p, fSigmaParams[(int)type]);
  }

  double GetNSigma(ParticleType type, double p, double dedx) const {
    if (!IsValid(type, p)) return 999.0;
    double mean = GetMean(type, p);
    double sigma = GetSigma(type, p);
    if (sigma <= 0) return 999.0;
    return (dedx - mean) / sigma;
  }
};

StNuclearIdMaker::StNuclearIdMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName)
    : StMaker(name), mPicoDstMaker(picoMaker), mPicoDst(nullptr), mOutName(outName), m_histManager(0),
      m_centrality(0), m_cent9(-1), m_cent16(-1), m_refMultCorr(-1.0), m_centWeight(1.0), m_centralityPercent(-1.0) {}

StNuclearIdMaker::~StNuclearIdMaker() {
  if (m_histManager) {
    delete m_histManager;
    m_histManager = 0;
  }
  if (m_centrality) {
    delete m_centrality;
    m_centrality = 0;
  }
}

Int_t StNuclearIdMaker::Init() {
  std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) {
    std::cerr << "[StNuclearIdMaker] GetHistConfigPath() returned empty; no histograms will be filled." << std::endl;
    m_histManager = 0;
    return kStOK;
  }
  m_histManager = new HistManager();
  if (!m_histManager->LoadFromFile(histPath.c_str())) {
    std::cerr << "[StNuclearIdMaker] Failed to load hist config from " << histPath << std::endl;
    delete m_histManager;
    m_histManager = 0;
    return kStOK;
  }

  m_centrality = new CentralityHelper();
  if (!m_centrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "[StNuclearIdMaker] CentralityHelper init failed" << std::endl;
  }

  return kStOK;
}

void StNuclearIdMaker::Clear(Option_t* opt) {
  StMaker::Clear(opt);
  mNuclearMom.clear();
  mNuclearId.clear();
  mNuclearType.clear();
}

Int_t StNuclearIdMaker::Make() {
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;
  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  NuclearPID pid;
  NuclearIdCutConfig& cuts = ConfigManager::GetInstance().GetNuclearIdCuts();

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

  if (m_histManager && m_histManager->HasHistogram("hRefMultVsNTOFMatch")) {
    m_histManager->Fill("hRefMultVsNTOFMatch", (Double_t)nBTOFMatch, (Double_t)rawMult);
  }

  CentralityRejectReason centReason = kCentralityOk;

  if (m_centrality && m_centrality->IsEnabled()) {
    if (!m_centrality->CheckBadRun(runId, centReason)) {
      return kStOK;
    }
    if (!m_centrality->CheckPileup(rawMult, nBTOFMatch, vz, centReason)) {
      return kStOK;
    }
    if (!m_centrality->ComputeBins(event, rawMult, vz, m_cent9, m_cent16, m_refMultCorr, m_centWeight, centReason)) {
      return kStOK;
    }
    if (m_histManager && centCfg.fillCentralityQA && m_histManager->HasHistogram("hRawMult")) {
      m_histManager->Fill("hRawMult", (Double_t)rawMult);
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

  if (m_histManager && m_histManager->HasHistogram("hVz")) {
    m_histManager->Fill("hVz", pVtx.Z());
    m_histManager->Fill("hRefMult", refMult);
  }

  double d_mean   = 3.48096e+00;
  double d_sigma  = 1.41458e-01;
  double He3_mean = 1.92385e+00;
  double He3_sigma= 0.94e-01;
  double t_mean   = 7.76906e+00;
  double t_sigma  = 3.41755e-01;

  Int_t nTracks = mPicoDst->numberOfTracks();

  // Sentinel flags: check once before the track loop whether per-cent histograms
  // exist in the loaded YAML. This avoids per-track HasHistogram calls for
  // dynamic names like Form("hDedxP_CentBin%d", cent9).
  const bool hasCentBinDedxP    = m_histManager && m_histManager->HasHistogram("hDedxP_CentBin0");
  const bool hasCentBinDedxP_d  = m_histManager && m_histManager->HasHistogram("hDedxP_d_CentBin0");
  const bool hasCentBinDedxP_t  = m_histManager && m_histManager->HasHistogram("hDedxP_t_CentBin0");
  const bool hasCentBinDedxP_3He= m_histManager && m_histManager->HasHistogram("hDedxP_3He_CentBin0");
  const bool hasCentBinDedxP_4He= m_histManager && m_histManager->HasHistogram("hDedxP_4He_CentBin0");
  const bool hasCentBinPvsEta_d = m_histManager && m_histManager->HasHistogram("hPvsEta_d_CentBin0");
  const bool hasCentBinPvsEta_t = m_histManager && m_histManager->HasHistogram("hPvsEta_t_CentBin0");
  const bool hasCentBinPvsEta_3He=m_histManager && m_histManager->HasHistogram("hPvsEta_3He_CentBin0");
  const bool hasCentBinPvsEta_4He=m_histManager && m_histManager->HasHistogram("hPvsEta_4He_CentBin0");
  const bool hasCentBinMult_d   = m_histManager && m_histManager->HasHistogram("hMult_d_CentBin0");

  for (Int_t it = 0; it < nTracks; it++) {
    StPicoTrack* trk = mPicoDst->track(it);
    if (!trk) continue;

    if (trk->nHitsDedx() < 15) continue;
    if (trk->gPt() < 0.1) continue;

    TVector3 pMom = trk->pMom();
    Double_t p = pMom.Mag();

    Double_t dedx = trk->dEdx();
    if (dedx <= 0) continue;

    if (m_histManager && m_histManager->HasHistogram("hDedxP")) {
      m_histManager->Fill("hDedxP", p, dedx);
      if (hasCentBinDedxP && m_cent9 >= 0 && m_cent9 <= 8) {
        m_histManager->Fill(Form("hDedxP_CentBin%d", m_cent9), p, dedx);
      }

      if (fabs(trk->nSigmaElectron()) < cuts.nSigmaFill1D) m_histManager->Fill("hDedxP_e", p, dedx);
      if (fabs(trk->nSigmaPion()) < cuts.nSigmaFill1D) m_histManager->Fill("hDedxP_pi", p, dedx);
      if (fabs(trk->nSigmaKaon()) < cuts.nSigmaFill1D) m_histManager->Fill("hDedxP_K", p, dedx);
      if (fabs(trk->nSigmaProton()) < cuts.nSigmaFill1D) m_histManager->Fill("hDedxP_p", p, dedx);

      if (fabs(trk->nSigmaElectron()) > cuts.nSigmaExclude &&
          fabs(trk->nSigmaPion()) > cuts.nSigmaExclude &&
          fabs(trk->nSigmaKaon()) > cuts.nSigmaExclude &&
          fabs(trk->nSigmaProton()) > cuts.nSigmaExclude) {
        m_histManager->Fill("hDedxP_else", p, dedx);
      }
    }

    double nSigma_d   = pid.GetNSigma(NuclearPID::kDeuteron, p, dedx);
    double nSigma_t   = pid.GetNSigma(NuclearPID::kTriton,  p, dedx);
    double nSigma_3He = pid.GetNSigma(NuclearPID::kHe3,     p, dedx);
    double nSigma_4He = pid.GetNSigma(NuclearPID::kHe4,     p, dedx);

    if (m_histManager && m_histManager->HasHistogram("hDedxP_d")) {
      if (fabs(nSigma_d)   < cuts.maxNSigmaNuclear) {
        m_histManager->Fill("hDedxP_d", p, dedx);
        if (hasCentBinDedxP_d && m_cent9 >= 0 && m_cent9 <= 8) m_histManager->Fill(Form("hDedxP_d_CentBin%d", m_cent9), p, dedx);
      }
      if (fabs(nSigma_t)   < cuts.maxNSigmaNuclear) {
        m_histManager->Fill("hDedxP_t", p, dedx);
        if (hasCentBinDedxP_t && m_cent9 >= 0 && m_cent9 <= 8) m_histManager->Fill(Form("hDedxP_t_CentBin%d", m_cent9), p, dedx);
      }
      if (fabs(nSigma_3He) < cuts.maxNSigmaNuclear) {
        m_histManager->Fill("hDedxP_3He", p, dedx);
        if (hasCentBinDedxP_3He && m_cent9 >= 0 && m_cent9 <= 8) m_histManager->Fill(Form("hDedxP_3He_CentBin%d", m_cent9), p, dedx);
      }
      if (fabs(nSigma_4He) < cuts.maxNSigmaNuclear) {
        m_histManager->Fill("hDedxP_4He", p, dedx);
        if (hasCentBinDedxP_4He && m_cent9 >= 0 && m_cent9 <= 8) m_histManager->Fill(Form("hDedxP_4He_CentBin%d", m_cent9), p, dedx);
      }
    }

    const double M_d   = 1.87561;
    const double M_t   = 2.80892;
    const double M_3He = 2.80839;
    const double M_4He = 3.72742;
    double eta = pMom.PseudoRapidity();
    double pT  = pMom.Perp();
    double pz  = pMom.Z();

    if (fabs(nSigma_d) < cuts.maxNSigmaNuclear) {
      double E = TMath::Sqrt(M_d*M_d + p*p);
      double y = 0.5 * TMath::Log((E + pz) / (E - pz));
      if (m_histManager && m_histManager->HasHistogram("hPvsEta_d")) {
        m_histManager->Fill("hPvsEta_d", p, eta);
        m_histManager->Fill("hPvsY_d", p, y);
        m_histManager->Fill("hPvsPt_d", p, pT);
        if (hasCentBinPvsEta_d && m_cent9 >= 0 && m_cent9 <= 8) {
          m_histManager->Fill(Form("hPvsEta_d_CentBin%d", m_cent9), p, eta);
          m_histManager->Fill(Form("hPvsY_d_CentBin%d", m_cent9), p, y);
          m_histManager->Fill(Form("hPvsPt_d_CentBin%d", m_cent9), p, pT);
        }
      }
    }
    if (fabs(nSigma_t) < cuts.maxNSigmaNuclear) {
      double E = TMath::Sqrt(M_t*M_t + p*p);
      double y = 0.5 * TMath::Log((E + pz) / (E - pz));
      if (m_histManager && m_histManager->HasHistogram("hPvsEta_t")) {
        m_histManager->Fill("hPvsEta_t", p, eta);
        m_histManager->Fill("hPvsY_t", p, y);
        m_histManager->Fill("hPvsPt_t", p, pT);
        if (hasCentBinPvsEta_t && m_cent9 >= 0 && m_cent9 <= 8) {
          m_histManager->Fill(Form("hPvsEta_t_CentBin%d", m_cent9), p, eta);
          m_histManager->Fill(Form("hPvsY_t_CentBin%d", m_cent9), p, y);
          m_histManager->Fill(Form("hPvsPt_t_CentBin%d", m_cent9), p, pT);
        }
      }
    }
    if (fabs(nSigma_3He) < cuts.maxNSigmaNuclear) {
      double p_act = 2.0 * p;
      double pz_act = 2.0 * pz;
      double pT_act = 2.0 * pT;
      double E = TMath::Sqrt(M_3He*M_3He + p_act*p_act);
      double y = 0.5 * TMath::Log((E + pz_act) / (E - pz_act));
      if (m_histManager && m_histManager->HasHistogram("hPvsEta_3He")) {
        m_histManager->Fill("hPvsEta_3He", p_act, eta);
        m_histManager->Fill("hPvsY_3He", p_act, y);
        m_histManager->Fill("hPvsPt_3He", p_act, pT_act);
        if (hasCentBinPvsEta_3He && m_cent9 >= 0 && m_cent9 <= 8) {
          m_histManager->Fill(Form("hPvsEta_3He_CentBin%d", m_cent9), p_act, eta);
          m_histManager->Fill(Form("hPvsY_3He_CentBin%d", m_cent9), p_act, y);
          m_histManager->Fill(Form("hPvsPt_3He_CentBin%d", m_cent9), p_act, pT_act);
        }
      }
    }
    if (fabs(nSigma_4He) < cuts.maxNSigmaNuclear) {
      double p_act = 2.0 * p;
      double pz_act = 2.0 * pz;
      double pT_act = 2.0 * pT;
      double E = TMath::Sqrt(M_4He*M_4He + p_act*p_act);
      double y = 0.5 * TMath::Log((E + pz_act) / (E - pz_act));
      if (m_histManager && m_histManager->HasHistogram("hPvsEta_4He")) {
        m_histManager->Fill("hPvsEta_4He", p_act, eta);
        m_histManager->Fill("hPvsY_4He", p_act, y);
        m_histManager->Fill("hPvsPt_4He", p_act, pT_act);
        if (hasCentBinPvsEta_4He && m_cent9 >= 0 && m_cent9 <= 8) {
          m_histManager->Fill(Form("hPvsEta_4He_CentBin%d", m_cent9), p_act, eta);
          m_histManager->Fill(Form("hPvsY_4He_CentBin%d", m_cent9), p_act, y);
          m_histManager->Fill(Form("hPvsPt_4He_CentBin%d", m_cent9), p_act, pT_act);
        }
      }
    }

    // Populate mNuclearList
    // p/q upper limit: reject tracks with raw TPC rigidity p >= maxPOverQ
    // (for 3He/4He this is the value BEFORE the x2 correction, i.e. the measured p/q)
    bool passMaxP = (cuts.maxPOverQ <= 0 || p < cuts.maxPOverQ);
    bool is_d   = passMaxP && (fabs(nSigma_d) < cuts.maxNSigmaNuclear);
    bool is_t   = passMaxP && (fabs(nSigma_t) < cuts.maxNSigmaNuclear);
    bool is_3He = passMaxP && (fabs(nSigma_3He) < cuts.maxNSigmaNuclear);
    bool is_4He = passMaxP && (fabs(nSigma_4He) < cuts.maxNSigmaNuclear);


    if (cuts.m2_selection) {
      bool hasTof = false;
      double beta_val = -1.0;
      double m2_val = -1.0;
      if (trk->isTofTrack()) {
        int idx = trk->bTofPidTraitsIndex();
        if (idx >= 0) {
          StPicoBTofPidTraits* btof = mPicoDst->btofPidTraits(idx);
          if (btof) {
            beta_val = btof->btofBeta();
            if (beta_val > 0) {
              hasTof = true;
              m2_val = p*p*(1.0/(beta_val*beta_val) - 1.0);
            }
          }
        }
      }

      if (!hasTof) {
        is_d = is_t = is_3He = is_4He = false;
      } else {
        // Apply m2 mass window cuts
        is_d = is_d && (m2_val > d_mean   - cuts.m2SigmaCut * d_sigma   && m2_val < d_mean   + cuts.m2SigmaCut * d_sigma);
        is_t = is_t && (m2_val > t_mean   - cuts.m2SigmaCut * t_sigma   && m2_val < t_mean   + cuts.m2SigmaCut * t_sigma);
        is_3He = is_3He && (m2_val > He3_mean - cuts.m2SigmaCut * He3_sigma && m2_val < He3_mean + cuts.m2SigmaCut * He3_sigma);
        is_4He = is_4He && (m2_val > cuts.minM2_M2cut && m2_val < cuts.maxM2_M2cut);
      }
    }

    if (is_d || is_t || is_3He || is_4He) {
      int best_type = -1;
      double min_nSigma = 999.0;
      if (is_d && fabs(nSigma_d) < min_nSigma) { min_nSigma = fabs(nSigma_d); best_type = 0; }
      if (is_t && fabs(nSigma_t) < min_nSigma) { min_nSigma = fabs(nSigma_t); best_type = 1; }
      if (is_3He && fabs(nSigma_3He) < min_nSigma) { min_nSigma = fabs(nSigma_3He); best_type = 2; }
      if (is_4He && fabs(nSigma_4He) < min_nSigma) { min_nSigma = fabs(nSigma_4He); best_type = 3; }

      if (best_type >= 0) {
        TVector3 act_pMom = pMom;
        if (best_type == 2 || best_type == 3) {
          act_pMom = 2.0 * pMom;
        }
        mNuclearMom.push_back(act_pMom);
        mNuclearId.push_back(trk->id());
        mNuclearType.push_back(best_type);

        if (m_histManager && m_histManager->HasHistogram("hVz_d")) {
          TVector3 pVtx = event->primaryVertex();
          double vz = pVtx.Z();
          if (best_type == 0) m_histManager->Fill("hVz_d", vz);
          if (best_type == 1) m_histManager->Fill("hVz_t", vz);
          if (best_type == 2) m_histManager->Fill("hVz_3He", vz);
          if (best_type == 3) m_histManager->Fill("hVz_4He", vz);
        }
      }
    }

    if (!trk->isTofTrack()) {
      if (m_histManager && m_histManager->HasHistogram("hDedxP_cut")) m_histManager->Fill("hDedxP_cut", p, dedx);
      continue;
    }

    int idx = trk->bTofPidTraitsIndex();
    if (idx < 0) {
      if (m_histManager && m_histManager->HasHistogram("hDedxP_cut")) m_histManager->Fill("hDedxP_cut", p, dedx);
      continue;
    }
    StPicoBTofPidTraits* btof = mPicoDst->btofPidTraits(idx);
    if (!btof) {
      if (m_histManager && m_histManager->HasHistogram("hDedxP_cut")) m_histManager->Fill("hDedxP_cut", p, dedx);
      continue;
    }
    double beta = btof->btofBeta();
    if (beta <= 0) {
      if (m_histManager && m_histManager->HasHistogram("hDedxP_cut")) m_histManager->Fill("hDedxP_cut", p, dedx);
      continue;
    }

    double m2 = p*p*(1.0/(beta*beta) - 1.0);
    if (m_histManager && m_histManager->HasHistogram("hM2P")) m_histManager->Fill("hM2P", p, m2);

    if (m_histManager && m_histManager->HasHistogram("hDedxP_d_m2")) {
      if (m2 > d_mean   - cuts.m2SigmaCut * d_sigma   && m2 < d_mean   + cuts.m2SigmaCut * d_sigma)   m_histManager->Fill("hDedxP_d_m2", p, dedx);
      if (m2 > t_mean   - cuts.m2SigmaCut * t_sigma   && m2 < t_mean   + cuts.m2SigmaCut * t_sigma)   m_histManager->Fill("hDedxP_t_m2", p, dedx);
      if (m2 > He3_mean - cuts.m2SigmaCut * He3_sigma && m2 < He3_mean + cuts.m2SigmaCut * He3_sigma) m_histManager->Fill("hDedxP_3He_m2", p, dedx);
    }

    if (m2 > cuts.minM2_M2cut && m2 < cuts.maxM2_M2cut && p > cuts.minP_M2cut) continue;
    else if (m_histManager && m_histManager->HasHistogram("hDedxP_cut")) m_histManager->Fill("hDedxP_cut", p, dedx);
  }

  if (m_histManager && m_histManager->HasHistogram("hMult_d")) {
    int count[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < mNuclearType.size(); ++i) {
      if (mNuclearType[i] >= 0 && mNuclearType[i] < 4) {
        count[mNuclearType[i]]++;
      }
    }

    m_histManager->Fill("hMult_d", count[0]);
    m_histManager->Fill("hMult_t", count[1]);
    m_histManager->Fill("hMult_3He", count[2]);
    m_histManager->Fill("hMult_4He", count[3]);

    if (hasCentBinMult_d && m_cent9 >= 0 && m_cent9 <= 8) {
      m_histManager->Fill(Form("hMult_d_CentBin%d", m_cent9), count[0]);
      m_histManager->Fill(Form("hMult_t_CentBin%d", m_cent9), count[1]);
      m_histManager->Fill(Form("hMult_3He_CentBin%d", m_cent9), count[2]);
      m_histManager->Fill(Form("hMult_4He_CentBin%d", m_cent9), count[3]);
    }
  }

  return kStOK;
}

Int_t StNuclearIdMaker::Finish() {
  if (mOutName != "" && m_histManager) {
    TFile* fout = new TFile(mOutName.Data(), "RECREATE");
    fout->cd();
    WriteHistograms();
    m_histManager->ReleaseOwnership();
    fout->Close();
    delete fout;
  }
  std::cout << "StNuclearIdMaker::Finish() processed events" << std::endl;
  return kStOK;
}

void StNuclearIdMaker::WriteHistograms() {
  if (m_histManager) m_histManager->Write();
}

//-----------------------------------------------------------------------------
void StNuclearIdMaker::FillKstar(Double_t k_star, Double_t q_lab, Int_t type, Int_t cent9) {
  if (!m_histManager) return;
  static const char* kSpecies[] = {"d", "t", "3He", "4He"};
  if (type < 0 || type > 3) return;
  const char* sp = kSpecies[type];

  m_histManager->Fill(Form("hKstar_%s", sp), k_star);
  m_histManager->Fill(Form("hQlab_%s", sp),  q_lab);

  if (cent9 >= 0 && cent9 <= 8) {
    m_histManager->Fill(Form("hKstar_%s_CentBin%d", sp, cent9), k_star);
  }
}

//-----------------------------------------------------------------------------
void StNuclearIdMaker::FillKstarSideband(Double_t k_star, Double_t q_lab, Int_t type, Int_t sbSign, Int_t cent9) {
  if (!m_histManager) return;
  const char* typeStr = "d";
  if (type == 1) typeStr = "t";
  else if (type == 2) typeStr = "3He";
  else if (type == 3) typeStr = "4He";

  const char* sbStr = (sbSign > 0) ? "SBPos" : "SBNeg";

  m_histManager->Fill(Form("hKstar_%s_%s", typeStr, sbStr), k_star);
  m_histManager->Fill(Form("hQlab_%s_%s",  typeStr, sbStr), q_lab);

  if (cent9 >= 0 && cent9 <= 8) {
    m_histManager->Fill(Form("hKstar_%s_%s_CentBin%d", typeStr, sbStr, cent9), k_star);
  }
}

void StNuclearIdMaker::FillKstarMass(Double_t k_star, Double_t invMass, Int_t type, Int_t cent9) {
  if (!m_histManager) return;
  const char* typeStr = "d";
  if (type == 1) typeStr = "t";
  else if (type == 2) typeStr = "3He";
  else if (type == 3) typeStr = "4He";

  if (cent9 >= 0 && cent9 <= 8) {
    m_histManager->Fill(Form("hKstarMass_%s_CentBin%d", typeStr, cent9), k_star, invMass);
  }
}
