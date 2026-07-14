// checkHistAnaFemtoPhiDeuteron.C - Draw histograms from run_anaFemtoPhiDeuteron.C output and write PDF.
// Invoke via: ./script/singularity_checkHistAnaFemtoPhiDeuteron.sh <root_file> <mainconf_path>
// Or: root4star -b -q 'analysis/run_checkHistAnaFemtoPhiDeuteron.C("rootfile/...","anaName","config/mainconf/...")'
// If input is anaName_jobid_merge.root, jobid (32 hex) is parsed and PDF becomes anaName_checkHistAnaFemtoPhiDeuteron_jobid.pdf.
// With config loaded, cut regions are overlaid on pre-cut histograms.

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>
#include <TLine.h>
#include <TMath.h>
#include <TObject.h>
#include <TString.h>
#include <TStyle.h>
#include <TLatex.h>
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

static Bool_t gConfigLoaded = kFALSE;

static void setDeuteronMass2AxisRange(TH1* h, Double_t ymax = 16.0) {
  if (!h) return;
  if (h->InheritsFrom("TH2")) {
    ((TH2*)h)->GetYaxis()->SetRangeUser(0.0, ymax);
  } else {
    h->GetXaxis()->SetRangeUser(0.0, ymax);
  }
}

// ROOT files booked before hist YAML relabel still carry proton fork titles; fix at draw time.
static void prepareDeuteronHist(TH1* h, const char* key) {
  if (!h || !key) return;
  TString k(key);
  if (k == "hDeuteron_Pt_PreFemtoCut")
    h->SetTitle("^{2}H p_{T} pre-femto cut;p_{T} [GeV/c];Counts");
  else if (k == "hDeuteron_Eta_PreFemtoCut")
    h->SetTitle("^{2}H #eta pre-femto cut;#eta;Counts");
  else if (k == "hDeuteron_NSigmaDeuteron_PreFemtoCut")
    h->SetTitle("^{2}H n#sigma_{d} pre-femto cut;n#sigma_{d};Counts");
  else if (k == "hDeuteron_Mass2_PreFemtoCut")
    h->SetTitle("^{2}H TOF m^{2} pre-femto cut;m^{2} [(GeV/c^{2})^{2}];Counts");
  else if (k == "hDeuteron_DCA_PreFemtoCut")
    h->SetTitle("^{2}H DCA pre-femto cut;DCA [cm];Counts");
  else if (k == "hDeuteron_Pt")
    h->SetTitle("^{2}H p_{T};p_{T} [GeV/c];Counts");
  else if (k == "hDeuteron_Eta")
    h->SetTitle("^{2}H #eta;#eta;Counts");
  else if (k == "hDeuteron_Phi")
    h->SetTitle("^{2}H #phi;#phi [rad];Counts");
  else if (k == "hDeuteron_NSigmaDeuteron")
    h->SetTitle("^{2}H n#sigma_{d};n#sigma_{d};Counts");
  else if (k == "hDeuteron_Mass2")
    h->SetTitle("^{2}H TOF m^{2};m^{2} [(GeV/c^{2})^{2}];Counts");
  else if (k == "hDeuteron_DCA")
    h->SetTitle("^{2}H DCA;DCA [cm];Counts");
  else if (k == "hDeuteron_Y_PreFemtoCut")
    h->SetTitle("^{2}H y_{cm} pre-femto cut;y_{cm};Counts");
  else if (k == "hDeuteron_PtVsY_PreFemtoCut")
    h->SetTitle("^{2}H p_{T} vs y_{cm} pre-femto;y_{cm};p_{T} [GeV/c]");
  else if (k == "hDeuteron_Y_FemtoCut")
    h->SetTitle("^{2}H y_{cm} after femto cut;y_{cm};Counts");
  else if (k == "hDeuteron_PtVsY_FemtoCut")
    h->SetTitle("^{2}H p_{T} vs y_{cm} femto cut;y_{cm};p_{T} [GeV/c]");
  else if (k == "hDeuteron_Mass2VsP")
    h->SetTitle("^{2}H TOF m^{2} vs p;p [GeV/c];m^{2}");
  else if (k == "hDeuteron_Mass2VsP_PreFemtoCut_wide")
    h->SetTitle("^{2}H TOF m^{2} vs p pre-femto (wide);p [GeV/c];m^{2}");
  else if (k == "hDeuteron_Mass2VsP_wide")
    h->SetTitle("^{2}H TOF m^{2} vs p femto cut (wide);p [GeV/c];m^{2}");
  else if (k == "hDeuteron_NHitsRatio_FemtoCut")
    h->SetTitle("^{2}H nHitsFit/nHitsMax (femto cut);ratio;Counts");
  else if (k == "hDeuteron_NCand")
    h->SetTitle("^{2}H candidates per event;N_{^{2}H};Counts");
  else if (k == "hNSigmaDeuteronVsP")
    h->SetTitle("n#sigma_{d} vs p (after ID cut);p [GeV/c];n#sigma_{d}");
  else if (k == "hNSigmaDeuteronVsP_All")
    h->SetTitle("n#sigma_{d} vs p (all tracks, no ID cut);p [GeV/c];n#sigma_{d}");
  if (k.Contains("Mass2")) {
    if (k.Contains("_wide")) setDeuteronMass2AxisRange(h, 20.0);
    else setDeuteronMass2AxisRange(h, 16.0);
  }
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
    h1->GetXaxis()->SetRangeUser(0.0, 0.65);
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
    std::cout << "[checkHistAnaFemtoPhiDeuteron] WARNING: cannot rebin " << h->GetName() << " with factor "
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
    std::cout << "[checkHistAnaFemtoPhiDeuteron] CF " << channel.c_str();
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
  std::cout << "[checkHistAnaFemtoPhiDeuteron] CF " << channel.c_str();
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
    std::cout << "[checkHistAnaFemtoPhiDeuteron] CF " << channel.c_str() << ": failed (empty norm integrals)\n";
  } else {
    std::cout << "[checkHistAnaFemtoPhiDeuteron] CF " << channel.c_str() << ": " << gCF->GetN()
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
  const char* fallbackChannels[] = {"phi_deuteron",         "phi_deuteron_signal", "phi_deuteron_leftSB",
                                    "phi_deuteron_rightSB", "phi_rot_deuteron",    0};
  for (Int_t i = 0; fallbackChannels[i]; ++i) {
    getOrComputeCf(fin, fallbackChannels[i], kFallbackNormQMin, kFallbackNormQMax, cfCache);
  }
}

static void populateCfCentCache(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache) {
  Int_t cent9Min = 0;
  Int_t cent9Max = 0;
  getCfCent9Range(cent9Min, cent9Max);
  const char* centChannels[] = {"phi_deuteron_signal", "phi_deuteron_leftSB", "phi_deuteron_rightSB", "phi_rot_deuteron", 0};
  for (Int_t i = 0; centChannels[i]; ++i) {
    const std::string channel(centChannels[i]);
    getOrComputeCfCentSlice(fin, channel, channelNormQMin(channel), channelNormQMax(channel), cent9Min, cent9Max,
                            cfCache);
  }
}

static const Double_t kCfKstarXMin = 0.0;
static const Double_t kCfKstarXMax = 0.65;

static void drawKstarSeMeHist(TH1* h) {
  if (!h) return;
  h->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
  h->Draw();
}

static void drawCentProjectedSeMe(TCanvas* canvas, Int_t pad, TFile* fin, const std::string& channel, Bool_t isSE,
                                  Int_t cent9Min, Int_t cent9Max, std::vector<TH1*>& centProjKeepAlive) {
  if (!canvas) return;
  canvas->cd(pad);
  TH1* h = getProjectedSeMeFromCent(fin, channel, isSE, cent9Min, cent9Max);
  if (h) {
    drawKstarSeMeHist(h);
    centProjKeepAlive.push_back(h);
  }
}

static void drawCfGraph(TGraphErrors* gCF) {
  if (!gCF) return;
  gCF->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
  gCF->Draw("AP");
  TH1* hFrame = gCF->GetHistogram();
  if (hFrame) hFrame->GetXaxis()->SetRangeUser(kCfKstarXMin, kCfKstarXMax);
}

static void drawCentSliceCf(TCanvas* canvas, Int_t pad, const std::string& channel,
                            std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->cd(pad);
  std::map<std::string, TGraphErrors*>::const_iterator it = cfCache.find(cfCentCacheKey(channel));
  if (it != cfCache.end() && it->second) drawCfGraph(it->second);
}

// Divide(4, 3): columns = channels, rows = SE / ME / CF.
static Int_t centSliceLayoutPad(Int_t col, Int_t rowSeMeCf) { return col + rowSeMeCf * 4 + 1; }

static void drawCentSlicePage(TCanvas* canvas, TFile* fin, Int_t cent9Min, Int_t cent9Max,
                              std::vector<TH1*>& centProjKeepAlive,
                              std::map<std::string, TGraphErrors*>& cfCache) {
  if (!canvas) return;
  canvas->Clear();
  canvas->Divide(4, 3);
  const char* channels[] = {"phi_deuteron_signal", "phi_deuteron_leftSB", "phi_deuteron_rightSB", "phi_rot_deuteron", 0};
  for (Int_t ic = 0; channels[ic]; ++ic) {
    const std::string channel(channels[ic]);
    drawCentProjectedSeMe(canvas, centSliceLayoutPad(ic, 0), fin, channel, kTRUE, cent9Min, cent9Max,
                          centProjKeepAlive);
    drawCentProjectedSeMe(canvas, centSliceLayoutPad(ic, 1), fin, channel, kFALSE, cent9Min, cent9Max,
                          centProjKeepAlive);
    drawCentSliceCf(canvas, centSliceLayoutPad(ic, 2), channel, cfCache);
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

static const char* kSidebandSliceChannels[] = {"phi_deuteron_signal", "phi_deuteron_leftSB", "phi_deuteron_rightSB", 0};

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

static Bool_t isSliceInCfPdf(const std::string& sliceId) {
  if (!isSliceInQaPdf(sliceId)) return kTRUE;
  if (!gConfigLoaded) return kFALSE;
  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  if (!femtoCfg.cfPdfExcludeQaSlices) return kTRUE;
  return kFALSE;
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
static Bool_t getSidebandWidthAlphas(Double_t& alphaL, Double_t& alphaR) {
  // Fallback matches default maker_auau3p85fxt_anaFemtoPhiDeuteron.yaml windows.
  alphaL = 0.014 / 0.015;
  alphaR = 0.014 / 0.025;
  if (!gConfigLoaded) return kFALSE;
  const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
  const FemtoConfig::ChannelDef* chSig = femtoCfg.FindChannel("phi_deuteron_signal");
  const FemtoConfig::ChannelDef* chL = femtoCfg.FindChannel("phi_deuteron_leftSB");
  const FemtoConfig::ChannelDef* chR = femtoCfg.FindChannel("phi_deuteron_rightSB");
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
                                             Int_t cent9Max, Double_t normQMin, Double_t normQMax,
                                             std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, "SBLR");
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  TH1* hSEL = getSliceProjectedSeMe(fin, "phi_deuteron_leftSB", kTRUE, cent9Min, cent9Max);
  TH1* hSER = getSliceProjectedSeMe(fin, "phi_deuteron_rightSB", kTRUE, cent9Min, cent9Max);
  TH1* hMEL = getSliceProjectedSeMe(fin, "phi_deuteron_leftSB", kFALSE, cent9Min, cent9Max);
  TH1* hMER = getSliceProjectedSeMe(fin, "phi_deuteron_rightSB", kFALSE, cent9Min, cent9Max);
  TH1* hSE = combineSidebandLR(hSEL, hSER);
  TH1* hME = combineSidebandLR(hMEL, hMER);
  delete hSEL;
  delete hSER;
  delete hMEL;
  delete hMER;
  TString logTag = Form("%s SBLR cent9 %d-%d", sliceId.c_str(), cent9Min, cent9Max);
  TGraphErrors* gCF =
      computeCfAndCache("phi_deuteron_SBLR", cacheKey, hSE, hME, normQMin, normQMax, cfCache, logTag.Data());
  delete hSE;
  delete hME;
  return gCF;
}

static TGraphErrors* getOrComputeSliceSigSubCf(TFile* fin, const std::string& sliceId, Int_t cent9Min,
                                               Int_t cent9Max, const std::string& sbChannel, const char* subTag,
                                               Double_t normQMin, Double_t normQMax,
                                               std::map<std::string, TGraphErrors*>& cfCache) {
  const std::string cacheKey = cfSliceCacheKey(sliceId, subTag);
  std::map<std::string, TGraphErrors*>::const_iterator cached = cfCache.find(cacheKey);
  if (cached != cfCache.end()) return cached->second;

  Double_t alphaL = 0.0;
  Double_t alphaR = 0.0;
  getSidebandWidthAlphas(alphaL, alphaR);
  Double_t alphaApply = 1.0;
  TH1* hSEsig = getSliceProjectedSeMe(fin, "phi_deuteron_signal", kTRUE, cent9Min, cent9Max);
  TH1* hMEsig = getSliceProjectedSeMe(fin, "phi_deuteron_signal", kFALSE, cent9Min, cent9Max);
  TH1* hSEsb = 0;
  TH1* hMEsb = 0;
  if (sbChannel == "SBLR") {
    TH1* hSEL = getSliceProjectedSeMe(fin, "phi_deuteron_leftSB", kTRUE, cent9Min, cent9Max);
    TH1* hSER = getSliceProjectedSeMe(fin, "phi_deuteron_rightSB", kTRUE, cent9Min, cent9Max);
    TH1* hMEL = getSliceProjectedSeMe(fin, "phi_deuteron_leftSB", kFALSE, cent9Min, cent9Max);
    TH1* hMER = getSliceProjectedSeMe(fin, "phi_deuteron_rightSB", kFALSE, cent9Min, cent9Max);
    hSEsb = combineSidebandScaledAverage(hSEL, hSER, alphaL, alphaR);
    hMEsb = combineSidebandScaledAverage(hMEL, hMER, alphaL, alphaR);
    delete hSEL;
    delete hSER;
    delete hMEL;
    delete hMER;
    alphaApply = 1.0;
  } else if (sbChannel == "phi_deuteron_leftSB") {
    hSEsb = getSliceProjectedSeMe(fin, sbChannel, kTRUE, cent9Min, cent9Max);
    hMEsb = getSliceProjectedSeMe(fin, sbChannel, kFALSE, cent9Min, cent9Max);
    alphaApply = alphaL;
  } else if (sbChannel == "phi_deuteron_rightSB") {
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
  TString logTag = Form("%s %s cent9 %d-%d alphaL=%.4f alphaR=%.4f apply=%.4f", sliceId.c_str(), subTag,
                        cent9Min, cent9Max, alphaL, alphaR, alphaApply);
  TGraphErrors* gCF = computeCfAndCache("phi_deuteron_signal", cacheKey, hSEcorr, hMEcorr, normQMin, normQMax,
                                        cfCache, logTag.Data());
  delete hSEcorr;
  delete hMEcorr;
  return gCF;
}

static void populateCfSliceCaches(TFile* fin, std::map<std::string, TGraphErrors*>& cfCache) {
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  const Double_t normQMin = channelNormQMin("phi_deuteron_signal");
  const Double_t normQMax = channelNormQMax("phi_deuteron_signal");
  for (size_t is = 0; is < slices.size(); ++is) {
    const FemtoConfig::CfCentSlice& sl = slices[is];
    for (Int_t ic = 0; kSidebandSliceChannels[ic]; ++ic) {
      const std::string channel(kSidebandSliceChannels[ic]);
      getOrComputeSliceChannelCf(fin, sl.id, sl.cent9Min, sl.cent9Max, channel, channelNormQMin(channel),
                                 channelNormQMax(channel), cfCache);
    }
    getOrComputeSliceSblrCf(fin, sl.id, sl.cent9Min, sl.cent9Max, normQMin, normQMax, cfCache);
    getOrComputeSliceSigSubCf(fin, sl.id, sl.cent9Min, sl.cent9Max, "phi_deuteron_leftSB", "CF_sig_sub_SBL",
                              normQMin, normQMax, cfCache);
    getOrComputeSliceSigSubCf(fin, sl.id, sl.cent9Min, sl.cent9Max, "phi_deuteron_rightSB", "CF_sig_sub_SBR",
                              normQMin, normQMax, cfCache);
    getOrComputeSliceSigSubCf(fin, sl.id, sl.cent9Min, sl.cent9Max, "SBLR", "CF_sig_sub_SBLR", normQMin,
                              normQMax, cfCache);
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

static void drawSidebandSlicePage(TCanvas* canvas, TFile* fin, const FemtoConfig::CfCentSlice& slice,
                                  std::vector<TH1*>& centProjKeepAlive,
                                  std::map<std::string, TGraphErrors*>& cfCache, Bool_t drawSubCfRow) {
  if (!canvas) return;
  canvas->Clear();
  const Int_t nRows = drawSubCfRow ? 4 : 3;
  canvas->Divide(4, nRows);
  const char* colTags[] = {"phi_deuteron_signal", "phi_deuteron_leftSB", "phi_deuteron_rightSB", "SBLR"};
  for (Int_t ic = 0; ic < 4; ++ic) {
    const std::string tag(colTags[ic]);
    if (tag == "SBLR") {
      TH1* hSEL = getSliceProjectedSeMe(fin, "phi_deuteron_leftSB", kTRUE, slice.cent9Min, slice.cent9Max);
      TH1* hSER = getSliceProjectedSeMe(fin, "phi_deuteron_rightSB", kTRUE, slice.cent9Min, slice.cent9Max);
      TH1* hMEL = getSliceProjectedSeMe(fin, "phi_deuteron_leftSB", kFALSE, slice.cent9Min, slice.cent9Max);
      TH1* hMER = getSliceProjectedSeMe(fin, "phi_deuteron_rightSB", kFALSE, slice.cent9Min, slice.cent9Max);
      TH1* hSE = combineSidebandLR(hSEL, hSER);
      TH1* hME = combineSidebandLR(hMEL, hMER);
      // Do not delete hSEL/hSER/hMEL/hMER: same ProjectionX objects as ic=1/2 (SB-L/R SE/ME panels).
      canvas->cd(sidebandSliceLayoutPad(ic, 0));
      if (hSE) {
        hSE->SetTitle(Form("SE SBLR %s", slice.id.c_str()));
        drawKstarSeMeHist(hSE);
        centProjKeepAlive.push_back(hSE);
      }
      canvas->cd(sidebandSliceLayoutPad(ic, 1));
      if (hME) {
        hME->SetTitle(Form("ME SBLR %s", slice.id.c_str()));
        drawKstarSeMeHist(hME);
        centProjKeepAlive.push_back(hME);
      }
      getOrComputeSliceSblrCf(fin, slice.id, slice.cent9Min, slice.cent9Max,
                              channelNormQMin("phi_deuteron_signal"), channelNormQMax("phi_deuteron_signal"),
                              cfCache);
      drawSliceCfGraph(canvas, sidebandSliceLayoutPad(ic, 2), slice.id, "SBLR", cfCache);
    } else {
      canvas->cd(sidebandSliceLayoutPad(ic, 0));
      TH1* hSE = getSliceProjectedSeMe(fin, tag, kTRUE, slice.cent9Min, slice.cent9Max);
      if (hSE) {
        drawKstarSeMeHist(hSE);
        centProjKeepAlive.push_back(hSE);
      }
      canvas->cd(sidebandSliceLayoutPad(ic, 1));
      TH1* hME = getSliceProjectedSeMe(fin, tag, kFALSE, slice.cent9Min, slice.cent9Max);
      if (hME) {
        drawKstarSeMeHist(hME);
        centProjKeepAlive.push_back(hME);
      }
      getOrComputeSliceChannelCf(fin, slice.id, slice.cent9Min, slice.cent9Max, tag, channelNormQMin(tag),
                                 channelNormQMax(tag), cfCache);
      drawSliceCfGraph(canvas, sidebandSliceLayoutPad(ic, 2), slice.id, tag, cfCache);
    }
  }
  if (drawSubCfRow) {
    const char* subTags[] = {"CF_sig_sub_SBL", "CF_sig_sub_SBR", "CF_sig_sub_SBLR"};
    const char* subSb[] = {"phi_deuteron_leftSB", "phi_deuteron_rightSB", "SBLR"};
    drawSliceCfGraph(canvas, sidebandSliceLayoutPad(0, 3), slice.id, "phi_deuteron_signal", cfCache);
    for (Int_t isb = 0; isb < 3; ++isb) {
      getOrComputeSliceSigSubCf(fin, slice.id, slice.cent9Min, slice.cent9Max, subSb[isb], subTags[isb],
                                channelNormQMin("phi_deuteron_signal"), channelNormQMax("phi_deuteron_signal"),
                                cfCache);
      drawSliceCfGraph(canvas, sidebandSliceLayoutPad(isb + 1, 3), slice.id, subTags[isb], cfCache);
    }
  }
  if (gPad) {
    TLatex* lat = new TLatex();
    lat->SetNDC(kTRUE);
    lat->SetTextSize(0.03);
    lat->DrawLatex(0.02, 0.98, Form("%s (cent9 %d-%d)", slice.id.c_str(), slice.cent9Min, slice.cent9Max));
  }
}

static void printCfSliceConsoleSummary(const std::map<std::string, TGraphErrors*>& cfCache) {
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSliceList();
  std::cout << "\n=== CF cent slices (15 default) ===\n";
  for (size_t is = 0; is < slices.size(); ++is) {
    const FemtoConfig::CfCentSlice& sl = slices[is];
    std::cout << "  slice " << sl.id << " cent9 [" << sl.cent9Min << "," << sl.cent9Max << "]:\n";
    for (Int_t ic = 0; kSidebandSliceChannels[ic]; ++ic) {
      const std::string channel(kSidebandSliceChannels[ic]);
      Int_t nPts = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, channel).c_str());
      TH1* hSE = 0;
      std::cout << "    " << channel << " raw CF nPoints=" << nPts << "\n";
      (void)hSE;
    }
    Int_t nSblr = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, "SBLR").c_str());
    std::cout << "    SBLR raw CF nPoints=" << nSblr << "\n";
    const char* subTags[] = {"CF_sig_sub_SBL", "CF_sig_sub_SBR", "CF_sig_sub_SBLR"};
    for (Int_t it = 0; it < 3; ++it) {
      Int_t nSub = getCachedCfPointCount(cfCache, cfSliceCacheKey(sl.id, subTags[it]).c_str());
      std::cout << "    " << subTags[it] << " nPoints=" << nSub << "\n";
    }
  }
  std::cout << "=============================================================\n\n";
}

void checkHistAnaFemtoPhiDeuteron(const Char_t* inputRootFile,
                                const Char_t* anaNameArg = "auau3p85fxt_anaFemtoPhiDeuteron",
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
        std::cerr << "[checkHistAnaFemtoPhiDeuteron] WARNING: FinalizeRapidityFrame failed; PDF note may be incomplete."
                  << std::endl;
      }
    } else if (mainconfPath && strlen(mainconfPath) > 0) {
      std::cerr << "[checkHistAnaFemtoPhiDeuteron] WARNING: Failed to load config " << mainconf.Data() << "; cut lines skipped." << std::endl;
    }
  }

  // Resolve symlinked share/figure to a physical path so ROOT PDF output
  // works the same in host and singularity-based runs.
  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaFemtoPhiDeuteron";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  TString pdfCfName = TString(outDir) + anaName + "_checkHistAnaFemtoPhiDeuteron_CF";
  if (jobid.Length()) pdfCfName += "_" + jobid;
  pdfCfName += ".pdf";
  std::cout << "Output QA PDF: " << pdfName.Data() << std::endl;
  std::cout << "Output CF PDF: " << pdfCfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from run_anaFemtoPhiDeuteron.C (StFemtoMaker output).\n";
  note += "Phi candidates built via ResonanceBuilder (KK); deuteron candidates via TrackPidBuilder (StNuclearIdHelper).\n";
  note += "Pair QA stage0 / strict TOF histograms mirror anaPhi hPhiPair_* naming.\n";
  const Double_t nEvtAll = getHistEntries(fin, "hVz");
  const Double_t nEvtAfter = getHistEntries(fin, "hVz_After");
  const Double_t nPhiCand = getHistEntries(fin, "hPhi_NCand");
  const Double_t nDeuteronCand = getHistEntries(fin, "hDeuteron_NCand");
  const Double_t nKstarSE = getHistEntries(fin, "hKstarSE_phi_deuteron");
  if (nEvtAll >= 0.0) {
    note += Form("Total statistics (input ROOT): events(all) = %.0f", nEvtAll);
    if (nEvtAfter >= 0.0) {
      note += Form(", events(after event cuts) = %.0f", nEvtAfter);
    }
    if (nPhiCand >= 0.0) {
      note += Form(", phi cand fills = %.0f", nPhiCand);
    }
    if (nDeuteronCand >= 0.0) {
      note += Form(", deuteron cand fills = %.0f", nDeuteronCand);
    }
    if (nKstarSE >= 0.0) {
      note += Form(", k* SE pairs = %.0f", nKstarSE);
    }
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
  note += "QA layout: pre-cut page immediately followed by post-cut page (Event, Track, deuteron, Phi).\n";
  note += "Phi QA: (A) KK pair Raw/AfterCuts + y-pT; (B) femto candidate pre/post. Kaon PID: before/after on Page 9.\n";
  note += Form("CF computed in checkHist from merged SE/ME (TGraphErrors, Poisson stat errors); cfRebinFactor=%d; norm region from maker YAML.\n",
               getCfRebinFactor());
  Int_t cfCent9MinNote = 2;
  Int_t cfCent9MaxNote = 8;
  getCfCent9Range(cfCent9MinNote, cfCent9MaxNote);
  note += Form("CF cent slice: cent9 [%d, %d] projected from hKstarSEVsCent/hKstarMEVsCent (0-60%% for default 2-8); "
               "layout 3x4 (rows SE/ME/CF, cols signal/leftSB/rightSB/rot).\n",
               cfCent9MinNote, cfCent9MaxNote);
  if (gConfigLoaded) {
    const CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
    if (centCfg.cent9MaxRefMultCorrBin >= 0 && centCfg.cent9MaxRefMultCorr > 0.0) {
      note += Form("Centrality cut (YAML): cent9=%d requires refMult_corr <= %.1f (Maker; re-run needed in ROOT).\n",
                   centCfg.cent9MaxRefMultCorrBin, centCfg.cent9MaxRefMultCorr);
    }
  }
  note += "phi_deuteron legacy channel has no hKstar*VsCent 2D; cent-slice CF pages skip it.\n";
  note += "Re-run analysis after hist/Maker changes so new keys exist in the ROOT file.\n";

  std::map<std::string, TGraphErrors*> cfCache;
  std::vector<TH1*> centProjKeepAlive;
  populateCfCache(fin, cfCache);
  populateCfCentCache(fin, cfCache);
  populateCfSliceCaches(fin, cfCache);

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaFemtoPhiDeuteron.C", inputs, note.Data(), true, anaName);

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
  c1->cd(8); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNDeuteron_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(9); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRawMult_vs_RefMultCorr"); if (h2) h2->Draw("colz");
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

  // Page 10: K multiplicity + deuteron n#sigma vs p (ID-cut vs all tracks)
  c1->Clear();
  c1->Divide(2, 3);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonPlusVsNKaonMinus"); if (h2) h2->Draw("colz");
  c1->cd(2); h1 = (TH1*)fin->Get("hNKaonPlus"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hNKaonMinus"); if (h1) h1->Draw();
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaDeuteronVsP"); if (h2) {
    prepareDeuteronHist(h2, "hNSigmaDeuteronVsP");
    h2->Draw("colz");
    if (gConfigLoaded) {
      const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
      drawCutLine2DH(h2, -femtoCfg.deuteronMaxAbsNSigma);
      drawCutLine2DH(h2, femtoCfg.deuteronMaxAbsNSigma);
    }
  }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaDeuteronVsP_All"); if (h2) {
    prepareDeuteronHist(h2, "hNSigmaDeuteronVsP_All");
    h2->Draw("colz");
  }
  c1->Print(pdfName);

  // Page 11a: deuteron QA 1D (pre-femto cut)
  c1->Clear();
  c1->Divide(3, 2);
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_Pt_PreFemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_Pt_PreFemtoCut");
      h1->Draw();
      drawCutLine1D(h1, femtoCfg.deuteronMinPtPre);
      drawCutLine1D(h1, femtoCfg.deuteronMaxPtPre);
      drawCutLine1D(h1, femtoCfg.deuteronMinPtPair);
      drawCutLine1D(h1, femtoCfg.deuteronMaxPtPair);
    }
    c1->cd(2); h1 = (TH1*)fin->Get("hDeuteron_Eta_PreFemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_Eta_PreFemtoCut");
      h1->Draw();
      drawCutLines1D(h1, -femtoCfg.deuteronMaxAbsEta, femtoCfg.deuteronMaxAbsEta);
    }
    c1->cd(3); h1 = (TH1*)fin->Get("hDeuteron_NSigmaDeuteron_PreFemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_NSigmaDeuteron_PreFemtoCut");
      h1->Draw();
      drawCutLines1D(h1, -femtoCfg.deuteronMaxAbsNSigma, femtoCfg.deuteronMaxAbsNSigma);
    }
    c1->cd(4); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_Mass2_PreFemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_Mass2_PreFemtoCut");
      h1->Draw();
      drawCutLines1D(h1, femtoCfg.deuteronMinMass2, femtoCfg.deuteronMaxMass2);
    }
    c1->cd(5); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_DCA_PreFemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_DCA_PreFemtoCut");
      h1->Draw();
      drawCutLine1D(h1, femtoCfg.deuteronMaxDca);
    }
    c1->cd(6); /* spare */;
  } else {
    c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_Pt_PreFemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_Pt_PreFemtoCut"); h1->Draw(); }
    c1->cd(2); h1 = (TH1*)fin->Get("hDeuteron_Eta_PreFemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_Eta_PreFemtoCut"); h1->Draw(); }
    c1->cd(3); h1 = (TH1*)fin->Get("hDeuteron_NSigmaDeuteron_PreFemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_NSigmaDeuteron_PreFemtoCut"); h1->Draw(); }
    c1->cd(4); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_Mass2_PreFemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_Mass2_PreFemtoCut"); h1->Draw(); }
    c1->cd(5); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_DCA_PreFemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_DCA_PreFemtoCut"); h1->Draw(); }
    c1->cd(6); /* spare */;
  }
  c1->Print(pdfName);

  // Page 11b: deuteron QA 1D (post-femto cut)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_Pt");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_Pt"); h1->Draw(); }
  c1->cd(2); h1 = (TH1*)fin->Get("hDeuteron_Eta");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_Eta"); h1->Draw(); }
  c1->cd(3); h1 = (TH1*)fin->Get("hDeuteron_Phi");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_Phi"); h1->Draw(); }
  c1->cd(4); h1 = (TH1*)fin->Get("hDeuteron_NSigmaDeuteron");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_NSigmaDeuteron"); h1->Draw(); }
  c1->cd(5); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_Mass2");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_Mass2"); h1->Draw(); }
  c1->cd(6); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDeuteron_DCA");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_DCA"); h1->Draw(); }
  c1->Print(pdfName);

  // Page 12a: deuteron y & y-pT (pre-femto cut)
  c1->Clear();
  c1->Divide(2, 1);
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    c1->cd(1); h1 = (TH1*)fin->Get("hDeuteron_Y_PreFemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_Y_PreFemtoCut");
      h1->Draw();
      drawCutLines1D(h1, femtoCfg.deuteronMinRapidityCm, femtoCfg.deuteronMaxRapidityCm);
    }
    c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_PtVsY_PreFemtoCut"); if (h2) {
      prepareDeuteronHist(h2, "hDeuteron_PtVsY_PreFemtoCut");
      h2->Draw("colz");
      drawCutLine2DH(h2, femtoCfg.deuteronMinRapidityCm);
      drawCutLine2DH(h2, femtoCfg.deuteronMaxRapidityCm);
    }
  } else {
    c1->cd(1); h1 = (TH1*)fin->Get("hDeuteron_Y_PreFemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_Y_PreFemtoCut"); h1->Draw(); }
    c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_PtVsY_PreFemtoCut");
    if (h2) { prepareDeuteronHist(h2, "hDeuteron_PtVsY_PreFemtoCut"); h2->Draw("colz"); }
  }
  c1->Print(pdfName);

  // Page 12b: deuteron y & y-pT (post-femto cut)
  c1->Clear();
  c1->Divide(3, 2);
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    c1->cd(1); h1 = (TH1*)fin->Get("hDeuteron_Y_FemtoCut"); if (h1) {
      prepareDeuteronHist(h1, "hDeuteron_Y_FemtoCut");
      h1->Draw();
      drawCutLines1D(h1, femtoCfg.deuteronMinRapidityCm, femtoCfg.deuteronMaxRapidityCm);
    }
    c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_PtVsY_FemtoCut"); if (h2) {
      prepareDeuteronHist(h2, "hDeuteron_PtVsY_FemtoCut");
      h2->Draw("colz");
      drawCutLine2DH(h2, femtoCfg.deuteronMinRapidityCm);
      drawCutLine2DH(h2, femtoCfg.deuteronMaxRapidityCm);
    }
  } else {
    c1->cd(1); h1 = (TH1*)fin->Get("hDeuteron_Y_FemtoCut");
    if (h1) { prepareDeuteronHist(h1, "hDeuteron_Y_FemtoCut"); h1->Draw(); }
    c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_PtVsY_FemtoCut");
    if (h2) { prepareDeuteronHist(h2, "hDeuteron_PtVsY_FemtoCut"); h2->Draw("colz"); }
  }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_Mass2VsP"); if (h2) {
    prepareDeuteronHist(h2, "hDeuteron_Mass2VsP");
    h2->Draw("colz");
    if (gConfigLoaded) {
      const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
      drawCutLine2DH(h2, femtoCfg.deuteronMinMass2);
      drawCutLine2DH(h2, femtoCfg.deuteronMaxMass2);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hDeuteron_NHitsRatio_FemtoCut");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_NHitsRatio_FemtoCut"); h1->Draw(); }
  c1->cd(5); /* spare */;
  c1->cd(6); /* spare */;
  c1->Print(pdfName);

  // Page 12c: deuteron TOF m2 vs p (wide 0-20)
  c1->Clear();
  c1->Divide(2, 1);
  if (gConfigLoaded) {
    const FemtoConfig& femtoCfg = ConfigManager::GetInstance().GetFemtoConfig();
    c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_Mass2VsP_PreFemtoCut_wide"); if (h2) {
      prepareDeuteronHist(h2, "hDeuteron_Mass2VsP_PreFemtoCut_wide");
      h2->Draw("colz");
      drawCutLine2DH(h2, femtoCfg.deuteronMinPMom);
      drawCutLine2DH(h2, femtoCfg.deuteronMaxPMom);
    }
    c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_Mass2VsP_wide"); if (h2) {
      prepareDeuteronHist(h2, "hDeuteron_Mass2VsP_wide");
      h2->Draw("colz");
      drawCutLine2DH(h2, femtoCfg.deuteronMinPMom);
      drawCutLine2DH(h2, femtoCfg.deuteronMaxPMom);
    }
  } else {
    c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_Mass2VsP_PreFemtoCut_wide");
    if (h2) { prepareDeuteronHist(h2, "hDeuteron_Mass2VsP_PreFemtoCut_wide"); h2->Draw("colz"); }
    c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeuteron_Mass2VsP_wide");
    if (h2) { prepareDeuteronHist(h2, "hDeuteron_Mass2VsP_wide"); h2->Draw("colz"); }
  }
  c1->Print(pdfName);

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

  // Page 15: Femto k* (legacy phi-deuteron channel)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hKstarSE_phi_deuteron"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(2); h1 = (TH1*)fin->Get("hKstarME_phi_deuteron"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(3);
  drawComputedCf(c1, 3, fin, "phi_deuteron", channelNormQMin("phi_deuteron"), channelNormQMax("phi_deuteron"), cfCache);
  c1->cd(4); h1 = (TH1*)fin->Get("hDeuteron_NCand");
  if (h1) { prepareDeuteronHist(h1, "hDeuteron_NCand"); h1->Draw(); }
  c1->Print(pdfName);

  // Page 16: Phi mass windows / sidebands / rotation
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhi_MKK_signal"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhi_MKK_leftSB"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhi_MKK_rightSB"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hPhi_MKK_rot"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hPhiRot_MKK"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hPhiRot_NCand"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 17: k* SE/ME/CF — signal channel
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hKstarSE_phi_deuteron_signal"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(2); h1 = (TH1*)fin->Get("hKstarME_phi_deuteron_signal"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(3);
  drawComputedCf(c1, 3, fin, "phi_deuteron_signal", channelNormQMin("phi_deuteron_signal"),
                 channelNormQMax("phi_deuteron_signal"), cfCache);
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hKstarSEVsCent_phi_deuteron_signal"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 18: k* SE/ME/CF — sideband channels
  c1->Clear();
  c1->Divide(2, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hKstarSE_phi_deuteron_leftSB"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(2); h1 = (TH1*)fin->Get("hKstarME_phi_deuteron_leftSB"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(3);
  drawComputedCf(c1, 3, fin, "phi_deuteron_leftSB", channelNormQMin("phi_deuteron_leftSB"),
                 channelNormQMax("phi_deuteron_leftSB"), cfCache);
  c1->cd(4); h1 = (TH1*)fin->Get("hKstarSE_phi_deuteron_rightSB"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(5); h1 = (TH1*)fin->Get("hKstarME_phi_deuteron_rightSB"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(6);
  drawComputedCf(c1, 6, fin, "phi_deuteron_rightSB", channelNormQMin("phi_deuteron_rightSB"),
                 channelNormQMax("phi_deuteron_rightSB"), cfCache);
  c1->Print(pdfName);

  // Page 19: k* SE/ME/CF — rotation background channel
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hKstarSE_phi_rot_deuteron"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(2); h1 = (TH1*)fin->Get("hKstarME_phi_rot_deuteron"); if (h1) drawKstarSeMeHist(h1);
  c1->cd(3);
  drawComputedCf(c1, 3, fin, "phi_rot_deuteron", channelNormQMin("phi_rot_deuteron"),
                 channelNormQMax("phi_rot_deuteron"), cfCache);
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hKstarSEVsCent_phi_rot_deuteron"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 20: k* SE/ME/CF — cent9 slice (default 0-60% = cent9 2-8), 3x4 (rows SE/ME/CF, cols per channel)
  Int_t cfCent9Min = 0;
  Int_t cfCent9Max = 0;
  getCfCent9Range(cfCent9Min, cfCent9Max);
  c1->SetCanvasSize(1800, 900);
  drawCentSlicePage(c1, fin, cfCent9Min, cfCent9Max, centProjKeepAlive, cfCache);
  c1->Print(pdfName);

  // QA representative slices (default pct_0_10, pct_0_20, pct_0_30): signal/SB-L/SB-R/SB-LR
  {
    const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
    const Int_t qaCanvasH = 1200;
    c1->SetCanvasSize(1800, qaCanvasH);
    for (size_t is = 0; is < allSlices.size(); ++is) {
      if (!isSliceInQaPdf(allSlices[is].id)) continue;
      drawSidebandSlicePage(c1, fin, allSlices[is], centProjKeepAlive, cfCache, kTRUE);
      c1->Print(pdfName);
    }
    c1->SetCanvasSize(1200, 800);
  }

  PdfHeader::ClosePdf(pdfName);

  // CF PDF: remaining centrality slices (SE/ME k* + raw/sub CF)
  {
    TString cfNote = "Multi-slice CF PDF from checkHistAnaFemtoPhiDeuteron.C.\n";
    Double_t cfAlphaL = 0.0;
    Double_t cfAlphaR = 0.0;
    getSidebandWidthAlphas(cfAlphaL, cfAlphaR);
    cfNote += Form("cfRebinFactor=%d; subCF alphaL=%.4f alphaR=%.4f (FemtoConfig mass-window width ratio); "
                   "SBLR uses (alphaL*L+alphaR*R)/2; negativeBinPolicy=%s.\n",
                   getCfRebinFactor(), cfAlphaL, cfAlphaR, getNegativeBinPolicy());
    cfNote += "Layout per slice: rows SE/ME/CF_raw/sub; cols signal/SB-L/SB-R/SB-LR.\n";
    if (gConfigLoaded && ConfigManager::GetInstance().GetFemtoConfig().cfPdfExcludeQaSlices) {
      cfNote += "Slices in cfCentSlicesQaPdfInclude are excluded (no duplicate with QA PDF).\n";
    }
    PdfHeader::OpenPdf(pdfCfName);
    PdfHeader::MakePdfHeaderPage(pdfCfName, "checkHistAnaFemtoPhiDeuteron.C", inputs, cfNote.Data(), true, anaName);
    const std::vector<FemtoConfig::CfCentSlice> allSlices = getCfCentSliceList();
    c1->SetCanvasSize(1800, 1200);
    for (size_t is = 0; is < allSlices.size(); ++is) {
      if (!isSliceInCfPdf(allSlices[is].id)) continue;
      drawSidebandSlicePage(c1, fin, allSlices[is], centProjKeepAlive, cfCache, kTRUE);
      c1->Print(pdfCfName);
    }
    c1->SetCanvasSize(1200, 800);
    PdfHeader::ClosePdf(pdfCfName);
  }

  // Console: verify key histograms exist and have entries
  {
    std::cout << "\n=== checkHistAnaFemtoPhiDeuteron: key histogram entries ===\n";
    const char* keys[] = {"hVz",
                           "hVz_After",
                           "hCentrality",
                           "hNSigmaKaon_Raw",
                           "hK_Pt",
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
                           "hPhiRot_MKK",
                           "hPhi_NCand",
                           "hDeuteron_Pt_PreFemtoCut",
                           "hDeuteron_Pt",
                           "hDeuteron_Y_PreFemtoCut",
                           "hDeuteron_Y_FemtoCut",
                           "hDeuteron_PtVsY_PreFemtoCut",
                           "hDeuteron_PtVsY_FemtoCut",
                           "hDeuteron_NCand",
                           "hDeuteron_Mass2VsP_PreFemtoCut_wide",
                           "hDeuteron_Mass2VsP_wide",
                           "hNSigmaDeuteronVsP_All",
                           "hMass2VsPt_TpcKaon",
                           "hDCAKK_All",
                           "hDCAKK_Pass",
                           "hKstarSE_phi_deuteron",
                           "hKstarME_phi_deuteron",
                           "hKstarSE_phi_deuteron_signal",
                           "hKstarME_phi_deuteron_signal",
                           "hKstarSE_phi_deuteron_leftSB",
                           "hKstarME_phi_deuteron_leftSB",
                           "hKstarSE_phi_deuteron_rightSB",
                           "hKstarME_phi_deuteron_rightSB",
                           "hKstarSE_phi_rot_deuteron",
                           "hKstarME_phi_rot_deuteron",
                           "hNDeuteron_vs_Cent9",
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
    const char* cfKeys[] = {"hCF_phi_deuteron",         "hCF_phi_deuteron_signal", "hCF_phi_deuteron_leftSB",
                            "hCF_phi_deuteron_rightSB", "hCF_phi_rot_deuteron",    0};
    const char* cfChannels[] = {"phi_deuteron", "phi_deuteron_signal", "phi_deuteron_leftSB", "phi_deuteron_rightSB",
                                "phi_rot_deuteron", 0};
    for (Int_t i = 0; cfKeys[i]; ++i) {
      Int_t nPts = getCachedCfPointCount(cfCache, cfChannels[i]);
      std::cout << "  " << cfKeys[i] << " (computed): nPoints=" << nPts;
      if (nPts < 1) std::cout << "  [empty — check SE/ME or norm region]";
      std::cout << "\n";
    }
    const char* cfCentChannels[] = {"phi_deuteron_signal", "phi_deuteron_leftSB", "phi_deuteron_rightSB", "phi_rot_deuteron",
                                     0};
    std::cout << Form("  CF cent slice (cent9 %d-%d, projected):\n", cfCent9Min, cfCent9Max);
    for (Int_t i = 0; cfCentChannels[i]; ++i) {
      Int_t nPts = getCachedCfPointCount(cfCache, cfCentCacheKey(cfCentChannels[i]).c_str());
      std::cout << "    " << cfCentChannels[i] << ": nPoints=" << nPts;
      if (nPts < 1) std::cout << "  [empty — check hKstar*VsCent in ROOT]";
      std::cout << "\n";
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
  std::cout << "Done. CF PDF: " << pdfCfName.Data() << std::endl;
}
