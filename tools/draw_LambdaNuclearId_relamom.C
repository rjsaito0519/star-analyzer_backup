#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TLegend.h"
#include "TMath.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>

void draw_LambdaNuclearId_relamom() {
  // スタイルの設定
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(1);

  // 入力ファイル
  TString inputFile =
    "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
    //"auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all.root";
    "auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all_LambdaImprove_WideSide.root";
  
  TFile *fIn = TFile::Open(inputFile, "READ");
  if (!fIn || fIn->IsZombie()) {
    std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
    return;
  }

  // 出力ディレクトリの作成
  TString outDir = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
                   "auau13p5_anaLambdaNuclearId/pdf";
  gSystem->mkdir(outDir, kTRUE);

  const char *species[] = {"d", "t", "3He", "4He"};
  int nSpecies = 4;
  int nCentBins = 9;
  const char *centLabels[] = {"70-80%", "60-70%", "50-60%", "40-50%", "30-40%", "20-30%", "10-20%", "5-10%", "0-5%"};

  // 各原子核についてループ
  for (int i = 0; i < nSpecies; ++i) {
    TString sp = species[i];

    int nRebin = 1;
    if (sp == "t" || sp == "3He") nRebin = 4;
    else if (sp == "4He") nRebin = 5;

    // 3x3のキャンバスを作成 (9つのCentrality用)
    TCanvas *c1 = new TCanvas(Form("c1_%s", sp.Data()),
                              Form("k* for Lambda - %s", sp.Data()), 1200, 900);
    c1->Divide(3, 3);

    // CF用の2ページ目のキャンバス
    TCanvas *c2 = new TCanvas(
        Form("c2_%s", sp.Data()),
        Form("Correlation Function for Lambda - %s", sp.Data()), 1200, 900);
    c2->Divide(3, 3);

    // True SB用の3ページ目のキャンバス
    TCanvas *c3 = new TCanvas(Form("c3_%s", sp.Data()),
                              Form("True Sidebands for Lambda - %s", sp.Data()),
                              1200, 900);
    c3->Divide(3, 3);

    // Mixed SB用の4ページ目のキャンバス
    TCanvas *c4 = new TCanvas(
        Form("c4_%s", sp.Data()),
        Form("Mixed Sidebands for Lambda - %s", sp.Data()), 1200, 900);
    c4->Divide(3, 3);

    // SB Subtraction用の5ページ目のキャンバス
    TCanvas *c5 = new TCanvas(Form("c5_%s", sp.Data()),
                              Form("SB Subtracted for Lambda - %s", sp.Data()),
                              1200, 900);
    c5->Divide(3, 3);

    // SB Subtraction CF用の6ページ目のキャンバス
    TCanvas *c6 = new TCanvas(
        Form("c6_%s", sp.Data()),
        Form("CF (SB Subtracted) for Lambda - %s", sp.Data()), 1200, 900);
    c6->Divide(3, 3);

    // True event histograms are in the "true" subdirectory
    TDirectory *dirNuc = (TDirectory *)fIn->Get("true");
    TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

    for (int j = 0; j < nCentBins; ++j) {
      c1->cd(j + 1);

      // ヒストグラムの取得
      TString histName = Form("hKstar_%s_CentBin%d", sp.Data(), j);
      TH1 *h = 0;
      if (dirNuc) {
        h = (TH1 *)dirNuc->Get(histName);
      }
      if (!h) {
        // fallback: try root directory (for older files without subdirectory
        // structure)
        h = (TH1 *)fIn->Get(histName);
      }

      TString mixName = Form("hKstar_Mixed_%s_CentBin%d", sp.Data(), j);
      TH1 *hmix = 0;
      if (dirMix) {
        hmix = (TH1 *)dirMix->Get(mixName);
      }
      if (!hmix) {
        hmix = (TH1 *)fIn->Get(mixName);
      }

      TH1 *hmix_unscaled = 0;

      if (h) {
        h->Rebin(nRebin); // Rebin
        h->GetXaxis()->SetRangeUser(0.0, 0.6);
        // 統計エラーを考慮 (事前にSumw2が呼ばれていない場合のために)
        if (!h->GetSumw2N()) {
          h->Sumw2();
        }

        // True: Red
        h->SetMarkerStyle(20);
        h->SetMarkerSize(0.8);
        h->SetLineColor(kRed);
        h->SetMarkerColor(kRed);

        if (hmix) {
          hmix->Rebin(nRebin); // Rebin
          hmix->GetXaxis()->SetRangeUser(0.0, 0.6);
          if (!hmix->GetSumw2N()) {
            hmix->Sumw2();
          }

          // Scale前のMixedヒストグラムをp.4のために保存しておく
          hmix_unscaled = (TH1 *)hmix->Clone(
              Form("hmix_unscaled_%s_CentBin%d", sp.Data(), j));

          // Mixed: Black
          hmix->SetMarkerStyle(20);
          hmix->SetMarkerSize(0.8);
          hmix->SetLineColor(kBlack);
          hmix->SetMarkerColor(kBlack);

          // 0.4 - 0.6 GeV/c で積分してスケールを合わせる
          int bin1 = h->FindBin(0.40001);
          int bin2 = h->FindBin(0.59999);
          double intTrue = h->Integral(bin1, bin2);
          double intMix =
              hmix->Integral(hmix->FindBin(0.40001), hmix->FindBin(0.59999));

          if (intMix > 0) {
            hmix->Scale(intTrue / intMix);
          }

          // Y軸の最大値を調整（TrueとMixedの両方が収まるように）
          double maxH = h->GetMaximum();
          double maxMix = hmix->GetMaximum();
          h->SetMaximum(TMath::Max(maxH, maxMix) * 1.2);
        }

        h->SetTitle(Form("%s CentBin%d (%s)", sp.Data(), j, centLabels[j]));
        // E1 オプションでエラーバー付きで描画
        h->Draw("E1");
        if (hmix) {
          hmix->Draw("E1 same");
        }
        TLegend *leg1 = new TLegend(0.15, 0.7, 0.55, 0.88);
        leg1->AddEntry(h, "Signal Window (True)", "pe");
        if (hmix) leg1->AddEntry(hmix, "Mixed", "pe");
        leg1->Draw();

        // --- 2ページ目: Correlation Function (CF = True / Mixed) ---
        c2->cd(j + 1);
        if (hmix) {
          TH1 *hCF = (TH1 *)h->Clone(Form("hCF_%s_CentBin%d", sp.Data(), j));
          hCF->SetTitle(Form("CF (True / Mixed) %s CentBin%d (%s)", sp.Data(), j, centLabels[j]));
          hCF->Divide(hmix);

          // 黒色でプロット、Y軸の範囲を[0, 2]に固定
          hCF->SetLineColor(kBlack);
          hCF->SetMarkerColor(kBlack);
          hCF->GetYaxis()->SetTitle("C(k*) = True / Mixed");
          hCF->GetYaxis()->SetRangeUser(0.0, 2.0);

          hCF->Draw("E1");
        }

        // --- 3ページ目: True SideBands ---
        c3->cd(j + 1);
        TH1 *h3 = (TH1 *)h->Clone(Form("h3_%s_CentBin%d", sp.Data(), j));
        h3->SetTitle(Form("True Sidebands %s CentBin%d (%s)", sp.Data(), j, centLabels[j]));
        h3->SetLineColor(kBlack);
        h3->SetMarkerColor(kBlack);
        h3->Draw("E1");

        TString nameSBNeg = Form("hKstar_%s_SBNeg_CentBin%d", sp.Data(), j);
        TString nameSBPos = Form("hKstar_%s_SBPos_CentBin%d", sp.Data(), j);
        TH1 *hSBNeg_tmp = dirNuc ? (TH1 *)dirNuc->Get(nameSBNeg) : (TH1 *)fIn->Get(nameSBNeg);
        TH1 *hSBPos_tmp = dirNuc ? (TH1 *)dirNuc->Get(nameSBPos) : (TH1 *)fIn->Get(nameSBPos);

        TH1 *hSBNeg_orig = hSBNeg_tmp ? (TH1 *)hSBNeg_tmp->Clone(Form("%s_orig", nameSBNeg.Data())) : 0;
        TH1 *hSBPos_orig = hSBPos_tmp ? (TH1 *)hSBPos_tmp->Clone(Form("%s_orig", nameSBPos.Data())) : 0;

        if (hSBNeg_orig) {
          if (!hSBNeg_orig->GetSumw2N()) hSBNeg_orig->Sumw2();
          hSBNeg_orig->Scale(1.0 / 3.0);
        }
        if (hSBPos_orig) {
          if (!hSBPos_orig->GetSumw2N()) hSBPos_orig->Sumw2();
          hSBPos_orig->Scale(1.0 / 3.0);
        }

        TH1 *hSBNeg = hSBNeg_orig ? (TH1 *)hSBNeg_orig->Clone(Form(
                                        "hSBNeg_p3_%s_CentBin%d", sp.Data(), j))
                                  : 0;
        TH1 *hSBPos = hSBPos_orig ? (TH1 *)hSBPos_orig->Clone(Form(
                                        "hSBPos_p3_%s_CentBin%d", sp.Data(), j))
                                  : 0;

        double max3 = h3->GetMaximum();
        if (hSBNeg) {
          hSBNeg->Rebin(nRebin);
          if (!hSBNeg->GetSumw2N())
            hSBNeg->Sumw2();
          hSBNeg->SetLineColor(kBlue);
          hSBNeg->SetMarkerColor(kBlue);
          hSBNeg->SetMarkerStyle(20);
          hSBNeg->SetMarkerSize(0.8);
          hSBNeg->Draw("E1 same");
          if (hSBNeg->GetMaximum() > max3)
            max3 = hSBNeg->GetMaximum();
        }
        if (hSBPos) {
          hSBPos->Rebin(nRebin);
          if (!hSBPos->GetSumw2N())
            hSBPos->Sumw2();
          hSBPos->SetLineColor(kGreen + 2); // 見やすい緑
          hSBPos->SetMarkerColor(kGreen + 2);
          hSBPos->SetMarkerStyle(20);
          hSBPos->SetMarkerSize(0.8);
          hSBPos->Draw("E1 same");
          if (hSBPos->GetMaximum() > max3)
            max3 = hSBPos->GetMaximum();
        }
        h3->SetMaximum(max3 * 1.2);
        
        TLegend *leg3 = new TLegend(0.15, 0.7, 0.55, 0.88);
        leg3->AddEntry(h3, "Signal Window", "pe");
        if (hSBNeg) leg3->AddEntry(hSBNeg, "Sideband (-)", "pe");
        if (hSBPos) leg3->AddEntry(hSBPos, "Sideband (+)", "pe");
        leg3->Draw();

        // --- 4ページ目: Mixed SideBands ---
        c4->cd(j + 1);

        TString mixNameSBNeg =
            Form("hKstar_Mixed_%s_SBNeg_CentBin%d", sp.Data(), j);
        TString mixNameSBPos =
            Form("hKstar_Mixed_%s_SBPos_CentBin%d", sp.Data(), j);
        TH1 *hmixSBNeg_tmp = dirMix ? (TH1 *)dirMix->Get(mixNameSBNeg) : (TH1 *)fIn->Get(mixNameSBNeg);
        TH1 *hmixSBPos_tmp = dirMix ? (TH1 *)dirMix->Get(mixNameSBPos) : (TH1 *)fIn->Get(mixNameSBPos);

        TH1 *hmixSBNeg_orig = hmixSBNeg_tmp ? (TH1 *)hmixSBNeg_tmp->Clone(Form("%s_orig", mixNameSBNeg.Data())) : 0;
        TH1 *hmixSBPos_orig = hmixSBPos_tmp ? (TH1 *)hmixSBPos_tmp->Clone(Form("%s_orig", mixNameSBPos.Data())) : 0;

        if (hmixSBNeg_orig) {
          if (!hmixSBNeg_orig->GetSumw2N()) hmixSBNeg_orig->Sumw2();
          hmixSBNeg_orig->Scale(1.0 / 3.0);
        }
        if (hmixSBPos_orig) {
          if (!hmixSBPos_orig->GetSumw2N()) hmixSBPos_orig->Sumw2();
          hmixSBPos_orig->Scale(1.0 / 3.0);
        }

        TH1 *hmixSBNeg = hmixSBNeg_orig
                             ? (TH1 *)hmixSBNeg_orig->Clone(Form(
                                   "hmixSBNeg_p4_%s_CentBin%d", sp.Data(), j))
                             : 0;
        TH1 *hmixSBPos = hmixSBPos_orig
                             ? (TH1 *)hmixSBPos_orig->Clone(Form(
                                   "hmixSBPos_p4_%s_CentBin%d", sp.Data(), j))
                             : 0;

        if (hmix_unscaled) {
          TH1 *h4 = hmix_unscaled;
          h4->SetName(Form("h4_%s_CentBin%d", sp.Data(), j));
          h4->SetTitle(Form("Mixed Sidebands %s CentBin%d (%s)", sp.Data(), j, centLabels[j]));
          h4->SetLineColor(kRed);
          h4->SetMarkerColor(kRed);
          h4->SetMarkerStyle(20);
          h4->SetMarkerSize(0.8);
          h4->Draw("E1");

          double max4 = h4->GetMaximum();
          if (hmixSBNeg) {
            hmixSBNeg->Rebin(nRebin);
            if (!hmixSBNeg->GetSumw2N())
              hmixSBNeg->Sumw2();
            hmixSBNeg->SetLineColor(kBlue);
            hmixSBNeg->SetMarkerColor(kBlue);
            hmixSBNeg->SetMarkerStyle(20);
            hmixSBNeg->SetMarkerSize(0.8);
            hmixSBNeg->Draw("E1 same");
            if (hmixSBNeg->GetMaximum() > max4)
              max4 = hmixSBNeg->GetMaximum();
          }
          if (hmixSBPos) {
            hmixSBPos->Rebin(nRebin);
            if (!hmixSBPos->GetSumw2N())
              hmixSBPos->Sumw2();
            hmixSBPos->SetLineColor(kGreen + 2);
            hmixSBPos->SetMarkerColor(kGreen + 2);
            hmixSBPos->SetMarkerStyle(20);
            hmixSBPos->SetMarkerSize(0.8);
            hmixSBPos->Draw("E1 same");
            if (hmixSBPos->GetMaximum() > max4)
              max4 = hmixSBPos->GetMaximum();
          }
          h4->SetMaximum(max4 * 1.2);
          
          TLegend *leg4 = new TLegend(0.15, 0.7, 0.55, 0.88);
          leg4->AddEntry(h4, "Signal Window (Mixed)", "pe");
          if (hmixSBNeg) leg4->AddEntry(hmixSBNeg, "Sideband (-) (Mixed)", "pe");
          if (hmixSBPos) leg4->AddEntry(hmixSBPos, "Sideband (+) (Mixed)", "pe");
          leg4->Draw();
        }

        // --- 5ページ目: Sideband Subtraction ---
        c5->cd(j + 1);
        TH1 *h5_true =
            (TH1 *)h->Clone(Form("h5_true_%s_CentBin%d", sp.Data(), j));
        h5_true->SetTitle(
            Form("Sideband Subtracted %s CentBin%d (%s)", sp.Data(), j, centLabels[j]));

        TH1 *hSB_tot_true = 0;
        if (hSBNeg_orig && hSBPos_orig) {
          hSB_tot_true = (TH1 *)hSBNeg_orig->Clone(
              Form("hSB_tot_true_%s_CentBin%d", sp.Data(), j));
          if (!hSB_tot_true->GetSumw2N()) hSB_tot_true->Sumw2();
          hSB_tot_true->Add(hSBPos_orig);
          hSB_tot_true->Rebin(nRebin);
        } else if (hSBNeg_orig) {
          hSB_tot_true = (TH1 *)hSBNeg_orig->Clone(
              Form("hSB_tot_true_%s_CentBin%d", sp.Data(), j));
          if (!hSB_tot_true->GetSumw2N()) hSB_tot_true->Sumw2();
          hSB_tot_true->Rebin(nRebin);
        } else if (hSBPos_orig) {
          hSB_tot_true = (TH1 *)hSBPos_orig->Clone(
              Form("hSB_tot_true_%s_CentBin%d", sp.Data(), j));
          if (!hSB_tot_true->GetSumw2N()) hSB_tot_true->Sumw2();
          hSB_tot_true->Rebin(nRebin);
        }

        if (hSB_tot_true) {
          h5_true->Add(hSB_tot_true, -1.0);
        }

        h5_true->SetLineColor(kBlack);
        h5_true->SetMarkerColor(kBlack);
        h5_true->SetMarkerStyle(20);
        h5_true->SetMarkerSize(0.8);

        TH1 *h5_mix = 0;
        if (hmix_unscaled) {
          h5_mix = (TH1 *)hmix_unscaled->Clone(
              Form("h5_mix_%s_CentBin%d", sp.Data(), j));

          TH1 *hmixSB_tot = 0;
          if (hmixSBNeg_orig && hmixSBPos_orig) {
            hmixSB_tot = (TH1 *)hmixSBNeg_orig->Clone(
                Form("hmixSB_tot_%s_CentBin%d", sp.Data(), j));
            if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
            hmixSB_tot->Add(hmixSBPos_orig);
            hmixSB_tot->Rebin(nRebin);
          } else if (hmixSBNeg_orig) {
            hmixSB_tot = (TH1 *)hmixSBNeg_orig->Clone(
                Form("hmixSB_tot_%s_CentBin%d", sp.Data(), j));
            if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
            hmixSB_tot->Rebin(nRebin);
          } else if (hmixSBPos_orig) {
            hmixSB_tot = (TH1 *)hmixSBPos_orig->Clone(
                Form("hmixSB_tot_%s_CentBin%d", sp.Data(), j));
            if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
            hmixSB_tot->Rebin(nRebin);
          }

          if (hmixSB_tot) {
            h5_mix->Add(hmixSB_tot, -1.0);
          }

          h5_mix->SetLineColor(kRed);
          h5_mix->SetMarkerColor(kRed);
          h5_mix->SetMarkerStyle(20);
          h5_mix->SetMarkerSize(0.8);

          // 0.4 - 0.6 GeV/c で積分してスケールを合わせる
          int bin1 = h5_true->FindBin(0.40001);
          int bin2 = h5_true->FindBin(0.59999);
          double int5_true = h5_true->Integral(bin1, bin2);
          double int5_mix = h5_mix->Integral(h5_mix->FindBin(0.40001),
                                             h5_mix->FindBin(0.59999));

          if (int5_mix > 0) {
            h5_mix->Scale(int5_true / int5_mix);
          }
        }

        double max5 = h5_true->GetMaximum();
        if (h5_mix && h5_mix->GetMaximum() > max5)
          max5 = h5_mix->GetMaximum();
        h5_true->SetMaximum(max5 * 1.2);

        h5_true->Draw("E1");
        if (h5_mix) {
          h5_mix->Draw("E1 same");
        }
        
        TLegend *leg5 = new TLegend(0.15, 0.7, 0.55, 0.88);
        leg5->AddEntry(h5_true, "True (SB Sub)", "pe");
        if (h5_mix) leg5->AddEntry(h5_mix, "Mixed (SB Sub)", "pe");
        leg5->Draw();

        // --- 6ページ目: Correlation Function (After SB Subtraction) ---
        c6->cd(j + 1);
        if (h5_mix) {
          TH1 *hCF_sb =
              (TH1 *)h5_true->Clone(Form("hCF_sb_%s_CentBin%d", sp.Data(), j));
          hCF_sb->SetTitle(
              Form("CF (SB Subtracted) %s CentBin%d (%s)", sp.Data(), j, centLabels[j]));
          hCF_sb->Divide(h5_mix);

          // 黒色でプロット、Y軸の範囲を[0, 2]に固定
          hCF_sb->SetLineColor(kBlack);
          hCF_sb->SetMarkerColor(kBlack);
          hCF_sb->GetYaxis()->SetTitle("C(k*) = True_sub / Mixed_sub");
          hCF_sb->GetYaxis()->SetRangeUser(0.0, 2.0);

          hCF_sb->Draw("E1");
        }

        if (hSBNeg_orig) delete hSBNeg_orig;
        if (hSBPos_orig) delete hSBPos_orig;
        if (hmixSBNeg_orig) delete hmixSBNeg_orig;
        if (hmixSBPos_orig) delete hmixSBPos_orig;

      } else {
        std::cerr << "Warning: Histogram " << histName << " not found in file."
                  << std::endl;
      }
    }

    // マルチページPDFとして保存
    TString outFile = Form("%s/kstar_%s.pdf", outDir.Data(), sp.Data());
    c1->Print(outFile + "("); // 1ページ目 (Open)
    c2->Print(outFile);       // 2ページ目
    c3->Print(outFile);       // 3ページ目
    c4->Print(outFile);       // 4ページ目
    c5->Print(outFile);       // 5ページ目
    c6->Print(outFile + ")"); // 6ページ目 (Close)

    delete c1;
    delete c2;
    delete c3;
    delete c4;
    delete c5;
    delete c6;
  }

  std::cout << "Drawing completed. PDF files saved to: " << outDir << std::endl;

  fIn->Close();
}
