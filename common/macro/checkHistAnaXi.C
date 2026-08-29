// checkHistAnaXi.C - Draw histograms from run_anaXi.C output and write PDF.
// Invoke via: ./script/singularity_checkHistAnaXi.sh <root_file> <mainconf_path>

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

// Project TH2 (x=topology, y=InvMass) onto x for a mass window; returns owned clone (may be empty).
static TH1D* projectTopoVsMass(TH2* h2, Double_t mLo, Double_t mHi, const char* name) {
  if (!h2) return 0;
  Int_t y1 = h2->GetYaxis()->FindBin(mLo);
  Int_t y2 = h2->GetYaxis()->FindBin(mHi);
  if (y2 < y1) {
    Int_t tmp = y1;
    y1 = y2;
    y2 = tmp;
  }
  TH1D* proj = h2->ProjectionX(name, y1, y2, "e");
  if (!proj) return 0;
  proj->SetDirectory(0);
  return proj;
}

static void drawSignalVsSideband(TH2* h2, Double_t cutX, Bool_t /*cutIsMax*/,
                                 const char* title, const char* tag) {
  if (!h2 || !gPad) return;
  const Double_t kXiMass = 1.32171;
  const Double_t sigHalf = 0.015;
  const Double_t sbInner = 0.025;
  const Double_t sbOuter = 0.040;

  TString nSig = TString::Format("hPreTopo_sig_%s", tag);
  TString nSbL = TString::Format("hPreTopo_sbL_%s", tag);
  TString nSbR = TString::Format("hPreTopo_sbR_%s", tag);
  TString nSb = TString::Format("hPreTopo_sb_%s", tag);

  TH1D* hSig = projectTopoVsMass(h2, kXiMass - sigHalf, kXiMass + sigHalf, nSig.Data());
  TH1D* hSbL = projectTopoVsMass(h2, kXiMass - sbOuter, kXiMass - sbInner, nSbL.Data());
  TH1D* hSbR = projectTopoVsMass(h2, kXiMass + sbInner, kXiMass + sbOuter, nSbR.Data());
  if (!hSig) return;

  TH1D* hSbSum = (TH1D*)hSig->Clone(nSb.Data());
  hSbSum->SetDirectory(0);
  hSbSum->Reset();
  if (hSbL) hSbSum->Add(hSbL);
  if (hSbR) hSbSum->Add(hSbR);

  Double_t iSig = hSig->Integral();
  Double_t iSb = hSbSum->Integral();
  if (iSig > 0.0) hSig->Scale(1.0 / iSig);
  if (iSb > 0.0) hSbSum->Scale(1.0 / iSb);

  hSig->SetLineColor(kBlue + 1);
  hSig->SetLineWidth(2);
  hSbSum->SetLineColor(kGray + 2);
  hSbSum->SetLineWidth(2);
  hSig->SetTitle(title);
  hSig->GetYaxis()->SetTitle("normalized counts");
  Double_t ymax = TMath::Max(hSig->GetMaximum(), hSbSum->GetMaximum()) * 1.15;
  if (ymax <= 0.0) ymax = 1.0;
  hSig->SetMaximum(ymax);
  hSig->Draw("hist");
  hSbSum->Draw("hist same");
  if (cutX > -900.0) drawCutLine1D(hSig, cutX, kRed, 2);
  TLatex* leg = new TLatex();
  leg->SetNDC(kTRUE);
  leg->SetTextSize(0.035);
  leg->SetTextColor(kBlue + 1);
  leg->DrawLatex(0.45, 0.88, Form("peak |M-%.4f|<%.0f MeV", kXiMass, sigHalf * 1000.0));
  leg->SetTextColor(kGray + 2);
  leg->DrawLatex(0.45, 0.83, Form("SB %.0f-%.0f MeV", sbInner * 1000.0, sbOuter * 1000.0));
  delete hSbL;
  delete hSbR;
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

void checkHistAnaXi(const Char_t* inputRootFile,
                    const Char_t* anaNameArg = "auau19_anaXi",
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
      std::cerr << "[checkHistAnaXi] WARNING: Failed to load config " << mainconf.Data() << "; cut lines skipped." << std::endl;
    }
  }

  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaXi";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from run_anaXi.C (StXiMaker output).\n";
  note += "Xi- Cascade (Lambda + pi-) helix/line reconstruction.\n";
  note += "Centrality QA: Pages 1b-1d when centrality is enabled in mainconf.\n";
  note += "Page 4: pre-topology InvMass vs DCA/cos/L/DCA(#Lambda,PV); signal vs sideband.\n";

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaXi.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: Event level (Xi)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hRefMult"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hLambda_InvMass"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hXi_InvMass"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hN"); if (h1) h1->Draw();
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
  c1->cd(7); h2 = (TH2*)fin->Get("hNXiPairs_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(8); h2 = (TH2*)fin->Get("hXi_InvMass_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(9); h2 = (TH2*)fin->Get("hXi_InvMass_vs_RefMultCorr"); if (h2) h2->Draw("colz");
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 1d: Xi InvMass per centrality bin
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin0"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin1"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin2"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin3"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin4"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin5"); if (h1) h1->Draw();
  c1->cd(7); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin6"); if (h1) h1->Draw();
  c1->cd(8); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin7"); if (h1) h1->Draw();
  c1->cd(9); h1 = (TH1*)fin->Get("hXi_InvMass_CentBin8"); if (h1) h1->Draw();
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 2: Xi kinematics and quality
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hXi_Pt"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hXi_Eta"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hXi_Phi"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hDCA12_Lambda"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hDCA_Cascade"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.maxDaughterDCA);
    }
  }
  c1->cd(6); h1 = (TH1*)fin->Get("hDCAV0_Xi"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.maxDCAV0);
    }
  }
  c1->cd(7); h1 = (TH1*)fin->Get("hCosPointing_Xi"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      drawCutLine1D(h1, lam.minCosPointing);
    }
  }
  c1->cd(8); h1 = (TH1*)fin->Get("hNSigmaProton"); if (h1) h1->Draw();
  c1->cd(9); h2 = (TH2*)fin->Get("hXi_InvMass_vs_Pt"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 3: Xi 2D QA and other daughters
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h2 = (TH2*)fin->Get("hXi_InvMass_vs_DecayLength"); if (h2) h2->Draw("colz");
  c1->cd(2); h2 = (TH2*)fin->Get("hXi_InvMass_vs_Y"); if (h2) h2->Draw("colz");
  c1->cd(3); h1 = (TH1*)fin->Get("hNSigmaPionLambda"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hNSigmaPionBachelor"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 4: Pre-topology mass vs topology + signal/sideband projections
  {
    Double_t cutDca = -999.0;
    Double_t cutCos = -999.0;
    Double_t cutL = -999.0;
    Double_t cutLamPv = -999.0;
    if (gConfigLoaded) {
      LambdaCutConfig& lam = ConfigManager::GetInstance().GetLambdaCuts();
      cutDca = lam.maxDCAV0;
      cutCos = lam.minCosPointing;
      cutL = lam.minDecayLengthXi;
      if (lam.minDcaLambdaToPV > 0.0) cutLamPv = lam.minDcaLambdaToPV;
    }
    c1->Clear();
    c1->Divide(3, 3);
    c1->cd(1); h1 = (TH1*)fin->Get("hXi_InvMass_preTopo"); if (h1) h1->Draw();
    c1->cd(2); h2 = (TH2*)fin->Get("hXi_InvMass_vs_DCAV0_preTopo"); if (h2) h2->Draw("colz");
    c1->cd(3); h2 = (TH2*)fin->Get("hXi_InvMass_vs_CosPointing_preTopo"); if (h2) h2->Draw("colz");
    c1->cd(4); h2 = (TH2*)fin->Get("hXi_InvMass_vs_DecayLength_preTopo"); if (h2) h2->Draw("colz");
    c1->cd(5);
    h2 = (TH2*)fin->Get("hXi_InvMass_vs_DCAV0_preTopo");
    drawSignalVsSideband(h2, cutDca, kTRUE, "DCA(#Xi,PV) preTopo;DCA [cm];norm.", "dca");
    c1->cd(6);
    h2 = (TH2*)fin->Get("hXi_InvMass_vs_CosPointing_preTopo");
    drawSignalVsSideband(h2, cutCos, kFALSE, "cos(#theta) preTopo;cos(#theta);norm.", "cos");
    c1->cd(7);
    h2 = (TH2*)fin->Get("hXi_InvMass_vs_DecayLength_preTopo");
    drawSignalVsSideband(h2, cutL, kFALSE, "L_{#Xi} preTopo;L_{#Xi} [cm];norm.", "len");
    c1->cd(8); h2 = (TH2*)fin->Get("hXi_InvMass_vs_DcaLambdaPV_preTopo"); if (h2) h2->Draw("colz");
    c1->cd(9);
    h2 = (TH2*)fin->Get("hXi_InvMass_vs_DcaLambdaPV_preTopo");
    drawSignalVsSideband(h2, cutLamPv, kFALSE, "DCA(#Lambda,PV) preTopo;DCA [cm];norm.", "lampv");
    c1->Print(pdfName);
  }

  fin->Close();
  PdfHeader::ClosePdf(pdfName);
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
