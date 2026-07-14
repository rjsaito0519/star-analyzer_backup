// checkHistAnaFemtoKaon.C - QA + CF from merged anaFemtoKaon ROOT output.
// Usage: ./script/singularity_checkHistAnaFemtoKaon.sh <merge.root> <mainconf>

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
#include <TLegend.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"
#include "cuts/FemtoConfig.h"

static Bool_t gConfigLoaded = kFALSE;

static const Double_t kKstarHistXMax = 2.0;
static const Double_t kCfKstarXMin = 0.0;
static const Double_t kCfKstarXMax = 0.65;

struct BachelorQaSpec {
  const char* histPrefix;
  const char* channelBase;
  const char* cutPrefix;
  const char* nCandKey;
  const char* nSigmaVsPKey;
  const char* nSigmaVsPAllKey;
  const char* nVsCentKey;
  const char* label;
};

static const BachelorQaSpec kBachelorQaSpecs[] = {
    {"hP_", "kaon_minus_proton", "proton", "hP_NCand", "hNSigmaProtonVsP", nullptr, "hNProton_vs_Cent9", "p"},
    {"hDeuteron_", "kaon_minus_deuteron", "deuteron", "hDeuteron_NCand", "hNSigmaDeuteronVsP",
     "hNSigmaDeuteronVsP_All", "hNDeuteron_vs_Cent9", "d"},
    {"hTriton_", "kaon_minus_triton", "triton", "hTriton_NCand", "hNSigmaTritonVsP", "hNSigmaTritonVsP_All",
     "hNTriton_vs_Cent9", "t"},
    {"hHe3_", "kaon_minus_he3", "he3", "hHe3_NCand", "hNSigmaHe3VsP", "hNSigmaHe3VsP_All", "hNHe3_vs_Cent9",
     "^{3}He"},
    {"hHe4_", "kaon_minus_he4", "he4", "hHe4_NCand", "hNSigmaHe4VsP", "hNSigmaHe4VsP_All", "hNHe4_vs_Cent9",
     "^{4}He"},
};
static const Int_t kNBachelorQaSpecs = sizeof(kBachelorQaSpecs) / sizeof(kBachelorQaSpecs[0]);

static const char* kChannelBases[] = {
    "kaon_minus_proton", "kaon_minus_deuteron", "kaon_minus_triton", "kaon_minus_he3", "kaon_minus_he4", nullptr};

static Bool_t loadConfig(const Char_t* mainconfPath) {
  if (!mainconfPath || strlen(mainconfPath) == 0) return kFALSE;
  gConfigLoaded = ConfigManager::GetInstance().LoadConfig(mainconfPath);
  return gConfigLoaded;
}

static Double_t channelNormQMin(const std::string& channel) {
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    const FemtoConfig::ChannelDef* ch = fc.FindChannel(channel);
    if (ch) return ch->normQMin;
  }
  return 0.5;
}

static Double_t channelNormQMax(const std::string& channel) {
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    const FemtoConfig::ChannelDef* ch = fc.FindChannel(channel);
    if (ch) return ch->normQMax;
  }
  return 1.0;
}

static Int_t getCfRebinFactor() {
  if (!gConfigLoaded) return 5;
  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  return (fc.cfRebinFactor >= 1) ? fc.cfRebinFactor : 1;
}

static TH1* rebinHistCopy(TH1* h, Int_t factor, const char* suffix) {
  if (!h || factor <= 1) return 0;
  if (h->GetNbinsX() % factor != 0) return 0;
  TH1* hRb = (TH1*)h->Clone(suffix ? suffix : "_rebinned");
  hRb->SetDirectory(0);
  hRb->Rebin(factor);
  return hRb;
}

static TGraphErrors* computeCfGraphFromSeMe(TH1* hSE, TH1* hME, Double_t normQMin, Double_t normQMax,
                                            const char* title) {
  if (!hSE || !hME) return 0;
  Int_t binLo = hSE->FindBin(normQMin + 1e-9);
  Int_t binHi = hSE->FindBin(normQMax - 1e-9);
  Double_t seNorm = hSE->Integral(binLo, binHi);
  Double_t meNorm = hME->Integral(binLo, binHi);
  if (seNorm <= 0 || meNorm <= 0) return 0;
  const Double_t scale = meNorm / seNorm;
  std::vector<Double_t> x, y, ey;
  for (Int_t ib = 1; ib <= hSE->GetNbinsX(); ++ib) {
    Double_t se = hSE->GetBinContent(ib);
    Double_t me = hME->GetBinContent(ib);
    if (se <= 0 || me <= 0) continue;
    Double_t cf = scale * se / me;
    x.push_back(hSE->GetBinCenter(ib));
    y.push_back(cf);
    ey.push_back(cf * TMath::Sqrt(1.0 / se + 1.0 / me));
  }
  if (x.empty()) return 0;
  TGraphErrors* g = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], 0, &ey[0]);
  g->SetTitle(title ? title : "C(k^{*})");
  g->SetMarkerStyle(20);
  g->SetMarkerSize(0.8);
  return g;
}

static TGraphErrors* getOrComputeCf(TFile* fin, const std::string& channel, Double_t normQMin, Double_t normQMax,
                                    std::map<std::string, TGraphErrors*>& cache) {
  if (cache.find(channel) != cache.end()) return cache[channel];
  TString seKey = Form("hKstarSE_%s", channel.c_str());
  TString meKey = Form("hKstarME_%s", channel.c_str());
  TH1* hSE = (TH1*)fin->Get(seKey);
  TH1* hME = (TH1*)fin->Get(meKey);
  const Int_t rebin = getCfRebinFactor();
  TH1* hSERb = rebinHistCopy(hSE, rebin, "_se_rb");
  TH1* hMERb = rebinHistCopy(hME, rebin, "_me_rb");
  TH1* hSEu = hSERb ? hSERb : hSE;
  TH1* hMEu = hMERb ? hMERb : hME;
  TString title = Form("CF %s;k^{*} [GeV/c];C(k^{*})", channel.c_str());
  TGraphErrors* g = computeCfGraphFromSeMe(hSEu, hMEu, normQMin, normQMax, title.Data());
  delete hSERb;
  delete hMERb;
  cache[channel] = g;
  return g;
}

static const Int_t kCfPanelsPerRow = 3;
static const Int_t kCfPanelsPerCol = 2;
static const Int_t kCfPanelsPerPage = kCfPanelsPerRow * kCfPanelsPerCol;

static const char* channelShortLabel(const char* channelBase) {
  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    if (strcmp(kBachelorQaSpecs[ib].channelBase, channelBase) == 0) return kBachelorQaSpecs[ib].label;
  }
  return channelBase;
}

static void drawCfGraph(TGraphErrors* g, Bool_t compact = kFALSE) {
  if (!g) return;
  g->GetXaxis()->SetLimits(kCfKstarXMin, kCfKstarXMax);
  g->SetMinimum(0.0);
  g->SetMaximum(2.0);
  if (compact) {
    g->SetTitle("");
    g->GetXaxis()->SetTitleSize(0.08);
    g->GetXaxis()->SetLabelSize(0.07);
    g->GetYaxis()->SetTitleSize(0.08);
    g->GetYaxis()->SetLabelSize(0.07);
    g->GetXaxis()->SetTitleOffset(0.9);
    g->GetYaxis()->SetTitleOffset(0.6);
    g->SetMarkerSize(0.6);
  } else {
    g->SetMarkerSize(0.8);
  }
  g->Draw("AP");
  TLine* one = new TLine(kCfKstarXMin, 1.0, kCfKstarXMax, 1.0);
  one->SetLineStyle(2);
  one->Draw();
}

static void drawCfPanelLabel(const char* text, Double_t x = 0.12, Double_t y = 0.88, Double_t textSize = 0.045) {
  TLatex* lat = new TLatex();
  lat->SetNDC();
  lat->SetTextSize(textSize);
  lat->DrawLatex(x, y, text);
}

static void beginCfGridPage(TCanvas* c, Int_t nPanels) {
  c->Clear();
  if (nPanels <= 1) {
    c->Divide(1, 1);
    return;
  }
  if (nPanels <= 2) {
    c->Divide(2, 1);
    return;
  }
  if (nPanels <= 4) {
    c->Divide(2, 2);
    return;
  }
  c->Divide(kCfPanelsPerRow, kCfPanelsPerCol);
}

static TH1* projectKstarVsCent(TH2* h2, Int_t cmin, Int_t cmax, const char* name) {
  if (!h2) return 0;
  Int_t yLo = h2->GetYaxis()->FindBin((Double_t)cmin - 0.01);
  Int_t yHi = h2->GetYaxis()->FindBin((Double_t)cmax + 0.01);
  TH1* h1 = h2->ProjectionX(name, yLo, yHi);
  if (h1) {
    h1->SetDirectory(0);
    h1->GetXaxis()->SetRangeUser(0.0, kKstarHistXMax);
  }
  return h1;
}

static TH1* getProjectedSeMeCent(TFile* fin, const std::string& channel, Bool_t isSE, Int_t cmin, Int_t cmax,
                                 const char* hname) {
  const char* kind = isSE ? "SE" : "ME";
  TH2* h2 = (TH2*)fin->Get(Form("hKstar%sVsCent_%s", kind, channel.c_str()));
  return projectKstarVsCent(h2, cmin, cmax, hname);
}

static TGraphErrors* getOrComputeCfCentSlice(TFile* fin, const std::string& channel, Int_t cmin, Int_t cmax,
                                             Double_t normQMin, Double_t normQMax,
                                             std::map<std::string, TGraphErrors*>& cache) {
  std::string key = channel + Form("_cent%d_%d", cmin, cmax);
  if (cache.find(key) != cache.end()) return cache[key];
  TH1* hSE = getProjectedSeMeCent(fin, channel, kTRUE, cmin, cmax, "_se_proj");
  TH1* hME = getProjectedSeMeCent(fin, channel, kFALSE, cmin, cmax, "_me_proj");
  TString title = Form("CF %s cent9 %d-%d;k^{*} [GeV/c];C(k^{*})", channel.c_str(), cmin, cmax);
  TGraphErrors* g = computeCfGraphFromSeMe(hSE, hME, normQMin, normQMax, title.Data());
  delete hSE;
  delete hME;
  cache[key] = g;
  return g;
}

static void drawKstarSeMeOverlayScaled(TH1* hSE, TH1* hME, Double_t normQMin, Double_t normQMax, Bool_t compact) {
  if (!hSE && !hME) return;

  Double_t meScale = 1.0;
  if (hSE && hME) {
    Int_t binLo = hSE->FindBin(normQMin + 1e-9);
    Int_t binHi = hSE->FindBin(normQMax - 1e-9);
    Double_t seNorm = hSE->Integral(binLo, binHi);
    Double_t meNorm = hME->Integral(binLo, binHi);
    if (seNorm > 0 && meNorm > 0) meScale = seNorm / meNorm;
  }

  TH1* hMEplot = hME;
  TH1* hMEscaled = 0;
  if (hME && meScale != 1.0) {
    hMEscaled = (TH1*)hME->Clone("_me_scaled");
    hMEscaled->SetDirectory(0);
    hMEscaled->Scale(meScale);
    hMEplot = hMEscaled;
  }

  if (hSE) {
    hSE->SetStats(0);
    hSE->SetLineColor(kBlack);
    hSE->SetLineWidth(compact ? 1 : 2);
    hSE->GetXaxis()->SetRangeUser(0.0, kKstarHistXMax);
    if (compact) {
      hSE->SetTitle("");
      hSE->GetXaxis()->SetLabelSize(0.07);
      hSE->GetYaxis()->SetLabelSize(0.07);
    }
    hSE->Draw("HIST");
  }
  if (hMEplot) {
    hMEplot->SetStats(0);
    hMEplot->SetLineColor(kRed);
    hMEplot->SetLineStyle(2);
    hMEplot->SetLineWidth(compact ? 1 : 2);
    hMEplot->GetXaxis()->SetRangeUser(0.0, kKstarHistXMax);
    if (hSE) hMEplot->Draw("HIST SAME");
    else hMEplot->Draw("HIST");
  }
  if (hSE && hMEplot && gPad) {
    TLegend* leg = new TLegend(0.52, 0.68, 0.88, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(compact ? 0.055 : 0.04);
    leg->AddEntry(hSE, "Same Event", "l");
    leg->AddEntry(hMEplot, "Mixed Event (scaled)", "l");
    leg->Draw();
  }
  delete hMEscaled;
}

static std::vector<FemtoConfig::CfCentSlice> getCfCentSlices() {
  if (gConfigLoaded) {
    const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
    if (!fc.cfCentSlices.empty()) return fc.cfCentSlices;
  }
  std::vector<FemtoConfig::CfCentSlice> out;
  FemtoConfig::CfCentSlice sl;
  sl.id = "cent9_8";
  sl.cent9Min = 8;
  sl.cent9Max = 8;
  out.push_back(sl);
  sl.id = "pct_0_10";
  sl.cent9Min = 7;
  sl.cent9Max = 8;
  out.push_back(sl);
  sl.id = "pct_0_20";
  sl.cent9Min = 6;
  sl.cent9Max = 8;
  out.push_back(sl);
  sl.id = "pct_0_30";
  sl.cent9Min = 5;
  sl.cent9Max = 8;
  out.push_back(sl);
  return out;
}

static void drawHist1(TFile* fin, const char* key, const char* opt = "") {
  TH1* h = (TH1*)fin->Get(key);
  if (h) {
    h->SetStats(0);
    h->Draw(opt && strlen(opt) ? opt : "");
  }
}

static void drawKaonMinusQa(TCanvas* c, TFile* fin, TString pdf) {
  c->Clear();
  c->Divide(2, 2);
  c->cd(1);
  gPad->SetLogy();
  drawHist1(fin, "hKm_Pt_PreFemtoCut");
  c->cd(2);
  drawHist1(fin, "hKm_Eta_PreFemtoCut");
  c->cd(3);
  drawHist1(fin, "hKm_NSigmaKaon_PreFemtoCut");
  c->cd(4);
  gPad->SetLogz();
  TH2* h2 = (TH2*)fin->Get("hKm_PtVsY_PreFemtoCut");
  if (h2) h2->Draw("colz");
  c->Print(pdf);
  c->Clear();
  c->Divide(2, 2);
  c->cd(1);
  gPad->SetLogy();
  drawHist1(fin, "hKm_Pt");
  c->cd(2);
  drawHist1(fin, "hKm_NCand");
  c->cd(3);
  gPad->SetLogz();
  h2 = (TH2*)fin->Get("hKm_Mass2VsP");
  if (h2) h2->Draw("colz");
  c->cd(4);
  drawHist1(fin, "hNKaonMinusFemto");
  c->Print(pdf);
}

static void drawBachelorKstar(TCanvas* c, TFile* fin, const char* channel, TString pdf) {
  c->Clear();
  c->Divide(2, 1);
  c->cd(1);
  gPad->SetLogy();
  drawHist1(fin, Form("hKstarSE_%s", channel));
  c->cd(2);
  gPad->SetLogy();
  drawHist1(fin, Form("hKstarME_%s", channel));
  c->Print(pdf);
}

static void drawAllInclusiveCfPages(TCanvas* c, TFile* fin, TString pdf, std::map<std::string, TGraphErrors*>& cache) {
  std::vector<const char*> channels;
  for (Int_t ic = 0; kChannelBases[ic]; ++ic) channels.push_back(kChannelBases[ic]);

  for (size_t i0 = 0; i0 < channels.size(); i0 += kCfPanelsPerPage) {
    const size_t nOnPage = TMath::Min((size_t)kCfPanelsPerPage, channels.size() - i0);
    beginCfGridPage(c, (Int_t)nOnPage);
    for (size_t ip = 0; ip < nOnPage; ++ip) {
      const char* channel = channels[i0 + ip];
      c->cd((Int_t)ip + 1);
      const Double_t qmin = channelNormQMin(channel);
      const Double_t qmax = channelNormQMax(channel);
      TGraphErrors* g = getOrComputeCf(fin, channel, qmin, qmax, cache);
      drawCfGraph(g, kTRUE);
      drawCfPanelLabel(Form("K^{-}-%s inclusive (norm %.2f-%.2f)", channelShortLabel(channel), qmin, qmax));
    }
    c->Print(pdf);
  }
}

static void drawCentSliceAllChannelsPage(TCanvas* c, TFile* fin, const FemtoConfig::CfCentSlice& sl, TString pdf,
                                         std::map<std::string, TGraphErrors*>& cache) {
  std::vector<const char*> channels;
  for (Int_t ic = 0; kChannelBases[ic]; ++ic) channels.push_back(kChannelBases[ic]);

  beginCfGridPage(c, (Int_t)channels.size());
  for (size_t ip = 0; ip < channels.size(); ++ip) {
    const char* channel = channels[ip];
    c->cd((Int_t)ip + 1);
    const Double_t qmin = channelNormQMin(channel);
    const Double_t qmax = channelNormQMax(channel);
    TGraphErrors* g = getOrComputeCfCentSlice(fin, channel, sl.cent9Min, sl.cent9Max, qmin, qmax, cache);
    drawCfGraph(g, kTRUE);
    drawCfPanelLabel(Form("K^{-}-%s %s (cent9 %d-%d)", channelShortLabel(channel), sl.id.c_str(), sl.cent9Min,
                          sl.cent9Max));
  }
  c->Print(pdf);
}

static void drawCentSliceSeMeOverlayPage(TCanvas* c, TFile* fin, const FemtoConfig::CfCentSlice& sl, TString pdf,
                                         std::vector<TH1*>& keepAlive) {
  std::vector<const char*> channels;
  for (Int_t ic = 0; kChannelBases[ic]; ++ic) channels.push_back(kChannelBases[ic]);

  beginCfGridPage(c, (Int_t)channels.size());
  for (size_t ip = 0; ip < channels.size(); ++ip) {
    const char* channel = channels[ip];
    c->cd((Int_t)ip + 1);
    const Double_t qmin = channelNormQMin(channel);
    const Double_t qmax = channelNormQMax(channel);
    TString tag = Form("_%s_cent%d_%d", channel, sl.cent9Min, sl.cent9Max);
    TH1* hSE = getProjectedSeMeCent(fin, channel, kTRUE, sl.cent9Min, sl.cent9Max, (tag + "_se").Data());
    TH1* hME = getProjectedSeMeCent(fin, channel, kFALSE, sl.cent9Min, sl.cent9Max, (tag + "_me").Data());
    if (hSE) keepAlive.push_back(hSE);
    if (hME) keepAlive.push_back(hME);
    drawKstarSeMeOverlayScaled(hSE, hME, qmin, qmax, kTRUE);
    drawCfPanelLabel(Form("K^{-}-%s %s (cent9 %d-%d)", channelShortLabel(channel), sl.id.c_str(), sl.cent9Min,
                          sl.cent9Max));
  }
  c->Print(pdf);
}

void checkHistAnaFemtoKaon(const Char_t* rootFile,
                           const Char_t* anaNameArg = "auau3p85fxt_anaFemtoKaon",
                           const Char_t* mainconfPath = 0)
{
  gStyle->SetOptStat(0);
  if (mainconfPath) loadConfig(mainconfPath);

  TFile* fin = TFile::Open(rootFile, "READ");
  if (!fin || fin->IsZombie()) {
    std::cerr << "ERROR: cannot open " << rootFile << std::endl;
    return;
  }

  TString anaName = anaNameArg;
  TString jobid = "0";
  TString base = gSystem->BaseName(rootFile);
  Int_t mergePos = base.Index("_merge.root");
  if (mergePos > 0) {
    TString stem = base(0, mergePos);
    Int_t us = stem.Last('_');
    if (us > 0 && us < stem.Length() - 1) jobid = stem(us + 1, stem.Length() - us - 1);
  }

  const char* figRoot = gSystem->Getenv("STAR_QA_FIGURE_ROOT");
  TString outDir = figRoot ? TString(figRoot) + "/" + anaName : TString("share/figure/") + anaName;
  gSystem->mkdir(outDir.Data(), kTRUE);
  TString pdfName = outDir + "/" + anaName + "_checkHistAnaFemtoKaon_" + jobid + ".pdf";

  PdfHeader::OpenPdf(pdfName);

  TString note = "K^{-}-(p,d,t,^{3}He,^{4}He) femto QA + CF from merged SE/ME (purity=1.0, no sidebands).\n";
  note += "CF computed in checkHist; norm region from maker YAML per channel.\n";
  note += "Cent slices: cent9 8-8, 7-8, 6-8, 5-8 (5 channels per page: SE + norm-scaled ME overlay, then CF).\n";
  std::vector<std::string> inputs;
  inputs.push_back(rootFile);
  if (mainconfPath) inputs.push_back(mainconfPath);
  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaFemtoKaon.C", inputs, note.Data(), kTRUE, anaName);

  TCanvas* c1 = new TCanvas("c1", "c1", 800, 600);

  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1);
  drawHist1(fin, "hVz_After");
  c1->cd(2);
  drawHist1(fin, "hRefMult_After");
  c1->cd(3);
  drawHist1(fin, "hCentrality");
  c1->cd(4);
  drawHist1(fin, "hNTracks");
  c1->Print(pdfName);

  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1);
  drawHist1(fin, "hPt");
  c1->cd(2);
  drawHist1(fin, "hEta");
  c1->cd(3);
  gPad->SetLogz();
  TH2* h2 = (TH2*)fin->Get("hNSigmaKaonVsP");
  if (h2) h2->Draw("colz");
  c1->cd(4);
  h2 = (TH2*)fin->Get("hMass2VsP");
  if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  drawKaonMinusQa(c1, fin, pdfName);

  for (Int_t ib = 0; ib < kNBachelorQaSpecs; ++ib) {
    const BachelorQaSpec& spec = kBachelorQaSpecs[ib];
    c1->Clear();
    c1->Divide(2, 2);
    c1->cd(1);
    gPad->SetLogy();
    drawHist1(fin, Form("%sPt_PreFemtoCut", spec.histPrefix));
    c1->cd(2);
    drawHist1(fin, spec.nSigmaVsPKey);
    c1->cd(3);
    drawHist1(fin, spec.nCandKey);
    c1->cd(4);
    gPad->SetLogz();
    h2 = (TH2*)fin->Get(Form("%sMass2VsP", spec.histPrefix));
    if (h2) h2->Draw("colz");
    TLatex* lat = new TLatex();
    lat->SetNDC();
    lat->DrawLatex(0.12, 0.92, Form("Bachelor %s (%s)", spec.label, spec.channelBase));
    c1->Print(pdfName);
  }

  std::map<std::string, TGraphErrors*> cfCache;

  for (Int_t ic = 0; kChannelBases[ic]; ++ic) {
    drawBachelorKstar(c1, fin, kChannelBases[ic], pdfName);
  }

  c1->SetCanvasSize(1800, 900);
  drawAllInclusiveCfPages(c1, fin, pdfName, cfCache);

  std::vector<TH1*> centProjKeepAlive;
  const std::vector<FemtoConfig::CfCentSlice> slices = getCfCentSlices();
  for (size_t is = 0; is < slices.size(); ++is) {
    drawCentSliceSeMeOverlayPage(c1, fin, slices[is], pdfName, centProjKeepAlive);
    drawCentSliceAllChannelsPage(c1, fin, slices[is], pdfName, cfCache);
  }
  for (size_t ih = 0; ih < centProjKeepAlive.size(); ++ih) delete centProjKeepAlive[ih];
  c1->SetCanvasSize(800, 600);

  PdfHeader::ClosePdf(pdfName);
  std::cout << "Wrote PDF: " << pdfName.Data() << std::endl;
  fin->Close();
  delete fin;
  delete c1;
}
