// Read-only comparison of existing anaLambda and anaLambda_KFParticle output.
// Usage (ROOT 5 / root4star):
// compareLambdaHelixKF(oldRoot, kfRoot, outputStem, 10000, 1.05, 1.25, 0.001)
// Plot parameters only; no track, event, PID or topology cuts are applied here.
// The caller must independently establish identical ordered input event ranges.
// This macro checks read counters, but equal counters alone do not prove identity.

#include <TROOT.h>
#include <TSystem.h>
#include <TFile.h>
#include <TH1.h>
#include <TH1D.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TNamed.h>
#include <TObjString.h>
#include <TString.h>
#include <TStyle.h>
#include <TMath.h>
#include <iostream>

static Int_t LambdaComparisonFailure(const char* message) {
  std::cerr << "[compareLambdaHelixKF] ERROR: " << message << std::endl;
  return 1;
}

// Match the standalone line emitted by KfParticleCutConfig::Dump, not a
// filename, comment or substring such as lambda_imp5_extra. Older configuration
// snapshots without this line retain the original reference-profile labels.
static Bool_t LambdaComparisonIsImp5(const TObjString* configuration) {
  if (!configuration) return kFALSE;
  TString lines("\n");
  lines += configuration->GetString();
  lines += "\n";
  lines.ReplaceAll("\r\n", "\n");
  return lines.Contains("\nselectionProfile: lambda_imp5\n");
}

// No interpolation or fractional redistribution: every displayed bin consists
// of an integer number of whole source bins with exactly matching boundaries.
static TH1D* LambdaComparisonRebin(const TH1* source, const char* name,
                                  Double_t low, Double_t high, Double_t width) {
  if (!source || source->GetDimension() != 1 || !TMath::Finite(low) ||
      !TMath::Finite(high) || !TMath::Finite(width) || width <= 0 || high <= low)
    return 0;
  const Double_t tolerance = 1.e-9;
  const Int_t outputBins = TMath::Nint((high - low) / width);
  if (outputBins <= 0 || TMath::Abs(low + outputBins * width - high) > tolerance)
    return 0;
  const TAxis* axis = source->GetXaxis();
  if (low < axis->GetXmin() - tolerance || high > axis->GetXmax() + tolerance)
    return 0;
  TH1D* result = new TH1D(name, "", outputBins, low, high);
  result->SetDirectory(0);
  result->Sumw2();
  Int_t first = 1;
  while (first <= source->GetNbinsX() &&
         axis->GetBinLowEdge(first) < low - tolerance) ++first;
  for (Int_t output = 1; output <= outputBins; ++output) {
    const Double_t left = result->GetXaxis()->GetBinLowEdge(output);
    const Double_t right = result->GetXaxis()->GetBinUpEdge(output);
    if (first > source->GetNbinsX() ||
        TMath::Abs(axis->GetBinLowEdge(first) - left) > tolerance) {
      delete result;
      return 0;
    }
    Double_t content = 0.;
    Double_t variance = 0.;
    Double_t lastEdge = left;
    while (first <= source->GetNbinsX() &&
           axis->GetBinUpEdge(first) <= right + tolerance) {
      const Double_t value = source->GetBinContent(first);
      const Double_t error = source->GetBinError(first);
      if (!TMath::Finite(value) || !TMath::Finite(error) || value < 0 || error < 0) {
        delete result;
        return 0;
      }
      content += value;
      variance += error * error;
      lastEdge = axis->GetBinUpEdge(first);
      ++first;
    }
    if (TMath::Abs(lastEdge - right) > tolerance) {
      delete result;
      return 0;
    }
    result->SetBinContent(output, content);
    result->SetBinError(output, TMath::Sqrt(variance));
  }
  // These are unweighted candidate histograms; entries here describe the
  // displayed range. The source all-mass entries are printed separately.
  result->SetEntries(result->Integral());
  return result;
}

static void LambdaComparisonStyle(TH1* histogram, Int_t color, Int_t style,
                                  const char* yTitle) {
  histogram->SetStats(kFALSE);
  histogram->SetTitle("");
  histogram->SetLineColor(color);
  histogram->SetLineStyle(style);
  histogram->SetLineWidth(2);
  histogram->SetMarkerColor(color);
  histogram->GetXaxis()->SetTitle("M_{p#pi^{-}} [GeV/c^{2}]");
  histogram->GetYaxis()->SetTitle(yTitle);
  histogram->GetXaxis()->SetTitleSize(0.047);
  histogram->GetYaxis()->SetTitleSize(0.047);
  histogram->GetXaxis()->SetLabelSize(0.039);
  histogram->GetYaxis()->SetLabelSize(0.039);
  histogram->GetXaxis()->SetTitleOffset(1.15);
  histogram->GetYaxis()->SetTitleOffset(1.45);
  histogram->GetXaxis()->SetNdivisions(505);
  histogram->SetMinimum(0.);
}

static void LambdaComparisonPanel(TH1* helix, TH1* kf, const char* title,
                                  Double_t helixCount, Double_t kfCount,
                                  Bool_t normalized = kFALSE,
                                  const char* kfLabel = "KFParticle") {
  gPad->SetLeftMargin(0.16);
  gPad->SetRightMargin(0.04);
  gPad->SetBottomMargin(0.15);
  gPad->SetTopMargin(0.06);
  gPad->SetTicks(1, 1);
  Double_t maximum = TMath::Max(helix->GetMaximum(), kf->GetMaximum());
  helix->SetMaximum(maximum > 0 ? maximum * 1.45 : 1.);
  helix->Draw(normalized && helixCount <= 0 ? "AXIS" : "HIST");
  if (!normalized || kfCount > 0) kf->Draw("HIST SAME");
  TLegend* legend = new TLegend(0.52, 0.75, 0.94, 0.91);
  legend->SetBorderSize(0);
  legend->SetFillStyle(0);
  legend->SetTextFont(42);
  legend->SetTextSize(0.031);
  legend->AddEntry(helix, Form("Helix: N = %.0f", helixCount), "l");
  legend->AddEntry(kf, Form("%s: N = %.0f", kfLabel, kfCount), "l");
  legend->Draw();
  TLatex* text = new TLatex();
  text->SetNDC(kTRUE);
  text->SetTextSize(0.034);
  text->DrawLatex(0.19, 0.91, title);
  if (normalized && (helixCount <= 0 || kfCount <= 0)) {
    text->SetTextSize(0.028);
    text->DrawLatex(0.19, 0.68, "N = 0: corresponding normalized shape unavailable");
  }
  gPad->RedrawAxis();
}

Int_t compareLambdaHelixKF(const char* helixRoot, const char* kfRoot,
                          const char* outputStem, Long64_t nInputEvents = 10000,
                          Double_t massMin = 1.05, Double_t massMax = 1.25,
                          Double_t binWidth = 0.001) {
  if (!helixRoot || !kfRoot || !outputStem || !outputStem[0] || nInputEvents <= 0)
    return LambdaComparisonFailure("invalid input filenames, output stem or event count");
  TString pdf = TString(outputStem) + ".pdf";
  TString png = TString(outputStem) + ".png";
  TString root = TString(outputStem) + ".root";
  if (!gSystem->AccessPathName(pdf) || !gSystem->AccessPathName(png) ||
      !gSystem->AccessPathName(root))
    return LambdaComparisonFailure("refusing to overwrite an existing QA output");

  TFile* oldFile = TFile::Open(helixRoot, "READ");
  TFile* newFile = TFile::Open(kfRoot, "READ");
  if (!oldFile || oldFile->IsZombie() || !newFile || newFile->IsZombie())
    return LambdaComparisonFailure("cannot open both analysis ROOT files");
  if (oldFile->TestBit(TFile::kRecovered) || newFile->TestBit(TFile::kRecovered))
    return LambdaComparisonFailure("refusing a recovered / potentially incomplete ROOT file");

  TH1* oldMass = dynamic_cast<TH1*>(oldFile->Get("hLambda_InvMass"));
  // KF hLambda_InvMass combines Lambda and anti-Lambda; do NOT compare that
  // combined histogram with the p+ pi- only legacy analysis.
  TH1* newMass = dynamic_cast<TH1*>(newFile->Get("hKfLambdaMassSelected"));
  TH1* oldRead = dynamic_cast<TH1*>(oldFile->Get("hRefMultVsNTOFMatch"));
  TH1* newRead = dynamic_cast<TH1*>(newFile->Get("hRefMultVsNTOFMatch"));
  TH1* oldAccepted = dynamic_cast<TH1*>(oldFile->Get("hN"));
  TH1* newAccepted = dynamic_cast<TH1*>(newFile->Get("hN"));
  TH1* stages = dynamic_cast<TH1*>(newFile->Get("hKfStages"));
  TNamed* status = dynamic_cast<TNamed*>(newFile->Get("KFRunStatus"));
  TObjString* configuration = dynamic_cast<TObjString*>(newFile->Get("KFParticleEffectiveConfiguration"));
  if (!oldMass || !newMass || !oldRead || !newRead || !oldAccepted ||
      !newAccepted || !stages || !status || !configuration)
    return LambdaComparisonFailure("required mass, event-counter or KF provenance object missing");
  const Bool_t isImp5 = LambdaComparisonIsImp5(configuration);
  const char* kfLabel = isImp5 ? "KFParticle (Imp5)" : "KFParticle";
  const char* selectionNote = isImp5
      ? "KFParticle (Imp5): common event/PID/nHits thresholds matched to Helix; KF-specific requirements and reconstructed-state topology differences remain. Not an efficiency-ratio or S/B measurement."
      : "Current event/PID/topology selections differ.";
  if (TString(status->GetTitle()) != "completed")
    return LambdaComparisonFailure("KFRunStatus is not completed");
  // Unlike legacy hVz, hRefMultVsNTOFMatch is filled before bad-run rejection.
  if (TMath::Abs(oldRead->GetEntries() - nInputEvents) > 0.1 ||
      TMath::Abs(newRead->GetEntries() - nInputEvents) > 0.1 ||
      TMath::Abs(stages->GetBinContent(1) - nInputEvents) > 0.1)
    return LambdaComparisonFailure("read counters disagree with requested equal input event count");
  const Double_t oldEvents = oldAccepted->GetEntries();
  const Double_t newEvents = newAccepted->GetEntries();
  if (oldEvents < 0 || newEvents <= 0 || oldEvents > nInputEvents ||
      newEvents > nInputEvents || TMath::Abs(newEvents - stages->GetBinContent(2)) > 0.1 ||
      TMath::Abs(newMass->GetEntries() - stages->GetBinContent(13)) > 0.1)
    return LambdaComparisonFailure("accepted-event or signed-candidate counters are inconsistent");

  TH1D* oldCounts = LambdaComparisonRebin(oldMass, "HelixLambdaCounts", massMin, massMax, binWidth);
  TH1D* newCounts = LambdaComparisonRebin(newMass, "KfLambdaCounts", massMin, massMax, binWidth);
  if (!oldCounts || !newCounts)
    return LambdaComparisonFailure("requested common bin edges do not match whole source bins, or invalid contents");
  const Double_t oldIntegral = oldCounts->Integral();
  const Double_t newIntegral = newCounts->Integral();
  TH1D* oldShape = dynamic_cast<TH1D*>(oldCounts->Clone("HelixLambdaUnitArea"));
  TH1D* newShape = dynamic_cast<TH1D*>(newCounts->Clone("KfLambdaUnitArea"));
  oldShape->SetDirectory(0);
  newShape->SetDirectory(0);
  if (oldIntegral > 0) oldShape->Scale(1. / oldIntegral);
  if (newIntegral > 0) newShape->Scale(1. / newIntegral);

  gROOT->SetBatch(kTRUE);
  gStyle->SetOptStat(0);
  TString countTitle = Form("Candidates / (%.3g MeV/c^{2})", 1000. * binWidth);
  TString shapeTitle = Form("Unit-area fraction / (%.3g MeV/c^{2})", 1000. * binWidth);
  LambdaComparisonStyle(oldCounts, kBlue + 1, 1, countTitle);
  LambdaComparisonStyle(newCounts, kRed + 1, 2, countTitle);
  LambdaComparisonStyle(oldShape, kBlue + 1, 1, shapeTitle);
  LambdaComparisonStyle(newShape, kRed + 1, 2, shapeTitle);
  TCanvas* canvas = new TCanvas("LambdaHelixKFComparison", Form("Lambda Helix / %s comparison", kfLabel), 1600, 850);
  canvas->SetFillColor(kWhite);
  TLatex* heading = new TLatex();
  heading->SetNDC(kTRUE);
  heading->SetTextFont(42);
  heading->SetTextSize(0.033);
  heading->DrawLatex(0.06, 0.953, Form("AuAu13p5 fixed target: #Lambda #rightarrow p#pi^{-}  |  Helix vs %s", kfLabel));
  heading->SetTextSize(0.024);
  heading->DrawLatex(0.06, 0.908, Form("Same first %lld input events; accepted / processed: Helix %.0f, %s %.0f", nInputEvents, oldEvents, kfLabel, newEvents));
  TPad* left = new TPad("LambdaRawCountsPanel", "Raw counts", 0., 0.14, 0.5, 0.865);
  TPad* right = new TPad("LambdaShapePanel", "Unit-area shapes", 0.5, 0.14, 1., 0.865);
  left->Draw();
  right->Draw();
  left->cd();
  LambdaComparisonPanel(oldCounts, newCounts, "Raw counts (no scaling)", oldIntegral, newIntegral, kFALSE, kfLabel);
  right->cd();
  LambdaComparisonPanel(oldShape, newShape, "Unit area (shape only)", oldIntegral, newIntegral, kTRUE, kfLabel);
  canvas->cd();
  heading->SetTextSize(isImp5 ? 0.020 : 0.022);
  heading->DrawLatex(0.06, isImp5 ? 0.105 : 0.086, Form("N = candidates in %.3f #leq M < %.3f GeV/c^{2}; only #Lambda (PDG +3122), not #bar{#Lambda}.", massMin, massMax));
  if (isImp5) {
    heading->DrawLatex(0.06, 0.066, "Imp5: common event / PID / nHits thresholds matched to Helix; KF-specific requirements remain.");
    heading->DrawLatex(0.06, 0.030, "Topology uses different reconstructed states; no efficiency-ratio or S/B measurement. No parent mass constraint.");
  } else {
    heading->DrawLatex(0.06, 0.046, "Current event / PID / topology selections differ; this is not a reconstruction-efficiency ratio. No parent mass constraint.");
  }
  canvas->Modified();
  canvas->Update();

  TString directory = gSystem->DirName(outputStem);
  if (gSystem->AccessPathName(directory) && gSystem->mkdir(directory, kTRUE) != 0)
    return LambdaComparisonFailure("cannot create QA output directory");
  TFile* output = TFile::Open(root, "CREATE");
  if (!output || output->IsZombie())
    return LambdaComparisonFailure("cannot create QA ROOT file");
  output->cd();
  TNamed sources("LambdaComparisonSources", Form("Helix: %s:hLambda_InvMass\nKF: %s:hKfLambdaMassSelected", helixRoot, kfRoot));
  TNamed scope("LambdaComparisonScope", Form("Same ordered input range is established by run manifest, not inferred from histogram counts. Requested input events=%lld; Helix read=%.0f accepted=%.0f; KF read=%.0f accepted=%.0f. %s Display range=[%.9g,%.9g), width=%.9g GeV/c^2. Source entries (all mass): Helix=%.0f KF=%.0f. Displayed candidates: Helix=%.0f KF=%.0f. Shapes separately normalized over displayed range. No mass fit or background subtraction.", nInputEvents, oldRead->GetEntries(), oldEvents, newRead->GetEntries(), newEvents, selectionNote, massMin, massMax, binWidth, oldMass->GetEntries(), newMass->GetEntries(), oldIntegral, newIntegral));
  Int_t bytes = oldCounts->Write() + newCounts->Write() + oldShape->Write() + newShape->Write();
  bytes += canvas->Write() + sources.Write() + scope.Write();
  bytes += configuration->Write("KfSourceEffectiveConfiguration");
  output->Flush();
  const Bool_t writeError = output->TestBit(TFile::kWriteError);
  output->Close();
  if (bytes <= 0 || writeError || output->TestBit(TFile::kWriteError))
    return LambdaComparisonFailure("error while writing QA ROOT file");
  canvas->Print(pdf);
  canvas->Print(png);
  FileStat_t pdfInfo;
  FileStat_t pngInfo;
  if (gSystem->GetPathInfo(pdf, pdfInfo) != 0 || pdfInfo.fSize <= 0 ||
      gSystem->GetPathInfo(png, pngInfo) != 0 || pngInfo.fSize <= 0)
    return LambdaComparisonFailure("PDF or PNG output is missing or empty");

  std::cout << "[compareLambdaHelixKF] PASS\n" << sources.GetTitle() << "\n"
            << scope.GetTitle() << "\n"
            << "Helix mass underflow=" << oldMass->GetBinContent(0)
            << " overflow=" << oldMass->GetBinContent(oldMass->GetNbinsX() + 1) << "\n"
            << "KF mass underflow=" << newMass->GetBinContent(0)
            << " overflow=" << newMass->GetBinContent(newMass->GetNbinsX() + 1) << "\n"
            << "PDF: " << pdf << "\nPNG: " << png << "\nROOT: " << root << std::endl;
  oldFile->Close();
  newFile->Close();
  return 0;
}



