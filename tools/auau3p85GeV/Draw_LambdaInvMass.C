#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TLine.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>

void Draw_LambdaInvMass() {
  // 스タイルの設定
  gStyle->SetOptStat(0); // エントリー数、平均、RMS等を表示しない
  gStyle->SetOptTitle(1);

  // 入力ファイル
  TString inputFile =
    "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
    //      "auau3p85_anaLambdaNuclearId/auau3p85_all1.root";
    "auau3p85_anaLambdaNuclearId/auau3p85_all1.root";
  TFile *fIn = TFile::Open(inputFile, "READ");
  if (!fIn || fIn->IsZombie()) {
    std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
    return;
  }

  // 出力ディレクトリの作成
  TString outDir = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
                   "auau3p85_anaLambdaNuclearId/pdf";
  gSystem->mkdir(outDir, kTRUE);

  // ヒストグラムの取得
  TString histName = "hLambda_InvMass";
  TH1 *h = (TH1 *)fIn->Get(histName);

  // もしlambdaディレクトリに入っている場合などにも対応
  if (!h) {
    TDirectory *dir = (TDirectory *)fIn->Get("lambda");
    if (dir) {
      h = (TH1 *)dir->Get(histName);
    }
  }

  if (!h) {
    std::cerr << "Error: Histogram " << histName << " not found." << std::endl;
    fIn->Close();
    return;
  }

  // キャンバスの作成
  TCanvas *c1 = new TCanvas("c1", "Lambda Invariant Mass", 800, 600);
  c1->SetLeftMargin(0.15);

  // 描画
  h->SetLineColor(kBlack);
  h->SetLineWidth(2);
  h->GetXaxis()->SetTitle("M_{p#pi^{-}} [GeV/c^{2}]");
  h->GetYaxis()->SetTitle("Counts");
  h->GetYaxis()->SetTitleOffset(1.5);
  h->Draw("HIST");

  // カット領域のパラメータ
  // yamlファイルの値:
  // MeanLambda: 1.11596
  // sigmaLambda: 2.96e-03
  // nsigmaLambda: 3.0
  double mean = 1.11596;
  double sigma = 2.96e-03;
  double nsigma = 3.0;

  double massMin = mean - nsigma * sigma;
  double massMax = mean + nsigma * sigma;

  double massSB1Min = mean - 4.0 * nsigma * sigma;
  double massSB1Max = massMin;

  double massSB2Min = massMax;
  double massSB2Max = mean + 4.0 * nsigma * sigma;

  double yMax = h->GetMaximum();

  // TLineでSignal領域を描画 (赤色)
  TLine *lineMin = new TLine(massMin, 0, massMin, yMax);
  lineMin->SetLineColor(kRed);
  lineMin->SetLineStyle(2); // 破線
  lineMin->SetLineWidth(2);
  lineMin->Draw("SAME");

  TLine *lineMax = new TLine(massMax, 0, massMax, yMax);
  lineMax->SetLineColor(kRed);
  lineMax->SetLineStyle(2); // 破線
  lineMax->SetLineWidth(2);
  lineMax->Draw("SAME");

  // TLineでSideband領域を描画 (青色)
  TLine *lineSB1Min = new TLine(massSB1Min, 0, massSB1Min, yMax);
  lineSB1Min->SetLineColor(kBlue);
  lineSB1Min->SetLineStyle(2);
  lineSB1Min->SetLineWidth(2);
  lineSB1Min->Draw("SAME");

  TLine *lineSB2Max = new TLine(massSB2Max, 0, massSB2Max, yMax);
  lineSB2Max->SetLineColor(kBlue);
  lineSB2Max->SetLineStyle(2);
  lineSB2Max->SetLineWidth(2);
  lineSB2Max->Draw("SAME");

  // PDFとして保存
  TString outFile = Form("%s/Lambda_InvMass.pdf", outDir.Data());
  c1->Print(outFile);

  std::cout << "Drawing completed. PDF file saved to: " << outFile << std::endl;

  fIn->Close();
}
