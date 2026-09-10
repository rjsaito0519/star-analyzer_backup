// checkHistAnaXi_KFParticle.C - QA PDF for StXiKFParticleMaker output.
// Invoke via: ./script/singularity_checkHistAnaXi_KFParticle.sh <root_file> <mainconf_path>

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TLine.h>
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
#include "cuts/KfParticleCutConfig.h"

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
  if (!h || !gPad || x < -1.0e8) return;
  Double_t ylo = gPad->GetUymin();
  Double_t yhi = gPad->GetUymax();
  TLine* l = new TLine(x, ylo, x, yhi);
  l->SetLineColor(color);
  l->SetLineStyle(style);
  l->Draw("same");
}

static void drawMassRefLine(TH1* h, Double_t mass = 1.32171) {
  drawCutLine1D(h, mass, kBlue + 1, 3);
}

static Bool_t isHex32(const TString& s) {
  if (s.Length() != 32) return kFALSE;
  for (Int_t i = 0; i < s.Length(); i++) {
    Char_t c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
      return kFALSE;
  }
  return kTRUE;
}

static void drawIfPresent1D(TFile* fin, const char* name, Bool_t logy = kFALSE) {
  TH1* h = (TH1*)fin->Get(name);
  if (!h) return;
  if (logy) gPad->SetLogy();
  h->Draw();
}

static void drawIfPresent2D(TFile* fin, const char* name) {
  TH2* h = (TH2*)fin->Get(name);
  if (!h) return;
  h->Draw("colz");
}

void checkHistAnaXi_KFParticle(const Char_t* inputRootFile,
                               const Char_t* anaNameArg = "auau3p9fxt_anaXi_KFParticle",
                               const Char_t* mainconfPath = 0)
{
  gROOT->SetBatch(kTRUE);
  gStyle->SetOptStat(1110);
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
      std::cerr << "[checkHistAnaXi_KFParticle] WARNING: Failed to load config "
                << mainconf.Data() << "; cut lines skipped." << std::endl;
    }
  }

  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaXi_KFParticle";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from anaXi_KFParticle / StXiKFParticleMaker.\n";
  note += "KFParticle Finder/Topo Xi (Lambda + bachelor).\n";
  note += "Page 1: event / KF stage counters.\n";
  note += "Page 2: raw vs selected Xi / anti-Xi mass.\n";
  note += "Page 3: intermediate Lambda (Finder raw, FromXi raw/selected).\n";
  note += "Page 4: topology / chi2 / L/sigma with KF cut lines when available.\n";
  note += "Page 5: kinematics and mass vs pT / L / y.\n";

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaXi_KFParticle.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;

  Double_t cutChi2 = -1.0e9;
  Double_t cutTopoChi2 = -1.0e9;
  Double_t cutDca12 = -1.0e9;
  Double_t cutDcaPv = -1.0e9;
  Double_t cutCos = -1.0e9;
  Double_t cutLdL = -1.0e9;
  Double_t cutMassLo = -1.0e9;
  Double_t cutMassHi = -1.0e9;
  if (gConfigLoaded) {
    KfParticleCutConfig& kf = ConfigManager::GetInstance().GetKfParticleCuts();
    cutChi2 = kf.maxChi2Ndf;
    cutTopoChi2 = kf.maxTopoChi2Ndf;
    cutDca12 = kf.maxDaughterDistance;
    cutDcaPv = kf.maxDistanceToPv;
    cutCos = kf.minCosPointing;
    cutLdL = kf.minDecayLengthSignificance;
    cutMassLo = kf.minMass;
    cutMassHi = kf.maxMass;
  }

  // Page 1: Event / KF stages
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); drawIfPresent1D(fin, "hVz");
  c1->cd(2); drawIfPresent1D(fin, "hRefMult");
  c1->cd(3); drawIfPresent1D(fin, "hN");
  c1->cd(4); drawIfPresent1D(fin, "hKfTrackCovCount");
  c1->cd(5); drawIfPresent1D(fin, "hKfEventSelection");
  c1->cd(6); drawIfPresent1D(fin, "hKfStages");
  c1->Print(pdfName);

  // Page 2: Xi mass spectra
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hKfXiMassRaw");
  if (h1) {
    h1->Draw();
    drawMassRefLine(h1);
    drawCutLine1D(h1, cutMassLo, kRed, 2);
    drawCutLine1D(h1, cutMassHi, kRed, 2);
  }
  c1->cd(2);
  h1 = (TH1*)fin->Get("hKfXiMassSelected");
  if (h1) {
    h1->Draw();
    drawMassRefLine(h1);
  }
  c1->cd(3);
  h1 = (TH1*)fin->Get("hXi_InvMass");
  if (h1) {
    h1->Draw();
    drawMassRefLine(h1);
  }
  c1->cd(4);
  h1 = (TH1*)fin->Get("hKfAntiXiMassRaw");
  if (h1) {
    h1->Draw();
    drawMassRefLine(h1);
  }
  c1->cd(5);
  h1 = (TH1*)fin->Get("hKfAntiXiMassSelected");
  if (h1) {
    h1->Draw();
    drawMassRefLine(h1);
  }
  c1->Print(pdfName);

  // Page 3: Intermediate Lambda masses
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hKfLambdaMassRaw");
  if (h1) {
    h1->Draw();
    drawCutLine1D(h1, 1.115683, kBlue + 1, 3);
  }
  c1->cd(2);
  h1 = (TH1*)fin->Get("hKfAntiLambdaMassRaw");
  if (h1) {
    h1->Draw();
    drawCutLine1D(h1, 1.115683, kBlue + 1, 3);
  }
  c1->cd(3);
  h1 = (TH1*)fin->Get("hKfLambdaMassFromXi");
  if (h1) {
    h1->Draw();
    drawCutLine1D(h1, 1.115683, kBlue + 1, 3);
  }
  c1->cd(4);
  h1 = (TH1*)fin->Get("hKfLambdaMassFromXiSelected");
  if (h1) {
    h1->Draw();
    drawCutLine1D(h1, 1.115683, kBlue + 1, 3);
  }
  c1->Print(pdfName);

  // Page 4: Topology / quality
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1);
  h1 = (TH1*)fin->Get("hDCA12");
  if (h1) {
    h1->Draw();
    if (cutDca12 >= 0.0) drawCutLine1D(h1, cutDca12);
  }
  c1->cd(2);
  h1 = (TH1*)fin->Get("hDCAV0");
  if (h1) {
    h1->Draw();
    if (cutDcaPv >= 0.0) drawCutLine1D(h1, cutDcaPv);
  }
  c1->cd(3);
  h1 = (TH1*)fin->Get("hCosPointing");
  if (h1) {
    h1->Draw();
    if (cutCos > -1.5) drawCutLine1D(h1, cutCos);
  }
  c1->cd(4);
  h1 = (TH1*)fin->Get("hKfChi2Ndf");
  if (h1) {
    h1->Draw();
    if (cutChi2 >= 0.0) drawCutLine1D(h1, cutChi2);
  }
  c1->cd(5);
  h1 = (TH1*)fin->Get("hKfTopoChi2Ndf");
  if (h1) {
    h1->Draw();
    if (cutTopoChi2 >= 0.0) drawCutLine1D(h1, cutTopoChi2);
  }
  c1->cd(6);
  h1 = (TH1*)fin->Get("hKfTopoChi2NdfRaw");
  if (h1) {
    h1->Draw();
    if (cutTopoChi2 >= 0.0) drawCutLine1D(h1, cutTopoChi2);
  }
  c1->cd(7);
  h1 = (TH1*)fin->Get("hKfDecayLengthSignificance");
  if (h1) {
    h1->Draw();
    if (cutLdL >= 0.0) drawCutLine1D(h1, cutLdL);
  }
  c1->cd(8); drawIfPresent1D(fin, "hKfPdg");
  c1->cd(9); drawIfPresent1D(fin, "hXi_Pt", kTRUE);
  c1->Print(pdfName);

  // Page 5: Kinematics / correlations
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); drawIfPresent1D(fin, "hXi_Eta");
  c1->cd(2); drawIfPresent2D(fin, "hXi_InvMass_vs_Pt");
  c1->cd(3); drawIfPresent2D(fin, "hXi_InvMass_vs_DecayLength");
  c1->cd(4); drawIfPresent2D(fin, "hXi_InvMass_vs_Y");
  c1->Print(pdfName);

  fin->Close();
  PdfHeader::ClosePdf(pdfName);
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
