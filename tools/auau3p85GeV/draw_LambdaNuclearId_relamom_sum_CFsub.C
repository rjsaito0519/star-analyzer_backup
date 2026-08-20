#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TLegend.h"
#include "TMath.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>

void draw_LambdaNuclearId_relamom_sum_CFsub() {
  // スタイルの設定
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(1);

  // 入力ファイル
  TString inputFile =
    "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
    //   "auau3p85_anaLambdaNuclearId/auau3p85_all1.root";
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

  const char *species[] = {"d", "t", "3He", "4He"};
  int nSpecies = 4;

  // 各原子核についてループ
  for (int i = 0; i < nSpecies; ++i) {
    TString sp = species[i];

    int nRebin = 1;
    if (sp == "t" || sp == "3He")
      nRebin = 4;
    else if (sp == "4He")
      nRebin = 10;

    TDirectory *dirNuc = (TDirectory *)fIn->Get("true");
    TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

    TString outFile = Form("%s/kstar_sum_CFsub_%s.pdf", outDir.Data(), sp.Data());

    for (int groupSet = 0; groupSet < 2; ++groupSet) {
      int nGroups = (groupSet == 0) ? 2 : 3;

      TCanvas *c1 =
          new TCanvas(Form("c1_%s_groupSet%d", sp.Data(), groupSet),
                      Form("k* for Lambda - %s", sp.Data()), 1200, 600);
      c1->Divide(nGroups, 1);

      TCanvas *c2 = new TCanvas(
          Form("c2_%s_groupSet%d", sp.Data(), groupSet),
          Form("Correlation Function for Lambda - %s", sp.Data()), 1200, 1200);
      c2->Divide(nGroups, 2);

      TCanvas *c3 = new TCanvas(
          Form("c3_%s_groupSet%d", sp.Data(), groupSet),
          Form("True Sidebands for Lambda - %s", sp.Data()), 1200, 600);
      c3->Divide(nGroups, 1);

      TCanvas *c4 = new TCanvas(
          Form("c4_%s_groupSet%d", sp.Data(), groupSet),
          Form("Mixed Sidebands for Lambda - %s", sp.Data()), 1200, 600);
      c4->Divide(nGroups, 1);

      TCanvas *c5 = new TCanvas(
          Form("c5_%s_groupSet%d", sp.Data(), groupSet),
          Form("CF Comparison for Lambda - %s", sp.Data()), 1200, 600);
      c5->Divide(nGroups, 1);

      TCanvas *c6 = new TCanvas(
          Form("c6_%s_groupSet%d", sp.Data(), groupSet),
          Form("CF (Purity Corrected) for Lambda - %s", sp.Data()), 1200, 1200);
      c6->Divide(nGroups, 2);

      for (int group = 0; group < nGroups; ++group) {
        c1->cd(group + 1);

        TString centLabel;
        int jStart, jEnd;
        if (groupSet == 0) {
          centLabel = (group == 0) ? "20-80%" : "0-20%";
          jStart = (group == 0) ? 0 : 6;
          jEnd = (group == 0) ? 5 : 8;
        } else {
          if (group == 0) {
            centLabel = "0-10%";
            jStart = 7;
            jEnd = 8;
          } else if (group == 1) {
            centLabel = "10-20%";
            jStart = 6;
            jEnd = 6;
          } else {
            centLabel = "20-60%";
            jStart = 2;
            jEnd = 5;
          }
        }

        TH1 *h = 0;
        TH1 *hmix = 0;
        TH1 *hSBNeg_orig = 0;
        TH1 *hSBPos_orig = 0;
        TH1 *hmixSBNeg_orig = 0;
        TH1 *hmixSBPos_orig = 0;

        for (int j = jStart; j <= jEnd; ++j) {
          TString histName = Form("hKstar_%s_CentBin%d", sp.Data(), j);
          TH1 *h_tmp =
              dirNuc ? (TH1 *)dirNuc->Get(histName) : (TH1 *)fIn->Get(histName);
          if (h_tmp) {
            if (!h) {
              h = (TH1 *)h_tmp->Clone(Form("hKstar_%s_groupSet%d_Group%d",
                                           sp.Data(), groupSet, group));
              if (!h->GetSumw2N())
                h->Sumw2();
            } else {
              TH1 *h_add = (TH1 *)h_tmp->Clone();
              if (!h_add->GetSumw2N())
                h_add->Sumw2();
              h->Add(h_add);
              delete h_add;
            }
          }

          TString mixName = Form("hKstar_Mixed_%s_CentBin%d", sp.Data(), j);
          TH1 *hmix_tmp =
              dirMix ? (TH1 *)dirMix->Get(mixName) : (TH1 *)fIn->Get(mixName);
          if (hmix_tmp) {
            if (!hmix) {
              hmix = (TH1 *)hmix_tmp->Clone(
                  Form("hKstar_Mixed_%s_groupSet%d_Group%d", sp.Data(),
                       groupSet, group));
              if (!hmix->GetSumw2N())
                hmix->Sumw2();
            } else {
              TH1 *hmix_add = (TH1 *)hmix_tmp->Clone();
              if (!hmix_add->GetSumw2N())
                hmix_add->Sumw2();
              hmix->Add(hmix_add);
              delete hmix_add;
            }
          }

          TString nameSBNeg = Form("hKstar_%s_SBNeg_CentBin%d", sp.Data(), j);
          TH1 *hSBNeg_tmp = dirNuc ? (TH1 *)dirNuc->Get(nameSBNeg)
                                   : (TH1 *)fIn->Get(nameSBNeg);
          if (hSBNeg_tmp) {
            if (!hSBNeg_orig) {
              hSBNeg_orig = (TH1 *)hSBNeg_tmp->Clone(
                  Form("hSBNeg_orig_%s_groupSet%d_Group%d", sp.Data(), groupSet,
                       group));
              if (!hSBNeg_orig->GetSumw2N())
                hSBNeg_orig->Sumw2();
            } else {
              TH1 *hSBNeg_add = (TH1 *)hSBNeg_tmp->Clone();
              if (!hSBNeg_add->GetSumw2N())
                hSBNeg_add->Sumw2();
              hSBNeg_orig->Add(hSBNeg_add);
              delete hSBNeg_add;
            }
          }

          TString nameSBPos = Form("hKstar_%s_SBPos_CentBin%d", sp.Data(), j);
          TH1 *hSBPos_tmp = dirNuc ? (TH1 *)dirNuc->Get(nameSBPos)
                                   : (TH1 *)fIn->Get(nameSBPos);
          if (hSBPos_tmp) {
            if (!hSBPos_orig) {
              hSBPos_orig = (TH1 *)hSBPos_tmp->Clone(
                  Form("hSBPos_orig_%s_groupSet%d_Group%d", sp.Data(), groupSet,
                       group));
              if (!hSBPos_orig->GetSumw2N())
                hSBPos_orig->Sumw2();
            } else {
              TH1 *hSBPos_add = (TH1 *)hSBPos_tmp->Clone();
              if (!hSBPos_add->GetSumw2N())
                hSBPos_add->Sumw2();
              hSBPos_orig->Add(hSBPos_add);
              delete hSBPos_add;
            }
          }

          TString mixNameSBNeg =
              Form("hKstar_Mixed_%s_SBNeg_CentBin%d", sp.Data(), j);
          TH1 *hmixSBNeg_tmp = dirMix ? (TH1 *)dirMix->Get(mixNameSBNeg)
                                      : (TH1 *)fIn->Get(mixNameSBNeg);
          if (hmixSBNeg_tmp) {
            if (!hmixSBNeg_orig) {
              hmixSBNeg_orig = (TH1 *)hmixSBNeg_tmp->Clone(
                  Form("hmixSBNeg_orig_%s_groupSet%d_Group%d", sp.Data(),
                       groupSet, group));
              if (!hmixSBNeg_orig->GetSumw2N())
                hmixSBNeg_orig->Sumw2();
            } else {
              TH1 *hmixSBNeg_add = (TH1 *)hmixSBNeg_tmp->Clone();
              if (!hmixSBNeg_add->GetSumw2N())
                hmixSBNeg_add->Sumw2();
              hmixSBNeg_orig->Add(hmixSBNeg_add);
              delete hmixSBNeg_add;
            }
          }

          TString mixNameSBPos =
              Form("hKstar_Mixed_%s_SBPos_CentBin%d", sp.Data(), j);
          TH1 *hmixSBPos_tmp = dirMix ? (TH1 *)dirMix->Get(mixNameSBPos)
                                      : (TH1 *)fIn->Get(mixNameSBPos);
          if (hmixSBPos_tmp) {
            if (!hmixSBPos_orig) {
              hmixSBPos_orig = (TH1 *)hmixSBPos_tmp->Clone(
                  Form("hmixSBPos_orig_%s_groupSet%d_Group%d", sp.Data(),
                       groupSet, group));
              if (!hmixSBPos_orig->GetSumw2N())
                hmixSBPos_orig->Sumw2();
            } else {
              TH1 *hmixSBPos_add = (TH1 *)hmixSBPos_tmp->Clone();
              if (!hmixSBPos_add->GetSumw2N())
                hmixSBPos_add->Sumw2();
              hmixSBPos_orig->Add(hmixSBPos_add);
              delete hmixSBPos_add;
            }
          }
        }

        if (hSBNeg_orig) hSBNeg_orig->Scale(1.0 / 3.0);
        if (hSBPos_orig) hSBPos_orig->Scale(1.0 / 3.0);
        if (hmixSBNeg_orig) hmixSBNeg_orig->Scale(1.0 / 3.0);
        if (hmixSBPos_orig) hmixSBPos_orig->Scale(1.0 / 3.0);

        TH1 *hmix_unscaled = 0;

        if (h) {
          h->SetTitle(
              Form("k* for Lambda - %s (%s)", sp.Data(), centLabel.Data()));
          h->Rebin(nRebin); // Rebin
          h->GetXaxis()->SetRangeUser(0.0, 0.6);

          // True: Red
          h->SetMarkerStyle(20);
          h->SetMarkerSize(0.8);
          h->SetLineColor(kRed);
          h->SetMarkerColor(kRed);

          if (hmix) {
            hmix->Rebin(nRebin); // Rebin
            hmix->GetXaxis()->SetRangeUser(0.0, 0.6);

            // Scale前のMixedヒストグラムをp.4のために保存しておく
            hmix_unscaled =
                (TH1 *)hmix->Clone(Form("hmix_unscaled_%s_groupSet%d_Group%d",
                                        sp.Data(), groupSet, group));

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

          // E1 オプションでエラーバー付きで描画
          h->Draw("E1");
          if (hmix) {
            hmix->Draw("E1 same");
          }

          // --- 2ページ目: Correlation Function (CF = True / Mixed) ---
          c2->cd(group + 1);
          if (hmix) {
            TH1 *hCF = (TH1 *)h->Clone(
                Form("hCF_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            hCF->SetTitle(
                Form("CF (True / Mixed) %s (%s)", sp.Data(), centLabel.Data()));
            hCF->Divide(hmix);

            // 黒色でプロット、Y軸の範囲を[0, 2]に固定
            hCF->SetLineColor(kBlack);
            hCF->SetMarkerColor(kBlack);
            hCF->GetYaxis()->SetTitle("C(k*) = True / Mixed");
            hCF->GetYaxis()->SetRangeUser(0.0, 2.0);

            hCF->Draw("E1");

            c2->cd(group + 1 + nGroups);
            TH1 *hCF_zoom = (TH1 *)hCF->Clone(Form(
                "hCF_zoom_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            hCF_zoom->SetTitle(
                Form("CF Zoom %s (%s)", sp.Data(), centLabel.Data()));
            hCF_zoom->GetXaxis()->SetRangeUser(0.0, 0.15);
            hCF_zoom->GetYaxis()->SetRangeUser(0.0, 30.0);
            hCF_zoom->Draw("E1");
          }

          // --- 3ページ目: True SideBands ---
          c3->cd(group + 1);
          TH1 *h3 = (TH1 *)h->Clone(
              Form("h3_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          h3->SetTitle(
              Form("True Sidebands %s (%s)", sp.Data(), centLabel.Data()));
          h3->SetLineColor(kBlack);
          h3->SetMarkerColor(kBlack);
          h3->Draw("E1");

          TH1 *hSBNeg = hSBNeg_orig ? (TH1 *)hSBNeg_orig->Clone(Form(
                                          "hSBNeg_p3_%s_groupSet%d_Group%d",
                                          sp.Data(), groupSet, group))
                                    : 0;
          TH1 *hSBPos = hSBPos_orig ? (TH1 *)hSBPos_orig->Clone(Form(
                                          "hSBPos_p3_%s_groupSet%d_Group%d",
                                          sp.Data(), groupSet, group))
                                    : 0;

          double max3 = h3->GetMaximum();
          if (hSBNeg) {
            hSBNeg->Rebin(nRebin);
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
            hSBPos->SetLineColor(kGreen + 2); // 見やすい緑
            hSBPos->SetMarkerColor(kGreen + 2);
            hSBPos->SetMarkerStyle(20);
            hSBPos->SetMarkerSize(0.8);
            hSBPos->Draw("E1 same");
            if (hSBPos->GetMaximum() > max3)
              max3 = hSBPos->GetMaximum();
          }
          h3->SetMaximum(max3 * 1.2);

          // --- 4ページ目: Mixed SideBands ---
          c4->cd(group + 1);

          TH1 *hmixSBNeg = hmixSBNeg_orig
                               ? (TH1 *)hmixSBNeg_orig->Clone(
                                     Form("hmixSBNeg_p4_%s_groupSet%d_Group%d",
                                          sp.Data(), groupSet, group))
                               : 0;
          TH1 *hmixSBPos = hmixSBPos_orig
                               ? (TH1 *)hmixSBPos_orig->Clone(
                                     Form("hmixSBPos_p4_%s_groupSet%d_Group%d",
                                          sp.Data(), groupSet, group))
                               : 0;

          if (hmix_unscaled) {
            TH1 *h4 = hmix_unscaled;
            h4->SetName(
                Form("h4_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            h4->SetTitle(
                Form("Mixed Sidebands %s (%s)", sp.Data(), centLabel.Data()));
            h4->SetLineColor(kRed);
            h4->SetMarkerColor(kRed);
            h4->SetMarkerStyle(20);
            h4->SetMarkerSize(0.8);
            h4->Draw("E1");

            double max4 = h4->GetMaximum();
            if (hmixSBNeg) {
              hmixSBNeg->Rebin(nRebin);
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
              hmixSBPos->SetLineColor(kGreen + 2);
              hmixSBPos->SetMarkerColor(kGreen + 2);
              hmixSBPos->SetMarkerStyle(20);
              hmixSBPos->SetMarkerSize(0.8);
              hmixSBPos->Draw("E1 same");
              if (hmixSBPos->GetMaximum() > max4)
                max4 = hmixSBPos->GetMaximum();
            }
            h4->SetMaximum(max4 * 1.2);
          }

          // --- 5ページ目: CF Comparison ---
          c5->cd(group + 1);
          
          TH1 *hCF_sig = 0;
          if (hmix) {
            hCF_sig = (TH1 *)h->Clone(Form("hCF_sig_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            hCF_sig->Divide(hmix);
          }

          TH1 *hSB_tot_true = 0;
          if (hSBNeg_orig && hSBPos_orig) {
            hSB_tot_true = (TH1 *)hSBNeg_orig->Clone(
                Form("hSB_tot_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hSB_tot_true->GetSumw2N())
              hSB_tot_true->Sumw2();
            hSB_tot_true->Add(hSBPos_orig);
            hSB_tot_true->Rebin(nRebin);
          } else if (hSBNeg_orig) {
            hSB_tot_true = (TH1 *)hSBNeg_orig->Clone(
                Form("hSB_tot_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hSB_tot_true->GetSumw2N())
              hSB_tot_true->Sumw2();
            hSB_tot_true->Rebin(nRebin);
          } else if (hSBPos_orig) {
            hSB_tot_true = (TH1 *)hSBPos_orig->Clone(
                Form("hSB_tot_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hSB_tot_true->GetSumw2N())
              hSB_tot_true->Sumw2();
            hSB_tot_true->Rebin(nRebin);
          }

          TH1 *hmixSB_tot = 0;
          if (hmixSBNeg_orig && hmixSBPos_orig) {
            hmixSB_tot = (TH1 *)hmixSBNeg_orig->Clone(
                Form("hmixSB_tot_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSB_tot->GetSumw2N())
              hmixSB_tot->Sumw2();
            hmixSB_tot->Add(hmixSBPos_orig);
            hmixSB_tot->Rebin(nRebin);
          } else if (hmixSBNeg_orig) {
            hmixSB_tot = (TH1 *)hmixSBNeg_orig->Clone(
                Form("hmixSB_tot_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSB_tot->GetSumw2N())
              hmixSB_tot->Sumw2();
            hmixSB_tot->Rebin(nRebin);
          } else if (hmixSBPos_orig) {
            hmixSB_tot = (TH1 *)hmixSBPos_orig->Clone(
                Form("hmixSB_tot_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSB_tot->GetSumw2N())
              hmixSB_tot->Sumw2();
            hmixSB_tot->Rebin(nRebin);
          }

          TH1 *hCF_bg = 0;
          if (hSB_tot_true && hmixSB_tot) {
            int bin1 = hSB_tot_true->FindBin(0.40001);
            int bin2 = hSB_tot_true->FindBin(0.59999);
            double intSB_true = hSB_tot_true->Integral(bin1, bin2);
            double intSB_mix = hmixSB_tot->Integral(hmixSB_tot->FindBin(0.40001), hmixSB_tot->FindBin(0.59999));
            if (intSB_mix > 0) {
              hmixSB_tot->Scale(intSB_true / intSB_mix);
            }
            hCF_bg = (TH1 *)hSB_tot_true->Clone(Form("hCF_bg_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            hCF_bg->Divide(hmixSB_tot);
          }

          if (hCF_sig) {
            hCF_sig->SetTitle(Form("CF Comparison %s (%s)", sp.Data(), centLabel.Data()));
            hCF_sig->SetLineColor(kBlack);
            hCF_sig->SetMarkerColor(kBlack);
            hCF_sig->GetYaxis()->SetTitle("CF");
            hCF_sig->GetYaxis()->SetRangeUser(0.0, 2.0);
            hCF_sig->Draw("E1");
          }
          if (hCF_bg) {
            hCF_bg->SetLineColor(kRed);
            hCF_bg->SetMarkerColor(kRed);
            hCF_bg->SetMarkerStyle(20);
            hCF_bg->Draw("E1 same");
          }
          
          TLegend *leg5 = new TLegend(0.15, 0.7, 0.55, 0.88);
          if (hCF_sig) leg5->AddEntry(hCF_sig, "CF (Signal Window)", "pe");
          if (hCF_bg) leg5->AddEntry(hCF_bg, "CF (Sideband)", "pe");
          leg5->Draw();

          // --- 6ページ目: Correlation Function (Constant Purity Corrected) ---
          c6->cd(group + 1);
          if (hCF_sig && hCF_bg && h && hSB_tot_true) {
            double sn = 5.217;
            double purity = sn / (sn + 1.0);
            
            TH1 *hCF_sub = (TH1 *)hCF_sig->Clone(Form("hCF_sub_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            hCF_sub->SetTitle(Form("CF (Constant Purity Corrected) %s (%s)", sp.Data(), centLabel.Data()));
            
            if (purity > 0) {
              TH1 *hTerm2 = (TH1*)hCF_bg->Clone("hTerm2");
              hTerm2->Scale(1.0 - purity);
              hCF_sub->Add(hTerm2, -1.0);
              hCF_sub->Scale(1.0 / purity);
            } else {
              hCF_sub->Reset();
            }

            hCF_sub->SetLineColor(kBlack);
            hCF_sub->SetMarkerColor(kBlack);
            hCF_sub->GetYaxis()->SetTitle(Form("CF (Corrected, P=%.3f)", purity));
            hCF_sub->GetYaxis()->SetRangeUser(0.0, 2.0);
            hCF_sub->Draw("E1");

            c6->cd(group + 1 + nGroups);
            TH1 *hCF_sub_zoom = (TH1 *)hCF_sub->Clone(Form("hCF_sub_zoom_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            hCF_sub_zoom->SetTitle(Form("CF Zoom (Corrected) %s (%s)", sp.Data(), centLabel.Data()));
            hCF_sub_zoom->GetXaxis()->SetRangeUser(0.0, 0.15);
            hCF_sub_zoom->GetYaxis()->SetRangeUser(0.0, 30.0);
	    hCF_sub_zoom->SetLineColor(kGreen);
	    hCF_sub_zoom->SetMarkerColor(kGreen);
            hCF_sub_zoom->Draw("E1");
          }

        } else {
          std::cerr << "Warning: No histogram found for group " << group
                    << std::endl;
        }

        // Memory cleanup for orig histograms as they are no longer needed for
        // next group
        if (hSBNeg_orig)
          delete hSBNeg_orig;
        if (hSBPos_orig)
          delete hSBPos_orig;
        if (hmixSBNeg_orig)
          delete hmixSBNeg_orig;
        if (hmixSBPos_orig)
          delete hmixSBPos_orig;
      }

      // マルチページPDFとして保存
      if (groupSet == 0) {
        c1->Print(outFile + "("); // Open
      } else {
        c1->Print(outFile);
      }
      c2->Print(outFile);
      c3->Print(outFile);
      c4->Print(outFile);
      c5->Print(outFile);
      if (groupSet == 1) {
        c6->Print(outFile + ")"); // Close
      } else {
        c6->Print(outFile);
      }

      delete c1;
      delete c2;
      delete c3;
      delete c4;
      delete c5;
      delete c6;
    }
  } // end of species loop

  std::cout << "Drawing completed. PDF files saved to: " << outDir << std::endl;
  fIn->Close();
}
