// checkHistAnaKXiFemto.C - QA + CF PDF for K0short-Xi femto (signal + Xi sidebands).
// Invoke via: ./script/singularity_checkHistAnaKXiFemto.sh <root_file> <mainconf_path>
// Display zooms below are QA-only (do not change Maker fills or YAML hist axes).

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH1F.h>
#include <TH2.h>
#include <TLine.h>
#include <TMath.h>
#include <TString.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TPaveText.h>
#include <TGraphErrors.h>
#include <iostream>
#include <vector>
#include <limits.h>
#include <stdlib.h>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"
#include "cuts/FemtoConfig.h"

// Display-only axis windows (FXT 3.9 kinematics / CF convention).
static const Double_t kCfDrawMax = 0.5;
static const Double_t kCfNormLo = 0.6;
static const Double_t kCfNormHi = 1.0;
static const Double_t kKstarDrawMax = 1.6;
static const Double_t kPtDrawMaxK0 = 5.0;
static const Double_t kPtDrawMaxXi = 6.0;
static const Double_t kEtaDrawMin = -2.5;
static const Double_t kEtaDrawMax = 0.5;
static const Double_t kOpeningAngleDrawMax = 1.5;
static const Double_t kDphiStarZoomMin = 3.05;
static const Double_t kDphiStarZoomMax = 3.16;

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

static Bool_t hasEntries(const TH1* h) {
  return h && h->GetEntries() > 0;
}

static void drawEmptyPad() {
  static Int_t seq = 0;
  ++seq;
  TH1F* h = new TH1F(Form("hDummyPad%d", seq), ";;", 1, 0.0, 1.0);
  h->SetDirectory(0);
  h->SetStats(0);
  h->SetLineColor(kWhite);
  h->SetMinimum(0.0);
  h->SetMaximum(1.0);
  h->GetXaxis()->SetNdivisions(0);
  h->GetYaxis()->SetNdivisions(0);
  h->GetXaxis()->SetLabelSize(0);
  h->GetYaxis()->SetLabelSize(0);
  h->GetXaxis()->SetTickLength(0);
  h->GetYaxis()->SetTickLength(0);
  h->Draw("AXIS");
}

static void drawWindowDottedLines(TH1* h, Double_t xLo, Double_t xHi, Color_t color = kRed) {
  if (!h || !gPad) return;
  gPad->Update();
  Double_t yMin = gPad->GetUymin();
  Double_t yMax = gPad->GetUymax();
  TLine* l1 = new TLine(xLo, yMin, xLo, yMax);
  l1->SetLineColor(color);
  l1->SetLineStyle(3);
  l1->SetLineWidth(2);
  l1->Draw("same");
  TLine* l2 = new TLine(xHi, yMin, xHi, yMax);
  l2->SetLineColor(color);
  l2->SetLineStyle(3);
  l2->SetLineWidth(2);
  l2->Draw("same");
}

static TH1* sumTwoHists(TH1* a, TH1* b, const char* name) {
  if (!a && !b) return 0;
  TH1* out = 0;
  if (a) {
    out = (TH1*)a->Clone(name);
    out->SetDirectory(0);
    if (b) out->Add(b);
  } else {
    out = (TH1*)b->Clone(name);
    out->SetDirectory(0);
  }
  return out;
}

static void drawKstarSpectrum(TH1* h, Color_t color, const char* title) {
  if (!hasEntries(h)) return;
  h->SetLineColor(color);
  h->SetTitle(title);
  h->GetXaxis()->SetRangeUser(0.0, kKstarDrawMax);
  h->GetYaxis()->SetNoExponent();
  h->SetMinimum(0);
  h->Draw();
}

static void drawKstarCF(TH1* hSE, TH1* hME, const char* title = 0) {
  if (!hasEntries(hSE) || !hasEntries(hME)) return;
  Int_t binLo = hSE->FindBin(kCfNormLo + 1e-9);
  Int_t binHi = hSE->FindBin(kCfNormHi - 1e-9);
  Double_t seNorm = hSE->Integral(binLo, binHi);
  Double_t meNorm = hME->Integral(binLo, binHi);
  if (seNorm <= 0 || meNorm <= 0) return;

  Double_t scale = meNorm / seNorm;

  std::vector<Double_t> x;
  std::vector<Double_t> y;
  std::vector<Double_t> ex;
  std::vector<Double_t> ey;

  for (Int_t ib = 1; ib <= hSE->GetNbinsX(); ++ib) {
    Double_t kstarVal = hSE->GetBinCenter(ib);
    if (kstarVal > kCfDrawMax) break;

    Double_t se = hSE->GetBinContent(ib);
    Double_t me = hME->GetBinContent(ib);
    if (me <= 0) continue;

    Double_t cf = scale * se / me;
    Double_t err = cf * TMath::Sqrt((se > 0 ? 1.0 / se : 0.0) + (me > 0 ? 1.0 / me : 0.0));

    x.push_back(kstarVal);
    y.push_back(cf);
    ex.push_back(0.0);
    ey.push_back(err);
  }

  if (x.empty()) return;

  TGraphErrors* gCF = new TGraphErrors((Int_t)x.size(), &x[0], &y[0], &ex[0], &ey[0]);
  TString cfTitle = title ? title : "C(k*) signal";
  gCF->SetTitle(cfTitle + Form(";k* [GeV/c];C(k*)  (norm [%.1f,%.1f])", kCfNormLo, kCfNormHi));
  gCF->SetMarkerStyle(20);
  gCF->SetMarkerSize(0.8);
  gCF->SetMarkerColor(kBlack);
  gCF->SetLineColor(kBlack);

  gCF->Draw("AP");

  Double_t ymin = 1.0;
  Double_t ymax = 1.0;
  Bool_t haveScale = kFALSE;
  for (size_t i = 0; i < y.size(); ++i) {
    if (ey[i] > 0.25) continue;
    if (!haveScale) {
      ymin = y[i];
      ymax = y[i];
      haveScale = kTRUE;
    } else {
      if (y[i] < ymin) ymin = y[i];
      if (y[i] > ymax) ymax = y[i];
    }
  }
  ymin -= 0.08;
  ymax += 0.08;
  if (ymin > 0.85) ymin = 0.85;
  if (ymax < 1.15) ymax = 1.15;
  if (ymin < 0.7) ymin = 0.7;
  if (ymax > 1.5) ymax = 1.5;

  gCF->GetHistogram()->SetMinimum(ymin);
  gCF->GetHistogram()->SetMaximum(ymax);
  gCF->GetXaxis()->SetRangeUser(0.0, kCfDrawMax);

  TLine* line = new TLine(0.0, 1.0, kCfDrawMax, 1.0);
  line->SetLineColor(kRed);
  line->SetLineStyle(2);
  line->Draw("same");
}

static void drawMixSamplerQA(TH1* h) {
  if (!hasEntries(h)) return;
  h->SetTitle("ME sampler QA;bin;weighted counts");
  const char* labels[] = {
      "att", "elig", "fill", "skip", "cap", "pCut", "eligDir", "emptyDir",
      "selEmp", "fFwd", "fRev", "eFwd", "eRev", "overlap", "sigWin"};
  const Int_t nLab = (Int_t)(sizeof(labels) / sizeof(labels[0]));
  for (Int_t i = 0; i < nLab && i < h->GetNbinsX(); ++i) {
    h->GetXaxis()->SetBinLabel(i + 1, labels[i]);
  }
  h->GetXaxis()->SetLabelSize(0.04);
  h->GetXaxis()->LabelsOption("v");
  gPad->SetLogy();
  if (h->GetMinimum() <= 0) h->SetMinimum(0.5);
  h->Draw("hist");
}

static Bool_t isHex32(const TString& s) {
  if (s.Length() != 32) return kFALSE;
  for (Int_t i = 0; i < s.Length(); i++) {
    Char_t c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return kFALSE;
  }
  return kTRUE;
}

void checkHistAnaKXiFemto(const Char_t* inputRootFile,
                          const Char_t* anaNameArg = "auau19_anaKXiFemto",
                          const Char_t* mainconfPath = 0) {
  gROOT->SetBatch(kTRUE);
  gStyle->SetOptStat(0);
  gStyle->SetPalette(1);
  gStyle->SetTitleOffset(1.2, "x");
  gStyle->SetTitleOffset(1.4, "y");
  gStyle->SetPadLeftMargin(0.15);
  gStyle->SetPadRightMargin(0.12);
  gStyle->SetPadBottomMargin(0.12);

  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  TString mainConfigPath;
  if (mainconfPath && strlen(mainconfPath) > 0) {
    mainConfigPath = mainconfPath;
    if (mainConfigPath(0) != '/') mainConfigPath = TString(pwd) + "/" + mainConfigPath;
  } else {
    mainConfigPath = TString(pwd) + "/config/mainconf/main_auau19_anaKXiFemto.yaml";
  }

  if (!ConfigManager::GetInstance().LoadConfig(mainConfigPath.Data())) {
    std::cerr << "ERROR: Failed to load config: " << mainConfigPath.Data() << std::endl;
    return;
  }

  TString anaName = ConfigManager::GetInstance().GetAnaName().c_str();
  if (anaName.IsNull() && anaNameArg) anaName = anaNameArg;

  TString jobid;
  TString base = gSystem->BaseName(inputRootFile);
  base.ReplaceAll(".root", "");
  std::vector<TString> tokens;
  for (Int_t i = 0; i < base.Length();) {
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
  if (anaName.IsNull()) anaName = "auau19_anaKXiFemto";
  const Bool_t isFxt = anaName.Contains("fxt") || anaName.Contains("3p9");

  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t k0MassMin = fc.k0MassMin;
  const Double_t k0MassMax = fc.k0MassMax;
  const Double_t xiMassMin = fc.xiMassMin;
  const Double_t xiMassMax = fc.xiMassMax;
  const Double_t xiSbLMin = fc.xiSidebandLeftMin;
  const Double_t xiSbLMax = fc.xiSidebandLeftMax;
  const Double_t xiSbRMin = fc.xiSidebandRightMin;
  const Double_t xiSbRMax = fc.xiSidebandRightMax;

  TFile* fin = TFile::Open(inputRootFile);
  if (!fin || fin->IsZombie()) {
    std::cerr << "Error: Cannot open file " << inputRootFile << std::endl;
    return;
  }

  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaKXiFemto";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;
  std::cout << "K0 signal: [" << k0MassMin << ", " << k0MassMax << "]"
            << " Xi signal: [" << xiMassMin << ", " << xiMassMax << "]"
            << " leftSB: [" << xiSbLMin << ", " << xiSbLMax << "]"
            << " rightSB: [" << xiSbRMin << ", " << xiSbRMax << "]" << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);
  TString note = "Check histograms from run_anaK0XiFxtFemto.C (StK0XiFxtFemtoMaker).\n";
  note += Form("CF draw to %.1f GeV/c; norm in k*=[%.1f,%.1f]. SE/ME k* drawn to %.1f.\n",
               kCfDrawMax, kCfNormLo, kCfNormHi, kKstarDrawMax);
  note += Form("K0 signal (red): [%.3f, %.3f]; Xi signal (red): [%.3f, %.3f]; "
               "Xi leftSB/rightSB (blue): [%.3f, %.3f] / [%.3f, %.3f].\n",
               k0MassMin, k0MassMax, xiMassMin, xiMassMax, xiSbLMin, xiSbLMax, xiSbRMin, xiSbRMax);
  note += "Overview page: K0 | Xi | SE k* (dotted lines = YAML signal mass windows).\n";
  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaKXiFemto.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;

  TH1* hSE = (TH1*)fin->Get("hKstarSE_k0_xi");
  TH1* hME = (TH1*)fin->Get("hKstarME_k0_xi");
  TH2* h2SE_Cent = (TH2*)fin->Get("hKstarSEVsCent_k0_xi");
  TH2* h2ME_Cent = (TH2*)fin->Get("hKstarMEVsCent_k0_xi");
  const Bool_t haveVsCent = hasEntries(h2SE_Cent) || hasEntries(h2ME_Cent);
  TH1* hMixQA = (TH1*)fin->Get("hMixSamplerQA");

  // Page 1: signal k* + CF
  if (haveVsCent) {
    c1->Clear();
    c1->Divide(3, 2);
    c1->cd(1);
    drawKstarSpectrum(hSE, kRed, "Same Event k* (K^{0}_{S}#Xi^{-});k* [GeV/c];Counts");
    c1->cd(2);
    drawKstarSpectrum(hME, kBlue, "Mixed Event k* (K^{0}_{S}#Xi^{-});k* [GeV/c];Counts");
    c1->cd(3);
    if (hSE && hME) drawKstarCF(hSE, hME, "C(k*) signal");
    c1->cd(4);
    if (hasEntries(h2SE_Cent)) h2SE_Cent->Draw("colz");
    c1->cd(5);
    if (hasEntries(h2ME_Cent)) h2ME_Cent->Draw("colz");
    c1->cd(6);
    drawMixSamplerQA(hMixQA);
    c1->Print(pdfName);
  } else {
    // Same 3-wide pad geometry as K0 | Xi | SE; dummy fills the third slot.
    c1->Clear();
    c1->SetCanvasSize(1800, 600);
    c1->Divide(3, 1);
    c1->cd(1);
    drawKstarSpectrum(hSE, kRed, "Same Event k* (K^{0}_{S}#Xi^{-});k* [GeV/c];Counts");
    c1->cd(2);
    drawKstarSpectrum(hME, kBlue, "Mixed Event k* (K^{0}_{S}#Xi^{-});k* [GeV/c];Counts");
    c1->cd(3);
    drawEmptyPad();
    c1->Print(pdfName);
  }

  // Page 2: K0 | Xi | SE k*  (dotted lines mark the pair-selection mass windows)
  TH1* hK0Mass = (TH1*)fin->Get("hK0short_InvMass");
  TH1* hXiMass = (TH1*)fin->Get("hXi_InvMass");
  c1->Clear();
  c1->SetCanvasSize(1800, 600);
  c1->Divide(3, 1);
  c1->cd(1);
  if (hasEntries(hK0Mass)) {
    hK0Mass->SetMinimum(0);
    hK0Mass->SetTitle("K^{0}_{S} inv. mass;M_{#pi^{+}#pi^{-}} [GeV/c^{2}];Counts");
    hK0Mass->Draw();
    drawWindowDottedLines(hK0Mass, k0MassMin, k0MassMax, kRed);
  }
  c1->cd(2);
  if (hasEntries(hXiMass)) {
    hXiMass->SetMinimum(0);
    hXiMass->SetTitle("#Xi^{-} inv. mass;M_{#Lambda#pi} [GeV/c^{2}];Counts");
    hXiMass->Draw();
    drawWindowDottedLines(hXiMass, xiMassMin, xiMassMax, kRed);
  }
  c1->cd(3);
  drawKstarSpectrum(hSE, kRed, "SE k* (K^{0}_{S}#Xi^{-});k* [GeV/c];Counts");
  c1->Print(pdfName);

  delete c1;
  c1 = new TCanvas("c1", "canvas", 1200, 800);

  if (!haveVsCent) {
    c1->Clear();
    c1->Divide(2, 1);
    c1->cd(1);
    if (hSE && hME) drawKstarCF(hSE, hME, "C(k*) signal");
    c1->cd(2);
    drawMixSamplerQA(hMixQA);
    c1->Print(pdfName);
  }

  // K0 / Xi pT and eta (2x2)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hK0short_Pt");
  if (hasEntries(h1)) {
    gPad->SetLogy();
    h1->GetXaxis()->SetRangeUser(0.0, kPtDrawMaxK0);
    h1->Draw();
  }
  c1->cd(2);
  h1 = (TH1*)fin->Get("hK0short_Eta");
  if (hasEntries(h1)) {
    if (isFxt) h1->GetXaxis()->SetRangeUser(kEtaDrawMin, kEtaDrawMax);
    h1->SetMinimum(0);
    h1->Draw();
  }
  c1->cd(3);
  h1 = (TH1*)fin->Get("hXi_Pt");
  if (hasEntries(h1)) {
    gPad->SetLogy();
    h1->GetXaxis()->SetRangeUser(0.0, kPtDrawMaxXi);
    h1->Draw();
  }
  c1->cd(4);
  h1 = (TH1*)fin->Get("hXi_Eta");
  if (hasEntries(h1)) {
    if (isFxt) h1->GetXaxis()->SetRangeUser(kEtaDrawMin, kEtaDrawMax);
    h1->SetMinimum(0);
    h1->Draw();
  }
  c1->Print(pdfName);

  // Page 4: left / right Xi SB CF
  TH1* hSE_L = (TH1*)fin->Get("hKstarSE_k0_xi_leftSB");
  TH1* hME_L = (TH1*)fin->Get("hKstarME_k0_xi_leftSB");
  TH1* hSE_R = (TH1*)fin->Get("hKstarSE_k0_xi_rightSB");
  TH1* hME_R = (TH1*)fin->Get("hKstarME_k0_xi_rightSB");
  if (hasEntries(hSE_L) || hasEntries(hSE_R)) {
    c1->Clear();
    c1->Divide(3, 2);
    c1->cd(1);
    drawKstarSpectrum(hSE_L, kRed, "SE k* Xi left SB;k* [GeV/c];Counts");
    c1->cd(2);
    drawKstarSpectrum(hME_L, kBlue, "ME k* Xi left SB;k* [GeV/c];Counts");
    c1->cd(3);
    if (hSE_L && hME_L) drawKstarCF(hSE_L, hME_L, "C(k*) Xi left SB");
    c1->cd(4);
    drawKstarSpectrum(hSE_R, kRed, "SE k* Xi right SB;k* [GeV/c];Counts");
    c1->cd(5);
    drawKstarSpectrum(hME_R, kBlue, "ME k* Xi right SB;k* [GeV/c];Counts");
    c1->cd(6);
    if (hSE_R && hME_R) drawKstarCF(hSE_R, hME_R, "C(k*) Xi right SB");
    c1->Print(pdfName);
  }

  // Page 5: SB-LR + signal CF
  TH1* hSE_LR = sumTwoHists(hSE_L, hSE_R, "hSE_k0_xi_SBLR");
  TH1* hME_LR = sumTwoHists(hME_L, hME_R, "hME_k0_xi_SBLR");
  if (hasEntries(hSE_LR) && hasEntries(hME_LR)) {
    c1->Clear();
    c1->Divide(2, 2);
    c1->cd(1);
    drawKstarSpectrum(hSE_LR, kRed, "SE k* Xi SB-LR;k* [GeV/c];Counts");
    c1->cd(2);
    drawKstarSpectrum(hME_LR, kBlue, "ME k* Xi SB-LR;k* [GeV/c];Counts");
    c1->cd(3);
    drawKstarCF(hSE_LR, hME_LR, "C(k*) Xi SB-LR");
    c1->cd(4);
    if (hSE && hME) drawKstarCF(hSE, hME, "C(k*) signal");
    c1->Print(pdfName);
  }

  // Cent-binned CF only when vsCent is actually filled
  if (hasEntries(h2SE_Cent) && hasEntries(h2ME_Cent)) {
    c1->Clear();
    c1->Divide(3, 3);
    for (Int_t ic = 0; ic < 9; ic++) {
      c1->cd(ic + 1);
      TString hnameSE = TString::Format("hSE_proj_cent%d", ic);
      TString hnameME = TString::Format("hME_proj_cent%d", ic);
      TH1D* hProjSE = h2SE_Cent->ProjectionX(hnameSE, ic + 1, ic + 1);
      TH1D* hProjME = h2ME_Cent->ProjectionX(hnameME, ic + 1, ic + 1);
      if (!hasEntries(hProjSE) || !hasEntries(hProjME)) continue;
      TString title = TString::Format("C(k*) cent bin %d", ic);
      drawKstarCF(hProjSE, hProjME, title.Data());
    }
    c1->Print(pdfName);
  }

  // |DeltaPhi*| : two-body pair CM => identically ~pi; zoom the spike
  TH1* hDpsSE = (TH1*)fin->Get("hDeltaPhiStarSE_k0_xi");
  TH1* hDpsME = (TH1*)fin->Get("hDeltaPhiStarME_k0_xi");
  if (hasEntries(hDpsSE) || hasEntries(hDpsME)) {
    c1->Clear();
    c1->Divide(2, 2);
    c1->cd(1);
    if (hasEntries(hDpsSE)) {
      hDpsSE->SetLineColor(kRed);
      hDpsSE->SetTitle("SE |#Delta#phi^{*}| (signal, zoom);|#Delta#phi^{*}| [rad];Counts");
      hDpsSE->GetXaxis()->SetRangeUser(kDphiStarZoomMin, kDphiStarZoomMax);
      hDpsSE->Draw();
    }
    c1->cd(2);
    if (hasEntries(hDpsME)) {
      hDpsME->SetLineColor(kBlue);
      hDpsME->SetTitle("ME |#Delta#phi^{*}| (signal, zoom);|#Delta#phi^{*}| [rad];Counts");
      hDpsME->GetXaxis()->SetRangeUser(kDphiStarZoomMin, kDphiStarZoomMax);
      hDpsME->Draw();
    }
    c1->cd(3);
    TH1* hDpsSE_L = (TH1*)fin->Get("hDeltaPhiStarSE_k0_xi_leftSB");
    if (hasEntries(hDpsSE_L)) {
      hDpsSE_L->SetLineColor(kRed);
      hDpsSE_L->SetTitle("SE |#Delta#phi^{*}| (left SB, zoom);|#Delta#phi^{*}| [rad];Counts");
      hDpsSE_L->GetXaxis()->SetRangeUser(kDphiStarZoomMin, kDphiStarZoomMax);
      hDpsSE_L->Draw();
    }
    c1->cd(4);
    TPaveText* noteDps = new TPaveText(0.12, 0.25, 0.88, 0.75, "NDC");
    noteDps->SetFillColor(0);
    noteDps->SetBorderSize(1);
    noteDps->SetTextAlign(12);
    noteDps->SetTextSize(0.035);
    noteDps->AddText("|#Delta#phi*| of parent momenta in the pair CM");
    noteDps->AddText("is identically #pi (two-body back-to-back).");
    noteDps->AddText("Display zoomed to the last bins; definition is a Maker TODO.");
    noteDps->Draw();
    c1->Print(pdfName);
  }

  // DeltaEta / opening angle / DeltaPhi lab (signal)
  TH1* hDeSE = (TH1*)fin->Get("hDeltaEtaSE_k0_xi");
  TH1* hDeME = (TH1*)fin->Get("hDeltaEtaME_k0_xi");
  TH1* hOaSE = (TH1*)fin->Get("hOpeningAngleSE_k0_xi");
  TH1* hOaME = (TH1*)fin->Get("hOpeningAngleME_k0_xi");
  TH1* hDpSE = (TH1*)fin->Get("hDeltaPhiLabSE_k0_xi");
  TH1* hDpME = (TH1*)fin->Get("hDeltaPhiLabME_k0_xi");
  if (hasEntries(hDeSE) || hasEntries(hOaSE) || hasEntries(hDpSE)) {
    c1->Clear();
    c1->Divide(3, 2);
    c1->cd(1);
    if (hasEntries(hDeSE)) {
      hDeSE->SetLineColor(kRed);
      hDeSE->SetTitle("SE #Delta#eta (lab);#Delta#eta;Counts");
      hDeSE->Draw();
    }
    c1->cd(2);
    if (hasEntries(hDeME)) {
      hDeME->SetLineColor(kBlue);
      hDeME->SetTitle("ME #Delta#eta (lab);#Delta#eta;Counts");
      hDeME->Draw();
    }
    c1->cd(3);
    if (hasEntries(hOaSE)) {
      hOaSE->SetLineColor(kRed);
      hOaSE->SetTitle("SE opening angle;#theta [rad];Counts");
      hOaSE->GetXaxis()->SetRangeUser(0.0, kOpeningAngleDrawMax);
      hOaSE->Draw();
    }
    c1->cd(4);
    if (hasEntries(hOaME)) {
      hOaME->SetLineColor(kBlue);
      hOaME->SetTitle("ME opening angle;#theta [rad];Counts");
      hOaME->GetXaxis()->SetRangeUser(0.0, kOpeningAngleDrawMax);
      hOaME->Draw();
    }
    c1->cd(5);
    if (hasEntries(hDpSE)) {
      hDpSE->SetLineColor(kRed);
      hDpSE->SetTitle("SE #Delta#phi (lab);#Delta#phi [rad];Counts");
      hDpSE->SetMinimum(0);
      hDpSE->Draw();
    }
    c1->cd(6);
    if (hasEntries(hDpME)) {
      hDpME->SetLineColor(kBlue);
      hDpME->SetTitle("ME #Delta#phi (lab);#Delta#phi [rad];Counts");
      hDpME->SetMinimum(0);
      hDpME->Draw();
    }
    c1->Print(pdfName);
  }

  // 2D proximity maps (SE signal)
  TH2* hEtaPhi = (TH2*)fin->Get("hDeltaEta_vs_DeltaPhiLabSE_k0_xi");
  TH2* hKstarDps = (TH2*)fin->Get("hKstar_vs_DeltaPhiStarSE_k0_xi");
  if (hasEntries(hEtaPhi) || hasEntries(hKstarDps)) {
    c1->Clear();
    c1->Divide(2, 1);
    c1->cd(1);
    if (hasEntries(hEtaPhi)) {
      gPad->SetLogz();
      hEtaPhi->Draw("colz");
    }
    c1->cd(2);
    if (hasEntries(hKstarDps)) {
      hKstarDps->GetYaxis()->SetRangeUser(kDphiStarZoomMin, kDphiStarZoomMax);
      hKstarDps->GetXaxis()->SetRangeUser(0.0, kKstarDrawMax);
      gPad->SetLogz();
      hKstarDps->Draw("colz");
    }
    c1->Print(pdfName);
  }

  // Shared-track QA: skip empty ME/K0-skip frames; print counts
  TH1* hShareSE = (TH1*)fin->Get("hShareRejectSE");
  TH1* hShareME = (TH1*)fin->Get("hShareRejectME");
  TH1* hK0Skip = (TH1*)fin->Get("hK0_SkippedSharedWithXi");
  if (hasEntries(hShareSE) || hasEntries(hShareME) || hasEntries(hK0Skip)) {
    c1->Clear();
    TPaveText* box = new TPaveText(0.18, 0.22, 0.82, 0.78, "NDC");
    box->SetFillColor(0);
    box->SetBorderSize(1);
    box->SetTextAlign(12);
    box->SetTextSize(0.035);
    box->AddText("Pair-level ShareTracks (this ROOT)");
    box->AddText(Form("SE k* pairs: %.0f", hasEntries(hSE) ? hSE->GetEntries() : 0.0));
    box->AddText(Form("SE share reject: %.0f", hasEntries(hShareSE) ? hShareSE->GetEntries() : 0.0));
    box->AddText(Form("ME share reject: %.0f  (0 expected: mixed events)",
                      hasEntries(hShareME) ? hShareME->GetEntries() : 0.0));
    if (hasEntries(hK0Skip)) {
      box->AddText(Form("K0 recon skip-shared: %.0f", hK0Skip->GetEntries()));
    } else {
      box->AddText("K0 skip-shared hist: empty here (K0 dummy ROOT).");
    }
    box->Draw();
    c1->Print(pdfName);
  }

  PdfHeader::ClosePdf(pdfName);
  delete c1;
  fin->Close();
  delete fin;
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
