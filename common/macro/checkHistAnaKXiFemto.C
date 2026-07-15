// checkHistAnaKXiFemto.C - Draw histograms from run_anaKXiFemto.C output and write PDF.
// Invoke via: ./script/singularity_checkHistAnaKXiFemto.sh <root_file> <mainconf_path>

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TLine.h>
#include <TMath.h>
#include <TObject.h>
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

static Bool_t gConfigLoaded = kFALSE;

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

static void drawCent9ConventionNote() {
  if (!gPad) return;
  TLatex* note = new TLatex();
  note->SetNDC(kTRUE);
  note->SetTextSize(0.028);
  note->SetTextColor(kBlue + 1);
  note->DrawLatex(0.12, 0.96, "cent9: StRefMultCorr (0=peripheral, 8=central)");
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

static Bool_t isHex32(const TString& s) {
  if (s.Length() != 32) return kFALSE;
  for (Int_t i = 0; i < 32; i++) {
    Char_t c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
      return kFALSE;
  }
  return kTRUE;
}

void checkHistAnaKXiFemto(const Char_t* inputRootFile,
                          const Char_t* anaNameArg = "auau19_anaKXiFemto",
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
    }
  }

  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaKXiFemto";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from run_anaKXiFemto.C (StKXiFemtoMaker output).\n";
  note += "K0short-Xi correlation function relative momentum distributions.\n";
  note += "Includes K0short and Xi invariant mass QA.\n";

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaKXiFemto.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: Femtoscopy Relative Momentum (k*)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1);
  TH1* hSE = (TH1*)fin->Get("hKstarSE_k0_xi");
  TH1* hME = (TH1*)fin->Get("hKstarME_k0_xi");
  if (hSE) {
    hSE->SetLineColor(kRed);
    hSE->SetTitle("Same Event k*;k* [GeV/c];Counts");
    hSE->Draw();
  }
  c1->cd(2);
  if (hME) {
    hME->SetLineColor(kBlue);
    hME->SetTitle("Mixed Event k*;k* [GeV/c];Counts");
    hME->Draw();
  }
  c1->cd(3);
  if (hSE && hME) {
    drawKstarCF(hSE, hME);
  }
  c1->cd(4); h2 = (TH2*)fin->Get("hKstarSEVsCent_k0_xi"); if (h2) h2->Draw("colz");
  c1->cd(5); h2 = (TH2*)fin->Get("hKstarMEVsCent_k0_xi"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 2: K0short Reconstruction QA
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hK0short_InvMass"); if (h1) {
    h1->Draw();
    gPad->Update();
    Double_t yMax = h1->GetMaximum();
    TLine* l1 = new TLine(0.482, 0.0, 0.482, yMax);
    l1->SetLineColor(kRed);
    l1->SetLineStyle(2);
    l1->SetLineWidth(2);
    l1->Draw("same");
    TLine* l2 = new TLine(0.513, 0.0, 0.513, yMax);
    l2->SetLineColor(kRed);
    l2->SetLineStyle(2);
    l2->SetLineWidth(2);
    l2->Draw("same");
  }
  c1->cd(2); h1 = (TH1*)fin->Get("hK0short_Pt"); if (h1) { gPad->SetLogy(); h1->Draw(); }
  c1->cd(3); h1 = (TH1*)fin->Get("hK0short_Eta"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hDCA12"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hDCAV0"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hCosPointing"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 3: Xi Reconstruction QA
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hXi_InvMass"); if (h1) {
    h1->Draw();
    gPad->Update();
    Double_t yMax = h1->GetMaximum();
    TLine* l1 = new TLine(1.312, 0.0, 1.312, yMax);
    l1->SetLineColor(kRed);
    l1->SetLineStyle(2);
    l1->SetLineWidth(2);
    l1->Draw("same");
    TLine* l2 = new TLine(1.332, 0.0, 1.332, yMax);
    l2->SetLineColor(kRed);
    l2->SetLineStyle(2);
    l2->SetLineWidth(2);
    l2->Draw("same");
  }
  c1->cd(2); h1 = (TH1*)fin->Get("hXi_Eta"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hXi_Pt"); if (h1) { gPad->SetLogy(); h1->Draw(); }
  c1->cd(4); h1 = (TH1*)fin->Get("hDCA_Cascade"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hCosPointing_Xi"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hNSigmaProton"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 4: Centrality-dependent Correlation Functions C(k*)
  TH2* h2SE_Cent = (TH2*)fin->Get("hKstarSEVsCent_k0_xi");
  TH2* h2ME_Cent = (TH2*)fin->Get("hKstarMEVsCent_k0_xi");
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

  // Page 5: Centrality-dependent K0short Invariant Mass
  TH2* h2K0_MassCent = (TH2*)fin->Get("hK0short_InvMass_vs_Cent9");
  if (h2K0_MassCent) {
    c1->Clear();
    c1->Divide(3, 3);
    for (Int_t ic = 0; ic < 9; ic++) {
      c1->cd(ic + 1);
      TString hname = TString::Format("hK0Mass_proj_cent%d", ic);
      TH1D* hProj = h2K0_MassCent->ProjectionX(hname, ic + 1, ic + 1);
      TString title = TString::Format("K0s Mass Cent %d;Mass [GeV/c^{2}];Counts", ic);
      hProj->SetTitle(title.Data());
      hProj->Draw();
      gPad->Update();
      Double_t yMax = hProj->GetMaximum();
      TLine* l1 = new TLine(0.482, 0.0, 0.482, yMax);
      l1->SetLineColor(kRed);
      l1->SetLineStyle(2);
      l1->SetLineWidth(2);
      l1->Draw("same");
      TLine* l2 = new TLine(0.513, 0.0, 0.513, yMax);
      l2->SetLineColor(kRed);
      l2->SetLineStyle(2);
      l2->SetLineWidth(2);
      l2->Draw("same");
    }
    c1->Print(pdfName);
  }

  // Page 6: Centrality-dependent Xi Invariant Mass
  TH2* h2Xi_MassCent = (TH2*)fin->Get("hXi_InvMass_vs_Cent9");
  if (h2Xi_MassCent) {
    c1->Clear();
    c1->Divide(3, 3);
    for (Int_t ic = 0; ic < 9; ic++) {
      c1->cd(ic + 1);
      TString hname = TString::Format("hXiMass_proj_cent%d", ic);
      TH1D* hProj = h2Xi_MassCent->ProjectionX(hname, ic + 1, ic + 1);
      TString title = TString::Format("Xi Mass Cent %d;Mass [GeV/c^{2}];Counts", ic);
      hProj->SetTitle(title.Data());
      hProj->Draw();
      gPad->Update();
      Double_t yMax = hProj->GetMaximum();
      TLine* l1 = new TLine(1.312, 0.0, 1.312, yMax);
      l1->SetLineColor(kRed);
      l1->SetLineStyle(2);
      l1->SetLineWidth(2);
      l1->Draw("same");
      TLine* l2 = new TLine(1.332, 0.0, 1.332, yMax);
      l2->SetLineColor(kRed);
      l2->SetLineStyle(2);
      l2->SetLineWidth(2);
      l2->Draw("same");
    }
    c1->Print(pdfName);
  }

  fin->Close();
  PdfHeader::ClosePdf(pdfName);
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
