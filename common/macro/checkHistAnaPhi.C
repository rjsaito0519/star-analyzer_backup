// checkHistAnaPhi.C - Draw histograms from run_anaPhi.C output and write PDF.
// Invoke via: ./script/checkHistAnaPhi.sh <root_file> <mainconf_path>
// Or: root4star -b -q 'analysis/run_checkHistAnaPhi.C("rootfile/...","anaName","config/mainconf/main_anaName.yaml")'
// If input is anaName_jobid_merge.root, jobid (32 hex) is parsed and PDF becomes anaName_checkHistAnaPhi_jobid.pdf.
// With config loaded, cut regions are overlaid on pre-cut histograms.

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
#include <TEllipse.h>
#include <iostream>
#include <vector>
#include <limits.h>
#include <stdlib.h>

#include "../../include/PdfIOMan.h"
#include "ConfigManager.h"
#include "cuts/EventCutConfig.h"
#include "cuts/TrackCutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/PIDCutConfig.h"

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

static void drawCutLines1D(TH1* h, Double_t x1, Double_t x2, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t ylo = gPad->GetUymin();
  Double_t yhi = gPad->GetUymax();
  const Double_t kBig = 1e30;
  if (x1 > -kBig && x1 < kBig) {
    TLine* l1 = new TLine(x1, ylo, x1, yhi);
    l1->SetLineColor(color);
    l1->SetLineStyle(style);
    l1->Draw("same");
  }
  if (x2 > -kBig && x2 < kBig && TMath::Abs(x2 - x1) > 1e-9) {
    TLine* l2 = new TLine(x2, ylo, x2, yhi);
    l2->SetLineColor(color);
    l2->SetLineStyle(style);
    l2->Draw("same");
  }
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

static void drawCutLine2DH(TH2* h, Double_t yVal, Int_t color = kRed, Int_t style = 2) {
  if (!h || !gPad) return;
  Double_t xlo = gPad->GetUxmin();
  Double_t xhi = gPad->GetUxmax();
  TLine* l = new TLine(xlo, yVal, xhi, yVal);
  l->SetLineColor(color);
  l->SetLineStyle(style);
  l->Draw("same");
}

static void drawVtxCutCircle(Double_t centerX, Double_t centerY, Double_t radius,
                             Int_t color = kRed, Int_t style = 2) {
  if (!gPad || radius <= 0.0) return;
  TEllipse* circle = new TEllipse(centerX, centerY, radius, radius);
  circle->SetFillStyle(0);
  circle->SetLineColor(color);
  circle->SetLineStyle(style);
  circle->Draw("same");
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

static Double_t getHistEntries(TFile* fin, const char* key) {
  if (!fin || !key || key[0] == '\0') return -1.0;
  TObject* obj = fin->Get(key);
  if (!obj || !obj->InheritsFrom("TH1")) return -1.0;
  return ((TH1*)obj)->GetEntries();
}

void checkHistAnaPhi(const Char_t* inputRootFile,
                     const Char_t* anaNameArg = "auau19_anaPhi",
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

  // Parse input basename for anaName_jobid_merge.root -> anaName, jobid (32 hex)
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
      if (!ConfigManager::GetInstance().GetPhiCuts().FinalizeRapidityFrame(
              ConfigManager::GetInstance().GetCentralityCuts())) {
        std::cerr << "[checkHistAnaPhi] WARNING: FinalizeRapidityFrame failed; PDF note may be incomplete."
                  << std::endl;
      }
    } else if (mainconfPath && strlen(mainconfPath) > 0) {
      std::cerr << "[checkHistAnaPhi] WARNING: Failed to load config " << mainconf.Data() << "; cut lines skipped." << std::endl;
    }
  }

  // Resolve symlinked share/figure to a physical path so ROOT PDF output
  // works the same in host and singularity-based runs.
  TString outDir = resolveFigureRoot(pwd) + "/" + anaName + "/";
  if (gSystem->AccessPathName(outDir)) {
    gSystem->mkdir(outDir, kTRUE);
  }

  TString pdfName = TString(outDir) + anaName + "_checkHistAnaPhi";
  if (jobid.Length()) pdfName += "_" + jobid;
  pdfName += ".pdf";
  std::cout << "Output PDF: " << pdfName.Data() << std::endl;

  PdfHeader::OpenPdf(pdfName);

  std::vector<std::string> inputs;
  inputs.push_back((const char*)inputRootFile);

  TString note = "Check histograms from run_anaPhi.C (StPhiMaker output).\n";
  note += "Phi KK reconstruction: all hMKK_* pair histograms require strict TOF pair cut at fill time.\n";
  note += "Opening angle / pair rapidity cuts apply on top of strict TOF pair selection.\n";
  const Double_t nEvtAll = getHistEntries(fin, "hVz");
  const Double_t nEvtAfter = getHistEntries(fin, "hVz_After");
  const Double_t nPairsBothCuts = getHistEntries(fin, "hMKK_BothCuts");
  if (nEvtAll >= 0.0) {
    note += Form("Total statistics (input ROOT): events(all) = %.0f", nEvtAll);
    if (nEvtAfter >= 0.0) {
      note += Form(", events(after event cuts) = %.0f", nEvtAfter);
    }
    if (nPairsBothCuts >= 0.0) {
      note += Form(", KK pairs(both cuts) = %.0f", nPairsBothCuts);
    }
    note += "\n";
  } else {
    note += "Total statistics (input ROOT): hVz not found.\n";
  }
  if (gConfigLoaded) {
    note += ConfigManager::GetInstance().GetPhiCuts().GetRapidityFrameSummary().c_str();
    note += "\n";
  }
  note += "QA: Page 1 (Event pre-cut), Page 1e (Event post-cut), Page 2 (Track post-cut), Page 2b (Track pre-cut), Page 8b-8c (Pair QA stage0 / strict TOF), Page 9 (Kaon PID before/after).\n";
  note += "TOF QA: Page 3 (global TOF-matched m2 vs p), Page 3b (TPC K vs final K m2, DCA_KK), Page 3c (m2/q2 vs p/q, delta(1/beta)), Page 3d/3bp (PID vs pT).\n";
  note += "Re-run analysis after hist/Maker changes so new keys exist in the ROOT file.\n";

  PdfHeader::MakePdfHeaderPage(pdfName, "checkHistAnaPhi.C", inputs, note.Data(), true, anaName);

  TCanvas* c1 = new TCanvas("c1", "canvas", 1200, 800);
  TH1* h1 = 0;
  TH2* h2 = 0;

  // Page 1: Event Level
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minVz, ev.maxVz); } }
  c1->cd(2); h1 = (TH1*)fin->Get("hVzDiff"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, -ev.maxVzDiff, ev.maxVzDiff); } }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hVxVy"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
      drawVtxCutCircle(ev.vtxCenterX, ev.vtxCenterY, ev.maxVr);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMult"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minRefMult, ev.maxRefMult); } }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hVzVsRun"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultVsVz"); if (h2) h2->Draw("colz");
  c1->cd(7); h1 = (TH1*)fin->Get("hNTracks"); if (h1) { h1->Draw(); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); if (phi.maxNTr > 0) drawCutLine1D(h1, (Double_t)phi.maxNTr); } }
  c1->cd(8); h1 = (TH1*)fin->Get("hVr"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLine1D(h1, ev.maxVr); } }
  c1->cd(9); /* spare */;
  c1->Print(pdfName);

  // Page 1b: Centrality QA (event-level)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hCentrality"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hCentralityRaw"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hCentrality16"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMultCorr"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hRefMultWeight"); if (h1) h1->Draw();
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultVsNTOFMatch"); if (h2) h2->Draw("colz");
  c1->cd(7); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultVsNTOFMatchAfter"); if (h2) h2->Draw("colz");
  c1->cd(8); gPad->SetLogz(); h2 = (TH2*)fin->Get("hCentralityVsVz"); if (h2) h2->Draw("colz");
  c1->cd(9); h1 = (TH1*)fin->Get("hRawMult"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 1c: Centrality correlations (event observables vs cent9)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRawMult_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hRefMultCorr_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNTracks_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hTofMatchMult_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonPlus_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonMinus_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(7); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNPhiPairs_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(8); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMKK_vs_Cent9"); if (h2) h2->Draw("colz");
  c1->cd(9); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMKK_vs_RefMultCorr"); if (h2) h2->Draw("colz");
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 1d: M_KK per centrality bin (opening+rapidity cut)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hMKK_CentBin0"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hMKK_CentBin1"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hMKK_CentBin2"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hMKK_CentBin3"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hMKK_CentBin4"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hMKK_CentBin5"); if (h1) h1->Draw();
  c1->cd(7); h1 = (TH1*)fin->Get("hMKK_CentBin6"); if (h1) h1->Draw();
  c1->cd(8); h1 = (TH1*)fin->Get("hMKK_CentBin7"); if (h1) h1->Draw();
  c1->cd(9); h1 = (TH1*)fin->Get("hMKK_CentBin8"); if (h1) h1->Draw();
  drawCent9ConventionNote();
  c1->Print(pdfName);

  // Page 1e: Event Level (Post-Cut)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); h1 = (TH1*)fin->Get("hVz_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minVz, ev.maxVz); } }
  c1->cd(2); h1 = (TH1*)fin->Get("hVzDiff_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, -ev.maxVzDiff, ev.maxVzDiff); } }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hVxVy_After"); if (h2) {
    h2->Draw("colz");
    if (gConfigLoaded) {
      EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts();
      drawVtxCutCircle(ev.vtxCenterX, ev.vtxCenterY, ev.maxVr);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hRefMult_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLines1D(h1, ev.minRefMult, ev.maxRefMult); } }
  c1->cd(5); /* spare */;
  c1->cd(6); /* spare */;
  c1->cd(7); /* spare */;
  c1->cd(8); h1 = (TH1*)fin->Get("hVr_After"); if (h1) { h1->Draw(); if (gConfigLoaded) { EventCutConfig& ev = ConfigManager::GetInstance().GetEventCuts(); drawCutLine1D(h1, ev.maxVr); } }
  c1->cd(9); /* spare */;
  c1->Print(pdfName);

  // Page 2: Track Kinematics & Quality (Post-Cut)
  c1->Clear();
  c1->Divide(3, 3);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hPt"); if (h1) h1->Draw();
  c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hEta"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhi"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hCharge"); if (h1) h1->Draw("hist");
  c1->cd(5); h1 = (TH1*)fin->Get("hNHitsFit"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hNHitsRatio"); if (h1) h1->Draw();
  c1->cd(7); h1 = (TH1*)fin->Get("hNHitsDedx"); if (h1) h1->Draw();
  c1->cd(8); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDCA"); if (h1) h1->Draw();
  c1->cd(9); gPad->SetLogy(); h1 = (TH1*)fin->Get("hChi2"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 2b: Track Kinematics & Quality (Pre-Cut)
  c1->Clear();
  c1->Divide(3, 3);
  if (gConfigLoaded) {
    TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
    c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hPt_Raw"); if (h1) { h1->Draw(); drawCutLines1D(h1, tr.minPt, tr.maxPt); }
    c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hEta_Raw"); if (h1) { h1->Draw(); drawCutLines1D(h1, tr.minEta, tr.maxEta); }
    c1->cd(3); h1 = (TH1*)fin->Get("hPhi_Raw"); if (h1) h1->Draw();
    c1->cd(4); h1 = (TH1*)fin->Get("hCharge"); if (h1) h1->Draw("hist");
    c1->cd(5); h1 = (TH1*)fin->Get("hNHitsFit_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, (Double_t)tr.minNHitsFit); }
    c1->cd(6); h1 = (TH1*)fin->Get("hNHitsRatio_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, tr.minNHitsRatio); }
    c1->cd(7); h1 = (TH1*)fin->Get("hNHitsDedx_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, (Double_t)tr.minNHitsDedx); }
    c1->cd(8); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDCA_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, tr.maxDCA); }
    c1->cd(9); gPad->SetLogy(); h1 = (TH1*)fin->Get("hChi2_Raw"); if (h1) { h1->Draw(); drawCutLine1D(h1, tr.maxChi2); }
  } else {
    c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hPt_Raw"); if (h1) h1->Draw();
    c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hEta_Raw"); if (h1) h1->Draw();
    c1->cd(3); h1 = (TH1*)fin->Get("hPhi_Raw"); if (h1) h1->Draw();
    c1->cd(4); h1 = (TH1*)fin->Get("hCharge"); if (h1) h1->Draw("hist");
    c1->cd(5); h1 = (TH1*)fin->Get("hNHitsFit_Raw"); if (h1) h1->Draw();
    c1->cd(6); h1 = (TH1*)fin->Get("hNHitsRatio_Raw"); if (h1) h1->Draw();
    c1->cd(7); h1 = (TH1*)fin->Get("hNHitsDedx_Raw"); if (h1) h1->Draw();
    c1->cd(8); gPad->SetLogy(); h1 = (TH1*)fin->Get("hDCA_Raw"); if (h1) h1->Draw();
    c1->cd(9); gPad->SetLogy(); h1 = (TH1*)fin->Get("hChi2_Raw"); if (h1) h1->Draw();
  }
  c1->Print(pdfName);

  // Page 3: PID (TPC & TOF)
  c1->Clear();
  c1->Divide(3, 2);
  double pmax = 5.0;
  double dedxmax = 1e-6;
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDedxVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->GetYaxis()->SetRangeUser(0, dedxmax); h2->Draw("colz"); }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hBetaVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaPionVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaKaonVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); drawCutLine2DH(h2, phi.nSigmaKaon); drawCutLine2DH(h2, -phi.nSigmaKaon); } }
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaProtonVsP"); if (h2) { h2->GetXaxis()->SetRangeUser(0, pmax); h2->Draw("colz"); }
  c1->Print(pdfName);

  // Page 3d: PID vs pT (mirror of Page 3)
  c1->Clear();
  c1->Divide(3, 2);
  double ptmax = 5.0;
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDedxVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->GetYaxis()->SetRangeUser(0, dedxmax); h2->Draw("colz"); }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hBetaVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsPt"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, ptmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaPionVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); }
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaKaonVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); drawCutLine2DH(h2, phi.nSigmaKaon); drawCutLine2DH(h2, -phi.nSigmaKaon); } }
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNSigmaProtonVsPt"); if (h2) { h2->GetXaxis()->SetRangeUser(0, ptmax); h2->Draw("colz"); }
  c1->Print(pdfName);

  // Page 3bp: TOF m2 vs pT (TPC K vs final K)
  c1->Clear();
  c1->Divide(2, 1);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsPt_TpcKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, ptmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsPt_IsKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, ptmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->Print(pdfName);

  // Page 3b: TOF m2 (TPC K vs final K) + pair DCA_KK
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsP_TpcKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMass2VsP_IsKaon"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.minMass2Kaon);
      drawCutLine2DH(h2, pid.maxMass2Kaon);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hDCAKK_All"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine1D(h1, phi.maxDCAKK);
    }
  }
  c1->cd(4); h1 = (TH1*)fin->Get("hDCAKK_Pass"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLine1D(h1, phi.maxDCAKK);
    }
  }
  c1->Print(pdfName);

  // Page 3c: TOF diagnostics (m2/q2 vs p/q, delta(1/beta) vs p and 1D, beta vs p)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hM2q2VsPq"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hDeltaOneOverBetaVsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLine2DH(h2, pid.maxAbsDeltaOneOverBetaKaon);
      drawCutLine2DH(h2, -pid.maxAbsDeltaOneOverBetaKaon);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hDeltaOneOverBetaKaon"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PIDCutConfig& pid = ConfigManager::GetInstance().GetPIDCuts();
      drawCutLines1D(h1, -pid.maxAbsDeltaOneOverBetaKaon, pid.maxAbsDeltaOneOverBetaKaon);
    }
  }
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hBetaVsP"); if (h2) {
    h2->GetXaxis()->SetRangeUser(0, pmax);
    h2->Draw("colz");
  }
  c1->Print(pdfName);

  // Page 4: Event Plane & Misc
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogy(); h1 = (TH1*)fin->Get("hTriggerIds"); if (h1) { h1->SetFillColor(17); h1->Draw(); }
  c1->cd(2); gPad->SetLogy(0); h1 = (TH1*)fin->Get("hTofMatchMult"); if (h1) h1->Draw();
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hQxQy"); if (h2) h2->Draw("colz");
  c1->cd(4); h1 = (TH1*)fin->Get("hPsi2"); if (h1) { h1->SetMinimum(0); h1->Draw(); }
  c1->Print(pdfName);

  // Page 5: KK invariant mass (main phi analysis)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hMKK_SameEvent"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hMKK_AllCombinations"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hMKK_OpeningAngleCut"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hMKK_RapidityCut"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 6: KK mass (both cuts, mixed, background sub)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hMKK_BothCuts"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hMKK_MixedEvent"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hMKK_BackgroundSubtracted"); if (h1) h1->Draw();
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hMKK_vs_Pt"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 7: Opening angle & pair rapidity (raw and 2D)
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hOpeningAngle_Raw"); if (h1) { h1->Draw(); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); drawCutLines1D(h1, phi.minOpeningAngle, phi.maxOpeningAngle); } }
  c1->cd(2); h1 = (TH1*)fin->Get("hPairRapidity_Raw"); if (h1) { h1->Draw(); if (gConfigLoaded) { PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts(); drawCutLines1D(h1, phi.minPairRapidity, phi.maxPairRapidity); } }
  c1->cd(3); gPad->SetLogz(); h2 = (TH2*)fin->Get("hOpeningAngle_vs_MKK"); if (h2) h2->Draw("colz");
  c1->cd(4); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPairRapidity_vs_MKK"); if (h2) h2->Draw("colz");
  c1->cd(5); gPad->SetLogz(); h2 = (TH2*)fin->Get("hOpeningAngle_vs_Pt"); if (h2) h2->Draw("colz");
  c1->cd(6); gPad->SetLogz(); h2 = (TH2*)fin->Get("hOpeningAngle_vs_Rapidity"); if (h2) h2->Draw("colz");
  c1->Print(pdfName);

  // Page 8: KK pair pT (raw), rapidity vs pT, and after-cuts 1D
  c1->Clear();
  c1->Divide(3, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPairPt_Raw"); if (h1) h1->Draw();
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPairRapidity_vs_Pt"); if (h2) h2->Draw("colz");
  c1->cd(3); h1 = (TH1*)fin->Get("hOpeningAngle_AfterCuts"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hPairRapidity_AfterCuts"); if (h1) h1->Draw();
  c1->cd(5); h1 = (TH1*)fin->Get("hPairPt_AfterCuts"); if (h1) h1->Draw();
  c1->cd(6); h1 = (TH1*)fin->Get("hMKK_BothCuts"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 8b: staged pair QA mass and overlay
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hPhiPair_Mass_stage0"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hPhiPair_Mass_tofStrict"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hPhiPair_NPairs_stage0"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hPhiPair_NPairs_tofStrict"); if (h1) h1->Draw();
  c1->Print(pdfName);

  c1->Clear();
  c1->Divide(1, 1);
  c1->cd(1);
  TH1* hStage0 = (TH1*)fin->Get("hPhiPair_Mass_stage0");
  TH1* hTofStrict = (TH1*)fin->Get("hPhiPair_Mass_tofStrict");
  if (hStage0) {
    hStage0->SetLineColor(kBlue + 1);
    hStage0->SetTitle("Pair QA stage0 / strict TOF overlay;M_{KK} [GeV/c^{2}];Counts");
    hStage0->Draw("hist");
    if (hTofStrict) {
      hTofStrict->SetLineColor(kRed + 1);
      hTofStrict->Draw("hist same");
    }
    TLatex* tag = new TLatex();
    tag->SetNDC(kTRUE);
    tag->SetTextSize(0.035);
    tag->DrawLatex(0.15, 0.86, "Blue: stage0");
    if (hTofStrict) tag->DrawLatex(0.15, 0.80, "Red: strict TOF pair");
  }
  c1->Print(pdfName);

  // Page 8c: strict TOF pair QA mass projections
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiPair_MassVsPt_tofStrict"); if (h2) h2->Draw("colz");
  c1->cd(2); gPad->SetLogz(); h2 = (TH2*)fin->Get("hPhiPair_MassVsCent_tofStrict"); if (h2) { h2->Draw("colz"); drawCent9ConventionNote(); }
  c1->cd(3);
  h2 = (TH2*)fin->Get("hPhiPair_MassVsPt_tofStrict");
  if (h2) {
    Int_t b1 = h2->GetXaxis()->FindBin(0.2 + 1e-6);
    Int_t b2 = h2->GetXaxis()->FindBin(1.6 - 1e-6);
    TH1D* hProjPt = h2->ProjectionY("hPhiPair_Mass_tofStrict_pt0p2to1p6", b1, b2);
    if (hProjPt) {
      hProjPt->SetTitle("Strict TOF M_{KK} (0.2<p_{T}<1.6 GeV/c);M_{KK} [GeV/c^{2}];Counts");
      hProjPt->Draw();
    }
  }
  c1->cd(4);
  h2 = (TH2*)fin->Get("hPhiPair_MassVsCent_tofStrict");
  if (h2) {
    Int_t b1 = h2->GetXaxis()->FindBin(6.0 + 1e-6);
    Int_t b2 = h2->GetXaxis()->FindBin(8.0 - 1e-6);
    TH1D* hProjCent = h2->ProjectionY("hPhiPair_Mass_tofStrict_cent6to8", b1, b2);
    if (hProjCent) {
      hProjCent->SetTitle("Strict TOF M_{KK} (cent9: 6-8);M_{KK} [GeV/c^{2}];Counts");
      hProjCent->Draw();
    }
  }
  c1->Print(pdfName);

  // Page 9: Kaon QA (before vs after PID cut)
  c1->Clear();
  c1->Divide(3, 2);
  // Row 1: Before PID cut (all accepted tracks)
  c1->cd(1); h1 = (TH1*)fin->Get("hPt"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minPt, tr.maxPt);
    }
  }
  c1->cd(2); h1 = (TH1*)fin->Get("hEta"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minEta, tr.maxEta);
    }
  }
  c1->cd(3); h1 = (TH1*)fin->Get("hNSigmaKaon_Raw"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, -phi.nSigmaKaon, phi.nSigmaKaon);
    }
  }
  // Row 2: After PID cut (final Kaons)
  c1->cd(4); h1 = (TH1*)fin->Get("hK_Pt"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minPt, tr.maxPt);
    }
  }
  c1->cd(5); h1 = (TH1*)fin->Get("hK_Eta"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      TrackCutConfig& tr = ConfigManager::GetInstance().GetTrackCuts();
      drawCutLines1D(h1, tr.minEta, tr.maxEta);
    }
  }
  c1->cd(6); h1 = (TH1*)fin->Get("hK_NSigma"); if (h1) {
    h1->Draw();
    if (gConfigLoaded) {
      PhiCutConfig& phi = ConfigManager::GetInstance().GetPhiCuts();
      drawCutLines1D(h1, -phi.nSigmaKaon, phi.nSigmaKaon);
    }
  }
  c1->Print(pdfName);

  // Page 10: Rapidity window variations (|y| < 0.4, 0.3, 0.2, 0.1)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hMKK_RapidityCut_0p4"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hMKK_RapidityCut_0p3"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hMKK_RapidityCut_0p2"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hMKK_RapidityCut_0p1"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 11: M_KK in K+ multiplicity bins (angle+rapidity cut)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hMKK_Mult0to5_KPlus"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hMKK_Mult5to10_KPlus"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hMKK_Mult10to20_KPlus"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hMKK_Mult20up_KPlus"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 12: M_KK in K- multiplicity bins (angle+rapidity cut)
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); h1 = (TH1*)fin->Get("hMKK_Mult0to5_KMinus"); if (h1) h1->Draw();
  c1->cd(2); h1 = (TH1*)fin->Get("hMKK_Mult5to10_KMinus"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hMKK_Mult10to20_KMinus"); if (h1) h1->Draw();
  c1->cd(4); h1 = (TH1*)fin->Get("hMKK_Mult20up_KMinus"); if (h1) h1->Draw();
  c1->Print(pdfName);

  // Page 13: K+ and K- multiplicity (per event) and correlation
  c1->Clear();
  c1->Divide(2, 2);
  c1->cd(1); gPad->SetLogz(); h2 = (TH2*)fin->Get("hNKaonPlusVsNKaonMinus"); if (h2) h2->Draw("colz");
  c1->cd(2); h1 = (TH1*)fin->Get("hNKaonPlus"); if (h1) h1->Draw();
  c1->cd(3); h1 = (TH1*)fin->Get("hNKaonMinus"); if (h1) h1->Draw();
  c1->cd(4); /* spare */;
  c1->Print(pdfName);

  // Console: verify new TOF / K / DCA histograms exist and have entries (re-run analysis if missing or empty)
  {
    std::cout << "\n=== checkHistAnaPhi: TOF / K / DCA QA histogram entries ===\n";
    const char* keys[] = {"hVr",
                           "hVr_After",
                           "hVz_After",
                           "hVzDiff_After",
                           "hRefMult_After",
                           "hPhi_Raw",
                           "hNHitsDedx_Raw",
                           "hNHitsDedx",
                           "hNSigmaKaon_Raw",
                           "hBetaVsP",
                           "hMass2VsP",
                           "hBetaVsPt",
                           "hMass2VsPt",
                           "hDedxVsPt",
                           "hNSigmaPionVsPt",
                           "hNSigmaKaonVsPt",
                           "hNSigmaProtonVsPt",
                           "hMass2VsP_TpcKaon",
                           "hMass2VsP_IsKaon",
                           "hMass2VsPt_TpcKaon",
                           "hMass2VsPt_IsKaon",
                           "hK_Pt",
                           "hK_Eta",
                           "hK_NSigma",
                           "hDCAKK_All",
                           "hDCAKK_Pass",
                           "hM2q2VsPq",
                           "hDeltaOneOverBetaVsP",
                           "hDeltaOneOverBetaKaon",
                           "hPhiPair_Mass_stage0",
                           "hPhiPair_Mass_tofStrict",
                           "hPhiPair_MassVsPt_tofStrict",
                           "hPhiPair_MassVsCent_tofStrict",
                           "hPhiPair_NPairs_stage0",
                           "hPhiPair_NPairs_tofStrict",
                           0};
    for (Int_t i = 0; keys[i]; ++i) {
      TObject* o = fin->Get(keys[i]);
      if (!o) {
        std::cout << "  " << keys[i] << ": NOT IN FILE\n";
        continue;
      }
      if (o->InheritsFrom("TH1")) {
        TH1* hh = (TH1*)o;
        Double_t n = hh->GetEntries();
        std::cout << "  " << keys[i] << ": entries=" << n;
        if (n < 1.0)
          std::cout << "  [empty — use ROOT from a run after new hist fills]";
        std::cout << "\n";
      } else {
        std::cout << "  " << keys[i] << ": unexpected class " << o->ClassName() << "\n";
      }
    }
    std::cout << "=============================================================\n\n";
  }

  PdfHeader::ClosePdf(pdfName);

  delete c1;
  fin->Close();

  std::cout << "Done. PDF saved to: " << pdfName.Data() << std::endl;
}
