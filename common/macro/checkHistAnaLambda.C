// checkHistAnaLambda.C - Draw histograms from run_anaLambda.C output and write PDF.
// Invoke via: ./script/checkHistAnaLambda.sh <root_file> <mainconf_path>
// Or: root4star -b -q 'analysis/run_checkHistAnaLambda.C("rootfile/...","anaName","config/mainconf/main_anaName.yaml")'

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
#include <iostream>
#include <vector>
#include <limits.h>
#include <stdlib.h>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"
#include "cuts/LambdaCutConfig.h"

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

static void drawCutLine1D(TH1* h, Double_t x, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t ylo = gPad->GetUymin();
  Double_t yhi = gPad->GetUymax();
  TLine* l = new TLine(x, ylo, x, yhi);
  l->SetLineColor(color);
  l->SetLineStyle(style);
  l->Draw("same");
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

void checkHistAnaLambda(const Char_t* inputRootFile,
                        const Char_t* anaNameArg = "auau19_anaLambda",
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
    } else if (mainconfPath && strlen(mainconfPath) > 0) {
      std::cerr << "[checkHistAnaLambda] WARNING: Failed to load config " << mainconf.Data() << "; cut lines skipped." << std::endl;
    }
  }

  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaLambda";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from run_anaLambda.C (StLambdaMaker output).\n";
  note += "Lambda V0 (p + pi-) helix reconstruction.\n";
  note += "Centrality QA: Pages 1b-1d when centrality is enabled in mainconf.\n";

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaLambda.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: Event level (Lambda)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hRefMult"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hLambda_InvMass"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hN"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 1b: Centrality QA
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hCentrality"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hCentralityRaw"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hCentrality16"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMultCorr"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hRefMultWeight"); if (h1) h1->Draw();
  c1->cd(6); h2 = (TH2*)fin->Get("hRefMultVsNTOFMatch"); if (h2) h2->Draw("colz");
  c1->cd(7); h2 = (TH2*)fin->Get("hRefMultVsNTOFMatchAfter"); if (h2) h2->Draw("colz");
  c1->cd(8); h2 = (TH2*)fin->Get("hCentralityVsVz"); if (h2) h2->Draw("colz");
  c1->cd(9); h1 = (TH1*)fin->Get("hRawMult"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 1c: Centrality correlations
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h2 = (TH2*)fin->Get("hRawMult_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(2); h2 = (TH2*)fin->Get("hRefMultCorr_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(3); h2 = (TH2*)fin->Get("hNTracks_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(4); h2 = (TH2*)fin->Get("hTofMatchMult_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(5); h2 = (TH2*)fin->Get("hNProtonCand_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(6); h2 = (TH2*)fin->Get("hNPionCand_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(7); h2 = (TH2*)fin->Get("hNLambdaPairs_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(8); h2 = (TH2*)fin->Get("hLambda_InvMass_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(9); h2 = (TH2*)fin->Get("hLambda_InvMass_vs_RefMultCorr"); if (h2) h2->Draw("colz");
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 1d: Lambda InvMass per centrality bin
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin0"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin1"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin2"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin3"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin4"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin5"); if (h1) h1->Draw();
  c1->cd(7); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin6"); if (h1) h1->Draw();
  c1->cd(8); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin7"); if (h1) h1->Draw();
  c1->cd(9); h1 = (TH1*)fin->Get("hLambda_InvMass_CentBin8"); if (h1) h1->Draw();
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 2: Lambda kinematics and quality
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hLambda_Pt"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hLambda_Eta"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hLambda_Phi"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hDCA12"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.maxDaughterDCA);
    }
  }
  c1->cd(5); h1 = (TH1*)fin->Get("hDCAV0"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.maxDCAV0);
    }
  }
  c1->cd(6); h1 = (TH1*)fin->Get("hCosPointing"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.minCosPointing);
    }
  }
  c1->cd(7); h1 = (TH1*)fin->Get("hNSigmaProton"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.nSigmaProton);
      drawCutLine1D(h1, -lam.nSigmaProton);
    }
  }
  c1->cd(8); h1 = (TH1*)fin->Get("hNSigmaPion"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.nSigmaPion);
      drawCutLine1D(h1, -lam.nSigmaPion);
    }
  }
  c1->cd(9); h2 = (TH2*)fin->Get("hLambda_InvMass_vs_Pt"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 3: Lambda 2D QA
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h2 = (TH2*)fin->Get("hDCAV0_vs_InvMass"); if (h2) h2->Draw("colz");
  c1->cd(2); h2 = (TH2*)fin->Get("hCosPointing_vs_InvMass"); if (h2) h2->Draw("colz");
  c1->cd(3); h2 = (TH2*)fin->Get("hLambda_InvMass_vs_TransDecayLength"); if (h2) h2->Draw("colz");
  c1->cd(4); /* spare */;
  c1->Print(pdfName);

  fin->Close();
  PdfHeader::ClosePdf(pdfName);
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
