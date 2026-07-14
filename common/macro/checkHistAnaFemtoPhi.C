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
#include <TLine.h>
#include <TMath.h>
#include <TObject.h>
#include <TString.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TEllipse.h>
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

static const Double_t kKstarHistXMin = 0.0;
static const Double_t kKstarHistXMax = 3.0;
static const Double_t kCfKstarXMin = 0.0;
static const Double_t kCfKstarXMax = 0.65;
static const Double_t kCfYMin = -0.5;
static const Double_t kCfYMax = 2.5;


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
      std::cout << "      lambda_sig nPoints=" << nLam << ", CF_bkg_me nPoints=" << nBkgMe
                << ", CF_genuine nPoints=" << nGen << "\n";
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
  note += "Re-run analysis after hist/Maker changes so new keys exist in the ROOT file.\n";

  std::map<std::string, TGraphErrors*> cfCache;
  std::vector<TH1*> centProjKeepAlive;
  populateCfCache(fin, cfCache);
  populateCfCentCache(fin, cfCache);
  populateCfSliceCaches(fin, cfCache);
  populatePurityGenuineCaches(fin, cfCache);

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

  // Topic 3: lambda_sig and CF_genuine QA (representative slices x channel bases)
  {
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

  PdfHeader::ClosePdf(pdfName);




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
}
