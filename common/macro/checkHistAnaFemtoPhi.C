// checkHistAnaFemtoPhi.C - Draw histograms from run_anaFemtoPhi.C output and write PDF.
// Invoke via: ./script/singularity_checkHistAnaFemtoPhi.sh <root_file> <mainconf_path>
// Or: root4star -b -q 'analysis/run_checkHistAnaFemtoPhi.C("rootfile/...","anaName","config/mainconf/...")'
// If input is anaName_jobid_merge.root, jobid (32 hex) is parsed and PDF becomes anaName_checkHistAnaFemtoPhi_jobid.pdf.
// With config loaded, cut regions are overlaid on pre-cut histograms.

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TH3.h>
#include <TGraphErrors.h>
#include <TF1.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>
#include <TMatrixDSym.h>
#include <TLine.h>
#include <TMath.h>
#include <TObject.h>
#include <TString.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TEllipse.h>
#include <TNamed.h>
#include <TParameter.h>
#include <iostream>
#include <vector>
#include <limits.h>
#include <stdlib.h>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"
#include "cuts/EventCutConfig.h"
#include "cuts/TrackCutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/FemtoConfig.h"
#include <map>
#include <cstring>

static Bool_t gConfigLoaded = kFALSE;

static TString sanitizeGraphName(const std::string& key);

static const Double_t kKstarHistXMin = 0.0;
static const Double_t kKstarHistXMax = 3.0;
static const Double_t kCfKstarXMin = 0.0;
static const Double_t kCfKstarXMax = 0.65;
static const Double_t kCfYMin = 0.5;
static const Double_t kCfYMax = 2.0;


struct BachelorQaSpec {
  const char* histPrefix;
  const char* channelBase;
  const char* cutPrefix;
  const char* nCandKey;
  const char* nSigmaVsPKey;
  const char* nSigmaVsPAllKey;
  const char* nVsCentKey;
  const char* rotChannel;  // nullptr if no rotation channel
  const char* label;
};

static const BachelorQaSpec kBachelorQaSpecs[] = {
    {"hP_", "phi_proton", "proton", "hP_NCand", "hNSigmaProtonVsP", nullptr, "hNProton_vs_Cent9",
     "phi_rot_proton", "p"},
    {"hDeuteron_", "phi_deuteron", "deuteron", "hDeuteron_NCand", "hNSigmaDeuteronVsP",
     "hNSigmaDeuteronVsP_All", "hNDeuteron_vs_Cent9", nullptr, "d"},
    {"hTriton_", "phi_triton", "triton", "hTriton_NCand", "hNSigmaTritonVsP", "hNSigmaTritonVsP_All",
     "hNTriton_vs_Cent9", nullptr, "t"},
    {"hHe3_", "phi_he3", "he3", "hHe3_NCand", "hNSigmaHe3VsP", "hNSigmaHe3VsP_All", "hNHe3_vs_Cent9",
     nullptr, "^{3}He"},
    {"hHe4_", "phi_he4", "he4", "hHe4_NCand", "hNSigmaHe4VsP", "hNSigmaHe4VsP_All", "hNHe4_vs_Cent9",
     nullptr, "^{4}He"},
};
static const Int_t kNBachelorQaSpecs = sizeof(kBachelorQaSpecs) / sizeof(kBachelorQaSpecs[0]);

static const char* kChannelBases[] = {"phi_proton", "phi_deuteron", "phi_triton", "phi_he3", "phi_he4", nullptr};

// h-K correlations extension (two-body h-K CFs + Kubo-rule triplet background).
static const char* kHKaonSpecies[] = {"phikaon_plus", "phikaon_minus", nullptr};
static const char* kHKaonBachelors[] = {"proton", "deuteron", "triton", "he3", "he4", nullptr};
static const char* kHKaonBachLabels[] = {"p", "d", "t", "^{3}He", "^{4}He", nullptr};
static const char* kKuboBases[] = {"phi_proton", "phi_deuteron", nullptr};

static Bool_t isHKaonTwoBodyEnabled() {
  if (!gConfigLoaded) return kFALSE;
  return ConfigManager::GetInstance().GetFemtoConfig().enableHKaonTwoBody;
}

static Bool_t isKuboTripletEnabled() {
  if (!gConfigLoaded) return kFALSE;
  return ConfigManager::GetInstance().GetFemtoConfig().enableKuboTriplet;
}

static Bool_t isKuboGenuineEnabled() {
  if (!gConfigLoaded) return kFALSE;
  return ConfigManager::GetInstance().GetFemtoConfig().enableKuboGenuine;
}

static Bool_t isKstarMassFitCfEnabled() {
  if (!gConfigLoaded) return kTRUE;
  return ConfigManager::GetInstance().GetFemtoConfig().kstarMassFitCfEnabled;
}

static Bool_t isLegacyCfPagesEnabled() {
  if (!gConfigLoaded) return kFALSE;
  return ConfigManager::GetInstance().GetFemtoConfig().legacyCfPagesEnabled;
}

static void setBachelorMass2AxisRange(TH1* h, Double_t ymax = 16.0) {
  if (!h) return;
  if (h->InheritsFrom("TH2")) {
    ((TH2*)h)->GetYaxis()->SetRangeUser(0.0, ymax);
  } else {
    h->GetXaxis()->SetRangeUser(0.0, ymax);
  }
}

struct BachelorCuts {
  BachelorCuts()
    : maxDca(0.0),
      minPMom(0.0),
      maxPMom(0.0),
      minPtPre(0.0),
      maxPtPre(0.0),
      minPtPair(0.0),
      maxPtPair(0.0),
      maxAbsEta(0.0),
      maxAbsNSigma(0.0),
      minMass2(0.0),
      maxMass2(0.0),
      minRapidityCm(0.0),
      maxRapidityCm(0.0),
      tofMomentumThreshold(0.0),
      hasPMomWindow(kFALSE),
      hasMaxPtPre(kFALSE) {}

  Double_t maxDca;
  Double_t minPMom;
  Double_t maxPMom;
  Double_t minPtPre;
  Double_t maxPtPre;
  Double_t minPtPair;
  Double_t maxPtPair;
  Double_t maxAbsEta;
  Double_t maxAbsNSigma;
  Double_t minMass2;
  Double_t maxMass2;
  Double_t minRapidityCm;
  Double_t maxRapidityCm;
  Double_t tofMomentumThreshold;
  Bool_t hasPMomWindow;
  Bool_t hasMaxPtPre;
};

static BachelorCuts getBachelorCuts(const FemtoConfig& fc, const char* cutPrefix) {
  BachelorCuts c;
  c.maxDca = 0.0;
  c.minPMom = 0.0;
  c.maxPMom = 0.0;
  c.minPtPre = 0.0;
  c.maxPtPre = 0.0;
  c.minPtPair = 0.0;
  c.maxPtPair = 0.0;
  c.maxAbsEta = 0.0;
  c.maxAbsNSigma = 0.0;
  c.minMass2 = 0.0;
  c.maxMass2 = 0.0;
  c.minRapidityCm = 0.0;
  c.maxRapidityCm = 0.0;
  c.tofMomentumThreshold = 0.0;
  c.hasPMomWindow = kFALSE;
  c.hasMaxPtPre = kFALSE;
  if (!cutPrefix) return c;
  if (strcmp(cutPrefix, "proton") == 0) {
    c.maxDca = fc.protonMaxDca;
    c.minPtPre = fc.protonMinPtPre;
    c.minPtPair = fc.protonMinPtPair;
    c.maxPtPair = fc.protonMaxPtPair;
    c.maxAbsEta = fc.protonMaxAbsEta;
    c.maxAbsNSigma = fc.protonMaxAbsNSigma;
    c.minMass2 = fc.protonMinMass2;
    c.maxMass2 = fc.protonMaxMass2;
    c.minRapidityCm = fc.protonMinRapidityCm;
    c.maxRapidityCm = fc.protonMaxRapidityCm;
    c.tofMomentumThreshold = fc.protonTofMomentumThreshold;
  } else if (strcmp(cutPrefix, "deuteron") == 0) {
    c.maxDca = fc.deuteronMaxDca;
    c.minPMom = fc.deuteronMinPMom;
    c.maxPMom = fc.deuteronMaxPMom;
    c.minPtPre = fc.deuteronMinPtPre;
    c.maxPtPre = fc.deuteronMaxPtPre;
    c.minPtPair = fc.deuteronMinPtPair;
    c.maxPtPair = fc.deuteronMaxPtPair;
    c.maxAbsEta = fc.deuteronMaxAbsEta;
    c.maxAbsNSigma = fc.deuteronMaxAbsNSigma;
    c.minMass2 = fc.deuteronMinMass2;
    c.maxMass2 = fc.deuteronMaxMass2;
    c.minRapidityCm = fc.deuteronMinRapidityCm;
    c.maxRapidityCm = fc.deuteronMaxRapidityCm;
    c.tofMomentumThreshold = fc.deuteronTofMomentumThreshold;
    c.hasPMomWindow = kTRUE;
    c.hasMaxPtPre = kTRUE;
  } else if (strcmp(cutPrefix, "triton") == 0) {
    c.maxDca = fc.tritonMaxDca;
    c.minPMom = fc.tritonMinPMom;
    c.maxPMom = fc.tritonMaxPMom;
    c.minPtPre = fc.tritonMinPtPre;
    c.maxPtPre = fc.tritonMaxPtPre;
    c.minPtPair = fc.tritonMinPtPair;
    c.maxPtPair = fc.tritonMaxPtPair;
    c.maxAbsEta = fc.tritonMaxAbsEta;
    c.maxAbsNSigma = fc.tritonMaxAbsNSigma;
    c.minMass2 = fc.tritonMinMass2;
    c.maxMass2 = fc.tritonMaxMass2;
    c.minRapidityCm = fc.tritonMinRapidityCm;
    c.maxRapidityCm = fc.tritonMaxRapidityCm;
    c.tofMomentumThreshold = fc.tritonTofMomentumThreshold;
    c.hasPMomWindow = kTRUE;
    c.hasMaxPtPre = kTRUE;
  } else if (strcmp(cutPrefix, "he3") == 0) {
    c.maxDca = fc.he3MaxDca;
    c.minPMom = fc.he3MinPMom;
    c.maxPMom = fc.he3MaxPMom;
    c.minPtPre = fc.he3MinPtPre;
    c.maxPtPre = fc.he3MaxPtPre;
    c.minPtPair = fc.he3MinPtPair;
    c.maxPtPair = fc.he3MaxPtPair;
    c.maxAbsEta = fc.he3MaxAbsEta;
    c.maxAbsNSigma = fc.he3MaxAbsNSigma;
    c.minMass2 = fc.he3MinMass2;
    c.maxMass2 = fc.he3MaxMass2;
    c.minRapidityCm = fc.he3MinRapidityCm;
    c.maxRapidityCm = fc.he3MaxRapidityCm;
    c.tofMomentumThreshold = fc.he3TofMomentumThreshold;
    c.hasPMomWindow = kTRUE;
    c.hasMaxPtPre = kTRUE;
  } else if (strcmp(cutPrefix, "he4") == 0) {
    c.maxDca = fc.he4MaxDca;
    c.minPMom = fc.he4MinPMom;
    c.maxPMom = fc.he4MaxPMom;
    c.minPtPre = fc.he4MinPtPre;
    c.maxPtPre = fc.he4MaxPtPre;
    c.minPtPair = fc.he4MinPtPair;
    c.maxPtPair = fc.he4MaxPtPair;
    c.maxAbsEta = fc.he4MaxAbsEta;
    c.maxAbsNSigma = fc.he4MaxAbsNSigma;
    c.minMass2 = fc.he4MinMass2;
    c.maxMass2 = fc.he4MaxMass2;
    c.minRapidityCm = fc.he4MinRapidityCm;
    c.maxRapidityCm = fc.he4MaxRapidityCm;
    c.tofMomentumThreshold = fc.he4TofMomentumThreshold;
    c.hasPMomWindow = kTRUE;
    c.hasMaxPtPre = kTRUE;
  }
  return c;
}

static TString bachelorHistKey(const BachelorQaSpec& spec, const char* suffix) {
  return TString(spec.histPrefix) + suffix;
}

// ROOT files booked before hist YAML relabel still carry fork titles; fix at draw time.
static void prepareBachelorHist(TH1* h, const char* key, const BachelorQaSpec& spec) {
  if (!h || !key) return;
  TString k(key);
  const char* L = spec.label;
  if (k.EndsWith("_Pt_PreFemtoCut"))
    h->SetTitle(Form("%s p_{T} pre-femto cut;p_{T} [GeV/c];Counts", L));
  else if (k.EndsWith("_Eta_PreFemtoCut"))
    h->SetTitle(Form("%s #eta pre-femto cut;#eta;Counts", L));
  else if (k.Contains("NSigma") && k.Contains("_PreFemtoCut"))
    h->SetTitle(Form("%s n#sigma pre-femto cut;n#sigma;Counts", L));
  else if (k.EndsWith("_Mass2_PreFemtoCut"))
    h->SetTitle(Form("%s TOF m^{2} pre-femto cut;m^{2} [(GeV/c^{2})^{2}];Counts", L));
  else if (k.EndsWith("_DCA_PreFemtoCut"))
    h->SetTitle(Form("%s DCA pre-femto cut;DCA [cm];Counts", L));
  else if (k.EndsWith("_Pt") && !k.Contains("Vs"))
    h->SetTitle(Form("%s p_{T};p_{T} [GeV/c];Counts", L));
  else if (k.EndsWith("_Eta"))
    h->SetTitle(Form("%s #eta;#eta;Counts", L));
  else if (k.EndsWith("_Phi"))
    h->SetTitle(Form("%s #phi;#phi [rad];Counts", L));
  else if (k.Contains("NSigma") && !k.Contains("Vs") && !k.Contains("PreFemto"))
    h->SetTitle(Form("%s n#sigma;n#sigma;Counts", L));
  else if (k.EndsWith("_Mass2") && !k.Contains("Vs"))
    h->SetTitle(Form("%s TOF m^{2};m^{2} [(GeV/c^{2})^{2}];Counts", L));
  else if (k.EndsWith("_DCA"))
    h->SetTitle(Form("%s DCA;DCA [cm];Counts", L));
  else if (k.EndsWith("_Y_PreFemtoCut"))
    h->SetTitle(Form("%s y_{cm} pre-femto cut;y_{cm};Counts", L));
  else if (k.EndsWith("_PtVsY_PreFemtoCut"))
    h->SetTitle(Form("%s p_{T} vs y_{cm} pre-femto;y_{cm};p_{T} [GeV/c]", L));
  else if (k.EndsWith("_Y_FemtoCut"))
    h->SetTitle(Form("%s y_{cm} after femto cut;y_{cm};Counts", L));
  else if (k.EndsWith("_PtVsY_FemtoCut"))
    h->SetTitle(Form("%s p_{T} vs y_{cm} femto cut;y_{cm};p_{T} [GeV/c]", L));
  else if (k.EndsWith("_Mass2VsP") && !k.Contains("wide"))
    h->SetTitle(Form("%s TOF m^{2} vs p;p [GeV/c];m^{2}", L));
  else if (k.EndsWith("_Mass2VsP_PreFemtoCut_wide"))
    h->SetTitle(Form("%s TOF m^{2} vs p pre-femto (wide);p [GeV/c];m^{2}", L));
  else if (k.EndsWith("_Mass2VsP_wide"))
    h->SetTitle(Form("%s TOF m^{2} vs p femto cut (wide);p [GeV/c];m^{2}", L));
  else if (k.EndsWith("_NHitsRatio_FemtoCut"))
    h->SetTitle(Form("%s nHitsFit/nHitsMax (femto cut);ratio;Counts", L));
  else if (k.EndsWith("_NCand"))
    h->SetTitle(Form("%s candidates per event;N;Counts", L));
  if (k.Contains("Mass2")) {
    if (k.Contains("_wide")) setBachelorMass2AxisRange(h, 20.0);
    else setBachelorMass2AxisRange(h, 16.0);
  }
}

static const BachelorQaSpec* findBachelorSpecByBase(const char* channelBase) {
  if (!channelBase) return 0;
  for (Int_t i = 0; i < kNBachelorQaSpecs; ++i) {
    if (strcmp(kBachelorQaSpecs[i].channelBase, channelBase) == 0) return &kBachelorQaSpecs[i];
  }
  return 0;
}

static std::string channelSignal(const std::string& base) { return base + "_signal"; }
static std::string channelLeftSb(const std::string& base) { return base + "_leftSB"; }
static std::string channelRightSb(const std::string& base) { return base + "_rightSB"; }

static TString phiPairMomAngleKeyWithSuffix(const char* channelBase, Bool_t vsMkk, const char* suffix) {
  const std::string ch = channelSignal(channelBase);
  const char* sfx = suffix ? suffix : "";
  if (vsMkk) {
    return TString("hPhiPairMomAngle_vs_MKK_") + ch.c_str() + sfx;
  }
  return TString("hPhiPairMomAngle_") + ch.c_str() + sfx;
}

static TString phiPairMomAngleKey(const char* channelBase, Bool_t vsMkk, Bool_t tofStrict) {
  return phiPairMomAngleKeyWithSuffix(channelBase, vsMkk, tofStrict ? "_tofStrict" : "");
}

static TString resolveFigureRoot(const char* pwd) {
  const char* envFigureRoot = gSystem->Getenv("STAR_QA_FIGURE_ROOT");
  if (envFigureRoot && envFigureRoot[0] != '\0') {
    return TString(envFigureRoot);
  }
  TString figureRoot = TString(pwd ? pwd : ".") + "/share/figure";
  char resolved[PATH_MAX];
  if (realpath(figureRoot.Data(), resolved)) {
    return TString(resolved);
  }
  return figureRoot;
}

static void drawCutLines1D(TH1* h, Double_t x1, Double_t x2, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t ylo = gPad->GetUymin();
  Double_t yhi = gPad->GetUymax();
  const Double_t kBig = 1e30;
  if (x1 > -kBig && x1 < kBig) {
    TLine* l1 = new TLine(x1, ylo, x1, yhi);
    l1->SetLineColor(color);
    l1->SetLineStyle(style);
    l1->Draw("same");
  }
  if (x2 > -kBig && x2 < kBig && TMath::Abs(x2 - x1) > 1e-9) {
    TLine* l2 = new TLine(x2, ylo, x2, yhi);
    l2->SetLineColor(color);
    l2->SetLineStyle(style);
    l2->Draw("same");
  }
}

static void drawCutLine1D(TH1* h, Double_t x, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t ylo = gPad->GetUymin();
  Double_t yhi = gPad->GetUymax();
  TLine* l = new TLine(x, ylo, x, yhi);
  l->SetLineColor(color);
  l->SetLineStyle(style);
  l->Draw("same");
}

static void drawCutLine2DH(TH2* h, Double_t yVal, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t xlo = gPad->GetUxmin();
  Double_t xhi = gPad->GetUxmax();
  TLine* l = new TLine(xlo, yVal, xhi, yVal);
  l->SetLineColor(color);
  l->SetLineStyle(style);
  l->Draw("same");
}

static void drawCutLine2DV(TH2* h, Double_t xVal, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t ylo = gPad->GetUymin();
  Double_t yhi = gPad->GetUymax();
  TLine* l = new TLine(xVal, ylo, xVal, yhi);
  l->SetLineColor(color);
  l->SetLineStyle(style);
  l->Draw("same");
}

static void drawVtxCutCircle(Double_t centerX, Double_t centerY, Double_t radius,
                             Int_t color = kRed, Int_t style = 2) {
  if (!gPad || radius <= 0.0) return;
  TEllipse* circle = new TEllipse(centerX, centerY, radius, radius);
  circle->SetFillStyle(0);
  circle->SetLineColor(color);
  circle->SetLineStyle(style);
  circle->Draw("same");
}

static void drawCent9ConventionNote() {
  if (!gPad) return;
  TLatex* note = new TLatex();
  note->SetNDC(kTRUE);
  note->SetTextSize(0.028);
  note->SetTextColor(kBlue + 1);
  note->DrawLatex(0.12, 0.96, "cent9: StRefMultCorr (0=peripheral, 8=central)");
}

static Bool_t isHex32(const TString& s) {
  if (s.Length() != 32) return kFALSE;
  for (Int_t i = 0; i < 32; i++) {
    Char_t c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
      return kFALSE;
  }
  return kTRUE;
}

static Double_t getHistEntries(TFile* fin, const char* key) {
  if (!fin || !key || key[0] == '\0') return -1.0;
  TObject* obj = fin->Get(key);
  if (!obj || !obj->InheritsFrom("TH1")) return -1.0;
  return ((TH1*)obj)->GetEntries();
}

static std::string cfHistKey(const std::string& channel) {
  return std::string("hCF_") + channel;
}

static std::string kstarSeHistKey(const std::string& channel) {
  return std::string("hKstarSE_") + channel;
}

static std::string kstarMeHistKey(const std::string& channel) {
  return std::string("hKstarME_") + channel;
}

static std::string kstarSeVsCentHistKey(const std::string& channel) {
  return std::string("hKstarSEVsCent_") + channel;
}

static std::string kstarMeVsCentHistKey(const std::string& channel) {
  return std::string("hKstarMEVsCent_") + channel;
}

static std::string cfCentCacheKey(const std::string& channel) {
  return std::string("cent:") + channel;
}

static void getCfCent9Range(Int_t& cent9Min, Int_t& cent9Max) {
  cent9Min = 2;
  cent9Max = 8;
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    cent9Min = femtoCfg.cfCent9Min;
    cent9Max = femtoCfg.cfCent9Max;
  }
}

static TH1* projectKstarVsCent9(TH2* h2, Int_t cent9Min, Int_t cent9Max, const char* hname) {
  if (!h2) return 0;
  Int_t yLo = h2->GetYaxis()->FindBin((Double_t)cent9Min - 0.01);
  Int_t yHi = h2->GetYaxis()->FindBin((Double_t)cent9Max + 0.01);
  TH1* h1 = h2->ProjectionX(hname, yLo, yHi);
  if (h1) {
    h1->SetDirectory(0);
    h1->SetTitle(Form("%s (cent9 %d-%d);k^{*} [GeV/c];Counts", h2->GetTitle(), cent9Min, cent9Max));
    h1->GetXaxis()->SetRangeUser(0.0, kKstarHistXMax);
  }
  return h1;
}

static Int_t getCfRebinFactor() {
  const Int_t kFallbackCfRebinFactor = 5;
  if (!gConfigLoaded) return kFallbackCfRebinFactor;
  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  return (femtoCfg.cfRebinFactor >= 1) ? femtoCfg.cfRebinFactor : 1;
}

static TH1* rebinHistCopy(TH1* h, Int_t factor, const char* cloneSuffix) {
  if (!h || factor <= 1) return 0;
  const Int_t nBins = h->GetNbinsX();
  if (nBins % factor != 0) {
    std::cout << "[checkHistAnaFemtoPhi] WARNING: cannot rebin " << h->GetName() << " with factor "
              << factor << " (nbins=" << nBins << ")\n";
    return 0;
  }
  TH1* hRb = (TH1*)h->Clone(cloneSuffix ? cloneSuffix : "_rebinned");
  hRb->SetDirectory(0);
  hRb->Rebin(factor);
  return hRb;
}

static TGraphErrors* computeCfGraphFromSeMe(TH1* hSE, TH1* hME, Double_t normQMin, Double_t normQMax,
                                            const char* graphTitle) {
  if (!hSE || !hME) return 0;
  Int_t binLo = hSE->FindBin(normQMin + 1e-9);
  Int_t binHi = hSE->FindBin(normQMax - 1e-9);
  Double_t seNorm = hSE->Integral(binLo, binHi);
  Double_t meNorm = hME->Integral(binLo, binHi);
  if (seNorm <= 0 || meNorm <= 0) return 0;

  const Double_t scale = meNorm / seNorm;
  std::vector<Double_t> x;
  std::vector<Double_t> y;
  std::vector<Double_t> ey;
  x.reserve(hSE->GetNbinsX());
  y.reserve(hSE->GetNbinsX());
  ey.reserve(hSE->GetNbinsX());

  for (Int_t ib = 1; ib <= hSE->GetNbinsX(); ++ib) {
    Double_t se = hSE->GetBinContent(ib);
    Double_t me = hME->GetBinContent(ib);
    if (se <= 0 || me <= 0) continue;
    Double_t cf = scale * se / me;
    Double_t err = cf * TMath::Sqrt(1.0 / se + 1.0 / me);
    x.push_back(hSE->GetBinCenter(ib));
    y.push_back(cf);
    ey.push_back(err);
  }
  if (x.empty()) return 0;

  TGraphErrors* gCF = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
  gCF->SetTitle(graphTitle ? graphTitle : "C(k^{*})");
  gCF->SetMarkerStyle(20);
  gCF->SetMarkerSize(0.8);
  gCF->SetLineColor(kBlack);
  gCF->SetMarkerColor(kBlack);
  return gCF;
}

static TGraphErrors* computeCfAndCache(const std::string& channel, const std::string& cacheKey, TH1* hSE, TH1* hME,
                                       Double_t normQMin, Double_t normQMax, std::map<std::string, TGraphErrors*>& cfCache,
                                       const char* logTag) {
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  if (!hSE || !hME) {
    std::cout << "[checkHistAnaFemtoPhi] CF " << channel.c_str();
    if (logTag) std::cout << " (" << logTag << ")";
    std::cout << ": missing SE/ME histograms\n";
    cfCache[cacheKey] = 0;
    return 0;
  }

  const Int_t cfRebinFactor = getCfRebinFactor();
  TH1* hSEForCf = hSE;
  TH1* hMEForCf = hME;
  TH1* hSERebinned = rebinHistCopy(hSE, cfRebinFactor, "_se_rebin");
  TH1* hMERebinned = rebinHistCopy(hME, cfRebinFactor, "_me_rebin");
  if (hSERebinned) hSEForCf = hSERebinned;
  if (hMERebinned) hMEForCf = hMERebinned;

  Int_t binLo = hSEForCf->FindBin(normQMin + 1e-9);
  Int_t binHi = hSEForCf->FindBin(normQMax - 1e-9);
  Double_t seNorm = hSEForCf->Integral(binLo, binHi);
  Double_t meNorm = hMEForCf->Integral(binLo, binHi);
  Double_t aNorm = (seNorm > 0) ? meNorm / seNorm : 0.0;
  std::cout << "[checkHistAnaFemtoPhi] CF " << channel.c_str();
  if (logTag) std::cout << " (" << logTag << ")";
  std::cout << ": normQ=[" << normQMin << ", " << normQMax << "] GeV/c";
  if (cfRebinFactor > 1 && hSERebinned && hMERebinned) {
    std::cout << ", cfRebinFactor=" << cfRebinFactor << " (" << hSE->GetNbinsX() << " -> "
              << hSEForCf->GetNbinsX() << " bins)";
  }
  std::cout << ", seNorm=" << seNorm << ", meNorm=" << meNorm << ", a=meNorm/seNorm=" << aNorm << std::endl;

  TString cfTitle = Form("CF %s (checkHist);k^{*} [GeV/c];C(k^{*})", channel.c_str());
  if (logTag) cfTitle = Form("CF %s %s;k^{*} [GeV/c];C(k^{*})", channel.c_str(), logTag);
  TGraphErrors* gCF = computeCfGraphFromSeMe(hSEForCf, hMEForCf, normQMin, normQMax, cfTitle.Data());
  delete hSERebinned;
  delete hMERebinned;
  if (!gCF) {
    std::cout << "[checkHistAnaFemtoPhi] CF " << channel.c_str() << ": failed (empty norm integrals)\n";
  } else {
    std::cout << "[checkHistAnaFemtoPhi] CF " << channel.c_str() << ": " << gCF->GetN()
              << " points with Poisson stat errors\n";
  }
  cfCache[cacheKey] = gCF;
  return gCF;
}

static TGraphErrors* getOrComputeCf(TFile* fin, const std::string& channel, Double_t normQMin, Double_t normQMax,
                                    std::map<std::string, TGraphErrors*>& cfCache) {
  TH1* hSE = (TH1*)fin->Get(kstarSeHistKey(channel).c_str());
  TH1* hME = (TH1*)fin->Get(kstarMeHistKey(channel).c_str());
  return computeCfAndCache(channel, channel, hSE, hME, normQMin, normQMax, cfCache, 0);
}

static TH1* getProjectedSeMeFromCent(TFile* fin, const std::string& channel, Bool_t isSE, Int_t cent9Min,
                                   Int_t cent9Max) {
  const std::string key = isSE ? kstarSeVsCentHistKey(channel) : kstarMeVsCentHistKey(channel);
  TH2* h2 = (TH2*)fin->Get(key.c_str());
  if (!h2) return 0;
  TString hname = Form("_proj_%s_%s_cent%d_%d", isSE ? "se" : "me", channel.c_str(), cent9Min, cent9Max);
  return projectKstarVsCent9(h2, cent9Min, cent9Max, hname.Data());
}

static TGraphErrors* getOrComputeCfCentSlice(TFile* fin, const std::string& channel, Double_t normQMin,
                                             Double_t normQMax, Int_t cent9Min, Int_t cent9Max,
                                             std::map<std::string, TGraphErrors*>& cfCache) {
  TH1* hSE = getProjectedSeMeFromCent(fin, channel, kTRUE, cent9Min, cent9Max);
  TH1* hME = getProjectedSeMeFromCent(fin, channel, kFALSE, cent9Min, cent9Max);
  TString logTag = Form("cent9 %d-%d from hKstar*VsCent", cent9Min, cent9Max);
  TGraphErrors* gCF =
      computeCfAndCache(channel, cfCentCacheKey(channel), hSE, hME, normQMin, normQMax, cfCache, logTag.Data());
  delete hSE;
  delete hME;
  return gCF;
}

static Double_t channelNormQMin(const std::string& channel) {
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    const FemtoConfig::ChannelDef* ch = femtoCfg.FindChannel(channel);
    if (ch) return ch->normQMin;
  }
  return 0.5;
}

static Double_t channelNormQMax(const std::string& channel) {
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    const FemtoConfig::ChannelDef* ch = femtoCfg.FindChannel(channel);
    if (ch) return ch->normQMax;
  }
  return 1.0;
}

static void populateCfCache(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache) {
  const Double_t kFallbackNormQMin = 0.5;
  const Double_t kFallbackNormQMax = 1.0;
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    for (size_t ic = 0; ic < femtoCfg.channels.size(); ic++) {
      const FemtoConfig::ChannelDef& ch = femtoCfg.channels[ic];
      if (!ch.enabled) continue;
      getOrComputeCf(fin, ch.name, ch.normQMin, ch.normQMax, cfCache);
    }
    return;
  }
  const char* fallbackChannels[] = {"phi_proton", "phi_proton_signal", "phi_proton_leftSB",
                                    "phi_proton_rightSB", "phi_rot_proton",
                                    "phi_deuteron", "phi_deuteron_signal", "phi_deuteron_leftSB",
                                    "phi_deuteron_rightSB",
                                    "phi_triton", "phi_triton_signal", "phi_triton_leftSB",
                                    "phi_triton_rightSB",
                                    "phi_he3", "phi_he3_signal", "phi_he3_leftSB", "phi_he3_rightSB",
                                    "phi_he4", "phi_he4_signal", "phi_he4_leftSB",
                                    "phi_he4_rightSB", 0};
  for (Int_t i = 0; fallbackChannels[i]; ++i) {
    getOrComputeCf(fin, fallbackChannels[i], kFallbackNormQMin, kFallbackNormQMax, cfCache);
  }
}

static void populateCfCentCache(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache) {
  Int_t cent9Min = 0;
  Int_t cent9Max = 0;
  getCfCent9Range(cent9Min, cent9Max);
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    const std::string base(kChannelBases[ib]);
    const char* centChannels[] = {channelSignal(base).c_str(), channelLeftSb(base).c_str(),
                                  channelRightSb(base).c_str(), 0};
    const BachelorQaSpec* spec = findBachelorSpecByBase(base.c_str());
    for (Int_t i = 0; centChannels[i]; ++i) {
      const std::string channel(centChannels[i]);
      getOrComputeCfCentSlice(fin, channel, channelNormQMin(channel), channelNormQMax(channel), cent9Min,
                              cent9Max, cfCache);
    }
    if (spec && spec->rotChannel) {
      const std::string rotCh(spec->rotChannel);
      getOrComputeCfCentSlice(fin, rotCh, channelNormQMin(rotCh), channelNormQMax(rotCh), cent9Min, cent9Max,
                              cfCache);
    }
  }
}

static void drawKstarSeMeHist(TH1* h) {
  if (!h) return;
  h->GetXaxis()->SetRangeUser(kKstarHistXMin, kKstarHistXMax);
  h->Draw();
}

// Scale ME so its integral in [normQMin, normQMax] matches SE (inverse of CF norm factor).
static Double_t computeMeOverlayScale(TH1* hSE, TH1* hME, Double_t normQMin, Double_t normQMax) {
  if (!hSE || !hME) return 1.0;
  Int_t binLo = hSE->FindBin(normQMin + 1e-9);
  Int_t binHi = hSE->FindBin(normQMax - 1e-9);
  Double_t seNorm = hSE->Integral(binLo, binHi);
  Double_t meNorm = hME->Integral(binLo, binHi);
  if (seNorm <= 0 || meNorm <= 0) return 1.0;
  return seNorm / meNorm;
}

static void drawKstarSeMeOverlay(TH1* hSE, TH1* hME, Double_t normQMin, Double_t normQMax,
                                 std::vector<TH1*>& keepAlive) {
  if (!hSE && !hME) return;
  TH1* hMEPlot = hME;
  Bool_t meScaled = kFALSE;
  Double_t meScale = 1.0;
  if (hSE && hME) {
    meScale = computeMeOverlayScale(hSE, hME, normQMin, normQMax);
    if (TMath::Abs(meScale - 1.0) > 1e-12) {
      hMEPlot = (TH1*)hME->Clone();
      hMEPlot->SetDirectory(0);
      hMEPlot->Scale(meScale);
      keepAlive.push_back(hMEPlot);
      meScaled = kTRUE;
    }
  }
  if (hSE) {
    hSE->SetLineColor(kBlack);
    hSE->SetLineWidth(2);
    hSE->GetXaxis()->SetRangeUser(kKstarHistXMin, kKstarHistXMax);
    hSE->Draw("HIST");
  }
  if (hMEPlot) {
    hMEPlot->SetLineColor(kRed);
    hMEPlot->SetLineStyle(2);
    hMEPlot->SetLineWidth(2);
    hMEPlot->GetXaxis()->SetRangeUser(kKstarHistXMin, kKstarHistXMax);
    if (hSE) {
      hMEPlot->Draw("HIST SAME");
    } else {
      hMEPlot->Draw("HIST");
    }
  }
  if (hSE && hMEPlot && gPad) {
    TLegend* leg = new TLegend(0.55, 0.68, 0.88, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->AddEntry(hSE, "Same Event", "l");
    if (meScaled) {
      leg->AddEntry(hMEPlot, Form("Mixed Event (norm x%.3g)", meScale), "l");
    } else {
      leg->AddEntry(hMEPlot, "Mixed Event", "l");
    }
    leg->Draw();
  }
}

static void drawCentProjectedSeMeOverlay(TCanvas* canvas, Int_t pad, TFile* fin, const std::string& channel,
                                         Int_t cent9Min, Int_t cent9Max, std::vector<TH1*>& centProjKeepAlive) {
  if (!canvas) return;
  canvas->cd(pad);
  TH1* hSE = getProjectedSeMeFromCent(fin, channel, kTRUE, cent9Min, cent9Max);
  TH1* hME = getProjectedSeMeFromCent(fin, channel, kFALSE, cent9Min, cent9Max);
  if (hSE) centProjKeepAlive.push_back(hSE);
  if (hME) centProjKeepAlive.push_back(hME);
  drawKstarSeMeOverlay(hSE, hME, channelNormQMin(channel), channelNormQMax(channel), centProjKeepAlive);
}

static void drawCfGraph(TGraphErrors* gCF) {
  if (!gCF) return;
  gCF->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
  gCF->Draw("AP");
  TH1* hFrame = gCF->GetHistogram();
  if (hFrame) {
    hFrame->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
    hFrame->GetYaxis()->SetRangeUser(kCfYMin, kCfYMax);
  }
}

static void drawCentSliceCf(TCanvas* canvas, Int_t pad, const std::string& channel,
                            std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->cd(pad);
  std::map<std::string, TGraphErrors*>::const_iterator it = cfCache.find(cfCentCacheKey(channel));
  if (it != cfCache.end() && it->second) drawCfGraph(it->second);
}

// Divide(nCols, 2): columns = channels, rows = SE+ME overlay / CF.
static Int_t centSliceLayoutPad(Int_t col, Int_t rowOverlayCf, Int_t nCols) { return col + rowOverlayCf * nCols + 1; }

static void drawCentSlicePageForBase(TCanvas* canvas, TFile* fin, const std::string& channelBase,
                              const char* rotChannel, Int_t cent9Min, Int_t cent9Max,
                              std::vector<TH1*>& centProjKeepAlive,
                              std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->Clear();
  const char* channels[5];
  Int_t nCols = 3;
  channels[0] = channelSignal(channelBase).c_str();
  channels[1] = channelLeftSb(channelBase).c_str();
  channels[2] = channelRightSb(channelBase).c_str();
  channels[3] = rotChannel;
  channels[4] = 0;
  if (rotChannel && rotChannel[0] != '\0') nCols = 4;
  canvas->Divide(nCols, 2);
  for (Int_t ic = 0; ic < nCols; ++ic) {
    const std::string channel(channels[ic]);
    drawCentProjectedSeMeOverlay(canvas, centSliceLayoutPad(ic, 0, nCols), fin, channel, cent9Min, cent9Max,
                                 centProjKeepAlive);
    drawCentSliceCf(canvas, centSliceLayoutPad(ic, 1, nCols), channel, cfCache);
  }
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98, Form("%s cent slice (cent9 %d-%d)", channelBase.c_str(), cent9Min, cent9Max));
  }
}

static void drawComputedCf(TCanvas* canvas, Int_t pad, TFile* fin, const std::string& channel, Double_t normQMin,
                           Double_t normQMax, std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->cd(pad);
  TGraphErrors* gCF = getOrComputeCf(fin, channel, normQMin, normQMax, cfCache);
  if (gCF) drawCfGraph(gCF);
}

static Int_t getCachedCfPointCount(const std::map<std::string, TGraphErrors*>& cfCache, const char* channel) {
  std::map<std::string, TGraphErrors*>::const_iterator it = cfCache.find(channel);
  if (it == cfCache.end() || !it->second) return -1;
  return it->second->GetN();
}

static void freeCfCache(std::map<std::string, TGraphErrors*>& cfCache) {
  for (std::map<std::string, TGraphErrors*>::iterator it = cfCache.begin(); it != cfCache.end(); ++it) {
    if (it->second) delete it->second;
  }
  cfCache.clear();
}

static void freeCentProjKeepAlive(std::vector<TH1*>& centProjKeepAlive) {
  for (size_t i = 0; i < centProjKeepAlive.size(); ++i) {
    if (centProjKeepAlive[i]) delete centProjKeepAlive[i];
  }
  centProjKeepAlive.clear();
}


static std::string cfSliceCacheKey(const std::string& sliceId, const std::string& tag) {
  return std::string("slice:") + sliceId + ":" + tag;
}

static std::vector<FemtoConfig::CfCentSlice> getCfCentSliceList() {
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    if (!femtoCfg.cfCentSlices.empty()) return femtoCfg.cfCentSlices;
  }
  std::vector<FemtoConfig::CfCentSlice> fallback;
  for (Int_t i = 0; i <= 8; ++i) {
    FemtoConfig::CfCentSlice sl;
    sl.id = Form("cent9_%d", i);
    sl.cent9Min = i;
    sl.cent9Max = i;
    fallback.push_back(sl);
  }
  static const char* kPctIds[] = {"pct_0_10", "pct_0_20", "pct_0_30", "pct_0_40", "pct_0_50", "pct_0_60", 0};
  static const Int_t kPctMin[] = {7, 6, 5, 4, 3, 2};
  static const Int_t kPctMax[] = {8, 8, 8, 8, 8, 8};
  for (Int_t ip = 0; kPctIds[ip]; ++ip) {
    FemtoConfig::CfCentSlice sl;
    sl.id = kPctIds[ip];
    sl.cent9Min = kPctMin[ip];
    sl.cent9Max = kPctMax[ip];
    fallback.push_back(sl);
  }
  return fallback;
}

static Bool_t isSliceInQaPdf(const std::string& sliceId) {
  if (!gConfigLoaded) {
    return sliceId == "pct_0_10" || sliceId == "pct_0_20" || sliceId == "pct_0_30";
  }
  return ConfigManager::GetInstance().GetFemtoConfig().IsCfCentSliceInQaPdf(sliceId);
}


static TH1* combineSidebandLR(TH1* left, TH1* right) {
  if (!left && !right) return 0;
  if (!left) {
    TH1* c = (TH1*)right->Clone("_sblr");
    c->SetDirectory(0);
    return c;
  }
  if (!right) {
    TH1* c = (TH1*)left->Clone("_sblr");
    c->SetDirectory(0);
    return c;
  }
  TH1* sum = (TH1*)left->Clone("_sblr");
  sum->SetDirectory(0);
  sum->Add(right);
  return sum;
}

// Sub-CF alpha from FemtoConfig channel mass-window widths: alpha = w_signal / w_sideband.
static Bool_t getSidebandWidthAlphas(const std::string& channelBase, Double_t& alphaL, Double_t& alphaR) {
  alphaL = 0.014 / 0.015;
  alphaR = 0.014 / 0.025;
  if (!gConfigLoaded) return kFALSE;
  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  const FemtoConfig::ChannelDef* chSig = femtoCfg.FindChannel(channelSignal(channelBase));
  const FemtoConfig::ChannelDef* chL = femtoCfg.FindChannel(channelLeftSb(channelBase));
  const FemtoConfig::ChannelDef* chR = femtoCfg.FindChannel(channelRightSb(channelBase));
  if (!chSig || !chL || !chR) return kFALSE;
  const Double_t wSig = chSig->signalMax - chSig->signalMin;
  const Double_t wL = chL->signalMax - chL->signalMin;
  const Double_t wR = chR->signalMax - chR->signalMin;
  if (wSig <= 0 || wL <= 0 || wR <= 0) return kFALSE;
  alphaL = wSig / wL;
  alphaR = wSig / wR;
  return kTRUE;
}

// (alphaL * left + alphaR * right) / 2; clones inputs (does not delete left/right).
static TH1* combineSidebandScaledAverage(TH1* left, TH1* right, Double_t alphaL, Double_t alphaR) {
  TH1* out = 0;
  if (left) {
    out = (TH1*)left->Clone("_sb_lr_scaled");
    out->SetDirectory(0);
    out->Scale(alphaL);
  }
  if (right) {
    TH1* rScaled = (TH1*)right->Clone("_sb_r_scaled");
    rScaled->SetDirectory(0);
    rScaled->Scale(alphaR);
    if (out) {
      out->Add(rScaled);
      delete rScaled;
    } else {
      out = rScaled;
    }
  }
  if (out) out->Scale(0.5);
  return out;
}

static const char* getNegativeBinPolicy() {
  if (!gConfigLoaded) return "zero";
  return ConfigManager::GetInstance().GetFemtoConfig().negativeBinPolicy.c_str();
}

static TH1* subtractSidebandHist(TH1* signal, TH1* sb, Double_t alpha) {
  if (!signal || !sb) return 0;
  TH1* out = (TH1*)signal->Clone("_sb_sub");
  out->SetDirectory(0);
  out->Add(sb, -alpha);
  const char* policy = getNegativeBinPolicy();
  if (policy && TString(policy) == "zero") {
    for (Int_t ib = 1; ib <= out->GetNbinsX(); ++ib) {
      if (out->GetBinContent(ib) < 0.0) {
        out->SetBinContent(ib, 0.0);
        out->SetBinError(ib, 0.0);
      }
    }
  }
  return out;
}

static TH1* getSliceProjectedSeMe(TFile* fin, const std::string& channel, Bool_t isSE, Int_t cent9Min,
                                  Int_t cent9Max) {
  return getProjectedSeMeFromCent(fin, channel, isSE, cent9Min, cent9Max);
}

static TGraphErrors* getOrComputeSliceChannelCf(TFile* fin, const std::string& sliceId, Int_t cent9Min,
                                                Int_t cent9Max, const std::string& channel, Double_t normQMin,
                                                Double_t normQMax, std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, channel);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  TH1* hSE = getSliceProjectedSeMe(fin, channel, kTRUE, cent9Min, cent9Max);
  TH1* hME = getSliceProjectedSeMe(fin, channel, kFALSE, cent9Min, cent9Max);
  TString logTag = Form("%s cent9 %d-%d", sliceId.c_str(), cent9Min, cent9Max);
  TGraphErrors* gCF =
      computeCfAndCache(channel, cacheKey, hSE, hME, normQMin, normQMax, cfCache, logTag.Data());
  delete hSE;
  delete hME;
  return gCF;
}

static TGraphErrors* getOrComputeSliceSblrCf(TFile* fin, const std::string& sliceId, Int_t cent9Min,
                                             Int_t cent9Max, const std::string& channelBase,
                                             Double_t normQMin, Double_t normQMax,
                                             std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, channelBase + ":SBLR");
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  const std::string chL = channelLeftSb(channelBase);
  const std::string chR = channelRightSb(channelBase);
  TH1* hSEL = getSliceProjectedSeMe(fin, chL, kTRUE, cent9Min, cent9Max);
  TH1* hSER = getSliceProjectedSeMe(fin, chR, kTRUE, cent9Min, cent9Max);
  TH1* hMEL = getSliceProjectedSeMe(fin, chL, kFALSE, cent9Min, cent9Max);
  TH1* hMER = getSliceProjectedSeMe(fin, chR, kFALSE, cent9Min, cent9Max);
  TH1* hSE = combineSidebandLR(hSEL, hSER);
  TH1* hME = combineSidebandLR(hMEL, hMER);
  delete hSEL;
  delete hSER;
  delete hMEL;
  delete hMER;
  TString logTag = Form("%s %s SBLR cent9 %d-%d", sliceId.c_str(), channelBase.c_str(), cent9Min, cent9Max);
  TGraphErrors* gCF = computeCfAndCache(channelBase + "_SBLR", cacheKey, hSE, hME, normQMin, normQMax, cfCache,
                                        logTag.Data());
  delete hSE;
  delete hME;
  return gCF;
}

static TGraphErrors* getOrComputeSliceSigSubCf(TFile* fin, const std::string& sliceId, Int_t cent9Min,
                                               Int_t cent9Max, const std::string& channelBase,
                                               const std::string& sbChannel, const char* subTag,
                                               Double_t normQMin, Double_t normQMax,
                                               std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, channelBase + ":" + subTag);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  Double_t alphaL = 0.0;
  Double_t alphaR = 0.0;
  getSidebandWidthAlphas(channelBase, alphaL, alphaR);
  Double_t alphaApply = 1.0;
  const std::string chSig = channelSignal(channelBase);
  const std::string chL = channelLeftSb(channelBase);
  const std::string chR = channelRightSb(channelBase);
  TH1* hSEsig = getSliceProjectedSeMe(fin, chSig, kTRUE, cent9Min, cent9Max);
  TH1* hMEsig = getSliceProjectedSeMe(fin, chSig, kFALSE, cent9Min, cent9Max);
  TH1* hSEsb = 0;
  TH1* hMEsb = 0;
  if (sbChannel == "SBLR") {
    TH1* hSEL = getSliceProjectedSeMe(fin, chL, kTRUE, cent9Min, cent9Max);
    TH1* hSER = getSliceProjectedSeMe(fin, chR, kTRUE, cent9Min, cent9Max);
    TH1* hMEL = getSliceProjectedSeMe(fin, chL, kFALSE, cent9Min, cent9Max);
    TH1* hMER = getSliceProjectedSeMe(fin, chR, kFALSE, cent9Min, cent9Max);
    hSEsb = combineSidebandScaledAverage(hSEL, hSER, alphaL, alphaR);
    hMEsb = combineSidebandScaledAverage(hMEL, hMER, alphaL, alphaR);
    delete hSEL;
    delete hSER;
    delete hMEL;
    delete hMER;
    alphaApply = 1.0;
  } else if (sbChannel == chL) {
    hSEsb = getSliceProjectedSeMe(fin, sbChannel, kTRUE, cent9Min, cent9Max);
    hMEsb = getSliceProjectedSeMe(fin, sbChannel, kFALSE, cent9Min, cent9Max);
    alphaApply = alphaL;
  } else if (sbChannel == chR) {
    hSEsb = getSliceProjectedSeMe(fin, sbChannel, kTRUE, cent9Min, cent9Max);
    hMEsb = getSliceProjectedSeMe(fin, sbChannel, kFALSE, cent9Min, cent9Max);
    alphaApply = alphaR;
  } else {
    hSEsb = getSliceProjectedSeMe(fin, sbChannel, kTRUE, cent9Min, cent9Max);
    hMEsb = getSliceProjectedSeMe(fin, sbChannel, kFALSE, cent9Min, cent9Max);
    alphaApply = 1.0;
  }
  TH1* hSEcorr = subtractSidebandHist(hSEsig, hSEsb, alphaApply);
  TH1* hMEcorr = subtractSidebandHist(hMEsig, hMEsb, alphaApply);
  delete hSEsig;
  delete hMEsig;
  delete hSEsb;
  delete hMEsb;
  TString logTag = Form("%s %s %s cent9 %d-%d alphaL=%.4f alphaR=%.4f apply=%.4f", sliceId.c_str(),
                        channelBase.c_str(), subTag, cent9Min, cent9Max, alphaL, alphaR, alphaApply);
  TGraphErrors* gCF = computeCfAndCache(chSig, cacheKey, hSEcorr, hMEcorr, normQMin, normQMax, cfCache,
                                        logTag.Data());
  delete hSEcorr;
  delete hMEcorr;
  return gCF;
}

static void populateCfSliceCachesForBase(TFile* fin, const std::string& channelBase,
                                           std::map<std::string, TGraphErrors*>& cfCache) {
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  const std::string chSig = channelSignal(channelBase);
  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);
  const char* sliceChannels[] = {chSig.c_str(), channelLeftSb(channelBase).c_str(),
                                 channelRightSb(channelBase).c_str(), 0};
  for (size_t is = 0; is < slices.size(); ++is) {
    const FemtoConfig::CfCentSlice& sl = slices[is];
    for (Int_t ic = 0; sliceChannels[ic]; ++ic) {
      const std::string channel(sliceChannels[ic]);
      getOrComputeSliceChannelCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channel, channelNormQMin(channel),
                                 channelNormQMax(channel), cfCache);
    }
    getOrComputeSliceSblrCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channelBase, normQMin, normQMax, cfCache);
    getOrComputeSliceSigSubCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channelBase, channelLeftSb(channelBase),
                              "CF_sig_sub_SBL", normQMin, normQMax, cfCache);
    getOrComputeSliceSigSubCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channelBase, channelRightSb(channelBase),
                              "CF_sig_sub_SBR", normQMin, normQMax, cfCache);
    getOrComputeSliceSigSubCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channelBase, "SBLR", "CF_sig_sub_SBLR",
                              normQMin, normQMax, cfCache);
  }
}

static void populateCfSliceCaches(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache) {
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    populateCfSliceCachesForBase(fin, std::string(kChannelBases[ib]), cfCache);
  }
}

static Int_t sidebandSliceLayoutPad(Int_t col, Int_t row) { return col + row * 4 + 1; }

static void drawSliceCfGraph(TCanvas* canvas, Int_t pad, const std::string& sliceId, const std::string& tag,
                             std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->cd(pad);
  std::map<std::string, TGraphErrors*>::const_iterator it = cfCache.find(cfSliceCacheKey(sliceId, tag));
  if (it != cfCache.end() && it->second) drawCfGraph(it->second);
}

static void drawSidebandSlicePageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                           const std::string& channelBase, std::vector<TH1*>& centProjKeepAlive,
                                           std::map<std::string, TGraphErrors*>& cfCache, Bool_t drawSubCfRow) {
  if (!canvas) return;
  canvas->Clear();
  const Int_t nRows = drawSubCfRow ? 3 : 2;
  canvas->Divide(4, nRows);
  const std::string chSig = channelSignal(channelBase);
  const std::string chL = channelLeftSb(channelBase);
  const std::string chR = channelRightSb(channelBase);
  const char* colTags[] = {chSig.c_str(), chL.c_str(), chR.c_str(), "SBLR"};
  for (Int_t ic = 0; ic < 4; ++ic) {
    const std::string tag(colTags[ic]);
    if (tag == "SBLR") {
      TH1* hSEL = getSliceProjectedSeMe(fin, chL, kTRUE, slice.cent9Min, slice.cent9Max);
      TH1* hSER = getSliceProjectedSeMe(fin, chR, kTRUE, slice.cent9Min, slice.cent9Max);
      TH1* hMEL = getSliceProjectedSeMe(fin, chL, kFALSE, slice.cent9Min, slice.cent9Max);
      TH1* hMER = getSliceProjectedSeMe(fin, chR, kFALSE, slice.cent9Min, slice.cent9Max);
      TH1* hSE = combineSidebandLR(hSEL, hSER);
      TH1* hME = combineSidebandLR(hMEL, hMER);
      canvas->cd(sidebandSliceLayoutPad(ic, 0));
      if (hSE) {
        hSE->SetTitle(Form("SE+ME SBLR %s %s", channelBase.c_str(), slice.id.c_str()));
        centProjKeepAlive.push_back(hSE);
      }
      if (hME) centProjKeepAlive.push_back(hME);
      drawKstarSeMeOverlay(hSE, hME, channelNormQMin(chSig), channelNormQMax(chSig), centProjKeepAlive);
      getOrComputeSliceSblrCf(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                              channelNormQMin(chSig), channelNormQMax(chSig), cfCache);
      drawSliceCfGraph(canvas, sidebandSliceLayoutPad(ic, 1), slice.id, channelBase + ":SBLR", cfCache);
    } else {
      canvas->cd(sidebandSliceLayoutPad(ic, 0));
      TH1* hSE = getSliceProjectedSeMe(fin, tag, kTRUE, slice.cent9Min, slice.cent9Max);
      TH1* hME = getSliceProjectedSeMe(fin, tag, kFALSE, slice.cent9Min, slice.cent9Max);
      if (hSE) centProjKeepAlive.push_back(hSE);
      if (hME) centProjKeepAlive.push_back(hME);
      drawKstarSeMeOverlay(hSE, hME, channelNormQMin(tag), channelNormQMax(tag), centProjKeepAlive);
      getOrComputeSliceChannelCf(fin, slice.id, slice.cent9Min, slice.cent9Max, tag, channelNormQMin(tag),
                                 channelNormQMax(tag), cfCache);
      drawSliceCfGraph(canvas, sidebandSliceLayoutPad(ic, 1), slice.id, tag, cfCache);
    }
  }
  if (drawSubCfRow) {
    const char* subTags[] = {"CF_sig_sub_SBL", "CF_sig_sub_SBR", "CF_sig_sub_SBLR"};
    const char* subSb[] = {chL.c_str(), chR.c_str(), "SBLR"};
    drawSliceCfGraph(canvas, sidebandSliceLayoutPad(0, 2), slice.id, chSig, cfCache);
    for (Int_t isb = 0; isb < 3; ++isb) {
      getOrComputeSliceSigSubCf(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase, subSb[isb], subTags[isb],
                                channelNormQMin(chSig), channelNormQMax(chSig), cfCache);
      drawSliceCfGraph(canvas, sidebandSliceLayoutPad(isb + 1, 2), slice.id, channelBase + ":" + subTags[isb],
                       cfCache);
    }
  }
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98,
                   Form("%s %s (cent9 %d-%d)", channelBase.c_str(), slice.id.c_str(), slice.cent9Min, slice.cent9Max));
  }
}

static void printCfSliceConsoleSummary(const std::map<std::string, TGraphErrors*>& cfCache) {
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  std::cout << "\n=== CF cent slices (all channel bases) ===\n";
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    const std::string channelBase(kChannelBases[ib]);
    const std::string chSig = channelSignal(channelBase);
    const std::string chL = channelLeftSb(channelBase);
    const std::string chR = channelRightSb(channelBase);
    std::cout << "  channel base " << channelBase << ":\n";
    for (size_t is = 0; is < slices.size(); ++is) {
      const FemtoConfig::CfCentSlice& sl = slices[is];
      std::cout << "    slice " << sl.id << " cent9 [" << sl.cent9Min << "," << sl.cent9Max << "]:\n";
      for (const std::string* pch : {&chSig, &chL, &chR}) {
        Int_t nPts = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, *pch).c_str());
        std::cout << "      " << *pch << " raw CF nPoints=" << nPts << "\n";
      }
      Int_t nSblr = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, channelBase + ":SBLR").c_str());
      std::cout << "      SBLR raw CF nPoints=" << nSblr << "\n";
      const char* subTags[] = {"CF_sig_sub_SBL", "CF_sig_sub_SBR", "CF_sig_sub_SBLR"};
      for (Int_t it = 0; it < 3; ++it) {
        Int_t nSub = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, channelBase + ":" + subTags[it]).c_str());
        std::cout << "      " << subTags[it] << " nPoints=" << nSub << "\n";
      }
      Int_t nLam = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, std::string("lambda_sig_") + channelBase).c_str());
      Int_t nBkgMe = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, std::string("CF_bkg_me_") + channelBase).c_str());
      Int_t nGen = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, std::string("CF_genuine_") + channelBase).c_str());
      Int_t nCfSub =
          getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, std::string("CF_CFsub_SBLR_") + channelBase).c_str());
      std::cout << "      lambda_sig nPoints=" << nLam << ", CF_bkg_me nPoints=" << nBkgMe
                << ", CF_genuine nPoints=" << nGen << ", CF_CFsub_SBLR nPoints=" << nCfSub << "\n";
    }
  }
  std::cout << "=============================================================\n\n";
}

static std::string phiMkkVsKstarSeKey(const std::string& channel) {
  return std::string("hPhiMKK_vs_KstarSE_") + channel;
}

static std::string phiMkkVsKstarMeKey(const std::string& channel) {
  return std::string("hPhiMKK_vs_KstarME_") + channel;
}

static Bool_t getChannelSignalMassWindow(const std::string& channel, Double_t& sigMin, Double_t& sigMax) {
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    const FemtoConfig::ChannelDef* ch = femtoCfg.FindChannel(channel);
    if (ch) {
      sigMin = ch->signalMin;
      sigMax = ch->signalMax;
      return kTRUE;
    }
  }
  sigMin = 1.01;
  sigMax = 1.03;
  return kTRUE;
}

static TH2* projectMkkVsKstarForSlice(TH3* h3, Int_t cent9Min, Int_t cent9Max, const char* hname) {
  if (!h3) return 0;
  Int_t zLo = h3->GetZaxis()->FindBin((Double_t)cent9Min - 0.01);
  Int_t zHi = h3->GetZaxis()->FindBin((Double_t)cent9Max + 0.01);
  TString name = hname ? hname : "_mkk_kstar_proj";
  TH2F* h2 = new TH2F(name, Form("%s (cent9 %d-%d)", h3->GetTitle(), cent9Min, cent9Max),
                      h3->GetNbinsX(), h3->GetXaxis()->GetXmin(), h3->GetXaxis()->GetXmax(),
                      h3->GetNbinsY(), h3->GetYaxis()->GetXmin(), h3->GetYaxis()->GetXmax());
  h2->SetDirectory(0);
  for (Int_t ix = 1; ix <= h3->GetNbinsX(); ++ix) {
    for (Int_t iy = 1; iy <= h3->GetNbinsY(); ++iy) {
      Double_t sum = 0.0;
      Double_t sumErr2 = 0.0;
      for (Int_t iz = zLo; iz <= zHi; ++iz) {
        sum += h3->GetBinContent(ix, iy, iz);
        sumErr2 += h3->GetBinError(ix, iy, iz) * h3->GetBinError(ix, iy, iz);
      }
      h2->SetBinContent(ix, iy, sum);
      h2->SetBinError(ix, iy, TMath::Sqrt(sumErr2));
    }
  }
  return h2;
}

static void applyNegativeBinZeroHist(TH1* h) {
  if (!h) return;
  const char* policy = getNegativeBinPolicy();
  if (!policy || TString(policy) != "zero") return;
  for (Int_t ib = 1; ib <= h->GetNbinsX(); ++ib) {
    if (h->GetBinContent(ib) < 0.0) {
      h->SetBinContent(ib, 0.0);
      h->SetBinError(ib, 0.0);
    }
  }
}

static Bool_t fitLambdaFromSubMass(TH1* hSub, Double_t sigMin, Double_t sigMax, Double_t sigmaMin, Double_t sigmaMax,
                                   Bool_t useConstBkg, Double_t& lambdaSig, Double_t& lambdaBkg, Double_t& errLambdaSig) {
  lambdaSig = 0.0;
  lambdaBkg = 0.0;
  errLambdaSig = 0.0;
  if (!hSub) return kFALSE;
  Int_t binLo = hSub->GetXaxis()->FindBin(sigMin + 1e-9);
  Int_t binHi = hSub->GetXaxis()->FindBin(sigMax - 1e-9);
  if (hSub->Integral(binLo, binHi) <= 0.0) return kFALSE;

  TString fname = Form("fpurity_%lx", (unsigned long)hSub);
  TF1* f = 0;
  if (useConstBkg) {
    f = new TF1(fname + "_gc", "gaus(0)+[3]", sigMin, sigMax);
  } else {
    f = new TF1(fname + "_g", "gaus", sigMin, sigMax);
  }
  Double_t peak = hSub->GetMaximum();
  Int_t maxBin = hSub->GetMaximumBin();
  f->SetParameter(0, peak);
  f->SetParameter(1, hSub->GetXaxis()->GetBinCenter(maxBin));
  f->SetParameter(2, 0.006);
  f->SetParLimits(2, sigmaMin, sigmaMax);
  if (useConstBkg) f->SetParameter(3, 0.1 * peak);

  Int_t fitStat = hSub->Fit(f, "RQ0");
  if (fitStat != 0) {
    delete f;
    return kFALSE;
  }

  const Double_t nSig = f->GetParameter(0) * f->GetParameter(2) * TMath::Sqrt(2.0 * TMath::Pi());
  Double_t nBkg = 0.0;
  if (useConstBkg) nBkg = f->GetParameter(3) * (sigMax - sigMin);
  if (nSig <= 0.0 || (nSig + nBkg) <= 0.0) {
    delete f;
    return kFALSE;
  }
  lambdaSig = nSig / (nSig + nBkg);
  lambdaBkg = nBkg / (nSig + nBkg);
  errLambdaSig = lambdaSig * TMath::Sqrt(TMath::Power(f->GetParError(0) / (f->GetParameter(0) + 1e-12), 2) +
                                       TMath::Power(f->GetParError(2) / (f->GetParameter(2) + 1e-12), 2));
  delete f;
  return kTRUE;
}

static TGraphErrors* computeLambdaSigGraph(TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                           const std::string& channelBase) {
  const std::string chSig = channelSignal(channelBase);
  Double_t sigMin = 1.01;
  Double_t sigMax = 1.03;
  getChannelSignalMassWindow(chSig, sigMin, sigMax);

  TH3* h3SE = (TH3*)fin->Get(phiMkkVsKstarSeKey(chSig).c_str());
  TH3* h3ME = (TH3*)fin->Get(phiMkkVsKstarMeKey(chSig).c_str());
  if (!h3SE || !h3ME) return 0;

  TH2* h2SE = projectMkkVsKstarForSlice(h3SE, slice.cent9Min, slice.cent9Max, "_mkkse");
  TH2* h2ME = projectMkkVsKstarForSlice(h3ME, slice.cent9Min, slice.cent9Max, "_mkkme");
  if (!h2SE || !h2ME) {
    delete h2SE;
    delete h2ME;
    return 0;
  }

  Double_t purityMinK = 0.0;
  Double_t purityMaxK = 0.65;
  Int_t minEntries = 20;
  Double_t clampMin = 0.05;
  Double_t clampMax = 1.0;
  Double_t sigmaMin = 0.002;
  Double_t sigmaMax = 0.020;
  Bool_t useConstBkg = kTRUE;
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    purityMinK = fc.purityMinKstar;
    purityMaxK = fc.purityMaxKstar;
    minEntries = fc.purityMinEntriesPerBin;
    clampMin = fc.purityClampMin;
    clampMax = fc.purityClampMax;
    sigmaMin = fc.purityFitGaussSigmaMin;
    sigmaMax = fc.purityFitGaussSigmaMax;
    useConstBkg = fc.purityFitUseConstantBkg;
  }

  std::vector<Double_t> x;
  std::vector<Double_t> y;
  std::vector<Double_t> ey;
  for (Int_t iy = 1; iy <= h2SE->GetNbinsY(); ++iy) {
    const Double_t kstar = h2SE->GetYaxis()->GetBinCenter(iy);
    if (kstar < purityMinK || kstar > purityMaxK) continue;

    TH1* hSE_ib = h2SE->ProjectionX(Form("_se_ib_%d", iy), iy, iy);
    TH1* hME_ib = h2ME->ProjectionX(Form("_me_ib_%d", iy), iy, iy);
    if (!hSE_ib || !hME_ib) {
      delete hSE_ib;
      delete hME_ib;
      continue;
    }
    hSE_ib->SetDirectory(0);
    hME_ib->SetDirectory(0);

    if (hSE_ib->GetEntries() < minEntries) {
      delete hSE_ib;
      delete hME_ib;
      continue;
    }

    const Double_t nSE = hSE_ib->Integral();
    const Double_t nME = hME_ib->Integral();
    if (nME <= 0.0) {
      delete hSE_ib;
      delete hME_ib;
      continue;
    }
    const Double_t sIb = nSE / nME;
    TH1* hSub = (TH1*)hSE_ib->Clone("_sub_mass");
    hSub->SetDirectory(0);
    hSub->Add(hME_ib, -sIb);
    applyNegativeBinZeroHist(hSub);

    Double_t lambdaSig = 0.0;
    Double_t lambdaBkg = 0.0;
    Double_t errLambda = 0.0;
    if (!fitLambdaFromSubMass(hSub, sigMin, sigMax, sigmaMin, sigmaMax, useConstBkg, lambdaSig, lambdaBkg,
                              errLambda)) {
      delete hSE_ib;
      delete hME_ib;
      delete hSub;
      continue;
    }
    if (lambdaSig < clampMin) lambdaSig = clampMin;
    if (lambdaSig > clampMax) lambdaSig = clampMax;

    x.push_back(kstar);
    y.push_back(lambdaSig);
    ey.push_back(errLambda);

    delete hSE_ib;
    delete hME_ib;
    delete hSub;
  }
  delete h2SE;
  delete h2ME;
  if (x.empty()) return 0;

  TGraphErrors* g = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
  g->SetTitle(Form("#lambda_{sig}(k^{*}) %s %s", channelBase.c_str(), slice.id.c_str()));
  g->SetMarkerStyle(20);
  g->SetMarkerSize(0.8);
  return g;
}

static TGraphErrors* getOrComputeSliceMeBkgCf(TFile* fin, const std::string& sliceId, Int_t cent9Min, Int_t cent9Max,
                                              const std::string& channelBase, Double_t normQMin, Double_t normQMax,
                                              std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, std::string("CF_bkg_me_") + channelBase);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  const std::string chSig = channelSignal(channelBase);
  Double_t sigMin = 1.01;
  Double_t sigMax = 1.03;
  getChannelSignalMassWindow(chSig, sigMin, sigMax);

  TH3* h3SE = (TH3*)fin->Get(phiMkkVsKstarSeKey(chSig).c_str());
  TH3* h3ME = (TH3*)fin->Get(phiMkkVsKstarMeKey(chSig).c_str());
  if (!h3SE || !h3ME) {
    cfCache[cacheKey] = 0;
    return 0;
  }

  TH2* h2SE = projectMkkVsKstarForSlice(h3SE, cent9Min, cent9Max, "_mkkse_bkg");
  TH2* h2ME = projectMkkVsKstarForSlice(h3ME, cent9Min, cent9Max, "_mkkme_bkg");
  if (!h2SE || !h2ME) {
    delete h2SE;
    delete h2ME;
    cfCache[cacheKey] = 0;
    return 0;
  }

  Int_t kyLo = h2SE->GetYaxis()->FindBin(normQMin + 1e-9);
  Int_t kyHi = h2SE->GetYaxis()->FindBin(normQMax - 1e-9);
  Double_t nSE = h2SE->Integral(1, h2SE->GetNbinsX(), kyLo, kyHi);
  Double_t nME = h2ME->Integral(1, h2ME->GetNbinsX(), kyLo, kyHi);
  if (nME <= 0.0) {
    delete h2SE;
    delete h2ME;
    cfCache[cacheKey] = 0;
    return 0;
  }
  const Double_t sScale = nSE / nME;

  Int_t mxLo = h2ME->GetXaxis()->FindBin(sigMin + 1e-9);
  Int_t mxHi = h2ME->GetXaxis()->FindBin(sigMax - 1e-9);
  TH1* hSE_bkg = h2ME->ProjectionY(Form("_se_bkg_%s", channelBase.c_str()), mxLo, mxHi);
  if (hSE_bkg) {
    hSE_bkg->SetDirectory(0);
    hSE_bkg->Scale(sScale);
  }

  TH1* hME_ref = getSliceProjectedSeMe(fin, chSig, kFALSE, cent9Min, cent9Max);
  TString logTag = Form("%s %s ME-bkg CF cent9 %d-%d", sliceId.c_str(), channelBase.c_str(), cent9Min, cent9Max);
  TGraphErrors* gCF = computeCfAndCache(channelBase + "_bkg_me", cacheKey, hSE_bkg, hME_ref, normQMin, normQMax,
                                        cfCache, logTag.Data());
  delete h2SE;
  delete h2ME;
  delete hSE_bkg;
  delete hME_ref;
  return gCF;
}

static Double_t interpGraphY(const TGraphErrors* g, Double_t x, Double_t* eyOut = 0) {
  if (!g || g->GetN() < 1) return -1.0;
  if (x < g->GetX()[0] || x > g->GetX()[g->GetN() - 1]) return -1.0;
  Int_t idx = TMath::BinarySearch(g->GetN(), g->GetX(), x);
  if (idx < 0) idx = 0;
  if (idx >= g->GetN() - 1) idx = g->GetN() - 2;
  const Double_t x0 = g->GetX()[idx];
  const Double_t x1 = g->GetX()[idx + 1];
  const Double_t y0 = g->GetY()[idx];
  const Double_t y1 = g->GetY()[idx + 1];
  const Double_t t = (x1 > x0) ? (x - x0) / (x1 - x0) : 0.0;
  if (eyOut) *eyOut = TMath::Sqrt(TMath::Power(g->GetEY()[idx], 2) * (1.0 - t) + TMath::Power(g->GetEY()[idx + 1], 2) * t);
  return y0 + t * (y1 - y0);
}

static TGraphErrors* computeGenuineCfGraph(TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                         const std::string& channelBase, Double_t normQMin, Double_t normQMax,
                                         std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(slice.id, std::string("CF_genuine_") + channelBase);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  const std::string chSig = channelSignal(channelBase);
  TGraphErrors* gMeas = getOrComputeSliceChannelCf(fin, slice.id, slice.cent9Min, slice.cent9Max, chSig, normQMin,
                                                   normQMax, cfCache);
  TGraphErrors* gBkg = getOrComputeSliceMeBkgCf(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase, normQMin,
                                                normQMax, cfCache);
  const std::string lamKey = cfSliceCacheKey(slice.id, std::string("lambda_sig_") + channelBase);
  TGraphErrors* gLambda = 0;
  std::map<std::string, TGraphErrors*>::const_iterator itLam = cfCache.find(lamKey);
  if (itLam != cfCache.end()) {
    gLambda = itLam->second;
  } else {
    gLambda = computeLambdaSigGraph(fin, slice, channelBase);
    cfCache[lamKey] = gLambda;
  }
  if (!gMeas || !gBkg || !gLambda) {
    cfCache[cacheKey] = 0;
    return 0;
  }

  std::vector<Double_t> x;
  std::vector<Double_t> y;
  std::vector<Double_t> ey;
  for (Int_t ip = 0; ip < gMeas->GetN(); ++ip) {
    const Double_t kstar = gMeas->GetX()[ip];
    const Double_t cMeas = gMeas->GetY()[ip];
    const Double_t eMeas = gMeas->GetEY()[ip];

    Double_t eBkg = 0.0;
    const Double_t cBkg = interpGraphY(gBkg, kstar, &eBkg);
    Double_t eLam = 0.0;
    const Double_t lambdaSig = interpGraphY(gLambda, kstar, &eLam);
    if (cBkg < 0.0 || lambdaSig <= 0.0) continue;
    const Double_t lambdaBkg = 1.0 - lambdaSig;

    const Double_t numer = (cMeas - 1.0) - lambdaBkg * (cBkg - 1.0);
    const Double_t cGen = 1.0 + numer / lambdaSig;
    const Double_t dGenDcMeas = 1.0 / lambdaSig;
    const Double_t dGenDcBkg = -lambdaBkg / lambdaSig;
    // d/dlambda of 1 + numer/lambda, with lambda_bkg = 1 - lambda.
    const Double_t dGenDLam = (cBkg - 1.0) / lambdaSig - numer / (lambdaSig * lambdaSig);
    const Double_t err = TMath::Sqrt(TMath::Power(dGenDcMeas * eMeas, 2) + TMath::Power(dGenDcBkg * eBkg, 2) +
                                     TMath::Power(dGenDLam * eLam, 2));

    x.push_back(kstar);
    y.push_back(cGen);
    ey.push_back(err);
  }

  if (x.empty()) {
    cfCache[cacheKey] = 0;
    return 0;
  }
  TGraphErrors* gGen = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
  gGen->SetTitle(Form("C_{genuine}(k^{*}) %s %s", channelBase.c_str(), slice.id.c_str()));
  gGen->SetMarkerStyle(21);
  gGen->SetMarkerColor(kBlue + 1);
  gGen->SetLineColor(kBlue + 1);
  cfCache[cacheKey] = gGen;
  std::cout << "[checkHistAnaFemtoPhi] CF_genuine " << channelBase << " " << slice.id << ": " << gGen->GetN()
            << " points\n";
  return gGen;
}

static Bool_t isMethod5Enabled() {
  if (!gConfigLoaded) return kFALSE;
  if (!ConfigManager::GetInstance().GetFemtoConfig().legacyCfPagesEnabled) return kFALSE;
  return ConfigManager::GetInstance().GetFemtoConfig().cfSubtractionMode == "method5";
}

static Bool_t isCfSubWriteSidecar() {
  if (!gConfigLoaded) return kTRUE;
  return ConfigManager::GetInstance().GetFemtoConfig().cfSubWriteSidecarRoot;
}

static Int_t getCfSubEffectiveRebin(const std::string& channelBase) {
  Int_t base = getCfRebinFactor();
  Int_t extra = 1;
  if (gConfigLoaded) {
    extra = ConfigManager::GetInstance().GetFemtoConfig().cfSubLowStatsRebinExtra;
  }
  if (extra > 1 &&
      (channelBase == "phi_triton" || channelBase == "phi_he3" || channelBase == "phi_he4")) {
    return base * extra;
  }
  return base;
}

// Project TH3 (M_KK x k* x cent9) to inclusive M_KK for a cent9 slice (sum over k*).
static TH1* projectMkkForCentSlice(TH3* h3, Int_t cent9Min, Int_t cent9Max, const char* hname) {
  if (!h3) return 0;
  Int_t zLo = h3->GetZaxis()->FindBin((Double_t)cent9Min - 0.01);
  Int_t zHi = h3->GetZaxis()->FindBin((Double_t)cent9Max + 0.01);
  TString name = hname ? hname : "_mkk_cent_proj";
  TH1D* h1 = new TH1D(name, Form("%s (cent9 %d-%d)", h3->GetTitle(), cent9Min, cent9Max), h3->GetNbinsX(),
                      h3->GetXaxis()->GetXmin(), h3->GetXaxis()->GetXmax());
  h1->SetDirectory(0);
  if (!h1->GetSumw2N()) h1->Sumw2();
  for (Int_t ix = 1; ix <= h3->GetNbinsX(); ++ix) {
    Double_t sum = 0.0;
    Double_t sumErr2 = 0.0;
    for (Int_t iy = 1; iy <= h3->GetNbinsY(); ++iy) {
      for (Int_t iz = zLo; iz <= zHi; ++iz) {
        sum += h3->GetBinContent(ix, iy, iz);
        const Double_t err = h3->GetBinError(ix, iy, iz);
        sumErr2 += err * err;
      }
    }
    h1->SetBinContent(ix, sum);
    h1->SetBinError(ix, TMath::Sqrt(sumErr2));
  }
  return h1;
}

// method 5: constant phi M_KK purity P for slice (gaus + const fit on SE MKK projection).
static Bool_t estimatePhiMassPurityForSlice(TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                            const std::string& channelBase, Double_t& purity,
                                            Double_t& errPurity) {
  purity = 0.0;
  errPurity = 0.0;

  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    if (fc.cfSubPurityMode == "fixed") {
      purity = fc.cfSubPurityFixed;
      errPurity = 0.0;
      return purity > 0.0;
    }
  }

  const std::string chSig = channelSignal(channelBase);
  Double_t sigMin = 1.01;
  Double_t sigMax = 1.03;
  getChannelSignalMassWindow(chSig, sigMin, sigMax);

  Bool_t useConstBkg = kTRUE;
  Double_t sigmaMin = 0.002;
  Double_t sigmaMax = 0.020;
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    useConstBkg = fc.purityFitUseConstantBkg;
    sigmaMin = fc.purityFitGaussSigmaMin;
    sigmaMax = fc.purityFitGaussSigmaMax;
  }

  TH1* hMkk = 0;
  TH3* h3SE = (TH3*)fin->Get(phiMkkVsKstarSeKey(chSig).c_str());
  if (h3SE) {
    // Signal-channel TH3 is already mass-window selected at fill; fit gaus+const on SE
    // projection (no SE-ME). SE-ME on a truncated window is unstable for constant P.
    hMkk = projectMkkForCentSlice(h3SE, slice.cent9Min, slice.cent9Max, "_mkk_purity_slice");
  }

  Double_t lambdaSig = 0.0;
  Double_t lambdaBkg = 0.0;
  Double_t errLam = 0.0;
  Bool_t ok = kFALSE;
  if (hMkk) {
    ok = fitLambdaFromSubMass(hMkk, sigMin, sigMax, sigmaMin, sigmaMax, useConstBkg, lambdaSig, lambdaBkg, errLam);
  }
  if (!ok) {
    delete hMkk;
    hMkk = 0;
    TH1* hFallback = (TH1*)fin->Get("hPhi_MKK");
    if (!hFallback) hFallback = (TH1*)fin->Get("hPhi_MKK_signal");
    if (hFallback) {
      hMkk = (TH1*)hFallback->Clone("_mkk_purity_fallback");
      hMkk->SetDirectory(0);
      // For inclusive hPhi_MKK, allow a wider fit window around the peak.
      Double_t fitMin = sigMin;
      Double_t fitMax = sigMax;
      if (fin->Get("hPhi_MKK") == hFallback) {
        fitMin = 0.99;
        fitMax = 1.06;
      }
      ok = fitLambdaFromSubMass(hMkk, fitMin, fitMax, sigmaMin, sigmaMax, useConstBkg, lambdaSig, lambdaBkg,
                                errLam);
      // Recompute P in the analysis signal window if fit used a wider range.
      if (ok && (fitMin < sigMin - 1e-9 || fitMax > sigMax + 1e-9)) {
        // Keep lambdaSig from fit as purity proxy in signal window (gaus+const ratio).
      }
    }
  }
  if (!ok && gConfigLoaded) {
    // Last resort: YAML fixed purity so method5 pages still produce CFsub.
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    if (fc.cfSubPurityFixed > 0.0 && fc.cfSubPurityFixed <= 1.0) {
      purity = fc.cfSubPurityFixed;
      errPurity = 0.0;
      delete hMkk;
      std::cout << "[checkHistAnaFemtoPhi] purity fit failed for " << channelBase << " " << slice.id
                << "; using cfSubPurityFixed=" << purity << "\n";
      return kTRUE;
    }
  }
  delete hMkk;
  if (!ok || lambdaSig <= 0.0) return kFALSE;

  Double_t clampMin = 0.05;
  Double_t clampMax = 1.0;
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    clampMin = fc.purityClampMin;
    clampMax = fc.purityClampMax;
  }
  if (lambdaSig < clampMin) lambdaSig = clampMin;
  if (lambdaSig > clampMax) lambdaSig = clampMax;

  purity = lambdaSig;
  // Cap pathological fit error so CF bars stay usable; full cov is a later systematic.
  errPurity = errLam;
  if (errPurity > 0.5) errPurity = 0.5;
  return kTRUE;
}

static TGraphErrors* applyPurityCfCorrection(TGraphErrors* gSig, TGraphErrors* gSB, Double_t purity,
                                             Double_t errPurity, const char* graphTitle) {
  if (!gSig || !gSB || purity <= 0.0) return 0;

  std::vector<Double_t> x;
  std::vector<Double_t> y;
  std::vector<Double_t> ey;
  const Double_t oneMinusP = 1.0 - purity;

  for (Int_t ip = 0; ip < gSig->GetN(); ++ip) {
    const Double_t kstar = gSig->GetX()[ip];
    const Double_t cSig = gSig->GetY()[ip];
    const Double_t eSig = gSig->GetEY()[ip];

    Double_t eSB = 0.0;
    const Double_t cSB = interpGraphY(gSB, kstar, &eSB);
    if (cSB < 0.0) continue;

    const Double_t cCorr = (cSig - oneMinusP * cSB) / purity;
    // Independent-error starting point: sigma^2 = (sig_sig^2 + (1-P)^2 sig_SB^2) / P^2
    Double_t eCorr =
        TMath::Sqrt(TMath::Power(eSig / purity, 2) + TMath::Power(oneMinusP * eSB / purity, 2));
    if (errPurity > 0.0) {
      const Double_t dCorrDP = -(cSig - cSB) / (purity * purity);
      eCorr = TMath::Sqrt(eCorr * eCorr + TMath::Power(dCorrDP * errPurity, 2));
    }

    x.push_back(kstar);
    y.push_back(cCorr);
    ey.push_back(eCorr);
  }

  if (x.empty()) return 0;
  TGraphErrors* gOut = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
  gOut->SetTitle(graphTitle ? graphTitle : "C_{CFsub}(k^{*})");
  gOut->SetMarkerStyle(21);
  gOut->SetMarkerColor(kRed + 1);
  gOut->SetLineColor(kRed + 1);
  return gOut;
}

static TGraphErrors* computeCfFromProjectedWithRebin(TH1* hSE, TH1* hME, Int_t rebinFactor, Double_t normQMin,
                                                     Double_t normQMax, const char* title) {
  if (!hSE || !hME) return 0;
  TH1* seUse = hSE;
  TH1* meUse = hME;
  TH1* seReb = 0;
  TH1* meReb = 0;
  if (rebinFactor > 1) {
    seReb = rebinHistCopy(hSE, rebinFactor, "_cfsub_se");
    meReb = rebinHistCopy(hME, rebinFactor, "_cfsub_me");
    if (seReb) seUse = seReb;
    if (meReb) meUse = meReb;
  }
  TGraphErrors* g = computeCfGraphFromSeMe(seUse, meUse, normQMin, normQMax, title);
  delete seReb;
  delete meReb;
  return g;
}

static TGraphErrors* getOrComputeSliceSbCfForMethod5(TFile* fin, const std::string& sliceId, Int_t cent9Min,
                                                     Int_t cent9Max, const std::string& channelBase,
                                                     const std::string& sbTag, Double_t normQMin, Double_t normQMax,
                                                     Int_t rebinFactor, std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, channelBase + ":CF_SB_" + sbTag);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  TH1* hSE = 0;
  TH1* hME = 0;
  if (sbTag == "SBLR") {
    const std::string chL = channelLeftSb(channelBase);
    const std::string chR = channelRightSb(channelBase);
    TH1* hSEL = getSliceProjectedSeMe(fin, chL, kTRUE, cent9Min, cent9Max);
    TH1* hSER = getSliceProjectedSeMe(fin, chR, kTRUE, cent9Min, cent9Max);
    TH1* hMEL = getSliceProjectedSeMe(fin, chL, kFALSE, cent9Min, cent9Max);
    TH1* hMER = getSliceProjectedSeMe(fin, chR, kFALSE, cent9Min, cent9Max);
    if (hSEL && !hSEL->GetSumw2N()) hSEL->Sumw2();
    if (hSER && !hSER->GetSumw2N()) hSER->Sumw2();
    if (hMEL && !hMEL->GetSumw2N()) hMEL->Sumw2();
    if (hMER && !hMER->GetSumw2N()) hMER->Sumw2();
    hSE = combineSidebandLR(hSEL, hSER);
    hME = combineSidebandLR(hMEL, hMER);
    delete hSEL;
    delete hSER;
    delete hMEL;
    delete hMER;
  } else {
    hSE = getSliceProjectedSeMe(fin, sbTag, kTRUE, cent9Min, cent9Max);
    hME = getSliceProjectedSeMe(fin, sbTag, kFALSE, cent9Min, cent9Max);
    if (hSE && !hSE->GetSumw2N()) hSE->Sumw2();
    if (hME && !hME->GetSumw2N()) hME->Sumw2();
  }

  TString title = Form("CF_SB %s %s %s", channelBase.c_str(), sbTag.c_str(), sliceId.c_str());
  TGraphErrors* g = computeCfFromProjectedWithRebin(hSE, hME, rebinFactor, normQMin, normQMax, title.Data());
  delete hSE;
  delete hME;
  cfCache[cacheKey] = g;
  return g;
}

static TGraphErrors* getOrComputeSliceSigCfForMethod5(TFile* fin, const std::string& sliceId, Int_t cent9Min,
                                                      Int_t cent9Max, const std::string& channelBase,
                                                      Double_t normQMin, Double_t normQMax, Int_t rebinFactor,
                                                      std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, std::string("CF_sig_method5_") + channelBase);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  const std::string chSig = channelSignal(channelBase);
  // Prefer standard cache when rebin matches global factor.
  if (rebinFactor == getCfRebinFactor()) {
    TGraphErrors* gStd =
        getOrComputeSliceChannelCf(fin, sliceId, cent9Min, cent9Max, chSig, normQMin, normQMax, cfCache);
    cfCache[cacheKey] = gStd;
    return gStd;
  }

  TH1* hSE = getSliceProjectedSeMe(fin, chSig, kTRUE, cent9Min, cent9Max);
  TH1* hME = getSliceProjectedSeMe(fin, chSig, kFALSE, cent9Min, cent9Max);
  if (hSE && !hSE->GetSumw2N()) hSE->Sumw2();
  if (hME && !hME->GetSumw2N()) hME->Sumw2();
  TString title = Form("CF_sig method5 %s %s", channelBase.c_str(), sliceId.c_str());
  TGraphErrors* g = computeCfFromProjectedWithRebin(hSE, hME, rebinFactor, normQMin, normQMax, title.Data());
  delete hSE;
  delete hME;
  cfCache[cacheKey] = g;
  return g;
}

static TGraphErrors* computeCfSubMethod5Graph(TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                              const std::string& channelBase, Double_t normQMin, Double_t normQMax,
                                              std::map<std::string, TGraphErrors*>& cfCache,
                                              std::map<std::string, Double_t>& purityCache) {
  const std::string cacheKey = cfSliceCacheKey(slice.id, std::string("CF_CFsub_SBLR_") + channelBase);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  const Int_t rebinFactor = getCfSubEffectiveRebin(channelBase);
  TGraphErrors* gSig = getOrComputeSliceSigCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                        normQMin, normQMax, rebinFactor, cfCache);

  std::string combine = "sumLR";
  if (gConfigLoaded) combine = ConfigManager::GetInstance().GetFemtoConfig().cfSubSidebandCombine;
  TGraphErrors* gSB = 0;
  if (combine == "avgCF_LR") {
    TGraphErrors* gL = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                       channelLeftSb(channelBase), normQMin, normQMax, rebinFactor,
                                                       cfCache);
    TGraphErrors* gR = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                       channelRightSb(channelBase), normQMin, normQMax, rebinFactor,
                                                       cfCache);
    if (gL && gR) {
      // Pointwise average of L and R CFs (systematic combine mode).
      std::vector<Double_t> x, y, ey;
      for (Int_t ip = 0; ip < gL->GetN(); ++ip) {
        const Double_t k = gL->GetX()[ip];
        Double_t eR = 0.0;
        const Double_t yR = interpGraphY(gR, k, &eR);
        if (yR < 0.0) continue;
        const Double_t yAvg = 0.5 * (gL->GetY()[ip] + yR);
        const Double_t eAvg = 0.5 * TMath::Sqrt(TMath::Power(gL->GetEY()[ip], 2) + TMath::Power(eR, 2));
        x.push_back(k);
        y.push_back(yAvg);
        ey.push_back(eAvg);
      }
      if (!x.empty()) {
        gSB = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
        gSB->SetTitle(Form("CF_SB avgCF_LR %s %s", channelBase.c_str(), slice.id.c_str()));
        cfCache[cfSliceCacheKey(slice.id, channelBase + ":CF_SB_avgCF_LR")] = gSB;
      }
    }
  } else {
    gSB = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase, "SBLR",
                                          normQMin, normQMax, rebinFactor, cfCache);
  }

  if (!gSig || !gSB) {
    cfCache[cacheKey] = 0;
    return 0;
  }

  Double_t purity = 0.0;
  Double_t errPurity = 0.0;
  if (!estimatePhiMassPurityForSlice(fin, slice, channelBase, purity, errPurity)) {
    std::cout << "[checkHistAnaFemtoPhi] CF_CFsub " << channelBase << " " << slice.id << ": purity fit failed\n";
    cfCache[cacheKey] = 0;
    return 0;
  }
  purityCache[cfSliceCacheKey(slice.id, std::string("phi_mass_purity_") + channelBase)] = purity;
  purityCache[cfSliceCacheKey(slice.id, std::string("phi_mass_purity_err_") + channelBase)] = errPurity;

  std::cout << "[checkHistAnaFemtoPhi] CF_CFsub " << channelBase << " " << slice.id << ": P=" << purity << " +/- "
            << errPurity << " rebin=" << rebinFactor << " combine=" << combine << " (cent9 " << slice.cent9Min << "-"
            << slice.cent9Max << ")\n";

  TString title = Form("C_{CFsub}(k^{*}) %s %s (P=%.3f)", channelBase.c_str(), slice.id.c_str(), purity);
  TGraphErrors* gSub = applyPurityCfCorrection(gSig, gSB, purity, errPurity, title.Data());
  cfCache[cacheKey] = gSub;
  if (gSub) {
    std::cout << "[checkHistAnaFemtoPhi] CF_CFsub " << channelBase << " " << slice.id << ": " << gSub->GetN()
              << " points\n";
  }

  // Also build left/right CFsub for systematic comparison (cached).
  TGraphErrors* gSBL = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                       channelLeftSb(channelBase), normQMin, normQMax, rebinFactor,
                                                       cfCache);
  TGraphErrors* gSBR = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                       channelRightSb(channelBase), normQMin, normQMax, rebinFactor,
                                                       cfCache);
  if (gSig && gSBL) {
    TString tL = Form("C_{CFsub,SBL} %s %s", channelBase.c_str(), slice.id.c_str());
    cfCache[cfSliceCacheKey(slice.id, std::string("CF_CFsub_SBL_") + channelBase)] =
        applyPurityCfCorrection(gSig, gSBL, purity, errPurity, tL.Data());
  }
  if (gSig && gSBR) {
    TString tR = Form("C_{CFsub,SBR} %s %s", channelBase.c_str(), slice.id.c_str());
    cfCache[cfSliceCacheKey(slice.id, std::string("CF_CFsub_SBR_") + channelBase)] =
        applyPurityCfCorrection(gSig, gSBR, purity, errPurity, tR.Data());
  }
  return gSub;
}

static void populateMethod5CachesForBase(TFile* fin, const std::string& channelBase,
                                         std::map<std::string, TGraphErrors*>& cfCache,
                                         std::map<std::string, Double_t>& purityCache) {
  if (!isMethod5Enabled()) return;
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  const std::string chSig = channelSignal(channelBase);
  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);
  for (size_t is = 0; is < slices.size(); ++is) {
    computeCfSubMethod5Graph(fin, slices[is], channelBase, normQMin, normQMax, cfCache, purityCache);
  }
}

static void populateMethod5Caches(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache,
                                  std::map<std::string, Double_t>& purityCache) {
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    populateMethod5CachesForBase(fin, std::string(kChannelBases[ib]), cfCache, purityCache);
  }
}

static TString sanitizeGraphName(const std::string& key) {
  TString name(key.c_str());
  name.ReplaceAll(":", "_");
  name.ReplaceAll("-", "_");
  name.ReplaceAll("/", "_");
  name.ReplaceAll(" ", "_");
  return name;
}

static void writeCfSubSidecarRoot(const TString& outDir, const TString& anaName, const TString& jobid,
                                  const Char_t* inputRootFile, const Char_t* mainconfPath,
                                  std::map<std::string, TGraphErrors*>& cfCache,
                                  const std::map<std::string, Double_t>& purityCache) {
  if (!isCfSubWriteSidecar() || !isMethod5Enabled()) return;

  TString outPath = outDir + anaName + "_checkHistAnaFemtoPhi_CFsub";
  if (jobid.Length()) outPath += "_" + jobid;
  outPath += ".root";

  TFile* fout = TFile::Open(outPath, "RECREATE");
  if (!fout || fout->IsZombie()) {
    std::cerr << "[checkHistAnaFemtoPhi] WARNING: cannot write sidecar " << outPath << std::endl;
    return;
  }

  // Persist method5 / related CF graphs without overwriting Maker products.
  for (std::map<std::string, TGraphErrors*>::iterator it = cfCache.begin(); it != cfCache.end(); ++it) {
    if (!it->second) continue;
    const std::string& key = it->first;
    const Bool_t keep = (key.find("CF_CFsub_") != std::string::npos) ||
                        (key.find("CF_sig_method5_") != std::string::npos) ||
                        (key.find(":CF_SB_") != std::string::npos) ||
                        (key.find("CF_genuine_") != std::string::npos) ||
                        (key.find("lambda_sig_") != std::string::npos) ||
                        (key.find("CF_bkg_me_") != std::string::npos) ||
                        (key.find(":CF_sig_sub_") != std::string::npos) ||
                        (key.find(":SBLR") != std::string::npos);
    if (!keep) continue;
    TGraphErrors* clone = (TGraphErrors*)it->second->Clone(sanitizeGraphName(key));
    if (clone) {
      clone->Write();
      delete clone;
    }
  }

  for (std::map<std::string, Double_t>::const_iterator it = purityCache.begin(); it != purityCache.end(); ++it) {
    TParameter<Double_t> p(sanitizeGraphName(it->first), it->second);
    p.Write();
  }

  TNamed metaInput("meta_inputRoot", inputRootFile ? inputRootFile : "");
  metaInput.Write();
  TNamed metaMain("meta_mainconf", mainconfPath ? mainconfPath : "");
  metaMain.Write();
  TNamed metaJob("meta_jobid", jobid.Data());
  metaJob.Write();
  TNamed metaMode("meta_cfSubtractionMode",
                  gConfigLoaded ? ConfigManager::GetInstance().GetFemtoConfig().cfSubtractionMode.c_str()
                                : "method5");
  metaMode.Write();
  TNamed metaPurityMode("meta_cfSubPurityMode",
                        gConfigLoaded ? ConfigManager::GetInstance().GetFemtoConfig().cfSubPurityMode.c_str()
                                      : "fit_slice");
  metaPurityMode.Write();
  TNamed metaCombine("meta_cfSubSidebandCombine",
                     gConfigLoaded ? ConfigManager::GetInstance().GetFemtoConfig().cfSubSidebandCombine.c_str()
                                   : "sumLR");
  metaCombine.Write();
  TParameter<Int_t> pRebin("meta_cfRebinFactor", getCfRebinFactor());
  pRebin.Write();
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    TParameter<Int_t> pExtra("meta_cfSubLowStatsRebinExtra", fc.cfSubLowStatsRebinExtra);
    pExtra.Write();
    TParameter<Double_t> pFixed("meta_cfSubPurityFixed", fc.cfSubPurityFixed);
    pFixed.Write();
  }

  fout->Write();
  fout->Close();
  delete fout;
  std::cout << "[checkHistAnaFemtoPhi] Wrote CFsub sidecar: " << outPath << std::endl;
}

static void dumpMethod5ConfigLog(const std::map<std::string, Double_t>& purityCache) {
  std::cout << "\n=== method5 CF-subtraction parameters ===\n";
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    std::cout << "  cfSubtractionMode=" << fc.cfSubtractionMode << "\n";
    std::cout << "  cfSubPurityMode=" << fc.cfSubPurityMode << "\n";
    std::cout << "  cfSubPurityFixed=" << fc.cfSubPurityFixed << "\n";
    std::cout << "  cfSubSidebandCombine=" << fc.cfSubSidebandCombine << "\n";
    std::cout << "  cfSubWriteSidecarRoot=" << (fc.cfSubWriteSidecarRoot ? "true" : "false") << "\n";
    std::cout << "  cfSubLowStatsRebinExtra=" << fc.cfSubLowStatsRebinExtra << "\n";
    std::cout << "  cfRebinFactor=" << fc.cfRebinFactor << "\n";
    std::cout << "  negativeBinPolicy=" << fc.negativeBinPolicy << "\n";
  } else {
    std::cout << "  (config not loaded; using method5 defaults)\n";
  }
  std::cout << "  fitted slice purities:\n";
  for (std::map<std::string, Double_t>::const_iterator it = purityCache.begin(); it != purityCache.end(); ++it) {
    if (it->first.find("phi_mass_purity_err_") != std::string::npos) continue;
    if (it->first.find("phi_mass_purity_") == std::string::npos) continue;
    Double_t err = 0.0;
    std::string errKey = it->first;
    const std::string needle = "phi_mass_purity_";
    size_t pos = errKey.find(needle);
    if (pos != std::string::npos) {
      errKey.replace(pos, needle.size(), "phi_mass_purity_err_");
      std::map<std::string, Double_t>::const_iterator itE = purityCache.find(errKey);
      if (itE != purityCache.end()) err = itE->second;
    }
    std::cout << "    " << it->first << " = " << it->second << " +/- " << err << "\n";
  }
  std::cout << "=========================================\n\n";
}

static void populatePurityGenuineCachesForBase(TFile* fin, const std::string& channelBase,
                                               std::map<std::string, TGraphErrors*>& cfCache) {
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  const std::string chSig = channelSignal(channelBase);
  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);
  for (size_t is = 0; is < slices.size(); ++is) {
    const FemtoConfig::CfCentSlice& sl = slices[is];
    const std::string lamKey = cfSliceCacheKey(sl.id, std::string("lambda_sig_") + channelBase);
    if (cfCache.find(lamKey) == cfCache.end()) {
      TGraphErrors* gLam = computeLambdaSigGraph(fin, sl, channelBase);
      cfCache[lamKey] = gLam;
      if (gLam) {
        std::cout << "[checkHistAnaFemtoPhi] lambda_sig " << channelBase << " " << sl.id << ": " << gLam->GetN()
                  << " points\n";
      }
    }
    getOrComputeSliceMeBkgCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channelBase, normQMin, normQMax, cfCache);
    computeGenuineCfGraph(fin, sl, channelBase, normQMin, normQMax, cfCache);
  }
}

static void populatePurityGenuineCaches(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache) {
  if (!isLegacyCfPagesEnabled()) return;
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    populatePurityGenuineCachesForBase(fin, std::string(kChannelBases[ib]), cfCache);
  }
}

static void drawCfGraphOverlay(TGraphErrors* gA, TGraphErrors* gB, const char* labelA, const char* labelB) {
  if (!gA && !gB) return;
  TGraphErrors* gFirst = gA ? gA : gB;
  if (gA) {
    gA->SetMarkerColor(kBlack);
    gA->SetLineColor(kBlack);
    gA->SetMarkerStyle(20);
    gA->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    gA->Draw("AP");
  }
  if (gB) {
    gB->SetMarkerColor(kBlue + 1);
    gB->SetLineColor(kBlue + 1);
    gB->SetMarkerStyle(21);
    TString opt = gA ? "P SAME" : "AP";
    if (!gA) gB->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    gB->Draw(opt);
  }
  TH1* hFrame = gFirst->GetHistogram();
  if (hFrame) {
    hFrame->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
    hFrame->GetYaxis()->SetRangeUser(kCfYMin, kCfYMax);
  }
  if (gPad && (gA || gB)) {
    TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    if (gA && labelA) leg->AddEntry(gA, labelA, "p");
    if (gB && labelB) leg->AddEntry(gB, labelB, "p");
    leg->Draw();
  }
}

static void drawGenuineCfSlicePageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                          const std::string& channelBase, std::vector<TH1*>& centProjKeepAlive,
                                          std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->Clear();
  canvas->Divide(2, 1);
  const std::string chSig = channelSignal(channelBase);
  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);

  canvas->cd(1);
  TH1* hSE = getSliceProjectedSeMe(fin, chSig, kTRUE, slice.cent9Min, slice.cent9Max);
  TH1* hME = getSliceProjectedSeMe(fin, chSig, kFALSE, slice.cent9Min, slice.cent9Max);
  if (hSE) centProjKeepAlive.push_back(hSE);
  if (hME) centProjKeepAlive.push_back(hME);
  drawKstarSeMeOverlay(hSE, hME, normQMin, normQMax, centProjKeepAlive);

  canvas->cd(2);
  TGraphErrors* gMeas = getOrComputeSliceChannelCf(fin, slice.id, slice.cent9Min, slice.cent9Max, chSig, normQMin,
                                                   normQMax, cfCache);
  TGraphErrors* gGen = computeGenuineCfGraph(fin, slice, channelBase, normQMin, normQMax, cfCache);
  drawCfGraphOverlay(gMeas, gGen, "C_{meas}", "C_{genuine}");

  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98,
                   Form("%s genuine CF %s (cent9 %d-%d)", channelBase.c_str(), slice.id.c_str(), slice.cent9Min,
                        slice.cent9Max));
  }
}

// ---------------------------------------------------------------------------
// Kubo-rule combinatorial background for h-phi (h-K correlations extension).
//
// Same three triplet histograms feed both constructions (apples-to-apples):
//   Old (pseudo-phi, average): C_bkg = norm(N_SEKp + N_SEKm)/N_ref -> 1 + (a+b)/2
//   New (Kubo, sum):           C_bkg = R+ + R- - 1                 -> 1 + (a+b)
// with R+/- = norm(N_SEKp,m)/N_ref, a = C_hK+ - 1, b = C_hK- - 1. See plan section 1.
// ---------------------------------------------------------------------------

static std::string tripSeKpHistKey(const std::string& base) { return std::string("hKstarTripSEKp_") + base; }
static std::string tripSeKmHistKey(const std::string& base) { return std::string("hKstarTripSEKm_") + base; }
static std::string tripMixHistKey(const std::string& base) { return std::string("hKstarTripMix_") + base; }

// Build old-average and Kubo-sum background CF graphs from the three triplet histograms.
static void computeKuboBkgGraphs(TFile* fin, const std::string& base, Double_t normQMin, Double_t normQMax,
                                 TGraphErrors** gOldOut, TGraphErrors** gKuboOut) {
  *gOldOut = 0;
  *gKuboOut = 0;
  TH1* hKp = (TH1*)fin->Get(tripSeKpHistKey(base).c_str());
  TH1* hKm = (TH1*)fin->Get(tripSeKmHistKey(base).c_str());
  TH1* hRef = (TH1*)fin->Get(tripMixHistKey(base).c_str());
  if (!hKp || !hKm || !hRef) return;

  const Int_t rb = getCfRebinFactor();
  TH1* hKpR = rebinHistCopy(hKp, rb, "_kubo_kp");
  TH1* hKmR = rebinHistCopy(hKm, rb, "_kubo_km");
  TH1* hRefR = rebinHistCopy(hRef, rb, "_kubo_ref");
  if (!hKpR) { hKpR = (TH1*)hKp->Clone("_kubo_kp"); hKpR->SetDirectory(0); }
  if (!hKmR) { hKmR = (TH1*)hKm->Clone("_kubo_km"); hKmR->SetDirectory(0); }
  if (!hRefR) { hRefR = (TH1*)hRef->Clone("_kubo_ref"); hRefR->SetDirectory(0); }

  // Old (average): the two SE-K terms fill one histogram, normalized so CF -> 1 at high k*.
  TH1* hSum = (TH1*)hKpR->Clone("_kubo_sum");
  hSum->SetDirectory(0);
  hSum->Add(hKmR);
  *gOldOut = computeCfGraphFromSeMe(hSum, hRefR, normQMin, normQMax,
                                    Form("C_{bkg}^{old} %s;k^{*} [GeV/c];C_{bkg}(k^{*})", base.c_str()));

  // Kubo (sum): R+ + R- - 1, each R normalized to unity in the norm band, computed bin-by-bin (aligned bins).
  const Int_t binLo = hRefR->FindBin(normQMin + 1e-9);
  const Int_t binHi = hRefR->FindBin(normQMax - 1e-9);
  const Double_t refNorm = hRefR->Integral(binLo, binHi);
  const Double_t kpNorm = hKpR->Integral(binLo, binHi);
  const Double_t kmNorm = hKmR->Integral(binLo, binHi);
  if (refNorm > 0.0 && kpNorm > 0.0 && kmNorm > 0.0) {
    const Double_t scaleKp = refNorm / kpNorm;
    const Double_t scaleKm = refNorm / kmNorm;
    std::vector<Double_t> x, y, ey;
    for (Int_t ib = 1; ib <= hRefR->GetNbinsX(); ++ib) {
      const Double_t kp = hKpR->GetBinContent(ib);
      const Double_t km = hKmR->GetBinContent(ib);
      const Double_t rf = hRefR->GetBinContent(ib);
      if (kp <= 0.0 || km <= 0.0 || rf <= 0.0) continue;
      const Double_t rp = scaleKp * kp / rf;
      const Double_t rm = scaleKm * km / rf;
      const Double_t erp = rp * TMath::Sqrt(1.0 / kp + 1.0 / rf);
      const Double_t erm = rm * TMath::Sqrt(1.0 / km + 1.0 / rf);
      x.push_back(hRefR->GetBinCenter(ib));
      y.push_back(rp + rm - 1.0);
      ey.push_back(TMath::Sqrt(erp * erp + erm * erm));
    }
    if (!x.empty()) {
      TGraphErrors* gKubo = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
      gKubo->SetTitle(Form("C_{bkg}^{Kubo} %s;k^{*} [GeV/c];C_{bkg}(k^{*})", base.c_str()));
      *gKuboOut = gKubo;
    }
  }
  delete hKpR;
  delete hKmR;
  delete hRefR;
  delete hSum;
}

// ALICE-style lambda decomposition with a supplied background CF (same algebra/Jacobian as
// computeGenuineCfGraph): C_gen = 1 + [ (C_meas-1) - lambda_bkg (C_bkg-1) ] / lambda_sig.
static TGraphErrors* computeGenuineFromParts(TGraphErrors* gMeas, TGraphErrors* gBkg, TGraphErrors* gLambda,
                                             const char* title) {
  if (!gMeas || !gBkg || !gLambda) return 0;
  std::vector<Double_t> x, y, ey;
  for (Int_t ip = 0; ip < gMeas->GetN(); ++ip) {
    const Double_t kstar = gMeas->GetX()[ip];
    const Double_t cMeas = gMeas->GetY()[ip];
    const Double_t eMeas = gMeas->GetEY()[ip];
    Double_t eBkg = 0.0;
    const Double_t cBkg = interpGraphY(gBkg, kstar, &eBkg);
    Double_t eLam = 0.0;
    const Double_t lambdaSig = interpGraphY(gLambda, kstar, &eLam);
    if (cBkg < 0.0 || lambdaSig <= 0.0) continue;
    const Double_t lambdaBkg = 1.0 - lambdaSig;
    const Double_t numer = (cMeas - 1.0) - lambdaBkg * (cBkg - 1.0);
    const Double_t cGen = 1.0 + numer / lambdaSig;
    const Double_t dGenDcMeas = 1.0 / lambdaSig;
    const Double_t dGenDcBkg = -lambdaBkg / lambdaSig;
    const Double_t dGenDLam = (cBkg - 1.0) / lambdaSig - numer / (lambdaSig * lambdaSig);
    const Double_t err = TMath::Sqrt(TMath::Power(dGenDcMeas * eMeas, 2) + TMath::Power(dGenDcBkg * eBkg, 2) +
                                     TMath::Power(dGenDLam * eLam, 2));
    x.push_back(kstar);
    y.push_back(cGen);
    ey.push_back(err);
  }
  if (x.empty()) return 0;
  TGraphErrors* g = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
  g->SetTitle(title ? title : "C_{genuine}(k^{*})");
  return g;
}

// Kubo pages for one base (p or d): K1 = old vs Kubo C_bkg; K2 = old vs Kubo genuine CF + ratio panel.
static void drawKuboPagesForBase(TCanvas* c1, TFile* fin, const std::string& base, const TString& pdfName,
                                 std::map<std::string, TGraphErrors*>& cfCache) {
  if (!c1) return;
  const std::string chSig = channelSignal(base);
  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);

  TGraphErrors* gOld = 0;
  TGraphErrors* gKubo = 0;
  computeKuboBkgGraphs(fin, base, normQMin, normQMax, &gOld, &gKubo);
  cfCache[std::string("kubo:CF_bkg_old_") + base] = gOld;
  cfCache[std::string("kubo:CF_bkg_kubo_") + base] = gKubo;

  // Page K1: old (average) vs Kubo (sum) background CF.
  c1->Clear();
  c1->Divide(1, 1);
  c1->cd(1);
  drawCfGraphOverlay(gOld, gKubo, "C_{bkg}^{old} (pseudo-#phi avg)", "C_{bkg}^{Kubo} (sum)");
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98, Form("Kubo background %s: old (avg) vs Kubo (sum)", base.c_str()));
  }
  c1->Print(pdfName);

  if (!isKuboGenuineEnabled()) return;

  // Page K2: old-bkg vs Kubo-bkg genuine CF (same C_meas, same lambda_sig), with Kubo/old ratio.
  Int_t cent9Min = 0;
  Int_t cent9Max = 0;
  getCfCent9Range(cent9Min, cent9Max);
  FemtoConfig::CfCentSlice slice;
  slice.id = std::string("kubo_integrated_") + base;
  slice.cent9Min = cent9Min;
  slice.cent9Max = cent9Max;
  TGraphErrors* gMeas =
      getOrComputeSliceChannelCf(fin, slice.id, cent9Min, cent9Max, chSig, normQMin, normQMax, cfCache);
  TGraphErrors* gLambda = computeLambdaSigGraph(fin, slice, base);
  cfCache[std::string("kubo:lambda_sig_") + base] = gLambda;

  TGraphErrors* gGenOld =
      computeGenuineFromParts(gMeas, gOld, gLambda, Form("C_{gen}^{old} %s", base.c_str()));
  TGraphErrors* gGenKubo =
      computeGenuineFromParts(gMeas, gKubo, gLambda, Form("C_{gen}^{Kubo} %s", base.c_str()));
  cfCache[std::string("kubo:CF_genuine_old_") + base] = gGenOld;
  cfCache[std::string("kubo:CF_genuine_kubo_") + base] = gGenKubo;

  c1->Clear();
  c1->Divide(2, 1);
  c1->cd(1);
  drawCfGraphOverlay(gGenOld, gGenKubo, "C_{gen} old-bkg", "C_{gen} Kubo-bkg");
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98, Form("%s genuine CF: old-bkg vs Kubo-bkg", base.c_str()));
  }

  c1->cd(2);
  TGraphErrors* gRatio = 0;
  if (gGenOld && gGenKubo) {
    std::vector<Double_t> rx, ry, rey;
    for (Int_t i = 0; i < gGenKubo->GetN(); ++i) {
      const Double_t k = gGenKubo->GetX()[i];
      Double_t eo = 0.0;
      const Double_t co = interpGraphY(gGenOld, k, &eo);
      if (co <= 0.0) continue;
      rx.push_back(k);
      ry.push_back(gGenKubo->GetY()[i] / co);
      rey.push_back(0.0);
    }
    if (!rx.empty()) {
      gRatio = new TGraphErrors((Int_t)rx.size(), &rx[0], &ry[0], 0, &rey[0]);
      gRatio->SetTitle(Form("C_{gen}^{Kubo}/C_{gen}^{old} %s;k^{*} [GeV/c];ratio", base.c_str()));
      gRatio->SetMarkerStyle(20);
      gRatio->SetMarkerColor(kBlue + 1);
      gRatio->SetLineColor(kBlue + 1);
      gRatio->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gRatio->Draw("AP");
      TH1* hFrame = gRatio->GetHistogram();
      if (hFrame) {
        hFrame->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
        hFrame->GetYaxis()->SetRangeUser(0.8, 1.3);
      }
      TLine* one = new TLine(kCfKstarXMin, 1.0, kCfKstarXMax, 1.0);
      one->SetLineStyle(2);
      one->SetLineColor(kGray + 2);
      one->Draw("SAME");
    }
  }
  cfCache[std::string("kubo:CF_genuine_ratio_") + base] = gRatio;
  c1->Print(pdfName);
}

// ---------------------------------------------------------------------------
// kstarMassFitCF (primary) and deprecated direct mass-fit / Topic 3 / Method 5.
// ---------------------------------------------------------------------------

static Bool_t isKstarMassFitCfWriteSidecar() {
  if (!gConfigLoaded) return kTRUE;
  return ConfigManager::GetInstance().GetFemtoConfig().kstarMassFitCfWriteSidecar;
}

static Double_t getKstarMassFitCfKstarBinTarget() {
  if (!gConfigLoaded) return 0.050;
  return ConfigManager::GetInstance().GetFemtoConfig().kstarMassFitCfKstarBinWidth;
}

static Int_t getKstarMassFitCfLowKstarMergeBins() {
  if (!gConfigLoaded) return 1;
  const Int_t n = ConfigManager::GetInstance().GetFemtoConfig().kstarMassFitCfLowKstarMergeBins;
  return (n >= 1) ? n : 1;
}

static Bool_t getKstarMassFitCfAlphaSingleWindow(Double_t& mMin, Double_t& mMax) {
  mMin = 0.0;
  mMax = 0.0;
  if (!gConfigLoaded) return kFALSE;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  if (fc.kstarMassFitCfAlphaMassMax <= fc.kstarMassFitCfAlphaMassMin) return kFALSE;
  mMin = fc.kstarMassFitCfAlphaMassMin;
  mMax = fc.kstarMassFitCfAlphaMassMax;
  return kTRUE;
}

static std::string kmfCacheSuffix(Double_t kstarBinTarget) {
  if (kstarBinTarget <= 0.0) return "_native";
  return Form("_dk%.0f", kstarBinTarget * 1000.0);
}

static std::string phiMkkVsKstarWideSeKey(const std::string& channelBase) {
  return std::string("hPhiMKK_vs_KstarSE_") + channelBase + "_wide";
}

static std::string phiMkkVsKstarWideMeKey(const std::string& channelBase) {
  return std::string("hPhiMKK_vs_KstarME_") + channelBase + "_wide";
}

struct KstarMassFitCfFitResult {
  Bool_t ok;
  Double_t nSig;
  Double_t errNSig;
  Double_t nBkg;
  Double_t purity;
  Double_t errPurity;
  Double_t amp;
  Double_t mean;
  Double_t sigma;
  Double_t polA;
  Double_t polB;
  Double_t polC;
  Bool_t usedPol2;
  KstarMassFitCfFitResult()
      : ok(kFALSE),
        nSig(0.0),
        errNSig(0.0),
        nBkg(0.0),
        purity(0.0),
        errPurity(0.0),
        amp(0.0),
        mean(0.0),
        sigma(0.0),
        polA(0.0),
        polB(0.0),
        polC(0.0),
        usedPol2(kFALSE) {}
};

static Bool_t fitPurityOneModel(TH1* hMass, Double_t fitMin, Double_t fitMax, Double_t sigMin, Double_t sigMax,
                                Double_t sigmaMin, Double_t sigmaMax, Bool_t usePol2, KstarMassFitCfFitResult& out) {
  TString fname = Form("fm3_%lx_%d", (unsigned long)hMass, usePol2 ? 1 : 0);
  TF1* f = 0;
  if (usePol2) {
    f = new TF1(fname + "_gp2", "gaus(0)+pol2(3)", fitMin, fitMax);
  } else {
    f = new TF1(fname + "_gc", "gaus(0)+[3]", fitMin, fitMax);
  }
  Double_t peak = hMass->GetMaximum();
  Int_t maxBin = hMass->GetMaximumBin();
  f->SetParameter(0, peak);
  f->SetParameter(1, hMass->GetXaxis()->GetBinCenter(maxBin));
  f->SetParameter(2, 0.006);
  f->SetParLimits(2, sigmaMin, sigmaMax);
  if (usePol2) {
    f->SetParameter(3, 0.05 * peak);
    f->SetParameter(4, 0.0);
    f->SetParameter(5, 0.0);
  } else {
    f->SetParameter(3, 0.1 * peak);
  }
  Int_t fitStat = hMass->Fit(f, "RQ0");
  if (fitStat != 0) {
    delete f;
    return kFALSE;
  }
  TF1* fGaus = new TF1(fname + "_gonly", "gaus", fitMin, fitMax);
  fGaus->SetParameters(f->GetParameter(0), f->GetParameter(1), f->GetParameter(2));
  Double_t nSig = fGaus->Integral(sigMin, sigMax);
  Double_t nBkg = 0.0;
  if (usePol2) {
    TF1* fBkg = new TF1(fname + "_bkg", "pol2", fitMin, fitMax);
    fBkg->SetParameters(f->GetParameter(3), f->GetParameter(4), f->GetParameter(5));
    nBkg = fBkg->Integral(sigMin, sigMax);
    out.polA = f->GetParameter(3);
    out.polB = f->GetParameter(4);
    out.polC = f->GetParameter(5);
    delete fBkg;
  } else {
    nBkg = f->GetParameter(3) * (sigMax - sigMin);
    out.polA = f->GetParameter(3);
    out.polB = 0.0;
    out.polC = 0.0;
  }
  delete fGaus;
  if (nSig <= 0.0 || (nSig + nBkg) <= 0.0) {
    delete f;
    return kFALSE;
  }
  out.ok = kTRUE;
  out.nSig = nSig;
  out.nBkg = nBkg;
  out.purity = nSig / (nSig + nBkg);
  out.amp = f->GetParameter(0);
  out.mean = f->GetParameter(1);
  out.sigma = f->GetParameter(2);
  out.usedPol2 = usePol2;
  const Double_t eA = f->GetParError(0);
  const Double_t eS = f->GetParError(2);
  out.errNSig = nSig * TMath::Sqrt(TMath::Power(eA / (f->GetParameter(0) + 1e-12), 2) +
                                   TMath::Power(eS / (f->GetParameter(2) + 1e-12), 2));
  out.errPurity = out.purity * TMath::Sqrt(TMath::Power(eA / (f->GetParameter(0) + 1e-12), 2) +
                                           TMath::Power(eS / (f->GetParameter(2) + 1e-12), 2));
  if (out.errPurity > 0.5) out.errPurity = 0.5;
  delete f;
  return kTRUE;
}

static Bool_t fitPurityGausPol2OrConst(TH1* hMass, Double_t fitMin, Double_t fitMax, Double_t sigMin, Double_t sigMax,
                                       Double_t sigmaMin, Double_t sigmaMax, Bool_t preferPol2, KstarMassFitCfFitResult& out) {
  out = KstarMassFitCfFitResult();
  if (!hMass) return kFALSE;
  Int_t binFitLo = hMass->GetXaxis()->FindBin(fitMin + 1e-9);
  Int_t binFitHi = hMass->GetXaxis()->FindBin(fitMax - 1e-9);
  if (hMass->Integral(binFitLo, binFitHi) <= 0.0) return kFALSE;

  if (preferPol2 && fitPurityOneModel(hMass, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, kTRUE, out))
    return kTRUE;
  if (fitPurityOneModel(hMass, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, kFALSE, out)) return kTRUE;
  if (!preferPol2 && fitPurityOneModel(hMass, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, kTRUE, out))
    return kTRUE;
  return kFALSE;
}

// Pure gaussian (no polynomial / const background). Used for S = F-αB where residual BG ~ 0.
static Bool_t fitPurityGausOnly(TH1* hMass, Double_t fitMin, Double_t fitMax, Double_t sigMin, Double_t sigMax,
                                Double_t sigmaMin, Double_t sigmaMax, KstarMassFitCfFitResult& out) {
  out = KstarMassFitCfFitResult();
  if (!hMass) return kFALSE;
  Int_t binFitLo = hMass->GetXaxis()->FindBin(fitMin + 1e-9);
  Int_t binFitHi = hMass->GetXaxis()->FindBin(fitMax - 1e-9);
  if (hMass->Integral(binFitLo, binFitHi) <= 0.0) return kFALSE;

  TString fname = Form("fm3g_%lx", (unsigned long)hMass);
  TF1* f = new TF1(fname + "_g", "gaus", fitMin, fitMax);
  Double_t peak = hMass->GetMaximum();
  Int_t maxBin = hMass->GetMaximumBin();
  f->SetParameter(0, peak > 0.0 ? peak : 1.0);
  f->SetParameter(1, hMass->GetXaxis()->GetBinCenter(maxBin));
  f->SetParameter(2, 0.006);
  f->SetParLimits(2, sigmaMin, sigmaMax);
  TFitResultPtr rfit = hMass->Fit(f, "RQS0");
  const Int_t fitStat = (Int_t)rfit;
  if (fitStat != 0 || rfit.Get() == 0) {
    delete f;
    return kFALSE;
  }
  Double_t nSig = f->Integral(sigMin, sigMax);
  if (nSig <= 0.0) {
    delete f;
    return kFALSE;
  }
  out.ok = kTRUE;
  out.nSig = nSig;
  out.nBkg = 0.0;
  out.purity = 1.0;
  out.amp = f->GetParameter(0);
  out.mean = f->GetParameter(1);
  out.sigma = f->GetParameter(2);
  out.polA = 0.0;
  out.polB = 0.0;
  out.polC = 0.0;
  out.usedPol2 = kFALSE;
  Bool_t usedCov = kFALSE;
  if (rfit->CovMatrixStatus() > 0) {
    TMatrixDSym cov = rfit->GetCovarianceMatrix();
    Double_t pars[3] = {f->GetParameter(0), f->GetParameter(1), f->GetParameter(2)};
    const Double_t errCov = f->IntegralError(sigMin, sigMax, pars, cov.GetMatrixArray());
    if (TMath::Finite(errCov) && errCov >= 0.0) {
      out.errNSig = errCov;
      usedCov = kTRUE;
    }
  }
  if (!usedCov) {
    const Double_t eA = f->GetParError(0);
    const Double_t eS = f->GetParError(2);
    out.errNSig = nSig * TMath::Sqrt(TMath::Power(eA / (f->GetParameter(0) + 1e-12), 2) +
                                     TMath::Power(eS / (f->GetParameter(2) + 1e-12), 2));
  }
  out.errPurity = 0.0;
  delete f;
  return kTRUE;
}

static void getKstarMassFitCfFitConfig(Double_t& fitMin, Double_t& fitMax, Double_t& sigmaMin, Double_t& sigmaMax,
                                Double_t& purityMinK, Double_t& purityMaxK, Int_t& minEntries, Double_t& clampMin,
                                Double_t& clampMax, Bool_t& preferPol2) {
  fitMin = 0.99;
  fitMax = 1.06;
  sigmaMin = 0.002;
  sigmaMax = 0.020;
  purityMinK = 0.0;
  purityMaxK = 0.65;
  minEntries = 20;
  clampMin = 0.05;
  clampMax = 1.0;
  preferPol2 = kTRUE;
  if (!gConfigLoaded) return;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  fitMin = fc.kstarMassFitCfFitMassMin;
  fitMax = fc.kstarMassFitCfFitMassMax;
  sigmaMin = fc.purityFitGaussSigmaMin;
  sigmaMax = fc.purityFitGaussSigmaMax;
  purityMinK = fc.purityMinKstar;
  purityMaxK = fc.purityMaxKstar;
  minEntries = fc.purityMinEntriesPerBin;
  clampMin = fc.purityClampMin;
  clampMax = fc.purityClampMax;
  preferPol2 = (fc.purityDirectFitModel == "gaus_pol2");
}

struct DirectMassFitBinRecord {
  Int_t iy;
  Double_t kstar;
  Double_t seIntegral;
  Bool_t seOk;
  Bool_t meOk;
  KstarMassFitCfFitResult seFit;
  KstarMassFitCfFitResult meFit;
};

// k* Rebin factor to reach a target bin width from the histogram's native width.
// targetWidth <= 0 means no rebin (factor 1).
static Int_t kmfRebinFactorForWidth(Double_t binWidth, Double_t targetWidth) {
  if (targetWidth <= 0.0 || binWidth <= 0.0) return 1;
  Int_t f = (Int_t)TMath::Nint(targetWidth / binWidth);
  if (f < 1) f = 1;
  return f;
}

static void computeDirectMassFitCfGraphs(TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                       const std::string& channelBase, std::map<std::string, TGraphErrors*>& cfCache,
                                       std::map<std::string, Double_t>& metaCache,
                                       std::vector<DirectMassFitBinRecord>* binRecordsOut, Double_t kstarBinTarget = -1.0) {
  if (kstarBinTarget < 0.0) kstarBinTarget = getKstarMassFitCfKstarBinTarget();
  const std::string suf = kmfCacheSuffix(kstarBinTarget);
  const std::string pTrueKey = cfSliceCacheKey(slice.id, std::string("method3_P_true_") + channelBase + suf);
  const std::string pMixKey = cfSliceCacheKey(slice.id, std::string("method3_P_mix_") + channelBase + suf);
  const std::string nSeKey = cfSliceCacheKey(slice.id, std::string("method3_Nsig_SE_") + channelBase + suf);
  const std::string nMeKey = cfSliceCacheKey(slice.id, std::string("method3_Nsig_ME_") + channelBase + suf);
  const std::string nSeBkgKey = cfSliceCacheKey(slice.id, std::string("method3_Nbkg_SE_") + channelBase + suf);
  const std::string nMeBkgKey = cfSliceCacheKey(slice.id, std::string("method3_Nbkg_ME_") + channelBase + suf);
  const std::string cfKey = cfSliceCacheKey(slice.id, std::string("CF_method3_") + channelBase + suf);

  if (cfCache.find(cfKey) != cfCache.end()) {
    if (binRecordsOut) binRecordsOut->clear();
    return;
  }

  TH3* h3SE = (TH3*)fin->Get(phiMkkVsKstarWideSeKey(channelBase).c_str());
  TH3* h3ME = (TH3*)fin->Get(phiMkkVsKstarWideMeKey(channelBase).c_str());
  if (!h3SE || !h3ME) {
    std::cout << "[checkHistAnaFemtoPhi] direct-mass-fit " << channelBase << " " << slice.id
              << ": missing wide TH3 (needs Maker re-run with hPhiMKK_vs_Kstar*_wide)\n";
    cfCache[cfKey] = 0;
    cfCache[pTrueKey] = 0;
    cfCache[pMixKey] = 0;
    cfCache[nSeKey] = 0;
    cfCache[nMeKey] = 0;
    return;
  }

  TH2* h2SE = projectMkkVsKstarForSlice(h3SE, slice.cent9Min, slice.cent9Max, "_m3_mkkse");
  TH2* h2ME = projectMkkVsKstarForSlice(h3ME, slice.cent9Min, slice.cent9Max, "_m3_mkkme");
  if (!h2SE || !h2ME) {
    delete h2SE;
    delete h2ME;
    cfCache[cfKey] = 0;
    return;
  }

  const Int_t kRebin = kmfRebinFactorForWidth(h2SE->GetYaxis()->GetBinWidth(1), kstarBinTarget);
  if (kRebin > 1) {
    h2SE = (TH2*)h2SE->RebinY(kRebin);
    h2ME = (TH2*)h2ME->RebinY(kRebin);
  }

  Double_t fitMin = 0.99, fitMax = 1.06, sigmaMin = 0.002, sigmaMax = 0.020;
  Double_t purityMinK = 0.0, purityMaxK = 0.65, clampMin = 0.05, clampMax = 1.0;
  Int_t minEntries = 20;
  Bool_t preferPol2 = kTRUE;
  getKstarMassFitCfFitConfig(fitMin, fitMax, sigmaMin, sigmaMax, purityMinK, purityMaxK, minEntries, clampMin, clampMax,
                      preferPol2);

  const std::string chSig = channelSignal(channelBase);
  Double_t sigMin = 1.012, sigMax = 1.026;
  getChannelSignalMassWindow(chSig, sigMin, sigMax);

  std::vector<Double_t> kx, pyT, eyT, pyM, eyM, nSe, eSe, nMe, eMe, cfx, cfy, cfe;
  std::vector<Double_t> nSeBkg, nMeBkg;
  std::vector<DirectMassFitBinRecord> records;
  Int_t nFail = 0;

  for (Int_t iy = 1; iy <= h2SE->GetNbinsY(); ++iy) {
    const Double_t kstar = h2SE->GetYaxis()->GetBinCenter(iy);
    if (kstar < purityMinK || kstar > purityMaxK) continue;

    TH1* hSE_ib = h2SE->ProjectionX(Form("_m3_se_rb%d_%d", kRebin, iy), iy, iy);
    TH1* hME_ib = h2ME->ProjectionX(Form("_m3_me_rb%d_%d", kRebin, iy), iy, iy);
    if (!hSE_ib || !hME_ib) {
      delete hSE_ib;
      delete hME_ib;
      continue;
    }
    hSE_ib->SetDirectory(0);
    hME_ib->SetDirectory(0);

    DirectMassFitBinRecord rec;
    rec.iy = iy;
    rec.kstar = kstar;
    rec.seIntegral = hSE_ib->Integral();
    rec.seOk = kFALSE;
    rec.meOk = kFALSE;

    if (hSE_ib->GetEntries() < minEntries) {
      ++nFail;
      delete hSE_ib;
      delete hME_ib;
      records.push_back(rec);
      continue;
    }

    KstarMassFitCfFitResult seFit, meFit;
    Bool_t okSE = fitPurityGausPol2OrConst(hSE_ib, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, preferPol2,
                                           seFit);
    Bool_t okME = fitPurityGausPol2OrConst(hME_ib, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, preferPol2,
                                           meFit);
    delete hSE_ib;
    delete hME_ib;

    rec.seOk = okSE;
    rec.meOk = okME;
    rec.seFit = seFit;
    rec.meFit = meFit;
    records.push_back(rec);

    if (!okSE || !okME || seFit.nSig <= 0.0 || meFit.nSig <= 0.0) {
      ++nFail;
      continue;
    }

    Double_t pT = seFit.purity;
    Double_t pM = meFit.purity;
    if (pT < clampMin) pT = clampMin;
    if (pT > clampMax) pT = clampMax;
    if (pM < clampMin) pM = clampMin;
    if (pM > clampMax) pM = clampMax;

    kx.push_back(kstar);
    pyT.push_back(pT);
    eyT.push_back(seFit.errPurity);
    pyM.push_back(pM);
    eyM.push_back(meFit.errPurity);
    nSe.push_back(seFit.nSig);
    eSe.push_back(seFit.errNSig);
    nMe.push_back(meFit.nSig);
    eMe.push_back(meFit.errNSig);
    nSeBkg.push_back(seFit.nBkg);
    nMeBkg.push_back(meFit.nBkg);

    const Double_t cf = seFit.nSig / meFit.nSig;
    const Double_t ecf =
        cf * TMath::Sqrt(TMath::Power(seFit.errNSig / (seFit.nSig + 1e-12), 2) +
                         TMath::Power(meFit.errNSig / (meFit.nSig + 1e-12), 2));
    cfx.push_back(kstar);
    cfy.push_back(cf);
    cfe.push_back(ecf);
  }

  delete h2SE;
  delete h2ME;

  std::cout << "[checkHistAnaFemtoPhi] direct-mass-fit " << channelBase << " " << slice.id << ": okBins=" << cfx.size()
            << " fail/skip=" << nFail << " kRebin=" << kRebin
            << " dkTarget=" << kstarBinTarget << "\n";

  if (kx.empty()) {
    cfCache[cfKey] = 0;
    cfCache[pTrueKey] = 0;
    cfCache[pMixKey] = 0;
    cfCache[nSeKey] = 0;
    cfCache[nMeKey] = 0;
    cfCache[cfSliceCacheKey(slice.id, std::string("CF_method3_norm_") + channelBase + suf)] = 0;
  } else {
    TGraphErrors* gPT = new TGraphErrors((Int_t)kx.size(), &kx[0], &pyT[0], 0, &eyT[0]);
    gPT->SetTitle(Form("P_{true}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gPT->SetMarkerStyle(20);
    gPT->SetMarkerColor(kBlack);
    cfCache[pTrueKey] = gPT;

    TGraphErrors* gPM = new TGraphErrors((Int_t)kx.size(), &kx[0], &pyM[0], 0, &eyM[0]);
    gPM->SetTitle(Form("P_{mix}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gPM->SetMarkerStyle(21);
    gPM->SetMarkerColor(kRed);
    cfCache[pMixKey] = gPM;

    TGraphErrors* gNSE = new TGraphErrors((Int_t)kx.size(), &kx[0], &nSe[0], 0, &eSe[0]);
    gNSE->SetTitle(Form("N_{sig}^{SE}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gNSE->SetMarkerStyle(20);
    cfCache[nSeKey] = gNSE;

    TGraphErrors* gNME = new TGraphErrors((Int_t)kx.size(), &kx[0], &nMe[0], 0, &eMe[0]);
    gNME->SetTitle(Form("N_{sig}^{ME}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gNME->SetMarkerStyle(21);
    gNME->SetMarkerColor(kRed);
    cfCache[nMeKey] = gNME;

    TGraphErrors* gNSEb = new TGraphErrors((Int_t)kx.size(), &kx[0], &nSeBkg[0], 0, 0);
    gNSEb->SetTitle(Form("N_{bkg}^{SE}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gNSEb->SetMarkerStyle(24);
    gNSEb->SetMarkerColor(kBlack);
    cfCache[nSeBkgKey] = gNSEb;

    TGraphErrors* gNMEb = new TGraphErrors((Int_t)kx.size(), &kx[0], &nMeBkg[0], 0, 0);
    gNMEb->SetTitle(Form("N_{bkg}^{ME}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gNMEb->SetMarkerStyle(25);
    gNMEb->SetMarkerColor(kRed);
    cfCache[nMeBkgKey] = gNMEb;

    TGraphErrors* gCF = new TGraphErrors((Int_t)cfx.size(), &cfx[0], &cfy[0], 0, &cfe[0]);
    gCF->SetTitle(Form("CF_{direct}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
    gCF->SetMarkerStyle(20);
    gCF->SetMarkerColor(kBlue + 1);
    cfCache[cfKey] = gCF;

    // High-k* normalized CF (same window as other CFs); raw CF_direct kept above.
    const std::string cfNormKey =
        cfSliceCacheKey(slice.id, std::string("CF_method3_norm_") + channelBase + suf);
    const std::string chSig = channelSignal(channelBase);
    const Double_t nQMin = channelNormQMin(chSig);
    const Double_t nQMax = channelNormQMax(chSig);
    Double_t sumC = 0.0;
    Int_t nNorm = 0;
    for (size_t i = 0; i < cfx.size(); ++i) {
      if (cfx[i] < nQMin || cfx[i] > nQMax) continue;
      sumC += cfy[i];
      ++nNorm;
    }
    if (nNorm > 0 && sumC > 0.0) {
      const Double_t scale = (Double_t)nNorm / sumC;
      std::vector<Double_t> ny, ne;
      ny.reserve(cfy.size());
      ne.reserve(cfe.size());
      for (size_t i = 0; i < cfy.size(); ++i) {
        ny.push_back(cfy[i] * scale);
        ne.push_back(cfe[i] * scale);
      }
      TGraphErrors* gCFn = new TGraphErrors((Int_t)cfx.size(), &cfx[0], &ny[0], 0, &ne[0]);
      gCFn->SetTitle(Form("CF_{direct}^{norm}(k^{*}) dmf %s %s", channelBase.c_str(), slice.id.c_str()));
      gCFn->SetMarkerStyle(21);
      gCFn->SetMarkerColor(kAzure + 2);
      cfCache[cfNormKey] = gCFn;
      metaCache[cfSliceCacheKey(slice.id, std::string("method3_normScale_") + channelBase + suf)] = scale;
    } else {
      cfCache[cfNormKey] = 0;
    }
  }

  metaCache[cfSliceCacheKey(slice.id, std::string("method3_nFail_") + channelBase + suf)] = (Double_t)nFail;
  metaCache[cfSliceCacheKey(slice.id, std::string("method3_nOk_") + channelBase + suf)] = (Double_t)cfx.size();
  Int_t iyHigh = -1, iyLow = -1;
  Double_t maxInt = -1.0, minInt = 1e300;
  Double_t kHigh = -1.0, kLow = -1.0;
  for (size_t ir = 0; ir < records.size(); ++ir) {
    if (!records[ir].seOk || !records[ir].meOk) continue;
    const Double_t integ = records[ir].seIntegral;
    if (integ > maxInt || (TMath::Abs(integ - maxInt) < 1e-9 && records[ir].kstar > kHigh)) {
      maxInt = integ;
      iyHigh = records[ir].iy;
      kHigh = records[ir].kstar;
    }
    if (integ < minInt || (TMath::Abs(integ - minInt) < 1e-9 && records[ir].kstar < kLow)) {
      minInt = integ;
      iyLow = records[ir].iy;
      kLow = records[ir].kstar;
    }
  }
  metaCache[cfSliceCacheKey(slice.id, std::string("method3_iy_high_") + channelBase + suf)] = (Double_t)iyHigh;
  metaCache[cfSliceCacheKey(slice.id, std::string("method3_iy_low_") + channelBase + suf)] = (Double_t)iyLow;
  metaCache[cfSliceCacheKey(slice.id, std::string("method3_k_high_") + channelBase + suf)] = kHigh;
  metaCache[cfSliceCacheKey(slice.id, std::string("method3_k_low_") + channelBase + suf)] = kLow;

  if (binRecordsOut) *binRecordsOut = records;
}

// Legacy direct mass-fit CF vs Kubo-bkg genuine CF (closure).
static void drawDirectMassFitKuboClosureForBase(TCanvas* c1, TFile* fin, const std::string& base, const TString& pdfName,
                                          std::map<std::string, TGraphErrors*>& cfCache,
                                          std::map<std::string, Double_t>& metaCache) {
  if (!c1 || !isLegacyCfPagesEnabled() || !isKuboTripletEnabled()) return;

  const FemtoConfig::CfCentSlice* slicePtr = 0;
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  for (size_t is = 0; is < slices.size(); ++is) {
    if (slices[is].id == "pct_0_10" || slices[is].id == "pct_0_20") {
      slicePtr = &slices[is];
      break;
    }
  }
  if (!slicePtr && !slices.empty()) slicePtr = &slices[0];
  if (!slicePtr) return;

  const Double_t dk = getKstarMassFitCfKstarBinTarget();
  const std::string suf = kmfCacheSuffix(dk);
  computeDirectMassFitCfGraphs(fin, *slicePtr, base, cfCache, metaCache, 0, dk);
  const std::string cfNormKey =
      cfSliceCacheKey(slicePtr->id, std::string("CF_method3_norm_") + base + suf);
  const std::string pTrueKey =
      cfSliceCacheKey(slicePtr->id, std::string("method3_P_true_") + base + suf);
  TGraphErrors* gM3 = cfCache.count(cfNormKey) ? cfCache[cfNormKey] : 0;
  TGraphErrors* gP = cfCache.count(pTrueKey) ? cfCache[pTrueKey] : 0;

  const std::string chSig = channelSignal(base);
  const Double_t nQMin = channelNormQMin(chSig);
  const Double_t nQMax = channelNormQMax(chSig);
  TGraphErrors* gOld = 0;
  TGraphErrors* gKubo = 0;
  computeKuboBkgGraphs(fin, base, nQMin, nQMax, &gOld, &gKubo);
  if (!gM3 || !gKubo || !gP) {
    c1->Clear();
    c1->cd(0);
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.035);
    lat->DrawLatex(0.12, 0.55, Form("direct-mass-fit/Kubo closure %s: missing CF_norm, P_true, or Kubo bkg", base.c_str()));
    c1->Print(pdfName);
    return;
  }

  // C_meas from signal-channel CF (same selection); prefer direct-mass-fit norm as primary meas for closure.
  TGraphErrors* gGenKubo =
      computeGenuineFromParts(gM3, gKubo, gP, Form("C_{gen}^{Kubo} from dmf %s", base.c_str()));
  cfCache[std::string("closure:CF_kubo_from_m3_") + base] = gGenKubo;

  c1->Clear();
  c1->Divide(2, 1);
  c1->cd(1);
  drawCfGraphOverlay(gM3, gGenKubo, "CF_{direct}^{norm} (dmf)", "C_{gen} (Kubo bkg)");
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98, Form("Closure %s [%s]: direct-mass-fit vs Kubo-weighted genuine", base.c_str(),
                                    slicePtr->id.c_str()));
  }
  c1->cd(2);
  if (gM3 && gGenKubo) {
    std::vector<Double_t> rx, ry;
    for (Int_t i = 0; i < gGenKubo->GetN(); ++i) {
      const Double_t k = gGenKubo->GetX()[i];
      Double_t eo = 0.0;
      const Double_t co = interpGraphY(gM3, k, &eo);
      if (co <= 0.0) continue;
      rx.push_back(k);
      ry.push_back(gGenKubo->GetY()[i] / co);
    }
    if (!rx.empty()) {
      TGraph* gRatio = new TGraph((Int_t)rx.size(), &rx[0], &ry[0]);
      gRatio->SetTitle(Form("C_{gen}^{Kubo}/CF_{direct}^{norm} %s;k^{*} [GeV/c];ratio", base.c_str()));
      gRatio->SetMarkerStyle(20);
      gRatio->Draw("AP");
      TH1* hF = gRatio->GetHistogram();
      if (hF) {
        hF->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
        hF->GetYaxis()->SetRangeUser(0.7, 1.3);
      }
      TLine* one = new TLine(kCfKstarXMin, 1.0, kCfKstarXMax, 1.0);
      one->SetLineStyle(2);
      one->Draw("SAME");
    }
  }
  c1->Print(pdfName);

  // Optional: show full-mass Kubo TH3 presence (schema QA).
  c1->Clear();
  c1->Divide(2, 2);
  const char* keys3[] = {"hKuboMKK_vs_KstarSEKp_", "hKuboMKK_vs_KstarSEKm_", "hKuboMKK_vs_KstarMix_",
                         "hKuboMKK_vs_KstarKK_"};
  const char* labs[] = {"SEKp full", "SEKm full", "Mix D full", "KK full"};
  for (Int_t ip = 0; ip < 4; ++ip) {
    c1->cd(ip + 1);
    TH3* h3 = (TH3*)fin->Get((std::string(keys3[ip]) + base).c_str());
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.04);
    if (!h3) {
      lat->DrawLatex(0.15, 0.5, Form("%s: MISSING (needs re-run)", labs[ip]));
    } else {
      lat->DrawLatex(0.12, 0.55, Form("%s", labs[ip]));
      lat->DrawLatex(0.12, 0.45, Form("entries=%.0f", h3->GetEntries()));
      lat->DrawLatex(0.12, 0.35, "TH3 M_{KK}#times k^{*}#times cent (full mass)");
    }
  }
  c1->cd(0);
  {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.97, Form("Kubo full-mass schema QA %s (kuboStoreFullMass)", base.c_str()));
  }
  c1->Print(pdfName);
}

static void populateDirectMassFitCachesForBase(TFile* fin, const std::string& channelBase,
                                         std::map<std::string, TGraphErrors*>& cfCache,
                                         std::map<std::string, Double_t>& metaCache) {
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  for (size_t is = 0; is < slices.size(); ++is) {
    computeDirectMassFitCfGraphs(fin, slices[is], channelBase, cfCache, metaCache, 0);
  }
}

static void populateDirectMassFitCaches(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache,
                                  std::map<std::string, Double_t>& metaCache) {
  if (!isLegacyCfPagesEnabled()) return;
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    populateDirectMassFitCachesForBase(fin, std::string(kChannelBases[ib]), cfCache, metaCache);
  }
}

static void drawDirectMassFitPanel(TFile* fin, const FemtoConfig::CfCentSlice& slice, const std::string& channelBase,
                                    Double_t kstarVal, Bool_t isSE, std::vector<TH1*>& keepAlive) {
  if (kstarVal < 0.0) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.04);
    lat->DrawLatex(0.15, 0.5, "insufficient k* bins for exemplar");
    return;
  }
  TH3* h3 = (TH3*)fin->Get(isSE ? phiMkkVsKstarWideSeKey(channelBase).c_str()
                                : phiMkkVsKstarWideMeKey(channelBase).c_str());
  if (!h3) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.035);
    lat->DrawLatex(0.12, 0.5, "missing wide TH3");
    return;
  }
  TH2* h2 = projectMkkVsKstarForSlice(h3, slice.cent9Min, slice.cent9Max, isSE ? "_m3ex_se" : "_m3ex_me");
  if (!h2) return;
  const Int_t kRebin = kmfRebinFactorForWidth(h2->GetYaxis()->GetBinWidth(1), getKstarMassFitCfKstarBinTarget());
  if (kRebin > 1) h2 = (TH2*)h2->RebinY(kRebin);
  const Int_t iy = h2->GetYaxis()->FindBin(kstarVal);
  if (iy < 1 || iy > h2->GetNbinsY()) {
    delete h2;
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.04);
    lat->DrawLatex(0.15, 0.5, "k* out of range for exemplar");
    return;
  }
  TH1* h1 = h2->ProjectionX(Form("_m3ex_x_%s_%d_%d", channelBase.c_str(), iy, isSE ? 1 : 0), iy, iy);
  delete h2;
  if (!h1) return;
  h1->SetDirectory(0);
  keepAlive.push_back(h1);

  Double_t fitMin = 0.99, fitMax = 1.06, sigmaMin = 0.002, sigmaMax = 0.020;
  Double_t purityMinK = 0.0, purityMaxK = 0.65, clampMin = 0.05, clampMax = 1.0;
  Int_t minEntries = 20;
  Bool_t preferPol2 = kTRUE;
  getKstarMassFitCfFitConfig(fitMin, fitMax, sigmaMin, sigmaMax, purityMinK, purityMaxK, minEntries, clampMin, clampMax,
                      preferPol2);
  const std::string chSig = channelSignal(channelBase);
  Double_t sigMin = 1.012, sigMax = 1.026;
  getChannelSignalMassWindow(chSig, sigMin, sigMax);

  KstarMassFitCfFitResult fr;
  Bool_t ok = fitPurityGausPol2OrConst(h1, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, preferPol2, fr);

  h1->SetLineColor(kBlack);
  h1->SetTitle(Form("%s %s k*=%.3f;M_{KK} [GeV/c^{2}];Counts", isSE ? "SE" : "ME", channelBase.c_str(), kstarVal));
  h1->GetXaxis()->SetRangeUser(fitMin - 0.01, fitMax + 0.01);
  h1->Draw("HIST");

  if (ok) {
    TF1* fDraw = 0;
    if (fr.usedPol2) {
      fDraw = new TF1(Form("_m3draw_%lx", (unsigned long)h1), "gaus(0)+pol2(3)", fitMin, fitMax);
      fDraw->SetParameters(fr.amp, fr.mean, fr.sigma, fr.polA, fr.polB, fr.polC);
    } else {
      fDraw = new TF1(Form("_m3draw_%lx", (unsigned long)h1), "gaus(0)+[3]", fitMin, fitMax);
      fDraw->SetParameters(fr.amp, fr.mean, fr.sigma, fr.polA);
    }
    fDraw->SetLineColor(kRed);
    fDraw->SetLineWidth(2);
    fDraw->Draw("SAME");
    TF1* fG = new TF1(Form("_m3g_%lx", (unsigned long)h1), "gaus", fitMin, fitMax);
    fG->SetParameters(fr.amp, fr.mean, fr.sigma);
    fG->SetLineColor(kBlue);
    fG->SetLineStyle(2);
    fG->Draw("SAME");
  }

  if (gPad) {
    Double_t ylo = gPad->GetUymin();
    Double_t yhi = gPad->GetUymax();
    TLine* l1 = new TLine(sigMin, ylo, sigMin, yhi);
    l1->SetLineColor(kGreen + 2);
    l1->SetLineStyle(2);
    l1->Draw("same");
    TLine* l2 = new TLine(sigMax, ylo, sigMax, yhi);
    l2->SetLineColor(kGreen + 2);
    l2->SetLineStyle(2);
    l2->Draw("same");
    TLine* f1 = new TLine(fitMin, ylo, fitMin, yhi);
    f1->SetLineColor(kGray + 1);
    f1->SetLineStyle(3);
    f1->Draw("same");
    TLine* f2 = new TLine(fitMax, ylo, fitMax, yhi);
    f2->SetLineColor(kGray + 1);
    f2->SetLineStyle(3);
    f2->Draw("same");

    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.032);
    if (ok) {
      lat->DrawLatex(0.14, 0.84, Form("k*=%.3f  N_{sig}=%.1f  P=%.3f %s", kstarVal, fr.nSig, fr.purity,
                                      fr.usedPol2 ? "gaus+pol2" : "gaus+const"));
    } else {
      lat->DrawLatex(0.14, 0.84, Form("k*=%.3f  fit FAILED", kstarVal));
    }
  }
}

static void drawDirectMassFitGuidePage(TCanvas* canvas) {
  if (!canvas) return;
  canvas->Clear();
  canvas->cd(1);
  gPad->SetMargin(0.08, 0.05, 0.05, 0.05);

  TLatex* title = new TLatex();
  title->SetNDC(kTRUE);
  title->SetTextSize(0.038);
  title->SetTextFont(62);
  title->DrawLatex(0.06, 0.94, "Legacy: Direct mass-fit CF (no sideband CF)");

  TLatex* t = new TLatex();
  t->SetNDC(kTRUE);
  t->SetTextSize(0.026);
  Double_t y = 0.88;
  const Double_t dy = 0.036;

  t->DrawLatex(0.06, y, "Input: hPhiMKK_vs_KstarSE/ME_<base>_wide (full M_{KK}; Maker fill without mass-window cut).");
  y -= dy;
  t->DrawLatex(0.06, y, "Per k* bin: fit SE and ME M_{KK} separately with gaus+pol2 (fallback gaus+const).");
  y -= dy;
  t->DrawLatex(0.06, y, "N_{sig} = gaus integral in channel signal window; P = N_{sig}/(N_{sig}+N_{bkg}).");
  y -= dy;
  t->DrawLatex(0.06, y, "CF_{direct}(k*) = N_{sig}^{SE}(k*) / N_{sig}^{ME}(k*)   (no sideband CF).");
  y -= dy * 1.2;

  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Not the same as:");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "Topic 3 C_{genuine}: SE-ME then gaus+const; uses ME-mass C_{bkg} formula.");
  y -= dy;
  t->DrawLatex(0.08, y, "CF-Sub (code method5): [CF_{sig}-(1-P)CF_{SB}]/P with sideband CF.");
  y -= dy * 1.2;

  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "k* definition:");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "k* = (1/2)|q*| in the pair CM frame (ComputeKStar; not single-particle rest frame).");
  y -= dy * 1.2;

  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Page layout per slice x base:");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "p1: N_{sig}^{SE/ME}, P_{true}/P_{mix}, CF_{direct}");
  y -= dy;
  t->DrawLatex(0.08, y, "p2: exemplar mass fits at high-stat and low-stat k* (SE and ME).");
  y -= dy * 1.2;
  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "k* binning:");
  t->SetTextFont(42);
  y -= dy;
  if (gConfigLoaded) {
    const Double_t dk = ConfigManager::GetInstance().GetFemtoConfig().kstarMassFitCfKstarBinWidth;
    if (dk > 0.0)
      t->DrawLatex(0.08, y, Form("kstarMassFitCfKstarBinWidth = %.3f GeV/c (RebinY from native 0.010 GeV/c).", dk));
    else
      t->DrawLatex(0.08, y, "kstarMassFitCfKstarBinWidth <= 0: native *Kstar binning (0.010 GeV/c).");
  } else {
    t->DrawLatex(0.08, y, "kstarMassFitCfKstarBinWidth from maker YAML.");
  }
}

static void drawDirectMassFitSlicePageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                        const std::string& channelBase, std::map<std::string, TGraphErrors*>& cfCache,
                                        std::map<std::string, Double_t>& metaCache) {
  if (!canvas) return;
  const Double_t kstarBinTarget = getKstarMassFitCfKstarBinTarget();
  computeDirectMassFitCfGraphs(fin, slice, channelBase, cfCache, metaCache, 0, kstarBinTarget);
  canvas->Clear();
  canvas->Divide(2, 2);

  const std::string suf = kmfCacheSuffix(kstarBinTarget);
  const std::string nSeKey = cfSliceCacheKey(slice.id, std::string("method3_Nsig_SE_") + channelBase + suf);
  const std::string nMeKey = cfSliceCacheKey(slice.id, std::string("method3_Nsig_ME_") + channelBase + suf);
  const std::string pTrueKey = cfSliceCacheKey(slice.id, std::string("method3_P_true_") + channelBase + suf);
  const std::string pMixKey = cfSliceCacheKey(slice.id, std::string("method3_P_mix_") + channelBase + suf);
  const std::string cfKey = cfSliceCacheKey(slice.id, std::string("CF_method3_") + channelBase + suf);

  TGraphErrors* gNSE = cfCache.count(nSeKey) ? cfCache[nSeKey] : 0;
  TGraphErrors* gNME = cfCache.count(nMeKey) ? cfCache[nMeKey] : 0;
  TGraphErrors* gPT = cfCache.count(pTrueKey) ? cfCache[pTrueKey] : 0;
  TGraphErrors* gPM = cfCache.count(pMixKey) ? cfCache[pMixKey] : 0;
  TGraphErrors* gCF = cfCache.count(cfKey) ? cfCache[cfKey] : 0;

  canvas->cd(1);
  if (gNSE || gNME) {
    TGraphErrors* gFirst = gNSE ? gNSE : gNME;
    gFirst->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    gFirst->SetMarkerColor(kBlack);
    gFirst->SetLineColor(kBlack);
    gFirst->Draw("AP");
    if (gNME && gNSE) {
      gNME->SetMarkerColor(kRed);
      gNME->SetLineColor(kRed);
      gNME->SetMarkerStyle(21);
      gNME->Draw("P SAME");
    } else if (gNME) {
      gNME->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gNME->Draw("AP");
    }
    if (gPad) {
      TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
      leg->SetBorderSize(0);
      leg->SetFillStyle(0);
      if (gNSE) leg->AddEntry(gNSE, "N_{sig}^{SE}", "p");
      if (gNME) leg->AddEntry(gNME, "N_{sig}^{ME}", "p");
      leg->Draw();
    }
  } else {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->DrawLatex(0.15, 0.5, "direct-mass-fit N_{sig}: missing wide TH3 or fit failed");
  }

  canvas->cd(2);
  if (gPT || gPM) {
    drawCfGraphOverlay(gPT, gPM, "P_{true}", "P_{mix}");
    TH1* hF = (gPT ? gPT : gPM)->GetHistogram();
    if (hF) hF->GetYaxis()->SetRangeUser(0.0, 1.05);
  }

  canvas->cd(3);
  {
    const std::string cfNormKey =
        cfSliceCacheKey(slice.id, std::string("CF_method3_norm_") + channelBase + suf);
    TGraphErrors* gCFn = cfCache.count(cfNormKey) ? cfCache[cfNormKey] : 0;
    if (gCF || gCFn) {
      if (gCF) {
        gCF->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
        gCF->Draw("AP");
        TH1* hF = gCF->GetHistogram();
        if (hF) {
          hF->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
          hF->GetYaxis()->SetRangeUser(kCfYMin, kCfYMax);
        }
      }
      if (gCFn) {
        gCFn->SetMarkerColor(kAzure + 2);
        gCFn->SetLineColor(kAzure + 2);
        gCFn->Draw(gCF ? "P SAME" : "AP");
      }
      TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
      leg->SetBorderSize(0);
      leg->SetFillStyle(0);
      if (gCF) leg->AddEntry(gCF, "CF_{direct} raw", "p");
      if (gCFn) leg->AddEntry(gCFn, "CF_{direct}^{norm}", "p");
      leg->Draw();
    }
  }

  canvas->cd(4);
  TLatex* note = new TLatex();
  note->SetNDC(kTRUE);
  note->SetTextSize(0.035);
  note->DrawLatex(0.12, 0.75, Form("%s dmf %s", channelBase.c_str(), slice.id.c_str()));
  note->DrawLatex(0.12, 0.65, Form("cent9 [%d,%d]", slice.cent9Min, slice.cent9Max));
  note->DrawLatex(0.12, 0.55, "CF_{direct} = N_{sig}^{SE} / N_{sig}^{ME}");
  note->DrawLatex(0.12, 0.45, "Needs hPhiMKK_vs_Kstar*_wide");
  if (kstarBinTarget > 0.0)
    note->DrawLatex(0.12, 0.38, Form("#Deltak* target = %.3f GeV/c", kstarBinTarget));
  Double_t kH = -1.0, kL = -1.0;
  if (metaCache.count(cfSliceCacheKey(slice.id, std::string("method3_k_high_") + channelBase + suf)))
    kH = metaCache[cfSliceCacheKey(slice.id, std::string("method3_k_high_") + channelBase + suf)];
  if (metaCache.count(cfSliceCacheKey(slice.id, std::string("method3_k_low_") + channelBase + suf)))
    kL = metaCache[cfSliceCacheKey(slice.id, std::string("method3_k_low_") + channelBase + suf)];
  note->DrawLatex(0.12, 0.30, Form("exemplar k* high=%.3f  low=%.3f", kH, kL));
}

static void drawDirectMassFitExemplarPageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                              const std::string& channelBase,
                                              std::map<std::string, Double_t>& metaCache,
                                              std::vector<TH1*>& keepAlive) {
  if (!canvas) return;
  canvas->Clear();
  canvas->Divide(2, 2);

  Double_t kHigh = -1.0, kLow = -1.0;
  const std::string suf = kmfCacheSuffix(getKstarMassFitCfKstarBinTarget());
  const std::string kh = cfSliceCacheKey(slice.id, std::string("method3_k_high_") + channelBase + suf);
  const std::string kl = cfSliceCacheKey(slice.id, std::string("method3_k_low_") + channelBase + suf);
  if (metaCache.count(kh)) kHigh = metaCache[kh];
  if (metaCache.count(kl)) kLow = metaCache[kl];

  canvas->cd(1);
  drawDirectMassFitPanel(fin, slice, channelBase, kHigh, kTRUE, keepAlive);
  canvas->cd(2);
  drawDirectMassFitPanel(fin, slice, channelBase, kHigh, kFALSE, keepAlive);
  canvas->cd(3);
  drawDirectMassFitPanel(fin, slice, channelBase, kLow, kTRUE, keepAlive);
  canvas->cd(4);
  drawDirectMassFitPanel(fin, slice, channelBase, kLow, kFALSE, keepAlive);

  if (gPad) {
    canvas->cd(0);
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.025);
    lat->DrawLatex(0.02, 0.97,
                   Form("%s direct-mass-fit exemplars %s (top=high-stat k*, bottom=low-stat k*)", channelBase.c_str(),
                        slice.id.c_str()));
  }
}

// Draw one already-projected M_KK slice with the legacy gaus+pol2/const fit into the
// current (small) pad. Used by the all-k* overview PDF.
static void drawMethod3MassHistCompact(TH1* h1, Double_t kstarVal, Bool_t isSE, Double_t fitMin, Double_t fitMax,
                                       Double_t sigMin, Double_t sigMax, Double_t sigmaMin, Double_t sigmaMax,
                                       Bool_t preferPol2) {
  if (!h1) return;
  if (gPad) gPad->SetMargin(0.16, 0.04, 0.14, 0.12);

  KstarMassFitCfFitResult fr;
  Bool_t ok = fitPurityGausPol2OrConst(h1, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, preferPol2, fr);

  h1->SetLineColor(kBlack);
  h1->SetStats(0);
  h1->SetTitle(Form("%s k*=%.3f;M_{KK} [GeV/c^{2}];", isSE ? "SE" : "ME", kstarVal));
  h1->GetXaxis()->SetRangeUser(fitMin - 0.01, fitMax + 0.01);
  h1->GetXaxis()->SetLabelSize(0.07);
  h1->GetYaxis()->SetLabelSize(0.07);
  h1->GetXaxis()->SetTitleSize(0.07);
  h1->GetXaxis()->SetTitleOffset(0.9);
  h1->GetXaxis()->SetNdivisions(505);
  h1->GetYaxis()->SetNdivisions(505);
  h1->SetTitleSize(0.09);
  h1->Draw("HIST");

  if (ok) {
    TF1* fDraw = 0;
    if (fr.usedPol2) {
      fDraw = new TF1(Form("_m3alldraw_%lx", (unsigned long)h1), "gaus(0)+pol2(3)", fitMin, fitMax);
      fDraw->SetParameters(fr.amp, fr.mean, fr.sigma, fr.polA, fr.polB, fr.polC);
    } else {
      fDraw = new TF1(Form("_m3alldraw_%lx", (unsigned long)h1), "gaus(0)+[3]", fitMin, fitMax);
      fDraw->SetParameters(fr.amp, fr.mean, fr.sigma, fr.polA);
    }
    fDraw->SetLineColor(kRed);
    fDraw->SetLineWidth(2);
    fDraw->Draw("SAME");
    TF1* fG = new TF1(Form("_m3allg_%lx", (unsigned long)h1), "gaus", fitMin, fitMax);
    fG->SetParameters(fr.amp, fr.mean, fr.sigma);
    fG->SetLineColor(kBlue);
    fG->SetLineStyle(2);
    fG->Draw("SAME");
  }

  if (gPad) {
    Double_t ylo = gPad->GetUymin();
    Double_t yhi = gPad->GetUymax();
    TLine* l1 = new TLine(sigMin, ylo, sigMin, yhi);
    l1->SetLineColor(kGreen + 2);
    l1->SetLineStyle(2);
    l1->Draw("same");
    TLine* l2 = new TLine(sigMax, ylo, sigMax, yhi);
    l2->SetLineColor(kGreen + 2);
    l2->SetLineStyle(2);
    l2->Draw("same");

    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.075);
    if (ok) {
      lat->DrawLatex(0.18, 0.80, Form("N_{sig}=%.0f P=%.2f", fr.nSig, fr.purity));
    } else {
      lat->SetTextColor(kGray + 2);
      lat->DrawLatex(0.18, 0.80, "fit --");
    }
  }
}


// Build S/B(k*) from cached N_sig and N_bkg graphs (identical x sampling). Returns 0 if unavailable.
static TGraph* buildMethod3SignalToBkgGraph(TGraphErrors* gSig, TGraphErrors* gBkg, Color_t color, Style_t style) {
  if (!gSig || !gBkg) return 0;
  const Int_t n = gSig->GetN();
  if (n <= 0 || gBkg->GetN() != n) return 0;
  const Double_t* xs = gSig->GetX();
  const Double_t* ys = gSig->GetY();
  const Double_t* yb = gBkg->GetY();
  std::vector<Double_t> vx, vy;
  for (Int_t i = 0; i < n; ++i) {
    if (yb[i] <= 0.0) continue;
    vx.push_back(xs[i]);
    vy.push_back(ys[i] / yb[i]);
  }
  if (vx.empty()) return 0;
  TGraph* g = new TGraph((Int_t)vx.size(), &vx[0], &vy[0]);
  g->SetMarkerStyle(style);
  g->SetMarkerColor(color);
  g->SetLineColor(color);
  return g;
}

// Purity / yield diagnostics page inserted after each all-k* mass-fit page (same base x slice).
// p1: P_true(k*) & P_mix(k*)   p2: N_sig^SE & N_sig^ME (log)
// p3: S/B^SE & S/B^ME (log)    p4: CF_direct(k*) = N_sig^SE / N_sig^ME
static void drawDirectMassFitPurityPageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                         const std::string& channelBase,
                                         std::map<std::string, TGraphErrors*>& cfCache,
                                         std::map<std::string, Double_t>& metaCache, Double_t kstarBinTarget = -1.0) {
  if (!canvas) return;
  if (kstarBinTarget < 0.0) kstarBinTarget = getKstarMassFitCfKstarBinTarget();
  computeDirectMassFitCfGraphs(fin, slice, channelBase, cfCache, metaCache, 0, kstarBinTarget);
  canvas->Clear();
  canvas->Divide(2, 2);

  const std::string suf = kmfCacheSuffix(kstarBinTarget);
  const std::string pTrueKey = cfSliceCacheKey(slice.id, std::string("method3_P_true_") + channelBase + suf);
  const std::string pMixKey = cfSliceCacheKey(slice.id, std::string("method3_P_mix_") + channelBase + suf);
  const std::string nSeKey = cfSliceCacheKey(slice.id, std::string("method3_Nsig_SE_") + channelBase + suf);
  const std::string nMeKey = cfSliceCacheKey(slice.id, std::string("method3_Nsig_ME_") + channelBase + suf);
  const std::string nSeBkgKey = cfSliceCacheKey(slice.id, std::string("method3_Nbkg_SE_") + channelBase + suf);
  const std::string nMeBkgKey = cfSliceCacheKey(slice.id, std::string("method3_Nbkg_ME_") + channelBase + suf);
  const std::string cfKey = cfSliceCacheKey(slice.id, std::string("CF_method3_") + channelBase + suf);

  TGraphErrors* gPT = cfCache.count(pTrueKey) ? cfCache[pTrueKey] : 0;
  TGraphErrors* gPM = cfCache.count(pMixKey) ? cfCache[pMixKey] : 0;
  TGraphErrors* gNSE = cfCache.count(nSeKey) ? cfCache[nSeKey] : 0;
  TGraphErrors* gNME = cfCache.count(nMeKey) ? cfCache[nMeKey] : 0;
  TGraphErrors* gNSEb = cfCache.count(nSeBkgKey) ? cfCache[nSeBkgKey] : 0;
  TGraphErrors* gNMEb = cfCache.count(nMeBkgKey) ? cfCache[nMeBkgKey] : 0;
  TGraphErrors* gCF = cfCache.count(cfKey) ? cfCache[cfKey] : 0;

  const Bool_t haveAny = (gPT || gPM || gNSE || gNME || gCF);

  // p1: purity vs k*
  canvas->cd(1);
  if (gPT || gPM) {
    TGraphErrors* gf = gPT ? gPT : gPM;
    if (gPT) {
      gPT->SetMarkerStyle(20);
      gPT->SetMarkerColor(kBlack);
      gPT->SetLineColor(kBlack);
      gPT->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gPT->Draw("AP");
    }
    if (gPM) {
      gPM->SetMarkerStyle(21);
      gPM->SetMarkerColor(kRed);
      gPM->SetLineColor(kRed);
      if (!gPT) gPM->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gPM->Draw(gPT ? "P SAME" : "AP");
    }
    if (gf->GetHistogram()) {
      gf->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
      gf->GetHistogram()->GetYaxis()->SetRangeUser(0.0, 1.05);
      gf->GetHistogram()->SetTitle(Form("Purity(k*) %s %s;k* [GeV/c];P = N_{sig}/(N_{sig}+N_{bkg})",
                                        channelBase.c_str(), slice.id.c_str()));
    }
    TLegend* leg = new TLegend(0.55, 0.20, 0.88, 0.38);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    if (gPT) leg->AddEntry(gPT, "P_{true} (SE)", "p");
    if (gPM) leg->AddEntry(gPM, "P_{mix} (ME)", "p");
    leg->Draw();
  } else {
    TLatex* t = new TLatex();
    t->SetNDC(kTRUE);
    t->DrawLatex(0.15, 0.5, "purity: missing wide TH3 or no successful fits");
  }

  // p2: N_sig vs k* (log)
  canvas->cd(2);
  if (gNSE || gNME) {
    gPad->SetLogy(1);
    TGraphErrors* gf = gNSE ? gNSE : gNME;
    if (gNSE) {
      gNSE->SetMarkerStyle(20);
      gNSE->SetMarkerColor(kBlack);
      gNSE->SetLineColor(kBlack);
      gNSE->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gNSE->Draw("AP");
    }
    if (gNME) {
      gNME->SetMarkerStyle(21);
      gNME->SetMarkerColor(kRed);
      gNME->SetLineColor(kRed);
      if (!gNSE) gNME->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gNME->Draw(gNSE ? "P SAME" : "AP");
    }
    if (gf->GetHistogram()) {
      gf->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
      gf->GetHistogram()->SetTitle(Form("N_{sig}(k*) %s %s;k* [GeV/c];N_{sig} (gaus integral)",
                                        channelBase.c_str(), slice.id.c_str()));
    }
    TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    if (gNSE) leg->AddEntry(gNSE, "N_{sig}^{SE}", "p");
    if (gNME) leg->AddEntry(gNME, "N_{sig}^{ME}", "p");
    leg->Draw();
  }

  // p3: S/B vs k* (log)
  canvas->cd(3);
  {
    TGraph* gSBse = buildMethod3SignalToBkgGraph(gNSE, gNSEb, kBlack, 20);
    TGraph* gSBme = buildMethod3SignalToBkgGraph(gNME, gNMEb, kRed, 21);
    if (gSBse || gSBme) {
      gPad->SetLogy(1);
      TGraph* gf = gSBse ? gSBse : gSBme;
      if (gSBse) {
        gSBse->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
        gSBse->Draw("AP");
      }
      if (gSBme) {
        if (!gSBse) gSBme->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
        gSBme->Draw(gSBse ? "P SAME" : "AP");
      }
      if (gf->GetHistogram()) {
        gf->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
        gf->GetHistogram()->SetTitle(Form("S/B(k*) %s %s;k* [GeV/c];N_{sig}/N_{bkg} in signal window",
                                          channelBase.c_str(), slice.id.c_str()));
      }
      TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
      leg->SetBorderSize(0);
      leg->SetFillStyle(0);
      if (gSBse) leg->AddEntry(gSBse, "S/B^{SE}", "p");
      if (gSBme) leg->AddEntry(gSBme, "S/B^{ME}", "p");
      leg->Draw();
    } else {
      TLatex* t = new TLatex();
      t->SetNDC(kTRUE);
      t->DrawLatex(0.15, 0.5, "S/B: no background yields");
    }
  }

  // p4: CF_direct vs k*
  canvas->cd(4);
  if (gCF) {
    gCF->SetMarkerStyle(20);
    gCF->SetMarkerColor(kBlue + 1);
    gCF->SetLineColor(kBlue + 1);
    gCF->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    gCF->Draw("AP");
    if (gCF->GetHistogram()) {
      gCF->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
      gCF->GetHistogram()->GetYaxis()->SetRangeUser(kCfYMin, kCfYMax);
      gCF->GetHistogram()->SetTitle(Form("CF_{direct}(k*) %s %s;k* [GeV/c];N_{sig}^{SE}/N_{sig}^{ME}",
                                         channelBase.c_str(), slice.id.c_str()));
    }
    TLine* one = new TLine(kCfKstarXMin, 1.0, kCfKstarXMax, 1.0);
    one->SetLineStyle(2);
    one->SetLineColor(kGray + 2);
    one->Draw("same");
  } else {
    TLatex* t = new TLatex();
    t->SetNDC(kTRUE);
    t->DrawLatex(0.15, 0.5, "CF_{direct}: unavailable");
  }

  canvas->cd(0);
  TLatex* title = new TLatex();
  title->SetNDC(kTRUE);
  title->SetTextSize(0.024);
  title->SetTextFont(62);
  TString binTag =
      (kstarBinTarget > 0.0) ? Form("  [#Deltak*#approx%.3f GeV/c]", kstarBinTarget) : "  [native #Deltak*]";
  title->DrawLatex(0.02, 0.985,
                   Form("%s  %s (cent9 [%d,%d])  direct-mass-fit purity / yield diagnostics%s%s", channelBase.c_str(),
                        slice.id.c_str(), slice.cent9Min, slice.cent9Max, binTag.Data(),
                        haveAny ? "" : "  [no data]"));
}

// ---------------------------------------------------------------------------
// kstarMassFitCF ROT/MIX mass-background subtraction QA (dedicated PDF)
// S = F - alpha * B,  alpha = int_side F / int_side B  (left+right SB windows)
// ---------------------------------------------------------------------------
// Mass-axis rebin applied to SE/ME F and B projections before alpha, S, and gaus fit.
// Trial value (YAML wiring deferred); set 1 to disable.
static const Int_t kKmfMassRebin = 3;

static Bool_t getChannelSidebandWindows(const std::string& channelBase, Double_t& lMin, Double_t& lMax,
                                        Double_t& rMin, Double_t& rMax) {
  lMin = 0.995;
  lMax = 1.010;
  rMin = 1.035;
  rMax = 1.060;
  if (!gConfigLoaded) return kTRUE;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  const FemtoConfig::ChannelDef* chL = fc.FindChannel(channelBase + "_leftSB");
  const FemtoConfig::ChannelDef* chR = fc.FindChannel(channelBase + "_rightSB");
  if (chL) {
    lMin = chL->signalMin;
    lMax = chL->signalMax;
  }
  if (chR) {
    rMin = chR->signalMin;
    rMax = chR->signalMax;
  }
  return kTRUE;
}

static const Int_t kKmfStatusOk = 0;
static const Int_t kKmfStatusFitFail = 1;
static const Int_t kKmfStatusNegYield = 2;
static const Int_t kKmfStatusLowStat = 3;
static const Int_t kKmfStatusNormFail = 4;
static const Int_t kKmfStatusNonFinite = 5;

static Double_t histIntegralRange(TH1* h, Double_t xMin, Double_t xMax) {
  if (!h) return 0.0;
  Int_t b0 = h->GetXaxis()->FindBin(xMin + 1e-9);
  Int_t b1 = h->GetXaxis()->FindBin(xMax - 1e-9);
  if (b1 < b0) return 0.0;
  return h->Integral(b0, b1);
}

static Double_t histIntegralAndErrorRange(TH1* h, Double_t xMin, Double_t xMax, Double_t& err) {
  err = 0.0;
  if (!h) return 0.0;
  Int_t b0 = h->GetXaxis()->FindBin(xMin + 1e-9);
  Int_t b1 = h->GetXaxis()->FindBin(xMax - 1e-9);
  if (b1 < b0) return 0.0;
  return h->IntegralAndError(b0, b1, err);
}

static Double_t kmfAlphaFromWindows(TH1* hF, TH1* hB, Double_t lMin, Double_t lMax, Double_t rMin, Double_t rMax,
                                    Double_t& alphaErr) {
  alphaErr = 0.0;
  Double_t eFL = 0.0, eFR = 0.0, eBL = 0.0, eBR = 0.0;
  const Double_t fSide =
      histIntegralAndErrorRange(hF, lMin, lMax, eFL) + histIntegralAndErrorRange(hF, rMin, rMax, eFR);
  const Double_t bSide =
      histIntegralAndErrorRange(hB, lMin, lMax, eBL) + histIntegralAndErrorRange(hB, rMin, rMax, eBR);
  const Double_t eF = TMath::Sqrt(eFL * eFL + eFR * eFR);
  const Double_t eB = TMath::Sqrt(eBL * eBL + eBR * eBR);
  if (bSide <= 0.0) return 0.0;
  const Double_t a = fSide / bSide;
  Double_t rel2 = 0.0;
  if (fSide > 0.0) rel2 += TMath::Power(eF / fSide, 2);
  rel2 += TMath::Power(eB / bSide, 2);
  alphaErr = TMath::Abs(a) * TMath::Sqrt(rel2);
  return a;
}

static Double_t kmfAlphaFromSingleWindow(TH1* hF, TH1* hB, Double_t mMin, Double_t mMax, Double_t& alphaErr) {
  alphaErr = 0.0;
  Double_t eF = 0.0, eB = 0.0;
  const Double_t fSide = histIntegralAndErrorRange(hF, mMin, mMax, eF);
  const Double_t bSide = histIntegralAndErrorRange(hB, mMin, mMax, eB);
  if (bSide <= 0.0) return 0.0;
  const Double_t a = fSide / bSide;
  Double_t rel2 = 0.0;
  if (fSide > 0.0) rel2 += TMath::Power(eF / fSide, 2);
  rel2 += TMath::Power(eB / bSide, 2);
  alphaErr = TMath::Abs(a) * TMath::Sqrt(rel2);
  return a;
}

static void kmfApplyAlphaErrorToS(TH1* hS, TH1* hB, Double_t alphaErr) {
  if (!hS || !hB || alphaErr <= 0.0) return;
  const Int_t n = hS->GetNbinsX();
  for (Int_t i = 1; i <= n; ++i) {
    const Double_t b = hB->GetBinContent(i);
    const Double_t e0 = hS->GetBinError(i);
    const Double_t eA = TMath::Abs(b) * alphaErr;
    hS->SetBinError(i, TMath::Sqrt(e0 * e0 + eA * eA));
  }
}

static void getKstarMassFitCfTemplateOrder(std::vector<std::string>& tags) {
  tags.clear();
  std::string def = "rot";
  Bool_t xcheck = kTRUE;
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    def = fc.kstarMassFitCfTemplate;
    xcheck = fc.kstarMassFitCfCrossCheck;
  }
  if (def != "rot" && def != "mix") def = "rot";
  tags.push_back(def);
  if (xcheck) tags.push_back(def == "rot" ? "mix" : "rot");
}

static Int_t kmfYieldStatus(Bool_t fitOk, Double_t nSig, Double_t errNSig, Bool_t lowStat) {
  if (lowStat) return kKmfStatusLowStat;
  if (!fitOk) return kKmfStatusFitFail;
  if (!TMath::Finite(nSig) || !TMath::Finite(errNSig)) return kKmfStatusNonFinite;
  if (nSig <= 0.0) return kKmfStatusNegYield;
  return kKmfStatusOk;
}

static void kmfWarnWindowOverlapOnce(Double_t fitMin, Double_t fitMax, Double_t sigMin, Double_t sigMax,
                                     Double_t aMin, Double_t aMax, Bool_t alphaSingle) {
  static Bool_t warned = kFALSE;
  if (warned || !alphaSingle) return;
  warned = kTRUE;
  const Bool_t overlapSig = (aMin < sigMax) && (aMax > sigMin);
  const Bool_t overlapFit = (aMin < fitMax) && (aMax > fitMin);
  if (overlapSig) {
    std::cerr << "[checkHistAnaFemtoPhi] WARNING: kstarMassFitCF α window [" << aMin << "," << aMax
              << "] overlaps signal window [" << sigMin << "," << sigMax << "]" << std::endl;
  }
  if (overlapFit) {
    std::cerr << "[checkHistAnaFemtoPhi] WARNING: kstarMassFitCF α window [" << aMin << "," << aMax
              << "] overlaps fit range [" << fitMin << "," << fitMax << "]" << std::endl;
  }
}

static std::string kmfBkgWideKey(const std::string& channelBase, const char* templateTag, Bool_t isSE) {
  // templateTag: "rot" or "mix" -> phi_rot_proton / phi_mix_proton
  const std::string bach =
      (channelBase.find("deuteron") != std::string::npos) ? "deuteron" : "proton";
  return std::string(isSE ? "hPhiMKK_vs_KstarSE_" : "hPhiMKK_vs_KstarME_") + "phi_" + templateTag + "_" + bach +
         "_wide";
}

static void drawKstarMassFitCfGuidePage(TCanvas* canvas) {
  if (!canvas) return;
  canvas->Clear();
  canvas->cd(1);
  gPad->SetMargin(0.08, 0.05, 0.05, 0.05);

  TLatex* title = new TLatex();
  title->SetNDC(kTRUE);
  title->SetTextSize(0.036);
  title->SetTextFont(62);
  title->DrawLatex(0.06, 0.94, "kstarMassFitCF: per-k* full M_{KK} fit, S = F - #alpha B");

  TLatex* t = new TLatex();
  t->SetNDC(kTRUE);
  t->SetTextSize(0.025);
  Double_t y = 0.88;
  const Double_t dy = 0.034;
  t->DrawLatex(0.06, y, "This PDF is the primary CF QA (simple SE/ME ratio is diagnostic only).");
  y -= dy;
  t->DrawLatex(0.06, y, "Pages below (phi-p / phi-d only): per k* bin, SE and ME rows.");
  y -= dy;
  t->DrawLatex(0.06, y, "Col1: F = full candidate (hPhiMKK_vs_Kstar*_phi_{p,d}_wide)");
  y -= dy;
  t->DrawLatex(0.06, y, "Col2: #alpha B = sideband-scaled background template");
  y -= dy;
  t->DrawLatex(0.06, y, "Col3: S = F - #alpha B with gaus-only fit (no pol; residual BG ~ 0)");
  y -= dy;
  t->DrawLatex(0.06, y, "Col4: overlay F (blue) + #alpha B (black) + S (red) on one pad");
  y -= dy * 1.2;
  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Mass rebin (before #alpha / S / fit):");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, Form("SE and ME: Rebin(%d) on F and B projections (kKmfMassRebin).",
                             kKmfMassRebin));
  y -= dy * 1.2;
  t->DrawLatex(0.08, y, Form("Low k*: merge first %d x %.3f-GeV/c bins before projection and fit.",
                             getKstarMassFitCfLowKstarMergeBins(), getKstarMassFitCfKstarBinTarget()));
  y -= dy * 1.2;
  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Scale:");
  t->SetTextFont(42);
  y -= dy;
  {
    Double_t aMin = 0.0, aMax = 0.0;
    if (getKstarMassFitCfAlphaSingleWindow(aMin, aMax)) {
      t->DrawLatex(0.08, y,
                   Form("#alpha = [#int F dM] / [#int B dM] over right-only M_{KK}: %.3f - %.3f GeV/c^{2}", aMin, aMax));
    } else {
      t->DrawLatex(0.08, y, "#alpha = [#int_{L}+#int_{R} F dM] / [#int_{L}+#int_{R} B dM]  (L/R = leftSB/rightSB)");
    }
  }
  y -= dy * 1.2;
  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Templates:");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "ROT: hPhiMKK_vs_Kstar{SE,ME}_phi_rot_{proton,deuteron}_wide");
  y -= dy;
  t->DrawLatex(0.08, y, "MIX: hPhiMKK_vs_Kstar{SE,ME}_phi_mix_{proton,deuteron}_wide (current K #times buffer opposite K)");
  y -= dy * 1.2;
  t->DrawLatex(0.06, y, "After each (base, template, cent) mass-fit series: CF pages from Y_SE/Y_ME on S.");
  y -= dy;
  t->DrawLatex(0.06, y, "C_{raw}(k*) = Y_SE(k*)/Y_ME(k*);  C_{norm} scaled to ~1 in channel normQMin-normQMax.");
  y -= dy;
  t->DrawLatex(0.06, y, "Graphs also written to sidecar ROOT (CF_kmf_{rot|mix}_*_{raw|norm}, kmf_Y_*, kmf_fitstatus_*).");
  y -= dy;
  t->DrawLatex(0.06, y, "k* binning matches kstarMassFitCfKstarBinWidth; centrality: pct_0_10 / 0_20 / 0_30.");
  y -= dy * 1.2;
  t->SetTextColor(kRed + 1);
  t->DrawLatex(0.06, y, "MIX keys need a farm re-run after switching to standard current#timesbuffer MIX.");
}

// One page per k* bin: Divide(4,2): row0 SE F / αB / S+fit / overlay, row1 ME.
// Optionally fills nSigSE/ME (+errors) from S fits when pointers are non-null.
static Bool_t drawKstarMassFitCfKstarPage(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                         const std::string& channelBase, const char* templateTag, Int_t iyFirst,
                                         Int_t iyLast, Double_t kstar, TH2* h2Fse, TH2* h2Fme, TH2* h2Bse, TH2* h2Bme,
                                         Double_t kstarBinTarget, std::vector<TH1*>& keepAlive, Double_t* nSigSE,
                                         Double_t* eSigSE, Double_t* nSigME, Double_t* eSigME, Int_t* statusSE,
                                         Int_t* statusME) {
  if (!canvas) return kFALSE;
  canvas->Clear();
  canvas->Divide(4, 2);

  Double_t fitMin = 0.99, fitMax = 1.06, sigmaMin = 0.002, sigmaMax = 0.020;
  Double_t purityMinK = 0.0, purityMaxK = 0.65, clampMin = 0.05, clampMax = 1.0;
  Int_t minEntries = 20;
  Bool_t preferPol2 = kTRUE;
  getKstarMassFitCfFitConfig(fitMin, fitMax, sigmaMin, sigmaMax, purityMinK, purityMaxK, minEntries, clampMin, clampMax,
                      preferPol2);
  (void)preferPol2;  // S uses gaus-only; purityDirectFitModel is legacy direct mass-fit only
  Double_t sigMin = 1.012, sigMax = 1.026;
  getChannelSignalMassWindow(channelSignal(channelBase), sigMin, sigMax);
  Double_t alphaMassMin = 0.0, alphaMassMax = 0.0;
  const Bool_t alphaSingle = getKstarMassFitCfAlphaSingleWindow(alphaMassMin, alphaMassMax);
  Double_t lMin = 0.0, lMax = 0.0, rMin = 0.0, rMax = 0.0;
  if (!alphaSingle) getChannelSidebandWindows(channelBase, lMin, lMax, rMin, rMax);
  kmfWarnWindowOverlapOnce(fitMin, fitMax, sigMin, sigMax, alphaMassMin, alphaMassMax, alphaSingle);
  const Double_t kstarLo = h2Fse->GetYaxis()->GetBinLowEdge(iyFirst);
  const Double_t kstarHi = h2Fse->GetYaxis()->GetBinUpEdge(iyLast);
  if (h2Fme && (h2Fme->GetYaxis()->GetBinLowEdge(iyFirst) != kstarLo ||
                h2Fme->GetYaxis()->GetBinUpEdge(iyLast) != kstarHi)) {
    std::cerr << "[checkHistAnaFemtoPhi] ERROR: kstarMassFitCF SE/ME k* merge mismatch for " << channelBase
              << " " << slice.id << " iy=[" << iyFirst << "," << iyLast << "]" << std::endl;
  }

  auto makeProj = [&](TH2* h2, const char* tag) -> TH1* {
    if (!h2) return 0;
    TH1* h = h2->ProjectionX(Form("_m3sub_%s_%s_%s_%d_%d", templateTag, channelBase.c_str(), tag, iyFirst, iyLast),
                             iyFirst, iyLast);
    if (!h) return 0;
    h->SetDirectory(0);
    keepAlive.push_back(h);
    return h;
  };

  TH1* hFse = makeProj(h2Fse, "Fse");
  TH1* hFme = makeProj(h2Fme, "Fme");
  TH1* hBse = makeProj(h2Bse, "Bse");
  TH1* hBme = makeProj(h2Bme, "Bme");

  // Rebin mass axis for SE and ME (F and B) before alpha, subtraction, and fit.
  auto rebinMass = [&](TH1* h) {
    if (!h || kKmfMassRebin <= 1) return;
    h->Rebin(kKmfMassRebin);
  };
  rebinMass(hFse);
  rebinMass(hFme);
  rebinMass(hBse);
  rebinMass(hBme);

  auto buildScaledAndS = [&](TH1* hF, TH1* hB, Double_t& alphaOut, Double_t& alphaErrOut, TH1*& hBscOut, TH1*& hSOut) {
    alphaOut = 0.0;
    alphaErrOut = 0.0;
    hBscOut = 0;
    hSOut = 0;
    if (!hF || !hB) return;
    alphaOut = alphaSingle ? kmfAlphaFromSingleWindow(hF, hB, alphaMassMin, alphaMassMax, alphaErrOut)
                           : kmfAlphaFromWindows(hF, hB, lMin, lMax, rMin, rMax, alphaErrOut);
    if (!TMath::Finite(alphaOut) || !TMath::Finite(alphaErrOut)) {
      alphaOut = 0.0;
      alphaErrOut = 0.0;
    }
    hBscOut = (TH1*)hB->Clone(Form("%s_scaled", hB->GetName()));
    hBscOut->SetDirectory(0);
    hBscOut->Scale(alphaOut);
    keepAlive.push_back(hBscOut);
    hSOut = (TH1*)hF->Clone(Form("%s_sub", hF->GetName()));
    hSOut->SetDirectory(0);
    hSOut->Add(hBscOut, -1.0);
    kmfApplyAlphaErrorToS(hSOut, hB, alphaErrOut);
    keepAlive.push_back(hSOut);
  };

  Double_t aSE = 0.0, aME = 0.0, aSEerr = 0.0, aMEerr = 0.0;
  TH1 *hBscSE = 0, *hSSE = 0, *hBscME = 0, *hSME = 0;
  buildScaledAndS(hFse, hBse, aSE, aSEerr, hBscSE, hSSE);
  buildScaledAndS(hFme, hBme, aME, aMEerr, hBscME, hSME);

  Bool_t okSE = kFALSE, okME = kFALSE;
  KstarMassFitCfFitResult frSE, frME;

  auto drawOverlay = [&](TH1* hF, TH1* hBsc, TH1* hS, const char* rowTag, Double_t alpha) {
    if (!hF && !hBsc && !hS) {
      TLatex* z = new TLatex();
      z->SetNDC(kTRUE);
      z->DrawLatex(0.2, 0.5, Form("%s: no overlay", rowTag));
      return;
    }
    // Clones so pad-1..3 styles / axes stay independent of the composite pad.
    TH1* cF = hF ? (TH1*)hF->Clone(Form("%s_ovF", hF->GetName())) : 0;
    TH1* cB = hBsc ? (TH1*)hBsc->Clone(Form("%s_ovB", hBsc->GetName())) : 0;
    TH1* cS = hS ? (TH1*)hS->Clone(Form("%s_ovS", hS->GetName())) : 0;
    if (cF) {
      cF->SetDirectory(0);
      keepAlive.push_back(cF);
    }
    if (cB) {
      cB->SetDirectory(0);
      keepAlive.push_back(cB);
    }
    if (cS) {
      cS->SetDirectory(0);
      keepAlive.push_back(cS);
    }

    Double_t ymin = 0.0, ymax = 0.0;
    Bool_t first = kTRUE;
    auto accum = [&](TH1* h) {
      if (!h) return;
      const Double_t lo = h->GetMinimum();
      const Double_t hi = h->GetMaximum();
      if (first) {
        ymin = lo;
        ymax = hi;
        first = kFALSE;
      } else {
        if (lo < ymin) ymin = lo;
        if (hi > ymax) ymax = hi;
      }
    };
    accum(cF);
    accum(cB);
    accum(cS);
    const Double_t pad = 0.08 * (ymax - ymin);
    if (pad > 0.0) {
      ymin -= pad;
      ymax += pad;
    } else {
      ymax = (ymax == 0.0) ? 1.0 : ymax * 1.1;
    }

    TH1* frame = cF ? cF : (cB ? cB : cS);
    frame->SetTitle(Form("%s overlay (#alpha=%.3f);#it{M}_{KK};Counts", rowTag, alpha));
    frame->SetMinimum(ymin);
    frame->SetMaximum(ymax);
    if (cF) {
      cF->SetLineColor(kBlue + 1);
      cF->SetMarkerColor(kBlue + 1);
      cF->SetMarkerStyle(20);
      cF->Draw("E");
      if (cB) {
        cB->SetLineColor(kBlack);
        cB->SetLineWidth(2);
        cB->Draw("HIST SAME");
      }
      if (cS) {
        cS->SetLineColor(kRed);
        cS->SetMarkerColor(kRed);
        cS->SetMarkerStyle(20);
        cS->Draw("E SAME");
      }
    } else if (cB) {
      cB->SetLineColor(kBlack);
      cB->Draw("HIST");
      if (cS) {
        cS->SetLineColor(kRed);
        cS->SetMarkerColor(kRed);
        cS->Draw("E SAME");
      }
    } else {
      cS->SetLineColor(kRed);
      cS->SetMarkerColor(kRed);
      cS->Draw("E");
    }
    TLatex* leg = new TLatex();
    leg->SetNDC(kTRUE);
    leg->SetTextSize(0.040);
    leg->SetTextColor(kBlue + 1);
    leg->DrawLatex(0.14, 0.86, "F");
    leg->SetTextColor(kBlack);
    leg->DrawLatex(0.22, 0.86, "#alpha B");
    leg->SetTextColor(kRed);
    leg->DrawLatex(0.38, 0.86, "S");
  };

  // Row 0: SE  pads 1-4
  canvas->cd(1);
  if (hFse) {
    hFse->SetTitle(Form("SE F  %.3f<k*<%.3f;#it{M}_{KK};Counts", kstarLo, kstarHi));
    hFse->SetLineColor(kBlue + 1);
    hFse->Draw("E");
  }
  canvas->cd(2);
  if (hBscSE) {
    hBscSE->SetTitle(Form("SE #alpha B (#alpha=%.3f #pm %.3f);#it{M}_{KK};Counts", aSE, aSEerr));
    hBscSE->SetLineColor(kBlack);
    hBscSE->Draw("HIST");
  } else {
    TLatex* z = new TLatex();
    z->SetNDC(kTRUE);
    z->DrawLatex(0.2, 0.5, "SE: missing BG");
  }
  canvas->cd(3);
  if (hSSE) {
    hSSE->SetTitle(Form("SE S = F-#alpha B (gaus);#it{M}_{KK};Counts"));
    hSSE->SetMarkerColor(kRed);
    hSSE->SetLineColor(kRed);
    hSSE->Draw("E");
    okSE = fitPurityGausOnly(hSSE, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, frSE);
    if (okSE) {
      TF1* fDraw = new TF1(Form("_m3sub_se_%d_%d", iyFirst, iyLast), "gaus", fitMin, fitMax);
      fDraw->SetParameters(frSE.amp, frSE.mean, frSE.sigma);
      fDraw->SetLineColor(kMagenta + 1);
      fDraw->SetLineWidth(2);
      fDraw->Draw("SAME");
    }
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.045);
    lat->DrawLatex(0.14, 0.84, Form("N_{sig}=%.1f %s", okSE ? frSE.nSig : 0.0, okSE ? "" : "FAIL"));
  }
  canvas->cd(4);
  drawOverlay(hFse, hBscSE, hSSE, "SE", aSE);

  // Row 1: ME  pads 5-8
  canvas->cd(5);
  if (hFme) {
    hFme->SetTitle(Form("ME F  %.3f<k*<%.3f;#it{M}_{KK};Counts", kstarLo, kstarHi));
    hFme->SetLineColor(kBlue + 1);
    hFme->Draw("E");
  }
  canvas->cd(6);
  if (hBscME) {
    hBscME->SetTitle(Form("ME #alpha B (#alpha=%.3f #pm %.3f);#it{M}_{KK};Counts", aME, aMEerr));
    hBscME->SetLineColor(kBlack);
    hBscME->Draw("HIST");
  } else {
    TLatex* z = new TLatex();
    z->SetNDC(kTRUE);
    z->DrawLatex(0.2, 0.5, "ME: missing BG");
  }
  canvas->cd(7);
  if (hSME) {
    hSME->SetTitle(Form("ME S = F-#alpha B (gaus);#it{M}_{KK};Counts"));
    hSME->SetMarkerColor(kRed);
    hSME->SetLineColor(kRed);
    hSME->Draw("E");
    okME = fitPurityGausOnly(hSME, fitMin, fitMax, sigMin, sigMax, sigmaMin, sigmaMax, frME);
    if (okME) {
      TF1* fDraw = new TF1(Form("_m3sub_me_%d_%d", iyFirst, iyLast), "gaus", fitMin, fitMax);
      fDraw->SetParameters(frME.amp, frME.mean, frME.sigma);
      fDraw->SetLineColor(kMagenta + 1);
      fDraw->SetLineWidth(2);
      fDraw->Draw("SAME");
    }
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.045);
    lat->DrawLatex(0.14, 0.84, Form("N_{sig}=%.1f %s", okME ? frME.nSig : 0.0, okME ? "" : "FAIL"));
  }
  canvas->cd(8);
  drawOverlay(hFme, hBscME, hSME, "ME", aME);

  canvas->cd(0);
  TLatex* title = new TLatex();
  title->SetNDC(kTRUE);
  title->SetTextSize(0.020);
  title->SetTextFont(62);
  title->DrawLatex(0.02, 0.985,
                   Form("%s  %s  template=%s  %.3f<k*<%.3f GeV/c  (x=%.3f)  Rebin(%d)  F | #alpha B | S+gaus | overlay",
                        channelBase.c_str(), slice.id.c_str(), templateTag, kstarLo, kstarHi, kstar,
                        kKmfMassRebin));
  (void)minEntries;
  (void)fin;
  (void)slice;

  const Int_t stSE = kmfYieldStatus(okSE, frSE.nSig, frSE.errNSig, kFALSE);
  const Int_t stME = kmfYieldStatus(okME, frME.nSig, frME.errNSig, kFALSE);
  if (nSigSE) *nSigSE = (stSE == kKmfStatusOk) ? frSE.nSig : 0.0;
  if (eSigSE) *eSigSE = (stSE == kKmfStatusOk) ? frSE.errNSig : 0.0;
  if (nSigME) *nSigME = (stME == kKmfStatusOk) ? frME.nSig : 0.0;
  if (eSigME) *eSigME = (stME == kKmfStatusOk) ? frME.errNSig : 0.0;
  if (statusSE) *statusSE = stSE;
  if (statusME) *statusME = stME;
  return (stSE == kKmfStatusOk && stME == kKmfStatusOk);
}

static void drawKstarMassFitCfPage(TCanvas* canvas, const FemtoConfig::CfCentSlice& slice,
                                    const std::string& channelBase, const char* templateTag,
                                    TGraphErrors* gNSE, TGraphErrors* gNME, TGraphErrors* gCF,
                                    TGraphErrors* gCFn) {
  if (!canvas) return;
  canvas->Clear();
  canvas->SetCanvasSize(1400, 1000);
  canvas->Divide(2, 2);

  auto drawOne = [&](Int_t pad, TGraphErrors* g, const char* ytitle, Bool_t logy, Double_t yMin, Double_t yMax) {
    canvas->cd(pad);
    if (!g) {
      TLatex* t = new TLatex();
      t->SetNDC(kTRUE);
      t->DrawLatex(0.2, 0.5, "no data");
      return;
    }
    if (logy) gPad->SetLogy(1);
    g->SetMarkerStyle(20);
    g->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    g->Draw("AP");
    if (g->GetHistogram()) {
      g->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
      g->GetHistogram()->SetTitle(Form("%s %s template=%s;%s", channelBase.c_str(), slice.id.c_str(),
                                       templateTag, ytitle));
      if (yMax > yMin) g->GetHistogram()->GetYaxis()->SetRangeUser(yMin, yMax);
    }
  };

  drawOne(1, gNSE, "k* [GeV/c];Y_{SE} from S fit", kTRUE, 0.0, 0.0);
  if (gNSE) gNSE->SetMarkerColor(kBlack);
  drawOne(2, gNME, "k* [GeV/c];Y_{ME} from S fit", kTRUE, 0.0, 0.0);
  if (gNME) {
    gNME->SetMarkerColor(kRed);
    gNME->SetMarkerStyle(21);
  }
  canvas->cd(3);
  if (gCF) {
    gCF->SetMarkerStyle(20);
    gCF->SetMarkerColor(kBlue + 1);
    gCF->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    gCF->Draw("AP");
    if (gCF->GetHistogram()) {
      gCF->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
      gCF->GetHistogram()->SetTitle(
          Form("C_{raw}=Y_{SE}/Y_{ME} %s %s %s;k* [GeV/c];C_{raw}", channelBase.c_str(), slice.id.c_str(),
               templateTag));
    }
    TLine* one = new TLine(kCfKstarXMin, 1.0, kCfKstarXMax, 1.0);
    one->SetLineStyle(2);
    one->SetLineColor(kGray + 2);
    one->Draw("same");
  } else {
    TLatex* t = new TLatex();
    t->SetNDC(kTRUE);
    t->DrawLatex(0.2, 0.5, "CF raw: no data");
  }
  canvas->cd(4);
  if (gCFn) {
    gCFn->SetMarkerStyle(21);
    gCFn->SetMarkerColor(kAzure + 2);
    gCFn->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    gCFn->Draw("AP");
    if (gCFn->GetHistogram()) {
      gCFn->GetHistogram()->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
      gCFn->GetHistogram()->GetYaxis()->SetRangeUser(0.0, 2.5);
      gCFn->GetHistogram()->SetTitle(
          Form("C_{norm} %s %s %s;k* [GeV/c];C_{norm}", channelBase.c_str(), slice.id.c_str(), templateTag));
    }
    TLine* one = new TLine(kCfKstarXMin, 1.0, kCfKstarXMax, 1.0);
    one->SetLineStyle(2);
    one->SetLineColor(kGray + 2);
    one->Draw("same");
  } else {
    TLatex* t = new TLatex();
    t->SetNDC(kTRUE);
    t->DrawLatex(0.2, 0.5, "CF norm: no data / norm window empty");
  }

  canvas->cd(0);
  TLatex* title = new TLatex();
  title->SetNDC(kTRUE);
  title->SetTextSize(0.024);
  title->SetTextFont(62);
  title->DrawLatex(0.02, 0.985,
                   Form("%s  %s  template=%s  kstarMassFitCF (S=F-#alpha B yields)", channelBase.c_str(),
                        slice.id.c_str(), templateTag));
}

static void drawKstarMassFitCfCachedPages(TCanvas* canvas, const TString& pdfPath,
                                          std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas || !isKstarMassFitCfEnabled()) return;
  const Double_t kstarBinTarget = getKstarMassFitCfKstarBinTarget();
  const std::string suf = kmfCacheSuffix(kstarBinTarget);
  std::vector<std::string> templates;
  getKstarMassFitCfTemplateOrder(templates);
  const char* bases[] = {"phi_proton", "phi_deuteron", 0};
  const char* rebinSliceIds[] = {"pct_0_10", "pct_0_20", "pct_0_30", 0};
  const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
  for (size_t it = 0; it < templates.size(); ++it) {
    const std::string tag = templates[it];
    for (Int_t ib = 0; bases[ib]; ++ib) {
      const std::string base(bases[ib]);
      for (Int_t isl = 0; rebinSliceIds[isl]; ++isl) {
        const FemtoConfig::CfCentSlice* slicePtr = 0;
        for (size_t is = 0; is < allSlices.size(); ++is) {
          if (allSlices[is].id == rebinSliceIds[isl]) {
            slicePtr = &allSlices[is];
            break;
          }
        }
        if (!slicePtr) continue;
        const std::string nSeKey =
            cfSliceCacheKey(slicePtr->id, std::string("kmf_Y_SE_") + tag + "_" + base + suf);
        const std::string nMeKey =
            cfSliceCacheKey(slicePtr->id, std::string("kmf_Y_ME_") + tag + "_" + base + suf);
        const std::string cfKey =
            cfSliceCacheKey(slicePtr->id, std::string("CF_kmf_") + tag + "_" + base + suf + "_raw");
        const std::string cfNormKey =
            cfSliceCacheKey(slicePtr->id, std::string("CF_kmf_") + tag + "_" + base + suf + "_norm");
        TGraphErrors* gNSE = cfCache.count(nSeKey) ? cfCache[nSeKey] : 0;
        TGraphErrors* gNME = cfCache.count(nMeKey) ? cfCache[nMeKey] : 0;
        TGraphErrors* gCF = cfCache.count(cfKey) ? cfCache[cfKey] : 0;
        TGraphErrors* gCFn = cfCache.count(cfNormKey) ? cfCache[cfNormKey] : 0;
        drawKstarMassFitCfPage(canvas, *slicePtr, base, tag.c_str(), gNSE, gNME, gCF, gCFn);
        canvas->Print(pdfPath);
      }
    }
  }
}

static void drawKstarMassFitCfSection(TCanvas* canvas, TFile* fin, const TString& pdfPath,
                                     std::vector<TH1*>& keepAlive, std::map<std::string, TGraphErrors*>& cfCache,
                                     std::map<std::string, Double_t>& metaCache) {
  if (!canvas || !fin || !isKstarMassFitCfEnabled()) return;

  const Double_t kstarBinTarget = getKstarMassFitCfKstarBinTarget();
  if (kstarBinTarget <= 0.0) return;

  Double_t fitMin = 0.99, fitMax = 1.06, sigmaMin = 0.002, sigmaMax = 0.020;
  Double_t purityMinK = 0.0, purityMaxK = 0.65, clampMin = 0.05, clampMax = 1.0;
  Int_t minEntries = 20;
  Bool_t preferPol2 = kTRUE;
  getKstarMassFitCfFitConfig(fitMin, fitMax, sigmaMin, sigmaMax, purityMinK, purityMaxK, minEntries, clampMin, clampMax,
                      preferPol2);
  const std::string suf = kmfCacheSuffix(kstarBinTarget);

  canvas->Clear();
  canvas->SetCanvasSize(1200, 900);
  drawKstarMassFitCfGuidePage(canvas);
  canvas->Print(pdfPath);

  const char* bases[] = {"phi_proton", "phi_deuteron", 0};
  std::vector<std::string> templates;
  getKstarMassFitCfTemplateOrder(templates);
  const char* rebinSliceIds[] = {"pct_0_10", "pct_0_20", "pct_0_30", 0};
  const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();

  for (size_t it = 0; it < templates.size(); ++it) {
    const std::string tag = templates[it];
    for (Int_t ib = 0; bases[ib]; ++ib) {
      const std::string base(bases[ib]);
      for (Int_t isl = 0; rebinSliceIds[isl]; ++isl) {
        const FemtoConfig::CfCentSlice* slicePtr = 0;
        for (size_t is = 0; is < allSlices.size(); ++is) {
          if (allSlices[is].id == rebinSliceIds[isl]) {
            slicePtr = &allSlices[is];
            break;
          }
        }
        if (!slicePtr) continue;

        TH3* h3Fse = (TH3*)fin->Get(phiMkkVsKstarWideSeKey(base).c_str());
        TH3* h3Fme = (TH3*)fin->Get(phiMkkVsKstarWideMeKey(base).c_str());
        TH3* h3Bse = (TH3*)fin->Get(kmfBkgWideKey(base, tag.c_str(), kTRUE).c_str());
        TH3* h3Bme = (TH3*)fin->Get(kmfBkgWideKey(base, tag.c_str(), kFALSE).c_str());
        if (!h3Fse || !h3Fme || !h3Bse || !h3Bme) {
          canvas->Clear();
          canvas->cd(1);
          TLatex* lat = new TLatex();
          lat->SetNDC(kTRUE);
          lat->SetTextSize(0.03);
          lat->DrawLatex(0.08, 0.55,
                         Form("%s %s template=%s: missing wide TH3 (need Maker re-run for MIX)", base.c_str(),
                              slicePtr->id.c_str(), tag.c_str()));
          lat->DrawLatex(0.08, 0.48, Form("F SE=%s ME=%s", phiMkkVsKstarWideSeKey(base).c_str(),
                                         phiMkkVsKstarWideMeKey(base).c_str()));
          lat->DrawLatex(0.08, 0.41, Form("B SE=%s", kmfBkgWideKey(base, tag.c_str(), kTRUE).c_str()));
          lat->DrawLatex(0.08, 0.34, Form("B ME=%s", kmfBkgWideKey(base, tag.c_str(), kFALSE).c_str()));
          canvas->Print(pdfPath);
          continue;
        }

        TH2* h2Fse = projectMkkVsKstarForSlice(h3Fse, slicePtr->cent9Min, slicePtr->cent9Max, "_kmfFse");
        TH2* h2Fme = projectMkkVsKstarForSlice(h3Fme, slicePtr->cent9Min, slicePtr->cent9Max, "_kmfFme");
        TH2* h2Bse = projectMkkVsKstarForSlice(h3Bse, slicePtr->cent9Min, slicePtr->cent9Max, "_kmfBse");
        TH2* h2Bme = projectMkkVsKstarForSlice(h3Bme, slicePtr->cent9Min, slicePtr->cent9Max, "_kmfBme");
        if (!h2Fse || !h2Fme || !h2Bse || !h2Bme) {
          delete h2Fse;
          delete h2Fme;
          delete h2Bse;
          delete h2Bme;
          continue;
        }
        if (h2Fse->GetNbinsY() != h2Fme->GetNbinsY() || h2Fse->GetNbinsY() != h2Bse->GetNbinsY() ||
            h2Fse->GetNbinsY() != h2Bme->GetNbinsY()) {
          std::cerr << "[checkHistAnaFemtoPhi] ERROR: kstarMassFitCF SE/ME TH2 k* binning mismatch for " << base
                    << " " << slicePtr->id << " template=" << tag << std::endl;
          delete h2Fse;
          delete h2Fme;
          delete h2Bse;
          delete h2Bme;
          continue;
        }
        const Int_t kRebin = kmfRebinFactorForWidth(h2Fse->GetYaxis()->GetBinWidth(1), kstarBinTarget);
        if (kRebin > 1) {
          h2Fse = (TH2*)h2Fse->RebinY(kRebin);
          h2Fme = (TH2*)h2Fme->RebinY(kRebin);
          h2Bse = (TH2*)h2Bse->RebinY(kRebin);
          h2Bme = (TH2*)h2Bme->RebinY(kRebin);
        }

        std::vector<Double_t> kx, ySE, eSE, yME, eME, cfx, cfy, cfe, stx, sty;
        canvas->SetCanvasSize(2400, 1000);
        const Int_t lowKstarMergeBins = getKstarMassFitCfLowKstarMergeBins();
        for (Int_t iy = 1; iy <= h2Fse->GetNbinsY(); ++iy) {
          const Int_t iyFirst = iy;
          const Int_t iyLast =
              (iyFirst == 1) ? TMath::Min(h2Fse->GetNbinsY(), lowKstarMergeBins) : iyFirst;
          iy = iyLast;
          const Double_t kstar =
              0.5 * (h2Fse->GetYaxis()->GetBinLowEdge(iyFirst) + h2Fse->GetYaxis()->GetBinUpEdge(iyLast));
          if (kstar < purityMinK || kstar > purityMaxK) continue;
          Double_t nF = h2Fse->Integral(1, h2Fse->GetNbinsX(), iyFirst, iyLast) +
                        h2Fme->Integral(1, h2Fme->GetNbinsX(), iyFirst, iyLast);
          Int_t stSE = kKmfStatusFitFail;
          Int_t stME = kKmfStatusFitFail;
          Double_t nse = 0.0, ese = 0.0, nme = 0.0, eme = 0.0;
          const Bool_t lowStat = (nF < (Double_t)minEntries);
          if (nF <= 0.0) {
            stSE = kKmfStatusLowStat;
            stME = kKmfStatusLowStat;
            stx.push_back(kstar);
            sty.push_back((Double_t)kKmfStatusLowStat);
            continue;
          }
          drawKstarMassFitCfKstarPage(canvas, fin, *slicePtr, base, tag.c_str(), iyFirst, iyLast, kstar, h2Fse,
                                      h2Fme, h2Bse, h2Bme, kstarBinTarget, keepAlive, &nse, &ese, &nme, &eme, &stSE,
                                      &stME);
          canvas->Print(pdfPath);
          if (lowStat) {
            stSE = kKmfStatusLowStat;
            stME = kKmfStatusLowStat;
          }
          stx.push_back(kstar);
          const Int_t stPair = (stSE != kKmfStatusOk) ? stSE : stME;
          sty.push_back((Double_t)stPair);
          if (stSE != kKmfStatusOk || stME != kKmfStatusOk) continue;
          if (!(nme > 0.0) || !TMath::Finite(nse) || !TMath::Finite(nme) || !TMath::Finite(ese) ||
              !TMath::Finite(eme)) {
            sty.back() = (Double_t)kKmfStatusNonFinite;
            continue;
          }
          const Double_t cf = nse / nme;
          const Double_t ecf =
              cf * TMath::Sqrt(TMath::Power(ese / (nse + 1e-12), 2) + TMath::Power(eme / (nme + 1e-12), 2));
          if (!TMath::Finite(cf) || !TMath::Finite(ecf)) {
            sty.back() = (Double_t)kKmfStatusNonFinite;
            continue;
          }
          kx.push_back(kstar);
          ySE.push_back(nse);
          eSE.push_back(ese);
          yME.push_back(nme);
          eME.push_back(eme);
          cfx.push_back(kstar);
          cfy.push_back(cf);
          cfe.push_back(ecf);
        }

        const std::string nSeKey =
            cfSliceCacheKey(slicePtr->id, std::string("kmf_Y_SE_") + tag + "_" + base + suf);
        const std::string nMeKey =
            cfSliceCacheKey(slicePtr->id, std::string("kmf_Y_ME_") + tag + "_" + base + suf);
        const std::string cfKey =
            cfSliceCacheKey(slicePtr->id, std::string("CF_kmf_") + tag + "_" + base + suf + "_raw");
        const std::string cfNormKey =
            cfSliceCacheKey(slicePtr->id, std::string("CF_kmf_") + tag + "_" + base + suf + "_norm");
        const std::string stKey =
            cfSliceCacheKey(slicePtr->id, std::string("kmf_fitstatus_") + tag + "_" + base + suf);

        TGraphErrors* gNSE = 0;
        TGraphErrors* gNME = 0;
        TGraphErrors* gCF = 0;
        TGraphErrors* gCFn = 0;
        if (!stx.empty()) {
          TGraphErrors* gSt = new TGraphErrors((Int_t)stx.size(), &stx[0], &sty[0], 0, 0);
          gSt->SetTitle(Form("kstarMassFitCF fit status %s %s %s", tag.c_str(), base.c_str(), slicePtr->id.c_str()));
          cfCache[stKey] = gSt;
        } else {
          cfCache[stKey] = 0;
        }
        if (!kx.empty()) {
          gNSE = new TGraphErrors((Int_t)kx.size(), &kx[0], &ySE[0], 0, &eSE[0]);
          gNSE->SetTitle(Form("Y_{SE}^{S}(k*) kmf-%s %s %s", tag.c_str(), base.c_str(), slicePtr->id.c_str()));
          cfCache[nSeKey] = gNSE;
          gNME = new TGraphErrors((Int_t)kx.size(), &kx[0], &yME[0], 0, &eME[0]);
          gNME->SetTitle(Form("Y_{ME}^{S}(k*) kmf-%s %s %s", tag.c_str(), base.c_str(), slicePtr->id.c_str()));
          cfCache[nMeKey] = gNME;
          gCF = new TGraphErrors((Int_t)cfx.size(), &cfx[0], &cfy[0], 0, &cfe[0]);
          gCF->SetTitle(Form("CF_kmf %s %s %s", tag.c_str(), base.c_str(), slicePtr->id.c_str()));
          cfCache[cfKey] = gCF;

          const std::string chSig = channelSignal(base);
          const Double_t nQMin = channelNormQMin(chSig);
          const Double_t nQMax = channelNormQMax(chSig);
          Double_t sumC = 0.0;
          Double_t sumE2 = 0.0;
          Int_t nNorm = 0;
          for (size_t i = 0; i < cfx.size(); ++i) {
            if (cfx[i] < nQMin || cfx[i] > nQMax) continue;
            if (!TMath::Finite(cfy[i]) || !TMath::Finite(cfe[i])) continue;
            sumC += cfy[i];
            sumE2 += cfe[i] * cfe[i];
            ++nNorm;
          }
          if (nNorm > 0 && sumC > 0.0 && TMath::Finite(sumC)) {
            const Double_t mean = sumC / (Double_t)nNorm;
            const Double_t errMean = TMath::Sqrt(sumE2) / (Double_t)nNorm;
            const Double_t scale = 1.0 / mean;
            const Double_t errScale = errMean / (mean * mean);
            std::vector<Double_t> ny, ne;
            for (size_t i = 0; i < cfy.size(); ++i) {
              ny.push_back(cfy[i] * scale);
              ne.push_back(TMath::Sqrt(TMath::Power(cfe[i] * scale, 2) + TMath::Power(cfy[i] * errScale, 2)));
            }
            gCFn = new TGraphErrors((Int_t)cfx.size(), &cfx[0], &ny[0], 0, &ne[0]);
            gCFn->SetTitle(Form("CF_{norm} kmf-%s %s %s", tag.c_str(), base.c_str(), slicePtr->id.c_str()));
            cfCache[cfNormKey] = gCFn;
            metaCache[cfSliceCacheKey(slicePtr->id, std::string("kmf_normScale_") + tag + "_" + base + suf)] = scale;
            metaCache[cfSliceCacheKey(slicePtr->id, std::string("kmf_normScaleErr_") + tag + "_" + base + suf)] =
                errScale;
          } else {
            cfCache[cfNormKey] = 0;
            metaCache[cfSliceCacheKey(slicePtr->id, std::string("kmf_normFail_") + tag + "_" + base + suf)] =
                (Double_t)kKmfStatusNormFail;
          }
        } else {
          cfCache[nSeKey] = 0;
          cfCache[nMeKey] = 0;
          cfCache[cfKey] = 0;
          cfCache[cfNormKey] = 0;
        }
        metaCache[cfSliceCacheKey(slicePtr->id, std::string("kmf_nOk_") + tag + "_" + base + suf)] =
            (Double_t)cfx.size();

        drawKstarMassFitCfPage(canvas, *slicePtr, base, tag.c_str(), gNSE, gNME, gCF, gCFn);
        canvas->Print(pdfPath);

        delete h2Fse;
        delete h2Fme;
        delete h2Bse;
        delete h2Bme;
      }
    }
  }
  canvas->SetCanvasSize(1200, 800);
  (void)minEntries;
  (void)preferPol2;
}


static void writeKstarMassFitCfSidecarRoot(const TString& outDir, const TString& anaName, const TString& jobid,
                                    const Char_t* inputRootFile, const Char_t* mainconfPath,
                                    std::map<std::string, TGraphErrors*>& cfCache,
                                    const std::map<std::string, Double_t>& metaCache) {
  if (!isKstarMassFitCfWriteSidecar()) return;

  TString outPath = outDir + anaName + "_checkHistAnaFemtoPhi_CFkmf";
  if (jobid.Length()) outPath += "_" + jobid;
  outPath += ".root";

  TFile* fout = TFile::Open(outPath, "RECREATE");
  if (!fout || fout->IsZombie()) {
    std::cerr << "[checkHistAnaFemtoPhi] WARNING: cannot write kstarMassFitCF sidecar " << outPath << std::endl;
    return;
  }

  for (std::map<std::string, TGraphErrors*>::iterator it = cfCache.begin(); it != cfCache.end(); ++it) {
    if (!it->second) continue;
    const std::string& key = it->first;
    if (key.find("CF_kmf_") == std::string::npos && key.find("kmf_") == std::string::npos) continue;
    TGraphErrors* clone = (TGraphErrors*)it->second->Clone(sanitizeGraphName(key));
    if (clone) {
      clone->Write();
      delete clone;
    }
  }
  for (std::map<std::string, Double_t>::const_iterator it = metaCache.begin(); it != metaCache.end(); ++it) {
    if (it->first.find("kmf_") == std::string::npos) continue;
    TParameter<Double_t> p(sanitizeGraphName(it->first), it->second);
    p.Write();
  }

  TNamed metaInput("meta_inputRoot", inputRootFile ? inputRootFile : "");
  metaInput.Write();
  TNamed metaMain("meta_mainconf", mainconfPath ? mainconfPath : "");
  metaMain.Write();
  TNamed metaJob("meta_jobid", jobid.Data());
  metaJob.Write();
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    TNamed metaEn("meta_kstarMassFitCfEnabled", fc.kstarMassFitCfEnabled ? "true" : "false");
    metaEn.Write();
    TNamed metaTpl("meta_kstarMassFitCfTemplate", fc.kstarMassFitCfTemplate.c_str());
    metaTpl.Write();
    TNamed metaX("meta_kstarMassFitCfCrossCheck", fc.kstarMassFitCfCrossCheck ? "true" : "false");
    metaX.Write();
    TParameter<Double_t> pMin("meta_kstarMassFitCfFitMassMin", fc.kstarMassFitCfFitMassMin);
    pMin.Write();
    TParameter<Double_t> pMax("meta_kstarMassFitCfFitMassMax", fc.kstarMassFitCfFitMassMax);
    pMax.Write();
    TParameter<Double_t> pDk("meta_kstarMassFitCfKstarBinWidth", fc.kstarMassFitCfKstarBinWidth);
    pDk.Write();
    TParameter<Double_t> pA0("meta_kstarMassFitCfAlphaMassMin", fc.kstarMassFitCfAlphaMassMin);
    pA0.Write();
    TParameter<Double_t> pA1("meta_kstarMassFitCfAlphaMassMax", fc.kstarMassFitCfAlphaMassMax);
    pA1.Write();
  }
  fout->Close();
  delete fout;
  std::cout << "Done. kstarMassFitCF sidecar: " << outPath << std::endl;
}

static void drawCfSubMethod5GuidePage(TCanvas* canvas) {
  if (!canvas) return;
  canvas->Clear();
  canvas->cd(1);
  gPad->SetMargin(0.08, 0.05, 0.05, 0.05);

  TLatex* title = new TLatex();
  title->SetNDC(kTRUE);
  title->SetTextSize(0.040);
  title->SetTextFont(62);
  title->DrawLatex(0.06, 0.94, "How to read method-5 CF-subtraction pages (4 panels)");

  TLatex* t = new TLatex();
  t->SetNDC(kTRUE);
  t->SetTextSize(0.028);
  Double_t y = 0.88;
  const Double_t dy = 0.038;

  t->DrawLatex(0.06, y, "Each following page is one centrality slice x bachelor base (p, d, t, ^{3}He, ^{4}He).");
  y -= dy;
  t->DrawLatex(0.06, y, "Slices shown: cfCentSlicesQaPdfInclude (default pct_0_10 / pct_0_20 / pct_0_30).");
  y -= dy * 1.2;

  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Formula (method 5):");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "C_{sig} = norm(SE_{sig}/ME_{sig}),   C_{SB} = norm(SE_{SB}/ME_{SB})");
  y -= dy;
  t->DrawLatex(0.08, y, "C_{CFsub} = [C_{sig} - (1-P) C_{SB}] / P");
  y -= dy;
  t->DrawLatex(0.08, y, "P = #phi M_{KK} purity in the signal window (slice-constant; fit_slice or fixed YAML).");
  y -= dy * 1.2;

  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Panel layout (2 x 2):");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "Top-left:   C_{sig} (black) vs C_{SB,SBLR} (red)  -- signal vs combined sideband CF");
  y -= dy;
  t->DrawLatex(0.08, y, "Top-right:  C_{sig} (black) vs C_{CFsub} (red)     -- raw vs purity-corrected CF");
  y -= dy;
  t->DrawLatex(0.08, y, "Bottom-left: C_{SB,L} vs C_{SB,R}                 -- left/right sideband CF shape check");
  y -= dy;
  t->DrawLatex(0.08, y, "Bottom-right: C_{CFsub,SBL} vs C_{CFsub,SBR}       -- L/R corrected CF systematic");
  y -= dy * 1.2;

  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Definitions:");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "SBLR = SE_{leftSB}+SE_{rightSB} and ME_{leftSB}+ME_{rightSB} summed before CF (no width #alpha).");
  y -= dy;
  t->DrawLatex(0.08, y, "norm: same normQ range for C_{sig} and C_{SB} (maker YAML, default 0.5-1.0 GeV/c).");
  y -= dy;
  t->DrawLatex(0.08, y, "Centrality: raw SE/ME counts summed over cent9 range, then CF built (not CF average).");
  y -= dy;
  t->DrawLatex(0.08, y, "Errors: Poisson-style on SE/ME bins; P uncertainty included when available.");
  y -= dy * 1.2;

  t->SetTextColor(kRed + 1);
  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "Not the same as:");
  t->SetTextFont(42);
  y -= dy;
  t->DrawLatex(0.08, y, "CF_sig_sub_* pages = count-level sideband subtraction before CF.");
  y -= dy;
  t->DrawLatex(0.08, y, "C_{genuine} pages (Topic 3) = ME-mass background CF + k*-dependent #lambda_{sig}(k*).");
  y -= dy * 1.2;

  t->SetTextColor(kBlack);
  t->SetTextSize(0.024);
  t->DrawLatex(0.06, y,
               "Header line on each page: <base> method5 CF-sub <slice> cent9[min,max] P=<value> rebin=<factor>");
  y -= dy * 0.9;
  t->DrawLatex(0.06, y, "Sidecar ROOT: <anaName>_checkHistAnaFemtoPhi_CFsub_<jobid>.root (graphs + P + meta).");
}

static void drawCfSubMethod5SlicePageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                             const std::string& channelBase,
                                             std::map<std::string, TGraphErrors*>& cfCache,
                                             std::map<std::string, Double_t>& purityCache) {
  if (!canvas) return;
  canvas->Clear();
  canvas->Divide(2, 2);
  const std::string chSig = channelSignal(channelBase);
  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);
  const Int_t rebinFactor = getCfSubEffectiveRebin(channelBase);

  TGraphErrors* gSig = getOrComputeSliceSigCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                        normQMin, normQMax, rebinFactor, cfCache);
  TGraphErrors* gSB = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                      "SBLR", normQMin, normQMax, rebinFactor, cfCache);
  TGraphErrors* gSBL = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                       channelLeftSb(channelBase), normQMin, normQMax, rebinFactor,
                                                       cfCache);
  TGraphErrors* gSBR = getOrComputeSliceSbCfForMethod5(fin, slice.id, slice.cent9Min, slice.cent9Max, channelBase,
                                                       channelRightSb(channelBase), normQMin, normQMax, rebinFactor,
                                                       cfCache);
  TGraphErrors* gSub = computeCfSubMethod5Graph(fin, slice, channelBase, normQMin, normQMax, cfCache, purityCache);

  canvas->cd(1);
  drawCfGraphOverlay(gSig, gSB, "C_{sig}", "C_{SB} (SBLR)");
  canvas->cd(2);
  drawCfGraphOverlay(gSig, gSub, "C_{sig}", "C_{CFsub}");
  canvas->cd(3);
  drawCfGraphOverlay(gSBL, gSBR, "C_{SB,L}", "C_{SB,R}");
  canvas->cd(4);
  TGraphErrors* gSubL = 0;
  TGraphErrors* gSubR = 0;
  std::map<std::string, TGraphErrors*>::const_iterator itL =
      cfCache.find(cfSliceCacheKey(slice.id, std::string("CF_CFsub_SBL_") + channelBase));
  std::map<std::string, TGraphErrors*>::const_iterator itR =
      cfCache.find(cfSliceCacheKey(slice.id, std::string("CF_CFsub_SBR_") + channelBase));
  if (itL != cfCache.end()) gSubL = itL->second;
  if (itR != cfCache.end()) gSubR = itR->second;
  drawCfGraphOverlay(gSubL, gSubR, "C_{CFsub,SBL}", "C_{CFsub,SBR}");

  Double_t P = 0.0;
  std::map<std::string, Double_t>::const_iterator itP =
      purityCache.find(cfSliceCacheKey(slice.id, std::string("phi_mass_purity_") + channelBase));
  if (itP != purityCache.end()) P = itP->second;
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.028);
    lat->DrawLatex(0.02, 0.97,
                   Form("%s method5 CF-sub %s cent9[%d,%d] P=%.3f rebin=%d", channelBase.c_str(), slice.id.c_str(),
                        slice.cent9Min, slice.cent9Max, P, rebinFactor));
  }
}

static void drawLambdaSigSlicePageForBase(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                          const std::string& channelBase, std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->Clear();
  canvas->Divide(1, 1);
  canvas->cd(1);
  const std::string lamKey = cfSliceCacheKey(slice.id, std::string("lambda_sig_") + channelBase);
  std::map<std::string, TGraphErrors*>::const_iterator it = cfCache.find(lamKey);
  if (it != cfCache.end() && it->second) {
    TGraphErrors* g = it->second;
    g->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
    g->GetHistogram()->GetYaxis()->SetRangeUser(0.0, 1.05);
    g->Draw("AP");
  } else {
    TGraphErrors* gLam = computeLambdaSigGraph(fin, slice, channelBase);
    if (gLam) {
      cfCache[lamKey] = gLam;
      gLam->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
      gLam->Draw("AP");
    }
  }
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.12, 0.92,
                   Form("#lambda_{sig}(k^{*}) %s %s (cent9 %d-%d)", channelBase.c_str(), slice.id.c_str(),
                        slice.cent9Min, slice.cent9Max));
  }
}

static const FemtoConfig::CfCentSlice* findCfCentSliceById(const std::vector<FemtoConfig::CfCentSlice>& slices,
                                                             const char* id) {
  if (!id) return 0;
  for (size_t i = 0; i < slices.size(); ++i) {
    if (slices[i].id == id) return &slices[i];
  }
  return 0;
}

// Scale ME M_KK overlay using k* norm-band integrals from M_KK vs k* slice projections.
static Double_t computeMeMkkOverlayScaleFromMkkKstar(TH2* h2SE, TH2* h2ME, Double_t normQMin, Double_t normQMax) {
  if (!h2SE || !h2ME) return 1.0;
  Int_t kyLo = h2SE->GetYaxis()->FindBin(normQMin + 1e-9);
  Int_t kyHi = h2SE->GetYaxis()->FindBin(normQMax - 1e-9);
  Double_t seNorm = h2SE->Integral(1, h2SE->GetNbinsX(), kyLo, kyHi);
  Double_t meNorm = h2ME->Integral(1, h2ME->GetNbinsX(), kyLo, kyHi);
  if (seNorm <= 0.0 || meNorm <= 0.0) return 1.0;
  return seNorm / meNorm;
}

static Double_t drawPhiMkkSeMeCentOverlayPad(TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                              const std::string& channelBase, std::vector<TH1*>& keepAlive) {
  const std::string chSig = channelSignal(channelBase);
  TH3* h3SE = (TH3*)fin->Get(phiMkkVsKstarSeKey(chSig).c_str());
  TH3* h3ME = (TH3*)fin->Get(phiMkkVsKstarMeKey(chSig).c_str());
  if (!h3SE || !h3ME) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.035);
    lat->DrawLatex(0.15, 0.5, Form("missing TH3: %s", channelBase.c_str()));
    return -1.0;
  }

  TH2* h2SE = projectMkkVsKstarForSlice(h3SE, slice.cent9Min, slice.cent9Max, "_mkkse_ov");
  TH2* h2ME = projectMkkVsKstarForSlice(h3ME, slice.cent9Min, slice.cent9Max, "_mkkme_ov");
  if (!h2SE || !h2ME) {
    delete h2SE;
    delete h2ME;
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.035);
    lat->DrawLatex(0.15, 0.5, Form("projection failed: %s", channelBase.c_str()));
    return -1.0;
  }

  const Double_t normQMin = channelNormQMin(chSig);
  const Double_t normQMax = channelNormQMax(chSig);
  const Double_t meScale = computeMeMkkOverlayScaleFromMkkKstar(h2SE, h2ME, normQMin, normQMax);

  TH1* hSE = h2SE->ProjectionX(Form("_mkk_xse_%s", channelBase.c_str()), 1, h2SE->GetNbinsY());
  TH1* hME = h2ME->ProjectionX(Form("_mkk_xme_%s", channelBase.c_str()), 1, h2ME->GetNbinsY());
  delete h2SE;
  delete h2ME;
  if (!hSE || !hME) {
    delete hSE;
    delete hME;
    return -1.0;
  }
  hSE->SetDirectory(0);
  hME->SetDirectory(0);
  keepAlive.push_back(hSE);
  keepAlive.push_back(hME);

  TH1* hMEs = (TH1*)hME->Clone(Form("_mkk_mes_%s", channelBase.c_str()));
  hMEs->SetDirectory(0);
  hMEs->Scale(meScale);
  keepAlive.push_back(hMEs);

  hSE->SetLineColor(kBlack);
  hSE->SetLineWidth(2);
  hSE->GetXaxis()->SetRangeUser(0.99, 1.06);
  hSE->SetTitle(Form("M_{KK} %s;M_{KK} [GeV/c^{2}];Counts", channelBase.c_str()));
  hSE->Draw("HIST");
  hMEs->SetLineColor(kRed);
  hMEs->SetLineStyle(2);
  hMEs->SetLineWidth(2);
  hMEs->Draw("HIST SAME");

  Double_t sigMin = 1.01;
  Double_t sigMax = 1.03;
  getChannelSignalMassWindow(chSig, sigMin, sigMax);
  if (gPad) {
    Double_t ylo = gPad->GetUymin();
    Double_t yhi = gPad->GetUymax();
    TLine* lLo = new TLine(sigMin, ylo, sigMin, yhi);
    lLo->SetLineColor(kGreen + 2);
    lLo->SetLineStyle(2);
    lLo->Draw("same");
    TLine* lHi = new TLine(sigMax, ylo, sigMax, yhi);
    lHi->SetLineColor(kGreen + 2);
    lHi->SetLineStyle(2);
    lHi->Draw("same");

    TLegend* leg = new TLegend(0.52, 0.68, 0.88, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.03);
    leg->AddEntry(hSE, "Same Event", "l");
    leg->AddEntry(hMEs, Form("Mixed Event (x%.3g)", meScale), "l");
    leg->Draw();

    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.045);
    const BachelorQaSpec* spec = findBachelorSpecByBase(channelBase.c_str());
    lat->DrawLatex(0.14, 0.84, spec ? spec->label : channelBase.c_str());
    lat->SetTextSize(0.028);
    lat->DrawLatex(0.14, 0.76, Form("norm k*: %.2f-%.2f GeV/c", normQMin, normQMax));
  }
  return meScale;
}

static void drawPhiMkkSeMeCent010SummaryPage(TCanvas* canvas, TFile* fin, std::vector<TH1*>& keepAlive) {
  if (!canvas || !fin) return;
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  const FemtoConfig::CfCentSlice* slice = findCfCentSliceById(slices, "pct_0_10");
  if (!slice) {
    std::cout << "[checkHistAnaFemtoPhi] WARNING: pct_0_10 slice not found; skip phi M_KK SE/ME summary page\n";
    return;
  }

  canvas->Clear();
  canvas->Divide(2, 3);
  std::cout << "[checkHistAnaFemtoPhi] phi M_KK SE/ME overlay page: " << slice->id << " cent9[" << slice->cent9Min
            << "," << slice->cent9Max << "]\n";

  Double_t scales[5] = {-1.0, -1.0, -1.0, -1.0, -1.0};
  Int_t nBases = 0;
  for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
    canvas->cd(ib + 1);
    scales[ib] = drawPhiMkkSeMeCentOverlayPad(fin, *slice, std::string(kChannelBases[ib]), keepAlive);
    ++nBases;
  }

  canvas->cd(6);
  gPad->SetMargin(0.08, 0.05, 0.05, 0.05);
  TLatex* t = new TLatex();
  t->SetNDC(kTRUE);
  t->SetTextFont(62);
  t->SetTextSize(0.038);
  t->DrawLatex(0.06, 0.92, "#phi M_{KK}: SE vs scaled ME (pct_0_10)");
  t->SetTextFont(42);
  t->SetTextSize(0.028);
  Double_t y = 0.84;
  const Double_t dy = 0.055;
  t->DrawLatex(0.06, y, Form("Centrality slice %s (cent9 %d-%d)", slice->id.c_str(), slice->cent9Min, slice->cent9Max));
  y -= dy;
  t->DrawLatex(0.06, y, "Source: hPhiMKK_vs_KstarSE/ME_<channel>_signal TH3; sum over k^{*}, cent9 range.");
  y -= dy;
  t->DrawLatex(0.06, y, "ME scale = integral(SE)/integral(ME) in k^{*} norm band (same as CF overlay).");
  y -= dy;
  t->DrawLatex(0.06, y, "Maker fills TH3 only inside signal mass window; dashed green = signal limits.");
  y -= dy * 1.2;
  t->SetTextFont(62);
  t->DrawLatex(0.06, y, "ME scale factors:");
  t->SetTextFont(42);
  y -= dy;
  for (Int_t ib = 0; ib < nBases; ++ib) {
    const BachelorQaSpec* spec = findBachelorSpecByBase(kChannelBases[ib]);
    t->DrawLatex(0.08, y,
                 Form("%s: %s", spec ? spec->label : kChannelBases[ib],
                      scales[ib] > 0.0 ? Form("x%.4f", scales[ib]) : "n/a"));
    y -= dy * 0.9;
  }
}

static void drawPhiPairMomAngleMkkLines(const FemtoConfig::ChannelDef* ch) {
  if (!ch || !gPad) return;
  Double_t xlo = gPad->GetUxmin();
  Double_t xhi = gPad->GetUxmax();
  TLine* lLo = new TLine(xlo, ch->signalMin, xhi, ch->signalMin);
  lLo->SetLineColor(kRed);
  lLo->SetLineStyle(2);
  lLo->Draw("same");
  TLine* lHi = new TLine(xlo, ch->signalMax, xhi, ch->signalMax);
  lHi->SetLineColor(kRed);
  lHi->SetLineStyle(2);
  lHi->Draw("same");
}

static void drawPhiBachelorPairAngleQaPage(TCanvas* canvas, TFile* fin, TString pdfName,
                                           const BachelorQaSpec& spec) {
  if (!canvas || !fin) return;
  TH1* h1 = 0;
  TH2* h2 = 0;
  canvas->Clear();
  canvas->Divide(2, 3);

  TString kLoose1d = phiPairMomAngleKey(spec.channelBase, kFALSE, kFALSE);
  TString kStrict1d = phiPairMomAngleKey(spec.channelBase, kFALSE, kTRUE);
  TString kLoose2d = phiPairMomAngleKey(spec.channelBase, kTRUE, kFALSE);
  TString kStrict2d = phiPairMomAngleKey(spec.channelBase, kTRUE, kTRUE);
  TString kPreMass2d = phiPairMomAngleKeyWithSuffix(spec.channelBase, kTRUE, "_preMass");
  TString kPreMassStrict2d = phiPairMomAngleKeyWithSuffix(spec.channelBase, kTRUE, "_preMass_tofStrict");

  const FemtoConfig::ChannelDef* chSig = 0;
  if (gConfigLoaded) {
    chSig = ConfigManager::GetInstance().GetFemtoConfig().FindChannel(channelSignal(spec.channelBase));
  }

  canvas->cd(1);
  h1 = (TH1*)fin->Get(kLoose1d);
  TH1* hStrict1d = (TH1*)fin->Get(kStrict1d);
  if (h1) {
    h1->SetLineColor(kBlack);
    h1->SetLineWidth(2);
    h1->Draw("HIST");
  }
  if (hStrict1d) {
    hStrict1d->SetLineColor(kRed);
    hStrict1d->SetLineStyle(2);
    hStrict1d->SetLineWidth(2);
    if (h1) {
      hStrict1d->Draw("HIST SAME");
    } else {
      hStrict1d->Draw("HIST");
    }
  }
  if (h1 || hStrict1d) {
    TLegend* leg = new TLegend(0.55, 0.72, 0.88, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    if (h1) leg->AddEntry(h1, "signal (no TOF strict)", "l");
    if (hStrict1d) leg->AddEntry(hStrict1d, "signal tofStrict", "l");
    leg->Draw();
  }

  canvas->cd(2);
  h2 = (TH2*)fin->Get(kLoose2d);
  if (h2) {
    h2->Draw("colz");
    drawPhiPairMomAngleMkkLines(chSig);
  }

  canvas->cd(3);
  h2 = (TH2*)fin->Get(kStrict2d);
  if (h2) {
    h2->Draw("colz");
    drawPhiPairMomAngleMkkLines(chSig);
  }

  canvas->cd(4);
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.035);
    lat->DrawLatex(0.05, 0.85, Form("#phi-%s pair momentum angle QA", spec.label));
    lat->DrawLatex(0.05, 0.78, Form("channel: %s_signal", spec.channelBase));
    if (chSig) {
      lat->DrawLatex(0.05, 0.71,
                     Form("signal M_{KK}: %.3f - %.3f GeV/c^{2}", chSig->signalMin, chSig->signalMax));
    }
    lat->DrawLatex(0.05, 0.64, "Pad1: #theta_{p} (black=no TOF strict, red=tofStrict)");
    lat->DrawLatex(0.05, 0.57, "Pad2/3: signal-window #theta_{p} vs M_{KK}; CF pipeline unchanged");
    lat->DrawLatex(0.05, 0.50, "Pad5/6: preMass = all M_{KK}, no KK opening/rapidity cut");
  }

  canvas->cd(5);
  h2 = (TH2*)fin->Get(kPreMass2d);
  if (h2) {
    h2->Draw("colz");
  }

  canvas->cd(6);
  h2 = (TH2*)fin->Get(kPreMassStrict2d);
  if (h2) {
    h2->Draw("colz");
  }

  canvas->Print(pdfName);
}

static void drawBachelorFemtoQaPages(TCanvas* canvas, TFile* fin, TString pdfName, const BachelorQaSpec& spec) {
  if (!canvas || !fin) return;
  TH1* h1 = 0;
  TH2* h2 = 0;
  BachelorCuts cuts;
  if (gConfigLoaded) {
    cuts = getBachelorCuts(ConfigManager::GetInstance().GetFemtoConfig(), spec.cutPrefix);
  }

  // Page 11a: bachelor QA 1D (pre-femto cut)
  canvas->Clear();
  canvas->Divide(3, 2);
  TString kPtPre = bachelorHistKey(spec, "Pt_PreFemtoCut");
  TString kEtaPre = bachelorHistKey(spec, "Eta_PreFemtoCut");
  TString kNsPre;
  if (strcmp(spec.cutPrefix, "proton") == 0) kNsPre = bachelorHistKey(spec, "NSigmaProton_PreFemtoCut");
  else if (strcmp(spec.cutPrefix, "deuteron") == 0) kNsPre = bachelorHistKey(spec, "NSigmaDeuteron_PreFemtoCut");
  else if (strcmp(spec.cutPrefix, "triton") == 0) kNsPre = bachelorHistKey(spec, "NSigmaTriton_PreFemtoCut");
  else if (strcmp(spec.cutPrefix, "he3") == 0) kNsPre = bachelorHistKey(spec, "NSigmaHe3_PreFemtoCut");
  else if (strcmp(spec.cutPrefix, "he4") == 0) kNsPre = bachelorHistKey(spec, "NSigmaHe4_PreFemtoCut");
  TString kM2Pre = bachelorHistKey(spec, "Mass2_PreFemtoCut");
  TString kDcaPre = bachelorHistKey(spec, "DCA_PreFemtoCut");
  if (gConfigLoaded) {
    canvas->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get(kPtPre); if (h1) {
      prepareBachelorHist(h1, kPtPre, spec); h1->Draw();
      drawCutLine1D(h1, cuts.minPtPre);
      if (cuts.hasMaxPtPre) drawCutLine1D(h1, cuts.maxPtPre);
      drawCutLine1D(h1, cuts.minPtPair);
      drawCutLine1D(h1, cuts.maxPtPair);
    }
    canvas->cd(2); h1 = (TH1*)fin->Get(kEtaPre); if (h1) {
      prepareBachelorHist(h1, kEtaPre, spec); h1->Draw();
      drawCutLines1D(h1, -cuts.maxAbsEta, cuts.maxAbsEta);
    }
    canvas->cd(3); h1 = (TH1*)fin->Get(kNsPre); if (h1) {
      prepareBachelorHist(h1, kNsPre, spec); h1->Draw();
      drawCutLines1D(h1, -cuts.maxAbsNSigma, cuts.maxAbsNSigma);
    }
    canvas->cd(4); gPad->SetLogy(); h1 = (TH1*)fin->Get(kM2Pre); if (h1) {
      prepareBachelorHist(h1, kM2Pre, spec); h1->Draw();
      drawCutLines1D(h1, cuts.minMass2, cuts.maxMass2);
    }
    canvas->cd(5); gPad->SetLogy(); h1 = (TH1*)fin->Get(kDcaPre); if (h1) {
      prepareBachelorHist(h1, kDcaPre, spec); h1->Draw();
      drawCutLine1D(h1, cuts.maxDca);
    }
    canvas->cd(6);
  } else {
    canvas->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get(kPtPre);
    if (h1) { prepareBachelorHist(h1, kPtPre, spec); h1->Draw(); }
    canvas->cd(2); h1 = (TH1*)fin->Get(kEtaPre);
    if (h1) { prepareBachelorHist(h1, kEtaPre, spec); h1->Draw(); }
    canvas->cd(3); h1 = (TH1*)fin->Get(kNsPre);
    if (h1) { prepareBachelorHist(h1, kNsPre, spec); h1->Draw(); }
    canvas->cd(4); gPad->SetLogy(); h1 = (TH1*)fin->Get(kM2Pre);
    if (h1) { prepareBachelorHist(h1, kM2Pre, spec); h1->Draw(); }
    canvas->cd(5); gPad->SetLogy(); h1 = (TH1*)fin->Get(kDcaPre);
    if (h1) { prepareBachelorHist(h1, kDcaPre, spec); h1->Draw(); }
    canvas->cd(6);
  }
  if (gPad) {
    TLatex* tag = new TLatex();
    tag->SetNDC(kTRUE);
    tag->SetTextSize(0.03);
    tag->DrawLatex(0.02, 0.98, Form("%s bachelor pre-femto QA (%s)", spec.label, spec.channelBase));
  }
  canvas->Print(pdfName);

  // Page 11b: post-femto 1D
  canvas->Clear();
  canvas->Divide(3, 2);
  TString kPt = bachelorHistKey(spec, "Pt");
  TString kEta = bachelorHistKey(spec, "Eta");
  TString kPhi = bachelorHistKey(spec, "Phi");
  TString kNs = kNsPre; kNs.ReplaceAll("_PreFemtoCut", "");
  TString kM2 = bachelorHistKey(spec, "Mass2");
  TString kDca = bachelorHistKey(spec, "DCA");
  canvas->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get(kPt);
  if (h1) { prepareBachelorHist(h1, kPt, spec); h1->Draw(); }
  canvas->cd(2); h1 = (TH1*)fin->Get(kEta);
  if (h1) { prepareBachelorHist(h1, kEta, spec); h1->Draw(); }
  canvas->cd(3); h1 = (TH1*)fin->Get(kPhi);
  if (h1) { prepareBachelorHist(h1, kPhi, spec); h1->Draw(); }
  canvas->cd(4); h1 = (TH1*)fin->Get(kNs);
  if (h1) { prepareBachelorHist(h1, kNs, spec); h1->Draw(); }
  canvas->cd(5); gPad->SetLogy(); h1 = (TH1*)fin->Get(kM2);
  if (h1) { prepareBachelorHist(h1, kM2, spec); h1->Draw(); }
  canvas->cd(6); gPad->SetLogy(); h1 = (TH1*)fin->Get(kDca);
  if (h1) { prepareBachelorHist(h1, kDca, spec); h1->Draw(); }
  canvas->Print(pdfName);

  // Page 12a: y & y-pT pre-femto
  canvas->Clear();
  canvas->Divide(2, 1);
  TString kYPre = bachelorHistKey(spec, "Y_PreFemtoCut");
  TString kPtYPre = bachelorHistKey(spec, "PtVsY_PreFemtoCut");
  if (gConfigLoaded) {
    canvas->cd(1); h1 = (TH1*)fin->Get(kYPre); if (h1) {
      prepareBachelorHist(h1, kYPre, spec); h1->Draw();
      drawCutLines1D(h1, cuts.minRapidityCm, cuts.maxRapidityCm);
    }
    canvas->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get(kPtYPre); if (h2) {
      prepareBachelorHist(h2, kPtYPre, spec); h2->Draw("colz");
      drawCutLine2DH(h2, cuts.minRapidityCm);
      drawCutLine2DH(h2, cuts.maxRapidityCm);
    }
  } else {
    canvas->cd(1); h1 = (TH1*)fin->Get(kYPre);
    if (h1) { prepareBachelorHist(h1, kYPre, spec); h1->Draw(); }
    canvas->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get(kPtYPre);
    if (h2) { prepareBachelorHist(h2, kPtYPre, spec); h2->Draw("colz"); }
  }
  canvas->Print(pdfName);

  // Page 12b: y & y-pT post-femto + mass2 vs p
  canvas->Clear();
  canvas->Divide(3, 2);
  TString kY = bachelorHistKey(spec, "Y_FemtoCut");
  TString kPtY = bachelorHistKey(spec, "PtVsY_FemtoCut");
  TString kM2VsP = bachelorHistKey(spec, "Mass2VsP");
  TString kNHits = bachelorHistKey(spec, "NHitsRatio_FemtoCut");
  if (gConfigLoaded) {
    canvas->cd(1); h1 = (TH1*)fin->Get(kY); if (h1) {
      prepareBachelorHist(h1, kY, spec); h1->Draw();
      drawCutLines1D(h1, cuts.minRapidityCm, cuts.maxRapidityCm);
    }
    canvas->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get(kPtY); if (h2) {
      prepareBachelorHist(h2, kPtY, spec); h2->Draw("colz");
      drawCutLine2DH(h2, cuts.minRapidityCm);
      drawCutLine2DH(h2, cuts.maxRapidityCm);
    }
  } else {
    canvas->cd(1); h1 = (TH1*)fin->Get(kY);
    if (h1) { prepareBachelorHist(h1, kY, spec); h1->Draw(); }
    canvas->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get(kPtY);
    if (h2) { prepareBachelorHist(h2, kPtY, spec); h2->Draw("colz"); }
  }
  canvas->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get(kM2VsP); if (h2) {
    prepareBachelorHist(h2, kM2VsP, spec); h2->Draw("colz");
    if (gConfigLoaded) {
      drawCutLine2DH(h2, cuts.minMass2);
      drawCutLine2DH(h2, cuts.maxMass2);
    }
  }
  canvas->cd(4); h1 = (TH1*)fin->Get(kNHits);
  if (h1) { prepareBachelorHist(h1, kNHits, spec); h1->Draw(); }
  canvas->Print(pdfName);

  // Page 12c: wide TOF m2 vs p (nuclei only)
  if (cuts.hasPMomWindow) {
    TString kM2WidePre = bachelorHistKey(spec, "Mass2VsP_PreFemtoCut_wide");
    TString kM2Wide = bachelorHistKey(spec, "Mass2VsP_wide");
    if (fin->Get(kM2WidePre) || fin->Get(kM2Wide)) {
      canvas->Clear();
      canvas->Divide(2, 1);
      if (gConfigLoaded) {
        canvas->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get(kM2WidePre); if (h2) {
          prepareBachelorHist(h2, kM2WidePre, spec); h2->Draw("colz");
          drawCutLine2DH(h2, cuts.minMass2);
          drawCutLine2DH(h2, cuts.maxMass2);
          drawCutLine2DV(h2, cuts.tofMomentumThreshold);
        }
        canvas->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get(kM2Wide); if (h2) {
          prepareBachelorHist(h2, kM2Wide, spec); h2->Draw("colz");
          drawCutLine2DH(h2, cuts.minMass2);
          drawCutLine2DH(h2, cuts.maxMass2);
          drawCutLine2DV(h2, cuts.tofMomentumThreshold);
        }
      } else {
        canvas->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get(kM2WidePre);
        if (h2) { prepareBachelorHist(h2, kM2WidePre, spec); h2->Draw("colz"); }
        canvas->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get(kM2Wide);
        if (h2) { prepareBachelorHist(h2, kM2Wide, spec); h2->Draw("colz"); }
      }
      canvas->Print(pdfName);
    }
  }
}

void checkHistAnaFemtoPhi(const Char_t* inputRootFile,
                                const Char_t* anaNameArg = "auau3p85fxt_anaFemtoPhi",
                                const Char_t* mainconfPath = 0)
{
  gROOT->SetBatch(kTRUE);
  gStyle->SetOptStat(0);
  gStyle->SetPalette(1);
  gStyle->SetTitleOffset(1.2, "x");
  gStyle->SetTitleOffset(1.4, "y");
  gStyle->SetPadLeftMargin(0.15);
  gStyle->SetPadRightMargin(0.12);
  gStyle->SetPadBottomMargin(0.12);

  TFile* fin = TFile::Open(inputRootFile);
  if (!fin || fin->IsZombie()) {
    std::cerr << "Error: Cannot open file " << inputRootFile << std::endl;
    return;
  }

  // Parse input basename for anaName_jobid_merge.root -> anaName, jobid (32 hex)
  TString anaName(anaNameArg);
  TString jobid;
  TString base = gSystem->BaseName(inputRootFile);
  base.ReplaceAll(".root", "");
  std::vector<TString> tokens;
  for (Int_t i = 0; i < base.Length(); ) {
    Int_t j = base.Index("_", i);
    if (j < 0) {
      tokens.push_back(TString(base(i, base.Length() - i)));
      break;
    }
    tokens.push_back(TString(base(i, j - i)));
    i = j + 1;
  }
  for (size_t k = 0; k < tokens.size(); k++) {
    if (isHex32(tokens[k])) {
      jobid = tokens[k];
      anaName = tokens[0];
      for (size_t m = 1; m < k; m++) anaName += "_" + tokens[m];
      break;
    }
  }

  gConfigLoaded = kFALSE;
  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";
  if (gSystem->Load(TString(pwd) + "/lib/libStarAnaConfig.so") >= 0) {
    TString mainconf;
    if (mainconfPath && strlen(mainconfPath) > 0) {
      mainconf = mainconfPath;
      if (mainconf[0] != '/') mainconf = TString(pwd) + "/" + mainconf;
    } else {
      mainconf = TString(pwd) + "/config/mainconf/main_" + anaName + ".yaml";
    }
    if (ConfigManager::GetInstance().LoadConfig(mainconf.Data())) {
      gConfigLoaded = kTRUE;
      if (!ConfigManager::GetInstance().GetPhiCuts().FinalizeRapidityFrame(
              ConfigManager::GetInstance().GetCentralityCuts())) {
        std::cerr << "[checkHistAnaFemtoPhi] WARNING: FinalizeRapidityFrame failed; PDF note may be incomplete."
                  << std::endl;
      }
    } else if (mainconfPath && strlen(mainconfPath) > 0) {
      std::cerr << "[checkHistAnaFemtoPhi] WARNING: Failed to load config " << mainconf.Data() << "; cut lines skipped." << std::endl;
    }
  }

  // Resolve symlinked share/figure to a physical path so ROOT PDF output
  // works the same in host and singularity-based runs.
  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaFemtoPhi";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from run_anaFemtoPhi.C (StFemtoMaker output).\n";
  note += "Phi candidates via ResonanceBuilder (KK); bachelor tracks p/d/t/^{3}He/^{4}He via TrackPidBuilder.\n";
  note += "Pair QA stage0 / strict TOF histograms mirror anaPhi hPhiPair_* naming.\n";
  const Double_t nEvtAll = getHistEntries(fin, "hVz");
  const Double_t nEvtAfter = getHistEntries(fin, "hVz_After");
  const Double_t nPhiCand = getHistEntries(fin, "hPhi_NCand");
  const Double_t nProtonCand = getHistEntries(fin, "hP_NCand");
  const Double_t nDeuteronCand = getHistEntries(fin, "hDeuteron_NCand");
  const Double_t nTritonCand = getHistEntries(fin, "hTriton_NCand");
  const Double_t nHe3Cand = getHistEntries(fin, "hHe3_NCand");
  const Double_t nHe4Cand = getHistEntries(fin, "hHe4_NCand");
  if (nEvtAll >= 0.0) {
    note += Form("Total statistics (input ROOT): events(all) = %.0f", nEvtAll);
    if (nEvtAfter >= 0.0) {
      note += Form(", events(after event cuts) = %.0f", nEvtAfter);
    }
    if (nPhiCand >= 0.0) {
      note += Form(", phi cand fills = %.0f", nPhiCand);
    }
    if (nProtonCand >= 0.0) note += Form(", p cand fills = %.0f", nProtonCand);
    if (nDeuteronCand >= 0.0) note += Form(", d cand fills = %.0f", nDeuteronCand);
    if (nTritonCand >= 0.0) note += Form(", t cand fills = %.0f", nTritonCand);
    if (nHe3Cand >= 0.0) note += Form(", ^{3}He cand fills = %.0f", nHe3Cand);
    if (nHe4Cand >= 0.0) note += Form(", ^{4}He cand fills = %.0f", nHe4Cand);
    note += "\n";
    TH1* hVzCheck = (TH1*)fin->Get("hVz");
    if (hVzCheck && nEvtAll > 0.0 && hVzCheck->GetMaximum() < 1e-6) {
      note += "WARNING: hVz has entries but visible bins are empty — likely Vz axis mismatch (FXT ~200 cm). Re-run with updated hist YAML.\n";
    }
  } else {
    note += "Total statistics (input ROOT): hVz not found.\n";
  }
  if (gConfigLoaded) {
    note += ConfigManager::GetInstance().GetPhiCuts().GetRapidityFrameSummary().c_str();
    note += "\n";
  }
  note += "QA layout: pre-cut/post-cut Event, Track; bachelor QA loop (p,d,t,^{3}He,^{4}He); Phi.\n";
  note += "Phi QA: (A) KK pair Raw/AfterCuts + y-pT; (B) femto candidate pre/post. Kaon PID: before/after on Page 9.\n";
  note += Form("CF computed in checkHist from merged SE/ME (TGraphErrors, Poisson stat errors); cfRebinFactor=%d; norm region from maker YAML.\n",
               getCfRebinFactor());
  Int_t cfCent9MinNote = 2;
  Int_t cfCent9MaxNote = 8;
  getCfCent9Range(cfCent9MinNote, cfCent9MaxNote);
  note += Form("CF cent slice: cent9 [%d, %d] projected from hKstarSEVsCent/hKstarMEVsCent (0-60%% for default 2-8); "
               "layout nCols x 2 (rows SE+ME overlay / CF, cols signal/leftSB/rightSB/rot).\n",
               cfCent9MinNote, cfCent9MaxNote);
  note += "k* count histograms display 0-3.0 GeV/c; CF graphs remain 0-0.65 GeV/c.\n";
  note += "hPhi_MKK_vs_BetaGamma: both K daughters must have TOF match (beta from btofBeta).\n";
  note += "hPhiPairMomAngle_*: #phi-bachelor 3-momentum angle QA only (not used in CF). "
          "_signal = m_phiQaLoose + signal mass window; _tofStrict = betaGamma>0 + same window; "
          "_preMass = all M_KK before opening/rapidity cuts.\n";
  if (gConfigLoaded) {
    const CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
    if (centCfg.cent9MaxRefMultCorrBin >= 0 && centCfg.cent9MaxRefMultCorr > 0.0) {
      note += Form("Centrality cut (YAML): cent9=%d requires refMult_corr <= %.1f (Maker; re-run needed in ROOT).\n",
                   centCfg.cent9MaxRefMultCorrBin, centCfg.cent9MaxRefMultCorr);
    }
  }
  note += "Legacy integrated channels (phi_*) may lack hKstar*VsCent 2D; cent-slice pages skip missing keys.\n";
  note += "Single QA PDF only (no separate CF PDF). CF sideband slices: pct_0_10/20/30 x all channel bases.\n";
  note += "Topic 3: lambda_sig from scaled SE-ME MKK fit (gaus+const); C_bkg from ME mass shape; "
          "C_genuine = 1 + [(C_meas-1) - lambda_bkg(C_bkg-1)]/lambda_sig.\n";
  note += "C_genuine error: analytic propagation; lambda error from gaus fit partial derivatives.\n";
  note += "method 5 (CF-subtraction): CF_CFsub = [CF_sig - (1-P) CF_SB]/P with CF_SB from "
          "sideband SE/ME (SBLR sumLR by default); P = slice phi M_KK purity (fit_slice or fixed). "
          "Distinct from CF_genuine (ME-mass C_bkg). Existing count-level CF_sig_sub_* kept. "
          "One guide page precedes method5 4-panel slice pages. "
          "Last page: phi M_KK SE vs k*-norm-scaled ME for pct_0_10 (TH3 projection).\n";
  note += "Primary CF is kstarMassFitCF: per-k* full M_KK, S=F-αB (ROT default, MIX cross-check), "
          "C_raw = Y_SE/Y_ME, C_norm in channel normQMin-normQMax. "
          "QA PDF: ..._kstarMassFitCf[_jobid].pdf; sidecar ..._CFkmf[_jobid].root. "
          "Simple SE/ME ratio in this PDF is diagnostic only.\n";
  note += "legacyCfPagesEnabled=false omits Topic 3, direct mass-fit, and Method 5 pages.\n";
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    note += Form("method5 YAML: mode=%s purityMode=%s combine=%s lowStatsRebinExtra=%d writeSidecar=%s\n",
                 fc.cfSubtractionMode.c_str(), fc.cfSubPurityMode.c_str(), fc.cfSubSidebandCombine.c_str(),
                 fc.cfSubLowStatsRebinExtra, fc.cfSubWriteSidecarRoot ? "true" : "false");
    note += Form("kstarMassFitCF YAML: enabled=%s template=%s crossCheck=%s fitMass=[%.3f,%.3f] "
                 "kstarBinWidth=%.3f writeSidecar=%s\n",
                 fc.kstarMassFitCfEnabled ? "true" : "false", fc.kstarMassFitCfTemplate.c_str(),
                 fc.kstarMassFitCfCrossCheck ? "true" : "false", fc.kstarMassFitCfFitMassMin,
                 fc.kstarMassFitCfFitMassMax, fc.kstarMassFitCfKstarBinWidth,
                 fc.kstarMassFitCfWriteSidecar ? "true" : "false");
  }
  note += "Re-run analysis after hist/Maker changes so new keys exist in the ROOT file.\n";
  note += "Phi-daughter production PID is charge-independent (PassPhiDaughterTofPid): "
          "low p allows TPC-only, high p requires TOF. Loose QA remains TPC-based. "
          "hPhiDauPid_* and hPhiDauPidUsed_{real,rot,mix}_* compare K+/K- and real/ROT/MIX.\n";
  note += "phi_mix sampling: lazy uniform permutation of production-PID pair indices "
          "(current K+ x buffer K- and buffer K+ x current K- combined). "
          "Cap is stored candidates per current event. No sampling weights. "
          "NPairPopulation is exact; NEligibleExact is valid only on a full scan. "
          "hPhiMix_MKK is stored candidates only. Uncapped is validation-only.\n";

  std::map<std::string, TGraphErrors*> cfCache;
  std::map<std::string, Double_t> purityCache;
  std::map<std::string, Double_t> kmfMetaCache;
  std::vector<TH1*> centProjKeepAlive;
  populateCfCache(fin, cfCache);
  populateCfCentCache(fin, cfCache);
  populateCfSliceCaches(fin, cfCache);
  populatePurityGenuineCaches(fin, cfCache);
  populateMethod5Caches(fin, cfCache, purityCache);
  dumpMethod5ConfigLog(purityCache);
  populateDirectMassFitCaches(fin, cfCache, kmfMetaCache);

  TString kmfPdf;
  if (isKstarMassFitCfEnabled()) {
    kmfPdf = TString(outDir) + anaName + "_checkHistAnaFemtoPhi_kstarMassFitCf";
    if (jobid.Length()) kmfPdf += "_" + jobid;
    kmfPdf += ".pdf";
    PdfHeader::OpenPdf(kmfPdf);
    TCanvas* cKmf = new TCanvas("cKmf", "kstarMassFitCf", 1200, 800);
    drawKstarMassFitCfSection(cKmf, fin, kmfPdf, centProjKeepAlive, cfCache, kmfMetaCache);
    PdfHeader::ClosePdf(kmfPdf);
    delete cKmf;
  }

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaFemtoPhi.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: Event Level
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minVz, ev.maxVz); } }
  c1->cd(2); h1 = (TH1*)fin->Get("hVzDiff"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, -ev.maxVzDiff, ev.maxVzDiff); } }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hVxVy"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
      drawVtxCutCircle(ev.vtxCenterX, ev.vtxCenterY, ev.maxVr);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMult"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minRefMult, ev.maxRefMult); } }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hVzVsRun"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultVsVz"); if (h2) h2->Draw("colz");
  c1->cd(7); h1 = (TH1*)fin->Get("hNTracks"); if (h1) { h1->Draw(); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); if (phi.maxNTr > 0) drawCutLine1D(h1, (Double_t)phi.maxNTr); } }
  c1->cd(8); h1 = (TH1*)fin->Get("hVr"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLine1D(h1, ev.maxVr); } }
  c1->cd(9); /* spare */;
  c1->Print(pdfName);

  // Page 1e: Event Level (Post-Cut)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minVz, ev.maxVz); } }
  c1->cd(2); h1 = (TH1*)fin->Get("hVzDiff_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, -ev.maxVzDiff, ev.maxVzDiff); } }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hVxVy_After"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
      drawVtxCutCircle(ev.vtxCenterX, ev.vtxCenterY, ev.maxVr);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMult_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minRefMult, ev.maxRefMult); } }
  c1->cd(5); /* spare */;
  c1->cd(6); /* spare */;
  c1->cd(7); /* spare */;
  c1->cd(8); h1 = (TH1*)fin->Get("hVr_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLine1D(h1, ev.maxVr); } }
  c1->cd(9); /* spare */;
  c1->Print(pdfName);

  // Page 1b: Centrality QA (event-level)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hCentrality"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hCentralityRaw"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hCentrality16"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMultCorr"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hRefMultWeight"); if (h1) h1->Draw();
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultVsNTOFMatch"); if (h2) h2->Draw("colz");
  c1->cd(7); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultVsNTOFMatchAfter"); if (h2) h2->Draw("colz");
  c1->cd(8); gPad->SetLogz(); h2 = (TH2*)fin->Get("hCentralityVsVz"); if (h2) h2->Draw("colz");
  c1->cd(9); h1 = (TH1*)fin->Get("hRawMult"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 1c: Centrality correlations (event observables vs cent9)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRawMult_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultCorr_vs_Cent9"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      const CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
      if (centCfg.cent9MaxRefMultCorrBin >= 0 && centCfg.cent9MaxRefMultCorr > 0.0) {
        drawCutLine2DH(h2, centCfg.cent9MaxRefMultCorr);
      }
    }
  }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNTracks_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hTofMatchMult_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonPlus_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonMinus_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(7); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNPhiPairs_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(8); /* spare */;
  c1->cd(9); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRawMult_vs_RefMultCorr"); if (h2) h2->Draw("colz");
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 1d: Bachelor multiplicity vs cent9
  c1->Clear();
  c1->Divide(3, 2);
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    c1->cd(ib + 1);
    gPad->SetLogz();
    h2 = (TH2*)fin->Get(kBachelorQaSpecs[ib].nVsCentKey);
    if (h2) h2->Draw("colz");
  }
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 2b: Track Kinematics & Quality (Pre-Cut)
  c1->Clear();
  c1->Divide(3, 3);
  if (gConfigLoaded) {
    TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
    c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hPt_Raw"); if (h1) { h1->Draw(); drawCutLines1D(h1, tr.minPt, tr.maxPt); }
    c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hEta_Raw"); if (h1) { h1->Draw(); drawCutLines1D(h1, tr.minEta, tr.maxEta); }
    c1->cd(3); h1 = (TH1*)fin->Get("hPhi_Raw"); if (h1) h1->Draw();
    c1->cd(4); h1 = (TH1*)fin->Get("hCharge"); if (h1) h1->Draw("hist");
    c1->cd(5); h1 = (TH1*)fin->Get("hNHitsFit_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, (Double_t)tr.minNHitsFit); }
    c1->cd(6); h1 = (TH1*)fin->Get("hNHitsRatio_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, tr.minNHitsRatio); }
    c1->cd(7); h1 = (TH1*)fin->Get("hNHitsDedx_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, (Double_t)tr.minNHitsDedx); }
    c1->cd(8); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDCA_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, tr.maxDCA); }
    c1->cd(9); gPad->SetLogy(); h1 = (TH1*)fin->Get("hChi2_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, tr.maxChi2); }
  } else {
    c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hPt_Raw"); if (h1) h1->Draw();
    c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hEta_Raw"); if (h1) h1->Draw();
    c1->cd(3); h1 = (TH1*)fin->Get("hPhi_Raw"); if (h1) h1->Draw();
    c1->cd(4); h1 = (TH1*)fin->Get("hCharge"); if (h1) h1->Draw("hist");
    c1->cd(5); h1 = (TH1*)fin->Get("hNHitsFit_Raw"); if (h1) h1->Draw();
    c1->cd(6); h1 = (TH1*)fin->Get("hNHitsRatio_Raw"); if (h1) h1->Draw();
    c1->cd(7); h1 = (TH1*)fin->Get("hNHitsDedx_Raw"); if (h1) h1->Draw();
    c1->cd(8); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDCA_Raw"); if (h1) h1->Draw();
    c1->cd(9); gPad->SetLogy(); h1 = (TH1*)fin->Get("hChi2_Raw"); if (h1) h1->Draw();
  }
  c1->Print(pdfName);

  // Page 2: Track Kinematics & Quality (Post-Cut)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hPt"); if (h1) h1->Draw();
  c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hEta"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhi"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hCharge"); if (h1) h1->Draw("hist");
  c1->cd(5); h1 = (TH1*)fin->Get("hNHitsFit"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hNHitsRatio"); if (h1) h1->Draw();
  c1->cd(7); h1 = (TH1*)fin->Get("hNHitsDedx"); if (h1) h1->Draw();
  c1->cd(8); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDCA"); if (h1) h1->Draw();
  c1->cd(9); gPad->SetLogy(); h1 = (TH1*)fin->Get("hChi2"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 3: PID (TPC & TOF)
  c1->Clear();
  c1->Divide(3, 2);
  double pmax = 5.0;
  double dedxmax = 1e-6;
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDedxVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->GetYaxis()->SetRangeUser(0, dedxmax); h2->Draw("colz"); }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hBetaVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaPionVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaKaonVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); drawCutLine2DH(h2, phi.nSigmaKaon); drawCutLine2DH(h2, -phi.nSigmaKaon); } }
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaProtonVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); }
  c1->Print(pdfName);

  // Page 3d: PID vs pT (mirror of Page 3)
  c1->Clear();
  c1->Divide(3, 2);
  double ptmax = 5.0;
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDedxVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->GetYaxis()->SetRangeUser(0, dedxmax); h2->Draw("colz"); }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hBetaVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsPt"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, ptmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaPionVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaKaonVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); drawCutLine2DH(h2, phi.nSigmaKaon); drawCutLine2DH(h2, -phi.nSigmaKaon); } }
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaProtonVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); }
  c1->Print(pdfName);

  // Page 3bp: TOF m2 vs pT (TPC K vs final K)
  c1->Clear();
  c1->Divide(2, 1);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsPt_TpcKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, ptmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsPt_IsKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, ptmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->Print(pdfName);

  // Page 3b: TOF m2 (TPC K vs final K) + pair DCA_KK
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsP_TpcKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsP_IsKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hDCAKK_All"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine1D(h1, phi.maxDCAKK);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hDCAKK_Pass"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine1D(h1, phi.maxDCAKK);
    }
  }
  c1->Print(pdfName);

  // Page 3c: TOF diagnostics (m2/q2 vs p/q, delta(1/beta) vs p and 1D, beta vs p)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hM2q2VsPq"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeltaOneOverBetaVsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.maxAbsDeltaOneOverBetaKaon);
      drawCutLine2DH(h2, -pid.maxAbsDeltaOneOverBetaKaon);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hDeltaOneOverBetaKaon"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLines1D(h1, -pid.maxAbsDeltaOneOverBetaKaon, pid.maxAbsDeltaOneOverBetaKaon);
    }
  }
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hBetaVsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
  }
  c1->Print(pdfName);

  // Page 4: Event Plane & Misc
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hTriggerIds"); if (h1) { h1->SetFillColor(17); h1->Draw(); }
  c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hTofMatchMult"); if (h1) h1->Draw();
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hQxQy"); if (h2) h2->Draw("colz");
  c1->cd(4); h1 = (TH1*)fin->Get("hPsi2"); if (h1) { h1->SetMinimum(0); h1->Draw(); }
  c1->Print(pdfName);

  // Page 7a: KK pair kinematics (pre-cut: strict TOF, before opening/rapidity)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hOpeningAngle_Raw"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minOpeningAngle, phi.maxOpeningAngle);
    }
  }
  c1->cd(2); h1 = (TH1*)fin->Get("hPairRapidity_Raw"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minPairRapidity, phi.maxPairRapidity);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hPairPt_Raw"); if (h1) h1->Draw();
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPairRapidity_vs_Pt"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine2DH(h2, phi.minPairRapidity);
      drawCutLine2DH(h2, phi.maxPairRapidity);
    }
  }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hOpeningAngle_vs_MKK"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPairRapidity_vs_MKK"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 7b: KK pair kinematics (post-cut: opening + rapidity passed)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hOpeningAngle_AfterCuts"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPairRapidity_AfterCuts"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPairPt_AfterCuts"); if (h1) h1->Draw();
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hOpeningAngle_vs_Pt"); if (h2) h2->Draw("colz");
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hOpeningAngle_vs_Rapidity"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMKK_vs_Pt"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 8b: staged pair QA mass and overlay
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhiPair_Mass_stage0"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhiPair_Mass_tofStrict"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhiPair_NPairs_stage0"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hPhiPair_NPairs_tofStrict"); if (h1) h1->Draw();
  c1->Print(pdfName);

  c1->Clear();
  c1->Divide(1, 1);
  c1->cd(1);
  TH1* hStage0 = (TH1*)fin->Get("hPhiPair_Mass_stage0");
  TH1* hTofStrict = (TH1*)fin->Get("hPhiPair_Mass_tofStrict");
  if (hStage0) {
    hStage0->SetLineColor(kBlue + 1);
    hStage0->SetTitle("Pair QA stage0 / strict TOF overlay;M_{KK} [GeV/c^{2}];Counts");
    hStage0->Draw("hist");
    if (hTofStrict) {
      hTofStrict->SetLineColor(kRed + 1);
      hTofStrict->Draw("hist same");
    }
    TLatex* tag = new TLatex();
    tag->SetNDC(kTRUE);
    tag->SetTextSize(0.035);
    tag->DrawLatex(0.15, 0.86, "Blue: stage0");
    if (hTofStrict) tag->DrawLatex(0.15, 0.80, "Red: strict TOF pair");
  }
  c1->Print(pdfName);

  // Page 8c: strict TOF pair QA mass projections
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiPair_MassVsPt_tofStrict"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiPair_MassVsCent_tofStrict"); if (h2) { h2->Draw("colz"); drawCent9ConventionNote(); }
  c1->cd(3);
  h2 = (TH2*)fin->Get("hPhiPair_MassVsPt_tofStrict");
  if (h2) {
    Int_t b1 = h2->GetXaxis()->FindBin(0.2 + 1e-6);
    Int_t b2 = h2->GetXaxis()->FindBin(1.6 - 1e-6);
    TH1D* hProjPt = h2->ProjectionY("hPhiPair_Mass_tofStrict_pt0p2to1p6", b1, b2);
    if (hProjPt) {
      hProjPt->SetTitle("Strict TOF M_{KK} (0.2<p_{T}<1.6 GeV/c);M_{KK} [GeV/c^{2}];Counts");
      hProjPt->Draw();
    }
  }
  c1->cd(4);
  h2 = (TH2*)fin->Get("hPhiPair_MassVsCent_tofStrict");
  if (h2) {
    Int_t b1 = h2->GetXaxis()->FindBin(6.0 + 1e-6);
    Int_t b2 = h2->GetXaxis()->FindBin(8.0 - 1e-6);
    TH1D* hProjCent = h2->ProjectionY("hPhiPair_Mass_tofStrict_cent6to8", b1, b2);
    if (hProjCent) {
      hProjCent->SetTitle("Strict TOF M_{KK} (cent9: 6-8);M_{KK} [GeV/c^{2}];Counts");
      hProjCent->Draw();
    }
  }
  c1->Print(pdfName);

  // Page 8d: phi pair kinematics (stage0 vs strict TOF; stage0 is after opening+rapidity)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhiPair_Pt_stage0"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhiPair_Pt_tofStrict"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhiPair_Rapidity_stage0"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minPairRapidity, phi.maxPairRapidity);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hPhiPair_Rapidity_tofStrict"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minPairRapidity, phi.maxPairRapidity);
    }
  }
  c1->cd(5); h1 = (TH1*)fin->Get("hPhiPair_OpeningAngle_stage0"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minOpeningAngle, phi.maxOpeningAngle);
    }
  }
  c1->cd(6); h1 = (TH1*)fin->Get("hPhiPair_OpeningAngle_tofStrict"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minOpeningAngle, phi.maxOpeningAngle);
    }
  }
  c1->cd(0);
  {
    TLatex* stageNote = new TLatex();
    stageNote->SetNDC(kTRUE);
    stageNote->SetTextSize(0.028);
    stageNote->SetTextColor(kBlue + 1);
    stageNote->DrawLatex(0.12, 0.98, "stage0/tofStrict: after opening+rapidity (see Page 7a/b for Raw/AfterCuts)");
  }
  c1->Print(pdfName);

  // Page 9: Kaon QA (before vs after PID cut)
  c1->Clear();
  c1->Divide(3, 2);
  // Row 1: Before PID cut (all accepted tracks)
  c1->cd(1); h1 = (TH1*)fin->Get("hPt"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minPt, tr.maxPt);
    }
  }
  c1->cd(2); h1 = (TH1*)fin->Get("hEta"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minEta, tr.maxEta);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hNSigmaKaon_Raw"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, -phi.nSigmaKaon, phi.nSigmaKaon);
    }
  }
  // Row 2: After PID cut (final Kaons)
  c1->cd(4); h1 = (TH1*)fin->Get("hK_Pt"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minPt, tr.maxPt);
    }
  }
  c1->cd(5); h1 = (TH1*)fin->Get("hK_Eta"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minEta, tr.maxEta);
    }
  }
  c1->cd(6); h1 = (TH1*)fin->Get("hK_NSigma"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, -phi.nSigmaKaon, phi.nSigmaKaon);
    }
  }
  c1->Print(pdfName);

  // Page 10: K multiplicity + n#sigma vs p (all 5 bachelor species)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonPlusVsNKaonMinus"); if (h2) h2->Draw("colz");
  c1->cd(2); h1 = (TH1*)fin->Get("hNKaonPlus"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hNKaonMinus"); if (h1) h1->Draw();
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaProtonVsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, 5.0);
    h2->Draw("colz");
    if (gConfigLoaded) {
      BachelorCuts bc = getBachelorCuts(ConfigManager::GetInstance().GetFemtoConfig(), "proton");
      drawCutLine2DH(h2, -bc.maxAbsNSigma);
      drawCutLine2DH(h2, bc.maxAbsNSigma);
    }
  }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaDeuteronVsP_All"); if (h2) {
    prepareBachelorHist(h2, "hNSigmaDeuteronVsP_All", kBachelorQaSpecs[1]);
    h2->Draw("colz");
  }
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaTritonVsP_All"); if (h2) {
    prepareBachelorHist(h2, "hNSigmaTritonVsP_All", kBachelorQaSpecs[2]);
    h2->Draw("colz");
  }
  c1->cd(7); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaHe3VsP_All"); if (h2) {
    prepareBachelorHist(h2, "hNSigmaHe3VsP_All", kBachelorQaSpecs[3]);
    h2->Draw("colz");
  }
  c1->cd(8); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaHe4VsP_All"); if (h2) {
    prepareBachelorHist(h2, "hNSigmaHe4VsP_All", kBachelorQaSpecs[4]);
    h2->Draw("colz");
  }
  c1->cd(9); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaProtonVsPt_Pos"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 10pid: unified phi-daughter PID (loose vs production, K+/K-, real/ROT/MIX)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhiDauPid_NLoose_Kp"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhiDauPid_NProd_Kp"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhiDauPid_NReject_Kp"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hPhiDauPid_NLoose_Km"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hPhiDauPid_NProd_Km"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hPhiDauPid_NReject_Km"); if (h1) h1->Draw();
  c1->cd(7); h1 = (TH1*)fin->Get("hPhiDauPid_Category_Prod_Kp"); if (h1) h1->Draw();
  c1->cd(8); h1 = (TH1*)fin->Get("hPhiDauPid_Category_Prod_Km"); if (h1) h1->Draw();
  c1->cd(9); h1 = (TH1*)fin->Get("hPhiDauPid_Category_Reject_Km"); if (h1) h1->Draw();
  c1->Print(pdfName);

  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPid_TofMatchVsP_Loose_Kp"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPid_TofMatchVsP_Prod_Kp"); if (h2) h2->Draw("colz");
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPid_TofMatchVsP_Reject_Kp"); if (h2) h2->Draw("colz");
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPid_TofMatchVsP_Loose_Km"); if (h2) h2->Draw("colz");
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPid_TofMatchVsP_Prod_Km"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPid_TofMatchVsP_Reject_Km"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPidUsed_TofMatchVsP_real_Kp"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPidUsed_TofMatchVsP_rot_Kp"); if (h2) h2->Draw("colz");
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPidUsed_TofMatchVsP_mix_Kp"); if (h2) h2->Draw("colz");
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPidUsed_TofMatchVsP_real_Km"); if (h2) h2->Draw("colz");
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPidUsed_TofMatchVsP_rot_Km"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiDauPidUsed_TofMatchVsP_mix_Km"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 10mix: phi_mix sampling / cap QA (null-safe for older ROOT files)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hPhiMix_PairPopulationLog10");
  if (h1) { gPad->SetLogy(); h1->Draw(); }
  c1->cd(2);
  h1 = (TH1*)fin->Get("hPhiMix_AttemptedLog10");
  if (h1) { gPad->SetLogy(); h1->Draw(); }
  c1->cd(3);
  h1 = (TH1*)fin->Get("hPhiMix_NStoredWide");
  if (h1) { gPad->SetLogy(); h1->Draw(); }
  c1->cd(4);
  h1 = (TH1*)fin->Get("hPhiMix_CapHit");
  if (h1) h1->Draw();
  c1->cd(5);
  h1 = (TH1*)fin->Get("hPhiMix_KeepFraction");
  if (h1) h1->Draw();
  c1->cd(6);
  h1 = (TH1*)fin->Get("hPhiMix_FwdRevRatio");
  if (h1) h1->Draw();
  c1->cd(7);
  h1 = (TH1*)fin->Get("hPhiMix_MKK");
  if (h1) { gPad->SetLogy(); h1->Draw(); }
  c1->cd(8);
  h1 = (TH1*)fin->Get("hPhiMix_NCand");
  if (h1) h1->Draw();
  c1->cd(9);
  h2 = (TH2*)fin->Get("hPhiMixSamplerQA");
  if (h2) {
    gPad->SetLogz();
    h2->Draw("colz");
  }
  c1->Print(pdfName);

  // Page 10b: K- femto candidate QA (used when kaon-minus species is enabled)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hNKaonMinusFemto"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hKm_NCand"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hKm_Pt_PreFemtoCut"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hKm_Eta_PreFemtoCut"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hKm_NSigmaKaon_PreFemtoCut"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hKm_DCA_PreFemtoCut"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 10c: K- femto PID / rapidity QA
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hKm_Pt"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hKm_Eta"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hKm_NSigmaKaon"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hKm_DCA"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hKm_Y_PreFemtoCut"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hKm_Y_FemtoCut"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 10d: K- femto 2D / TOF QA
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hKm_PtVsY_PreFemtoCut"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hKm_PtVsY_FemtoCut"); if (h2) h2->Draw("colz");
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hKm_Mass2VsP"); if (h2) h2->Draw("colz");
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hKm_TofMatchVsP"); if (h2) h2->Draw("colz");
  c1->cd(5); h1 = (TH1*)fin->Get("hKm_Mass2_PreFemtoCut"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hKm_Mass2"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Pages 11a-12c: bachelor QA per species
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    drawBachelorFemtoQaPages(c1, fin, pdfName, kBachelorQaSpecs[ib]);
  }

  // Two-body h-K+/- CFs (phi-daughter kaon selection): one page per bachelor x kaon charge.
  if (isHKaonTwoBodyEnabled()) {
    for (Int_t ik = 0; kHKaonSpecies[ik]; ++ik) {
      const char* klab = (TString(kHKaonSpecies[ik]).EndsWith("plus")) ? "K^{+}" : "K^{-}";
      for (Int_t ib = 0; kHKaonBachelors[ib]; ++ib) {
        const std::string ch = std::string(kHKaonSpecies[ik]) + "_" + kHKaonBachelors[ib];
        c1->Clear();
        c1->Divide(2, 1);
        c1->cd(1);
        drawKstarSeMeOverlay((TH1*)fin->Get(kstarSeHistKey(ch).c_str()), (TH1*)fin->Get(kstarMeHistKey(ch).c_str()),
                             channelNormQMin(ch), channelNormQMax(ch), centProjKeepAlive);
        c1->cd(2);
        drawComputedCf(c1, 2, fin, ch, channelNormQMin(ch), channelNormQMax(ch), cfCache);
        if (gPad) {
          TLatex* lat = new TLatex();
          lat->SetNDC(kTRUE);
          lat->SetTextSize(0.03);
          lat->DrawLatex(0.02, 0.98, Form("Two-body %s-%s CF (%s)", kHKaonBachLabels[ib], klab, ch.c_str()));
        }
        c1->Print(pdfName);
      }
    }
  }

  // Page 13a: Phi candidate QA (pre-cut)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhi_MKK_PreCut"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhi_Pt_PreCut"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhi_Rapidity_PreCut"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minPairRapidity, phi.maxPairRapidity);
    }
  }
  c1->cd(4); /* spare */;
  c1->Print(pdfName);

  // Page 13b: Phi candidate QA (post-cut, femto resonance list)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhi_MKK"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhi_Pt"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhi_Rapidity"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, phi.minPairRapidity, phi.maxPairRapidity);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hPhi_NCand"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 14a: Phi y-pT (pre-cut)
  c1->Clear();
  c1->Divide(1, 1);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhi_PtVsY_PreCut"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine2DH(h2, phi.minPairRapidity);
      drawCutLine2DH(h2, phi.maxPairRapidity);
    }
  }
  c1->Print(pdfName);

  // Page 14b: Phi y-pT (post-cut, all candidates)
  c1->Clear();
  c1->Divide(1, 1);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhi_PtVsY_PostCut"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine2DH(h2, phi.minPairRapidity);
      drawCutLine2DH(h2, phi.maxPairRapidity);
    }
  }
  c1->Print(pdfName);

  // Page 14c: PID of quality tracks near signal-window phi (k* thresholds; before CF pages)
  {
    c1->Clear();
    c1->Divide(2, 2);
    Double_t kLoose = 1.0;
    Double_t kTight = 0.3;
    if (gConfigLoaded) {
      const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
      kLoose = femtoCfg.phiNearTrackMaxKstarLoose;
      kTight = femtoCfg.phiNearTrackMaxKstarTight;
    }
    c1->cd(1);
    gPad->SetLogz();
    h2 = (TH2*)fin->Get("hDedxVsP_PhiSignalNear_k1");
    if (h2) {
      h2->SetTitle(Form("dE/dx vs p (signal #phi, k^{*}<%.2f);p [GeV/c];dE/dx [GeV/cm]", kLoose));
      h2->Draw("colz");
    }
    c1->cd(2);
    gPad->SetLogz();
    h2 = (TH2*)fin->Get("hMass2ChargeVsP_PhiSignalNear_k1");
    if (h2) {
      h2->SetTitle(Form("m^{2}#times q vs p (signal #phi, k^{*}<%.2f);p [GeV/c];m^{2}#times q", kLoose));
      h2->Draw("colz");
    }
    c1->cd(3);
    gPad->SetLogz();
    h2 = (TH2*)fin->Get("hDedxVsP_PhiSignalNear_k03");
    if (h2) {
      h2->SetTitle(Form("dE/dx vs p (signal #phi, k^{*}<%.2f);p [GeV/c];dE/dx [GeV/cm]", kTight));
      h2->Draw("colz");
    }
    c1->cd(4);
    gPad->SetLogz();
    h2 = (TH2*)fin->Get("hMass2ChargeVsP_PhiSignalNear_k03");
    if (h2) {
      h2->SetTitle(Form("m^{2}#times q vs p (signal #phi, k^{*}<%.2f);p [GeV/c];m^{2}#times q", kTight));
      h2->Draw("colz");
    }
    c1->Print(pdfName);
  }

  // Page 15+: Femto k* legacy integrated CF (one page per channel base)
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    const BachelorQaSpec& spec = kBachelorQaSpecs[ib];
    const std::string base(spec.channelBase);
    c1->Clear();
    c1->Divide(2, 2);
    TString seKey = TString("hKstarSE_") + base.c_str();
    TString meKey = TString("hKstarME_") + base.c_str();
    c1->cd(1);
    drawKstarSeMeOverlay((TH1*)fin->Get(seKey), (TH1*)fin->Get(meKey), channelNormQMin(base), channelNormQMax(base),
                         centProjKeepAlive);
    c1->cd(2);
    drawComputedCf(c1, 2, fin, base, channelNormQMin(base), channelNormQMax(base), cfCache);
    c1->cd(3); h1 = (TH1*)fin->Get(spec.nCandKey);
    if (h1) { prepareBachelorHist(h1, spec.nCandKey, spec); h1->Draw(); }
    c1->Print(pdfName);
  }

  // Page 16: Phi mass windows / sidebands / rotation (proton rotation QA)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhi_MKK_signal"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhi_MKK_leftSB"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhi_MKK_rightSB"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hPhi_MKK_rot"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hPhiRot_MKK"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hPhiRot_NCand"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 16b: Phi M_KK vs beta-gamma (TOF K daughters, both matched)
  c1->Clear();
  c1->cd();
  gPad->SetLogz();
  h2 = (TH2*)fin->Get("hPhi_MKK_vs_BetaGamma");
  if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
      const FemtoConfig::ChannelDef* ch = femtoCfg.FindChannel("phi_deuteron_signal");
      if (ch) {
        Double_t ylo = gPad->GetUymin();
        Double_t yhi = gPad->GetUymax();
        TLine* lLo = new TLine(ch->signalMin, ylo, ch->signalMin, yhi);
        lLo->SetLineColor(kRed);
        lLo->SetLineStyle(2);
        lLo->Draw("same");
        TLine* lHi = new TLine(ch->signalMax, ylo, ch->signalMax, yhi);
        lHi->SetLineColor(kRed);
        lHi->SetLineStyle(2);
        lHi->Draw("same");
      }
    }
  }
  c1->Print(pdfName);

  // Per-channel signal / sideband / rotation k* pages
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    const BachelorQaSpec& spec = kBachelorQaSpecs[ib];
    const std::string base(spec.channelBase);
    const std::string sig = channelSignal(base);
    const std::string lsb = channelLeftSb(base);
    const std::string rsb = channelRightSb(base);

    c1->Clear();
    c1->Divide(2, 2);
    c1->cd(1);
    drawKstarSeMeOverlay((TH1*)fin->Get(TString("hKstarSE_") + sig.c_str()),
                         (TH1*)fin->Get(TString("hKstarME_") + sig.c_str()), channelNormQMin(sig),
                         channelNormQMax(sig), centProjKeepAlive);
    c1->cd(2);
    drawComputedCf(c1, 2, fin, sig, channelNormQMin(sig), channelNormQMax(sig), cfCache);
    c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get(TString("hKstarSEVsCent_") + sig.c_str()); if (h2) h2->Draw("colz");
    c1->Print(pdfName);

    c1->Clear();
    c1->Divide(2, 2);
    c1->cd(1);
    drawKstarSeMeOverlay((TH1*)fin->Get(TString("hKstarSE_") + lsb.c_str()),
                         (TH1*)fin->Get(TString("hKstarME_") + lsb.c_str()), channelNormQMin(lsb),
                         channelNormQMax(lsb), centProjKeepAlive);
    c1->cd(2);
    drawComputedCf(c1, 2, fin, lsb, channelNormQMin(lsb), channelNormQMax(lsb), cfCache);
    c1->cd(3);
    drawKstarSeMeOverlay((TH1*)fin->Get(TString("hKstarSE_") + rsb.c_str()),
                         (TH1*)fin->Get(TString("hKstarME_") + rsb.c_str()), channelNormQMin(rsb),
                         channelNormQMax(rsb), centProjKeepAlive);
    c1->cd(4);
    drawComputedCf(c1, 4, fin, rsb, channelNormQMin(rsb), channelNormQMax(rsb), cfCache);
    c1->Print(pdfName);

    if (spec.rotChannel) {
      const std::string rotCh(spec.rotChannel);
      c1->Clear();
      c1->Divide(2, 2);
      c1->cd(1);
      drawKstarSeMeOverlay((TH1*)fin->Get(TString("hKstarSE_") + rotCh.c_str()),
                           (TH1*)fin->Get(TString("hKstarME_") + rotCh.c_str()), channelNormQMin(rotCh),
                           channelNormQMax(rotCh), centProjKeepAlive);
      c1->cd(2);
      drawComputedCf(c1, 2, fin, rotCh, channelNormQMin(rotCh), channelNormQMax(rotCh), cfCache);
      c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get(TString("hKstarSEVsCent_") + rotCh.c_str()); if (h2) h2->Draw("colz");
      c1->Print(pdfName);
    }
  }

  // Phi-bachelor pair momentum angle QA (one page per bachelor; CF unchanged)
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    drawPhiBachelorPairAngleQaPage(c1, fin, pdfName, kBachelorQaSpecs[ib]);
  }

  // Cent9-projected CF slice pages (one per channel base)
  Int_t cfCent9Min = 0;
  Int_t cfCent9Max = 0;
  getCfCent9Range(cfCent9Min, cfCent9Max);
  c1->SetCanvasSize(1800, 900);
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    const BachelorQaSpec& spec = kBachelorQaSpecs[ib];
    drawCentSlicePageForBase(c1, fin, std::string(spec.channelBase), spec.rotChannel, cfCent9Min, cfCent9Max,
                             centProjKeepAlive, cfCache);
    c1->Print(pdfName);
  }
  c1->SetCanvasSize(1200, 800);

  // QA representative slices: pct_0_10/20/30 x all channel bases
  {
    const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
    const Int_t qaCanvasH = 1200;
    c1->SetCanvasSize(1800, qaCanvasH);
    for (size_t is = 0; is < allSlices.size(); ++is) {
      if (!isSliceInQaPdf(allSlices[is].id)) continue;
      for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
        drawSidebandSlicePageForBase(c1, fin, allSlices[is], std::string(kChannelBases[ib]), centProjKeepAlive,
                                     cfCache, kTRUE);
        c1->Print(pdfName);
      }
    }
    c1->SetCanvasSize(1200, 800);
  }

  // Primary CF (kstarMassFitCF): default template first, then cross-check if enabled.
  if (isKstarMassFitCfEnabled()) {
    drawKstarMassFitCfCachedPages(c1, pdfName, cfCache);
    c1->SetCanvasSize(1200, 800);
  }

  // Topic 3: lambda_sig and CF_genuine QA (representative slices x channel bases)
  if (isLegacyCfPagesEnabled()) {
    const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
    c1->SetCanvasSize(1400, 700);
    for (size_t is = 0; is < allSlices.size(); ++is) {
      if (!isSliceInQaPdf(allSlices[is].id)) continue;
      for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
        const std::string base(kChannelBases[ib]);
        drawLambdaSigSlicePageForBase(c1, fin, allSlices[is], base, cfCache);
        c1->Print(pdfName);
        drawGenuineCfSlicePageForBase(c1, fin, allSlices[is], base, centProjKeepAlive, cfCache);
        c1->Print(pdfName);
      }
    }
    c1->SetCanvasSize(1200, 800);
  }

  // Kubo-rule background + genuine CF (p, d): old (pseudo-phi average) vs Kubo (sum).
  if (isKuboTripletEnabled()) {
    c1->SetCanvasSize(1400, 700);
    for (Int_t ib = 0; kKuboBases[ib]; ++ib) {
      drawKuboPagesForBase(c1, fin, std::string(kKuboBases[ib]), pdfName, cfCache);
      drawDirectMassFitKuboClosureForBase(c1, fin, std::string(kKuboBases[ib]), pdfName, cfCache, kmfMetaCache);
    }
    c1->SetCanvasSize(1200, 800);
  }

  // Legacy: direct mass-fit pages (after Topic 3, before CF-Sub)
  if (isLegacyCfPagesEnabled()) {
    c1->Clear();
    c1->SetCanvasSize(1200, 900);
    drawDirectMassFitGuidePage(c1);
    c1->Print(pdfName);

    const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
    c1->SetCanvasSize(1400, 1000);
    for (size_t is = 0; is < allSlices.size(); ++is) {
      if (!isSliceInQaPdf(allSlices[is].id)) continue;
      for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
        const std::string base(kChannelBases[ib]);
        drawDirectMassFitSlicePageForBase(c1, fin, allSlices[is], base, cfCache, kmfMetaCache);
        c1->Print(pdfName);
        drawDirectMassFitExemplarPageForBase(c1, fin, allSlices[is], base, kmfMetaCache, centProjKeepAlive);
        c1->Print(pdfName);
      }
    }
    c1->SetCanvasSize(1200, 800);
  }

  // method 5: CF-subtraction pages (CF_sig / CF_SB / CF_CFsub + L/R systematics)
  if (isMethod5Enabled()) {
    c1->Clear();
    c1->SetCanvasSize(1200, 900);
    drawCfSubMethod5GuidePage(c1);
    c1->Print(pdfName);

    const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
    c1->SetCanvasSize(1400, 1000);
    for (size_t is = 0; is < allSlices.size(); ++is) {
      if (!isSliceInQaPdf(allSlices[is].id)) continue;
      for (Int_t ib = 0; kChannelBases[ib]; ++ib) {
        drawCfSubMethod5SlicePageForBase(c1, fin, allSlices[is], std::string(kChannelBases[ib]), cfCache,
                                         purityCache);
        c1->Print(pdfName);
      }
    }
    c1->SetCanvasSize(1200, 800);
  }

  // Last page: phi M_KK SE vs scaled ME for central 0-10% (pct_0_10)
  c1->Clear();
  c1->SetCanvasSize(1400, 900);
  drawPhiMkkSeMeCent010SummaryPage(c1, fin, centProjKeepAlive);
  c1->Print(pdfName);
  c1->SetCanvasSize(1200, 800);

  PdfHeader::ClosePdf(pdfName);

  writeCfSubSidecarRoot(outDir, anaName, jobid, inputRootFile, mainconfPath, cfCache, purityCache);

  writeKstarMassFitCfSidecarRoot(outDir, anaName, jobid, inputRootFile, mainconfPath, cfCache, kmfMetaCache);




  // Console: verify key histograms exist and have entries
  {
    std::cout << "\n=== checkHistAnaFemtoPhi: key histogram entries ===\n";
    const char* keys[] = {"hVz",
                           "hVz_After",
                           "hCentrality",
                           "hNSigmaKaon_Raw",
                           "hK_Pt",
                           "hNKaonMinusFemto",
                           "hKm_Pt_PreFemtoCut",
                           "hKm_Pt",
                           "hKm_Mass2VsP",
                           "hNSigmaProtonVsPt_Pos",
                           "hPairRapidity_vs_Pt",
                           "hOpeningAngle_Raw",
                           "hPairRapidity_AfterCuts",
                           "hPhiPair_Mass_stage0",
                           "hPhiPair_Mass_tofStrict",
                           "hPhi_MKK_PreCut",
                           "hPhi_PtVsY_PreCut",
                           "hPhi_MKK",
                           "hPhi_PtVsY_PostCut",
                           "hPhi_MKK_signal",
                           "hPhi_MKK_leftSB",
                           "hPhi_MKK_rightSB",
                           "hPhi_MKK_rot",
                           "hPhi_MKK_vs_BetaGamma",
                           "hPhiRot_MKK",
                           "hPhiMix_MKK",
                           "hPhiMix_NCand",
                           "hPhiMix_NStoredWide",
                           "hPhiMix_CapHit",
                           "hPhiMix_KeepFraction",
                           "hPhiMixSamplerQA",
                           "hPhi_NCand",
                           "hP_Pt_PreFemtoCut",
                           "hP_Pt",
                           "hP_NCand",
                           "hDeuteron_Pt_PreFemtoCut",
                           "hDeuteron_NCand",
                           "hTriton_Pt_PreFemtoCut",
                           "hTriton_NCand",
                           "hHe3_Pt_PreFemtoCut",
                           "hHe3_NCand",
                           "hHe4_Pt_PreFemtoCut",
                           "hHe4_Pt",
                           "hHe4_NCand",
                           "hNSigmaProtonVsP",
                           "hNSigmaDeuteronVsP_All",
                           "hNSigmaTritonVsP_All",
                           "hNSigmaHe3VsP_All",
                           "hNSigmaHe4VsP_All",
                           "hNProton_vs_Cent9",
                           "hNDeuteron_vs_Cent9",
                           "hNTriton_vs_Cent9",
                           "hNHe3_vs_Cent9",
                           "hNHe4_vs_Cent9",
                           "hMass2VsPt_TpcKaon",
                           "hDCAKK_All",
                           "hDCAKK_Pass",
                           "hDedxVsP_PhiSignalNear_k1",
                           "hMass2ChargeVsP_PhiSignalNear_k1",
                           "hDedxVsP_PhiSignalNear_k03",
                           "hMass2ChargeVsP_PhiSignalNear_k03",
                           0};
    for (Int_t i = 0; keys[i]; ++i) {
      TObject* o = fin->Get(keys[i]);
      if (!o) {
        std::cout << "  " << keys[i] << ": NOT IN FILE\n";
        continue;
      }
      if (o->InheritsFrom("TH1")) {
        TH1* hh = (TH1*)o;
        Double_t n = hh->GetEntries();
        std::cout << "  " << keys[i] << ": entries=" << n;
        if (n < 1.0)
          std::cout << "  [empty — use ROOT from a run after new hist fills]";
        std::cout << "\n";
      } else {
        std::cout << "  " << keys[i] << ": unexpected class " << o->ClassName() << "\n";
      }
    }
    std::cout << "  --- hPhiPairMomAngle (phi-bachelor QA) ---\n";
    for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
      const char* base = kBachelorQaSpecs[ib].channelBase;
      const TString angleKeyStrs[4] = {phiPairMomAngleKey(base, kFALSE, kFALSE), phiPairMomAngleKey(base, kFALSE, kTRUE),
                                         phiPairMomAngleKey(base, kTRUE, kFALSE), phiPairMomAngleKey(base, kTRUE, kTRUE)};
      for (Int_t ik = 0; ik < 4; ++ik) {
        TObject* o = fin->Get(angleKeyStrs[ik]);
        if (!o) {
          std::cout << "  " << angleKeyStrs[ik].Data() << ": NOT IN FILE\n";
          continue;
        }
        if (o->InheritsFrom("TH1")) {
          TH1* hh = (TH1*)o;
          std::cout << "  " << angleKeyStrs[ik].Data() << ": entries=" << hh->GetEntries() << "\n";
        }
      }
    }
    for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
      const BachelorQaSpec& spec = kBachelorQaSpecs[ib];
      const std::string base(spec.channelBase);
      const char* tags[] = {base.c_str(), channelSignal(base).c_str(), channelLeftSb(base).c_str(),
                            channelRightSb(base).c_str(), 0};
      for (Int_t it = 0; tags[it]; ++it) {
        Int_t nPts = getCachedCfPointCount(cfCache, tags[it]);
        std::cout << "  hCF_" << tags[it] << " (computed): nPoints=" << nPts;
        if (nPts < 1) std::cout << "  [empty — check SE/ME or norm region]";
        std::cout << "\n";
      }
      if (spec.rotChannel) {
        Int_t nRot = getCachedCfPointCount(cfCache, spec.rotChannel);
        std::cout << "  hCF_" << spec.rotChannel << " (computed): nPoints=" << nRot << "\n";
      }
    }
    std::cout << Form("  CF cent slice (cent9 %d-%d, projected):\n", cfCent9Min, cfCent9Max);
    for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
      const BachelorQaSpec& spec = kBachelorQaSpecs[ib];
      const std::string base(spec.channelBase);
      const char* centCh[] = {channelSignal(base).c_str(), channelLeftSb(base).c_str(),
                              channelRightSb(base).c_str(), spec.rotChannel, 0};
      std::cout << "    " << base << ":\n";
      for (Int_t ic = 0; centCh[ic]; ++ic) {
        Int_t nPts = getCachedCfPointCount(cfCache, cfCentCacheKey(centCh[ic]).c_str());
        std::cout << "      " << centCh[ic] << ": nPoints=" << nPts << "\n";
      }
    }
    if (isHKaonTwoBodyEnabled()) {
      std::cout << "  --- h-K two-body (phi-daughter kaon selection) ---\n";
      for (Int_t ik = 0; kHKaonSpecies[ik]; ++ik) {
        for (Int_t ib = 0; kHKaonBachelors[ib]; ++ib) {
          const std::string ch = std::string(kHKaonSpecies[ik]) + "_" + kHKaonBachelors[ib];
          std::cout << "    " << ch << ": SE=" << getHistEntries(fin, (std::string("hKstarSE_") + ch).c_str())
                    << " ME=" << getHistEntries(fin, (std::string("hKstarME_") + ch).c_str())
                    << " CF nPoints=" << getCachedCfPointCount(cfCache, ch.c_str()) << "\n";
        }
      }
    }
    if (isKuboTripletEnabled()) {
      std::cout << "  --- Kubo triplet background ---\n";
      const char* tripPrefixes[] = {"hKstarTripSEKp_", "hKstarTripSEKm_", "hKstarTripMix_", "hMKKtriplet_", 0};
      for (Int_t ib = 0; kKuboBases[ib]; ++ib) {
        std::cout << "    " << kKuboBases[ib] << ":";
        for (Int_t ip = 0; tripPrefixes[ip]; ++ip) {
          std::cout << " " << tripPrefixes[ip] << "="
                    << getHistEntries(fin, (std::string(tripPrefixes[ip]) + kKuboBases[ib]).c_str());
        }
        std::cout << "\n";
      }
    }
    printCfSliceConsoleSummary(cfCache);
    std::cout << "=============================================================\n\n";
  }

  delete c1;
  c1 = 0;
  // Pad-owned drawables may be deleted with the canvas; drop pointers only.
  centProjKeepAlive.clear();
  for (std::map<std::string, TGraphErrors*>::iterator it = cfCache.begin(); it != cfCache.end(); ++it) {
    it->second = 0;
  }
  cfCache.clear();
  fin->Close();

  std::cout << "Done. QA PDF: " << pdfName.Data() << std::endl;
  if (kmfPdf.Length()) {
    std::cout << "Done. kstarMassFitCF PDF: " << kmfPdf.Data() << std::endl;
  }
  if (isCfSubWriteSidecar() && isMethod5Enabled()) {
    TString sidecar = TString(outDir) + anaName + "_checkHistAnaFemtoPhi_CFsub";
    if (jobid.Length()) sidecar += "_" + jobid;
    sidecar += ".root";
    std::cout << "Done. CFsub sidecar: " << sidecar.Data() << std::endl;
  }
}
