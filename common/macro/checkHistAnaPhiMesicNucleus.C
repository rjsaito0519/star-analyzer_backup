#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TLine.h"
#include "TString.h"
#include "TSystem.h"

#include "ConfigManager.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/NuclearIdCutConfig.h"
#include "cuts/PhiMesicNucleusConfig.h"

#include <iostream>
#include <vector>

void draw1D(TFile* f, const char* key, const char* pdf, bool first, bool last,
            bool drawBand = false, double low = 0.0, double high = 0.0) {
  TH1* h = dynamic_cast<TH1*>(f->Get(key));
  if (!h) return;
  TCanvas c("c", "c", 900, 700);
  h->Draw("hist");
  if (drawBand) {
    const double yMax = h->GetMaximum() * 0.92;
    TLine l1(low, 0.0, low, yMax);
    TLine l2(high, 0.0, high, yMax);
    l1.SetLineColor(kRed + 1);
    l2.SetLineColor(kRed + 1);
    l1.SetLineStyle(2);
    l2.SetLineStyle(2);
    l1.SetLineWidth(2);
    l2.SetLineWidth(2);
    l1.Draw("same");
    l2.Draw("same");
  }
  if (first) c.Print(TString::Format("%s(", pdf));
  else if (last) c.Print(TString::Format("%s)", pdf));
  else c.Print(pdf);
}

void checkHistAnaPhiMesicNucleus(const Char_t* rootFile,
                                 const Char_t* anaName,
                                 const Char_t* mainconfPath = 0) {
  TString resolvedMainconf = mainconfPath ? mainconfPath : "";
  if (resolvedMainconf.IsNull()) {
    const char* envConf = gSystem->Getenv("STAR_ANA_MAINCONF");
    if (envConf && strlen(envConf) > 0) resolvedMainconf = envConf;
  }
  if (!resolvedMainconf.IsNull()) {
    ConfigManager::GetInstance().LoadConfig(resolvedMainconf.Data());
  }
  PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
  NuclearIdCutConfig& nuc = ConfigManager::GetInstance().GetNuclearIdCuts();
  PhiMesicNucleusConfig& pmn = ConfigManager::GetInstance().GetPhiMesicNucleusConfig();

  TFile* f = TFile::Open(rootFile, "READ");
  if (!f || f->IsZombie()) {
    std::cerr << "Cannot open " << rootFile << std::endl;
    return;
  }
  const char* figureRootEnv = gSystem->Getenv("STAR_QA_FIGURE_ROOT");
  TString figureRoot = (figureRootEnv && strlen(figureRootEnv) > 0) ? figureRootEnv : "share/figure";
  TString outDir = TString::Format("%s/%s", figureRoot.Data(), anaName);
  TString pdf = TString::Format("%s/checkHistAnaPhiMesicNucleus.pdf", outDir.Data());
  gSystem->mkdir(outDir, kTRUE);

  struct DrawSpec {
    std::string key;
    bool band;
    double low;
    double high;
  };
  std::vector<DrawSpec> specs;
  specs.push_back({"hNEvents", false, 0.0, 0.0});
  specs.push_back({"hVz", false, 0.0, 0.0});
  specs.push_back({"hCent9", false, 0.0, 0.0});
  specs.push_back({"hPhiP_LambdaMass", true, pmn.lambdaMassWindow.signalMin, pmn.lambdaMassWindow.signalMax});
  specs.push_back({"hPhiP_KplusM2", true, pid.minMass2Kaon, pid.maxMass2Kaon});
  specs.push_back({"hPhi3He3Body_LambdaMass", true, pmn.lambdaMassWindow.signalMin, pmn.lambdaMassWindow.signalMax});
  specs.push_back({"hPhi3He3Body_KplusM2", true, pid.minMass2Kaon, pid.maxMass2Kaon});
  specs.push_back({"hPhi3He3Body_DeuteronM2", true, nuc.minM2_M2cut, nuc.maxM2_M2cut});
  specs.push_back({"hParentMass_SE_phi3He_direct", false, 0.0, 0.0});
  specs.push_back({"hParentDeltaM_SE_phi3He_direct", false, 0.0, 0.0});
  specs.push_back({"hPhi3He2Body_HypertritonMass", true, pmn.hypertriton2BodyMassWindow.signalMin, pmn.hypertriton2BodyMassWindow.signalMax});
  specs.push_back({"hPhi3He2Body_KplusM2", true, pid.minMass2Kaon, pid.maxMass2Kaon});
  specs.push_back({"hParentMass_SE_phi3He_hypertriton_2body", false, 0.0, 0.0});
  specs.push_back({"hPhi4He3Body_LambdaMass", true, pmn.lambdaMassWindow.signalMin, pmn.lambdaMassWindow.signalMax});
  specs.push_back({"hPhi4He3Body_KplusM2", true, pid.minMass2Kaon, pid.maxMass2Kaon});
  specs.push_back({"hPhi4He3Body_TritonM2", true, nuc.minM2_M2cut, nuc.maxM2_M2cut});
  specs.push_back({"hParentMass_SE_phi4He_direct", false, 0.0, 0.0});
  specs.push_back({"hParentDeltaM_SE_phi4He_direct", false, 0.0, 0.0});
  specs.push_back({"hPhi4He2Body_HyperhydrogenMass", true, pmn.hyperhydrogen4MassWindow.signalMin, pmn.hyperhydrogen4MassWindow.signalMax});
  specs.push_back({"hPhi4He2Body_KplusM2", true, pid.minMass2Kaon, pid.maxMass2Kaon});
  specs.push_back({"hParentMass_SE_phi4He_hyperhydrogen", false, 0.0, 0.0});
  specs.push_back({"hParentMass_SE_phi3He_hypertriton_3body", false, 0.0, 0.0});
  specs.push_back({"hHypertriton3BodyMass_Raw", true, pmn.hypertriton3BodyMassWindow.signalMin, pmn.hypertriton3BodyMassWindow.signalMax});
  specs.push_back({"hParentMass_SE_phiP_direct", false, 0.0, 0.0});
  specs.push_back({"hLambdaMass_AfterTopology", true, pmn.lambdaMassWindow.signalMin, pmn.lambdaMassWindow.signalMax});

  bool printed = false;
  for (size_t i = 0; i < specs.size(); ++i) {
    TH1* h = dynamic_cast<TH1*>(f->Get(specs[i].key.c_str()));
    if (!h) continue;
    bool first = !printed;
    bool last = false;
    printed = true;
    draw1D(f, specs[i].key.c_str(), pdf.Data(), first, last, specs[i].band, specs[i].low, specs[i].high);
  }
  if (printed) {
    TCanvas c("cclose", "cclose", 600, 400);
    c.Print(TString::Format("%s)", pdf.Data()));
    std::cout << "Wrote " << pdf.Data() << std::endl;
  } else {
    std::cout << "No QA histogram found." << std::endl;
  }
  f->Close();
  delete f;
}
