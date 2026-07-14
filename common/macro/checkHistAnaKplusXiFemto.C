// checkHistAnaKplusXiFemto.C - Smoke QA for K+-Xi- femto ROOT output
// Invoke via: ./script/singularity_checkHistAnaKplusXiFemto.sh <root_file> <mainconf_path>

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TString.h>
#include <TStyle.h>
#include <iostream>
#include <vector>
#include <limits.h>
#include <stdlib.h>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"

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

void checkHistAnaKplusXiFemto(const Char_t* rootFile,
                              const Char_t* anaNameArg = "auau19_anaKplusXiFemto",
                              const Char_t* mainconfPath = 0) {
  gROOT->SetBatch(kTRUE);
  gStyle->SetOptStat(1110);

  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  TString mainConfigPath;
  if (mainconfPath && strlen(mainconfPath) > 0) {
    mainConfigPath = mainconfPath;
    if (mainConfigPath(0) != '/') mainConfigPath = TString(pwd) + "/" + mainConfigPath;
  } else {
    mainConfigPath = TString(pwd) + "/config/mainconf/main_auau19_anaKplusXiFemto.yaml";
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

  TFile* fin = TFile::Open(rootFile, "READ");
  if (!fin || fin->IsZombie()) {
    std::cerr << "ERROR: cannot open " << rootFile << std::endl;
    return;
  }

  TString figDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(figDir)) {
    gSystem->mkdir(figDir, kTRUE);
  }
  TString pdfName = figDir + anaName + "_checkHistAnaKplusXiFemto.pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)rootFile);
  TString note = "Check histograms from run_anaKplusXiFemto.C (StKplusXiFemtoMaker).\n";
  note += "K+-Xi- SE/ME k* and K+ TOF / Xi mass QA.\n";
  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaKplusXiFemto.C", inputs, note.Data(), true, anaName.Data());

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: event / K+ counts
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz"); if (h1) h1->Draw("hist");
  c1->cd(2); h1 = (TH1*)fin->Get("hRefMult"); if (h1) h1->Draw("hist");
  c1->cd(3); h1 = (TH1*)fin->Get("hCentrality"); if (h1) h1->Draw("hist");
  c1->cd(4); h1 = (TH1*)fin->Get("hNKp"); if (h1) h1->Draw("hist");
  c1->Print(pdfName);

  // Page 2: Xi / K+ spectra
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hXi_InvMass"); if (h1) h1->Draw("hist");
  c1->cd(2); h1 = (TH1*)fin->Get("hNXi"); if (h1) h1->Draw("hist");
  c1->cd(3); h1 = (TH1*)fin->Get("hKp_NSigma"); if (h1) h1->Draw("hist");
  c1->cd(4); h1 = (TH1*)fin->Get("hKp_Pt"); if (h1) h1->Draw("hist");
  c1->Print(pdfName);

  // Page 3: k* SE/ME + K+ m2
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hKstarSE_kp_xim"); if (h1) h1->Draw("hist");
  c1->cd(2); h1 = (TH1*)fin->Get("hKstarME_kp_xim"); if (h1) h1->Draw("hist");
  c1->cd(3); h2 = (TH2*)fin->Get("hKstarSEVsCent_kp_xim"); if (h2) h2->Draw("colz");
  c1->cd(4); h2 = (TH2*)fin->Get("hKp_Mass2VsP"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  PdfHeader::ClosePdf(pdfName);
  delete c1;
  fin->Close();
  delete fin;
  std::cout << "Wrote " << pdfName.Data() << std::endl;
}
