// checkHistAnaKminusXiFemto.C - QA + CF PDF for K--Xi- femto (mirrors checkHistAnaKXiFemto).
// Invoke via: ./script/singularity_checkHistAnaKminusXiFemto.sh <root_file> <mainconf_path>

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TLine.h>
#include <TMath.h>
#include <TString.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TGraphErrors.h>
#include <iostream>
#include <vector>
#include <limits.h>
#include <stdlib.h>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"
#include "cuts/FemtoConfig.h"

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

static void drawMassWindowLines(TH1* h, Double_t mLo, Double_t mHi, Color_t color = kRed) {
  if (!h || !gPad) return;
  gPad->Update();
  Double_t yMax = h->GetMaximum();
  if (yMax <= 0) yMax = 1.0;
  TLine* l1 = new TLine(mLo, 0.0, mLo, yMax);
  l1->SetLineColor(color);
  l1->SetLineStyle(2);
  l1->SetLineWidth(2);
  l1->Draw("same");
  TLine* l2 = new TLine(mHi, 0.0, mHi, yMax);
  l2->SetLineColor(color);
  l2->SetLineStyle(2);
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

static void drawKstarCF(TH1* hSE, TH1* hME) {
  if (!hSE || !hME) return;
  Int_t binLo = hSE->FindBin(0.6 + 1e-9);
  Int_t binHi = hSE->FindBin(1.0 - 1e-9);
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
    if (kstarVal > 0.5) break;  // draw up to 500 MeV/c

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
  gCF->SetTitle("Correlation Function C(k*);k* [GeV/c];C(k*)");
  gCF->SetMarkerStyle(20);
  gCF->SetMarkerSize(0.8);
  gCF->SetMarkerColor(kBlack);
  gCF->SetLineColor(kBlack);

  gCF->Draw("AP");
  gCF->GetHistogram()->SetMinimum(0.5);
  gCF->GetHistogram()->SetMaximum(1.8);
  gCF->GetXaxis()->SetRangeUser(0.0, 0.5);

  TLine* line = new TLine(0.0, 1.0, 0.5, 1.0);
  line->SetLineColor(kRed);
  line->SetLineStyle(2);
  line->Draw("same");
}

void checkHistAnaKminusXiFemto(const Char_t* rootFile,
                              const Char_t* anaNameArg = "auau19_anaKminusXiFemto",
                              const Char_t* mainconfPath = 0) {
  gROOT->SetBatch(kTRUE);
  gStyle->SetOptStat(0);
  gStyle->SetPalette(1);
  gStyle->SetTitleOffset(1.2, "x");
  gStyle->SetTitleOffset(1.4, "y");
  gStyle->SetPadLeftMargin(0.15);

  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  TString mainConfigPath;
  if (mainconfPath && strlen(mainconfPath) > 0) {
    mainConfigPath = mainconfPath;
    if (mainConfigPath(0) != '/') mainConfigPath = TString(pwd) + "/" + mainConfigPath;
  } else {
    mainConfigPath = TString(pwd) + "/config/mainconf/main_auau19_anaKminusXiFemto.yaml";
  }

  if (!ConfigManager::GetInstance().LoadConfig(mainConfigPath.Data())) {
    std::cerr << "ERROR: Failed to load config: " << mainConfigPath.Data() << std::endl;
    return;
  }

  TString anaName = ConfigManager::GetInstance().GetAnaName().c_str();
  if (anaName.IsNull() && anaNameArg) anaName = anaNameArg;
  if (anaName.IsNull()) {
    std::cerr << "ERROR: anaName empty" << std::endl;
    return;
  }

  const FemtoConfig& fc = ConfigManager::GetInstance().GetFemtoConfig();
  const Double_t xiMassMin = fc.xiMassMin;
  const Double_t xiMassMax = fc.xiMassMax;
  const Double_t xiSbLMin = fc.xiSidebandLeftMin;
  const Double_t xiSbLMax = fc.xiSidebandLeftMax;
  const Double_t xiSbRMin = fc.xiSidebandRightMin;
  const Double_t xiSbRMax = fc.xiSidebandRightMax;

  TFile* fin = TFile::Open(rootFile, "READ");
  if (!fin || fin->IsZombie()) {
    std::cerr << "ERROR: cannot open " << rootFile << std::endl;
    return;
  }

  TString figDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(figDir)) {
    gSystem->mkdir(figDir, kTRUE);
  }
  TString pdfName = figDir + anaName + "_checkHistAnaKminusXiFemto.pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;
  std::cout << "Xi signal: [" << xiMassMin << ", " << xiMassMax << "]"
            << " leftSB: [" << xiSbLMin << ", " << xiSbLMax << "]"
            << " rightSB: [" << xiSbRMin << ", " << xiSbRMax << "]" << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)rootFile);
  TString note = "Check histograms from run_anaKminusXiFemto.C (StKminusXiFemtoMaker).\n";
  note += Form("CF: C(k*) = (ME_norm/SE_norm)*(SE/ME), norm in k*=[0.6,1.0].\n");
  note += Form("Xi signal (red): [%.3f, %.3f]; leftSB (blue): [%.3f, %.3f]; rightSB (blue): [%.3f, %.3f].\n",
               xiMassMin, xiMassMax, xiSbLMin, xiSbLMax, xiSbRMin, xiSbRMax);
  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaKminusXiFemto.C", inputs, note.Data(), true, anaName.Data());

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: k* SE/ME + inclusive CF + vs Cent
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1);
  TH1* hSE = (TH1*)fin->Get("hKstarSE_km_xim");
  TH1* hME = (TH1*)fin->Get("hKstarME_km_xim");
  if (hSE) {
    hSE->SetLineColor(kRed);
    hSE->SetTitle("Same Event k* (K^{-}#Xi^{-});k* [GeV/c];Counts");
    hSE->Draw();
  }
  c1->cd(2);
  if (hME) {
    hME->SetLineColor(kBlue);
    hME->SetTitle("Mixed Event k* (K^{-}#Xi^{-});k* [GeV/c];Counts");
    hME->Draw();
  }
  c1->cd(3);
  if (hSE && hME) drawKstarCF(hSE, hME);
  c1->cd(4);
  h2 = (TH2*)fin->Get("hKstarSEVsCent_km_xim");
  if (h2) h2->Draw("colz");
  c1->cd(5);
  h2 = (TH2*)fin->Get("hKstarMEVsCent_km_xim");
  if (h2) h2->Draw("colz");
  c1->cd(6);
  h2 = (TH2*)fin->Get("hKm_Mass2VsP");
  if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 2: Xi mass (with cut lines) + K-/Xi QA
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hXi_InvMass");
  if (h1) {
    h1->Draw();
    drawMassWindowLines(h1, xiMassMin, xiMassMax, kRed);
    if (xiSbLMax > xiSbLMin) drawMassWindowLines(h1, xiSbLMin, xiSbLMax, kBlue);
    if (xiSbRMax > xiSbRMin) drawMassWindowLines(h1, xiSbRMin, xiSbRMax, kBlue);
  }
  c1->cd(2);
  h1 = (TH1*)fin->Get("hXi_Pt");
  if (h1) {
    gPad->SetLogy();
    h1->Draw();
  }
  c1->cd(3);
  h1 = (TH1*)fin->Get("hXi_Eta");
  if (h1) h1->Draw();
  c1->cd(4);
  h1 = (TH1*)fin->Get("hKm_Pt");
  if (h1) h1->Draw();
  c1->cd(5);
  h1 = (TH1*)fin->Get("hKm_NSigma");
  if (h1) h1->Draw();
  c1->cd(6);
  h1 = (TH1*)fin->Get("hNKm");
  if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 3: event QA
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hVz");
  if (h1) h1->Draw();
  c1->cd(2);
  h1 = (TH1*)fin->Get("hRefMult");
  if (h1) h1->Draw();
  c1->cd(3);
  h1 = (TH1*)fin->Get("hCentrality");
  if (h1) h1->Draw();
  c1->cd(4);
  h1 = (TH1*)fin->Get("hNXi");
  if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 4: left / right sideband CF
  TH1* hSE_L = (TH1*)fin->Get("hKstarSE_km_xim_leftSB");
  TH1* hME_L = (TH1*)fin->Get("hKstarME_km_xim_leftSB");
  TH1* hSE_R = (TH1*)fin->Get("hKstarSE_km_xim_rightSB");
  TH1* hME_R = (TH1*)fin->Get("hKstarME_km_xim_rightSB");
  if (hSE_L || hSE_R) {
    c1->Clear();
    c1->Divide(3, 2);
    c1->cd(1);
    if (hSE_L) {
      hSE_L->SetLineColor(kRed);
      hSE_L->SetTitle("SE k* left SB;k* [GeV/c];Counts");
      hSE_L->Draw();
    }
    c1->cd(2);
    if (hME_L) {
      hME_L->SetLineColor(kBlue);
      hME_L->SetTitle("ME k* left SB;k* [GeV/c];Counts");
      hME_L->Draw();
    }
    c1->cd(3);
    if (hSE_L && hME_L) drawKstarCF(hSE_L, hME_L);
    c1->cd(4);
    if (hSE_R) {
      hSE_R->SetLineColor(kRed);
      hSE_R->SetTitle("SE k* right SB;k* [GeV/c];Counts");
      hSE_R->Draw();
    }
    c1->cd(5);
    if (hME_R) {
      hME_R->SetLineColor(kBlue);
      hME_R->SetTitle("ME k* right SB;k* [GeV/c];Counts");
      hME_R->Draw();
    }
    c1->cd(6);
    if (hSE_R && hME_R) drawKstarCF(hSE_R, hME_R);
    c1->Print(pdfName);
  }

  // Page 5: combined left+right sideband CF
  TH1* hSE_LR = sumTwoHists(hSE_L, hSE_R, "hSE_km_xim_SBLR");
  TH1* hME_LR = sumTwoHists(hME_L, hME_R, "hME_km_xim_SBLR");
  if (hSE_LR && hME_LR) {
    c1->Clear();
    c1->Divide(2, 2);
    c1->cd(1);
    hSE_LR->SetLineColor(kRed);
    hSE_LR->SetTitle("SE k* SB-LR;k* [GeV/c];Counts");
    hSE_LR->Draw();
    c1->cd(2);
    hME_LR->SetLineColor(kBlue);
    hME_LR->SetTitle("ME k* SB-LR;k* [GeV/c];Counts");
    hME_LR->Draw();
    c1->cd(3);
    drawKstarCF(hSE_LR, hME_LR);
    c1->cd(4);
    if (hSE && hME) drawKstarCF(hSE, hME);
    c1->Print(pdfName);
  }

  // Page 6: centrality-dependent signal CF (cent9 0..8)
  TH2* h2SE_Cent = (TH2*)fin->Get("hKstarSEVsCent_km_xim");
  TH2* h2ME_Cent = (TH2*)fin->Get("hKstarMEVsCent_km_xim");
  if (h2SE_Cent && h2ME_Cent) {
    c1->Clear();
    c1->Divide(3, 3);
    for (Int_t ic = 0; ic < 9; ic++) {
      c1->cd(ic + 1);
      TString hnameSE = TString::Format("hSE_proj_cent%d", ic);
      TString hnameME = TString::Format("hME_proj_cent%d", ic);
      TH1D* hProjSE = h2SE_Cent->ProjectionX(hnameSE, ic + 1, ic + 1);
      TH1D* hProjME = h2ME_Cent->ProjectionX(hnameME, ic + 1, ic + 1);
      TString title = TString::Format("Cent Bin %d;k* [GeV/c];C(k*)", ic);
      hProjSE->SetTitle(title.Data());
      drawKstarCF(hProjSE, hProjME);
    }
    c1->Print(pdfName);
  }

  PdfHeader::ClosePdf(pdfName);
  delete c1;
  fin->Close();
  delete fin;
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
