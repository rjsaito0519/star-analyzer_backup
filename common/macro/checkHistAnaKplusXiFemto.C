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

  TString figDir = resolveFigureRoot(pwd) + "/" + anaName;
  gSystem->mkdir(figDir, kTRUE);
  TString pdfPath = figDir + "/" + anaName + "_checkHistAnaKplusXiFemto.pdf";

  gStyle->SetOptStat(1110);
  PdfIOMan pdf(pdfPath.Data());

  TCanvas c("c", "c", 1000, 800);
  c.Divide(2, 2);
  const char* keys1[] = {"hVz", "hRefMult", "hCentrality", "hNKp"};
  for (Int_t i = 0; i < 4; i++) {
    c.cd(i + 1);
    TH1* h = (TH1*)fin->Get(keys1[i]);
    if (h) h->Draw("hist");
  }
  pdf.SavePage(&c, "Event / K+ QA");

  c.Clear();
  c.Divide(2, 2);
  const char* keys2[] = {"hXi_InvMass", "hNXi", "hKp_NSigma", "hKp_Pt"};
  for (Int_t i = 0; i < 4; i++) {
    c.cd(i + 1);
    TH1* h = (TH1*)fin->Get(keys2[i]);
    if (h) h->Draw("hist");
  }
  pdf.SavePage(&c, "Xi / K+ spectra");

  c.Clear();
  c.Divide(2, 2);
  c.cd(1);
  TH1* hSE = (TH1*)fin->Get("hKstarSE_kp_xim");
  if (hSE) hSE->Draw("hist");
  c.cd(2);
  TH1* hME = (TH1*)fin->Get("hKstarME_kp_xim");
  if (hME) hME->Draw("hist");
  c.cd(3);
  TH2* hSEc = (TH2*)fin->Get("hKstarSEVsCent_kp_xim");
  if (hSEc) hSEc->Draw("colz");
  c.cd(4);
  TH2* hm2 = (TH2*)fin->Get("hKp_Mass2VsP");
  if (hm2) hm2->Draw("colz");
  pdf.SavePage(&c, "k* SE/ME and K+ m2");

  pdf.Close();
  fin->Close();
  delete fin;
  std::cout << "Wrote " << pdfPath << std::endl;
}
