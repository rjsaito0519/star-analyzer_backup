// ============================================================
// check_errors_sum.C
// draw_LambdaNuclearId_relamom_sum.C の各ステップの誤差を
// ビン毎に出力し、p.2 vs p.6 の誤差の差の原因を診断する。
// 出力: rootfile/.../pdf/error_check_<sp>.txt
// ROOT 5.34 対応版 (ラムダ不使用)
// ============================================================
#include "TFile.h"
#include "TH1.h"
#include "TString.h"
#include "TSystem.h"
#include <fstream>
#include <iostream>
#include <cmath>

void printBins(std::ofstream &out, TH1 *h, const char *label,
               double xlo = 0.0, double xhi = 0.6) {
  if (!h) {
    out << "  [" << label << "] : NULL\n";
    return;
  }
  out << "  [" << label << "]  Sumw2N=" << h->GetSumw2N()
      << "  Entries=" << h->GetEntries() << "\n";
  out << Form("  %-6s  %-12s  %-12s  %-10s\n",
              "BinCtr", "Content", "Error", "Err/Cont%");
  int n = h->GetNbinsX();
  for (int b = 1; b <= n; ++b) {
    double x = h->GetBinCenter(b);
    if (x < xlo || x > xhi) continue;
    double c = h->GetBinContent(b);
    double e = h->GetBinError(b);
    double rel = (c != 0) ? fabs(e / c) * 100.0 : -1.0;
    out << Form("  %6.4f  %12.4f  %12.4f  %9.2f\n", x, c, e, rel);
  }
  out << "\n";
}

// ヒストグラムをaccに加算する (ROOT5対応、ラムダなし)
void accumulate(TH1 *&acc, TH1 *tmp, const char *cname) {
  if (!tmp) return;
  if (!acc) {
    acc = (TH1*)tmp->Clone(cname);
    if (!acc->GetSumw2N()) acc->Sumw2();
  } else {
    TH1 *htmp2 = (TH1*)tmp->Clone("htmp_acc");
    if (!htmp2->GetSumw2N()) htmp2->Sumw2();
    acc->Add(htmp2);
    delete htmp2;
  }
}

void check_errors_sum() {
  TString inputFile =
      "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
      "auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all.root";
  TFile *fIn = TFile::Open(inputFile, "READ");
  if (!fIn || fIn->IsZombie()) {
    std::cerr << "Error: Cannot open " << inputFile << std::endl;
    return;
  }

  TString outDir = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
                   "auau13p5_anaLambdaNuclearId/pdf";
  gSystem->mkdir(outDir, kTRUE);

  const char *species[] = {"d", "t", "3He", "4He"};
  int nSpecies = 4;

  TDirectory *dirNuc = (TDirectory *)fIn->Get("true");
  TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

  for (int i = 0; i < nSpecies; ++i) {
    TString sp = species[i];

    int nRebin = 1;
    if (sp == "t" || sp == "3He") nRebin = 4;
    else if (sp == "4He") nRebin = 5;

    TString outTxt = Form("%s/error_check_%s.txt", outDir.Data(), sp.Data());
    std::ofstream out(outTxt.Data());
    out << "==========================================================\n";
    out << " Error diagnostic: " << sp.Data() << "  (nRebin=" << nRebin << ")\n";
    out << "==========================================================\n\n";

    for (int group = 0; group < 2; ++group) {
      TString centLabel = (group == 0) ? "30-80%" : "0-20%";
      int jStart = (group == 0) ? 0 : 6;
      int jEnd   = (group == 0) ? 5 : 8;

      out << "----------------------------------------------------------\n";
      out << " Centrality group " << group << " : " << centLabel.Data()
          << "  (CentBin " << jStart << " - " << jEnd << ")\n";
      out << "----------------------------------------------------------\n\n";

      TH1 *h              = 0;
      TH1 *hmix           = 0;
      TH1 *hSBNeg_orig    = 0;
      TH1 *hSBPos_orig    = 0;
      TH1 *hmixSBNeg_orig = 0;
      TH1 *hmixSBPos_orig = 0;

      for (int j = jStart; j <= jEnd; ++j) {
        TString hName = Form("hKstar_%s_CentBin%d", sp.Data(), j);
        TH1 *htmp = dirNuc ? (TH1*)dirNuc->Get(hName) : (TH1*)fIn->Get(hName);
        accumulate(h, htmp, Form("hTrue_%s_G%d", sp.Data(), group));

        TString mName = Form("hKstar_Mixed_%s_CentBin%d", sp.Data(), j);
        TH1 *mtmp = dirMix ? (TH1*)dirMix->Get(mName) : (TH1*)fIn->Get(mName);
        accumulate(hmix, mtmp, Form("hMix_%s_G%d", sp.Data(), group));

        TString sbNeg = Form("hKstar_%s_SBNeg_CentBin%d", sp.Data(), j);
        TH1 *sbn = dirNuc ? (TH1*)dirNuc->Get(sbNeg) : (TH1*)fIn->Get(sbNeg);
        accumulate(hSBNeg_orig, sbn, Form("hSBNeg_%s_G%d", sp.Data(), group));

        TString sbPos = Form("hKstar_%s_SBPos_CentBin%d", sp.Data(), j);
        TH1 *sbp = dirNuc ? (TH1*)dirNuc->Get(sbPos) : (TH1*)fIn->Get(sbPos);
        accumulate(hSBPos_orig, sbp, Form("hSBPos_%s_G%d", sp.Data(), group));

        TString msbNeg = Form("hKstar_Mixed_%s_SBNeg_CentBin%d", sp.Data(), j);
        TH1 *msbn = dirMix ? (TH1*)dirMix->Get(msbNeg) : (TH1*)fIn->Get(msbNeg);
        accumulate(hmixSBNeg_orig, msbn, Form("hmixSBNeg_%s_G%d", sp.Data(), group));

        TString msbPos = Form("hKstar_Mixed_%s_SBPos_CentBin%d", sp.Data(), j);
        TH1 *msbp = dirMix ? (TH1*)dirMix->Get(msbPos) : (TH1*)fIn->Get(msbPos);
        accumulate(hmixSBPos_orig, msbp, Form("hmixSBPos_%s_G%d", sp.Data(), group));
      }

      if (!h) {
        out << "  [SKIP] True histogram not found.\n\n";
        continue;
      }

      // ---- Rebin ----
      h->Rebin(nRebin);
      if (hmix) hmix->Rebin(nRebin);

      // ---- Scale Mixed ----
      double scaleFactor = 1.0;
      TH1 *hmix_unscaled = 0;
      if (hmix) {
        hmix_unscaled = (TH1*)hmix->Clone(Form("hmix_unc_%s_G%d", sp.Data(), group));
        int b1 = h->FindBin(0.40001);
        int b2 = h->FindBin(0.59999);
        double intTrue = h->Integral(b1, b2);
        double intMix  = hmix->Integral(hmix->FindBin(0.40001), hmix->FindBin(0.59999));
        if (intMix > 0) {
          scaleFactor = intTrue / intMix;
          hmix->Scale(scaleFactor);
        }
      }

      // ---- CF (p.2) ----
      TH1 *hCF = 0;
      if (hmix) {
        hCF = (TH1*)h->Clone(Form("hCF_%s_G%d", sp.Data(), group));
        hCF->Divide(hmix);
      }

      // ---- SB total (True side) ----
      TH1 *hSB_tot = 0;
      if (hSBNeg_orig && hSBPos_orig) {
        hSB_tot = (TH1*)hSBNeg_orig->Clone(Form("hSBtot_%s_G%d", sp.Data(), group));
        if (!hSB_tot->GetSumw2N()) hSB_tot->Sumw2();
        hSB_tot->Add(hSBPos_orig);
        hSB_tot->Rebin(nRebin);
      } else if (hSBNeg_orig) {
        hSB_tot = (TH1*)hSBNeg_orig->Clone(Form("hSBtot_%s_G%d", sp.Data(), group));
        if (!hSB_tot->GetSumw2N()) hSB_tot->Sumw2();
        hSB_tot->Rebin(nRebin);
      } else if (hSBPos_orig) {
        hSB_tot = (TH1*)hSBPos_orig->Clone(Form("hSBtot_%s_G%d", sp.Data(), group));
        if (!hSB_tot->GetSumw2N()) hSB_tot->Sumw2();
        hSB_tot->Rebin(nRebin);
      }

      // ---- SB-subtracted True ----
      TH1 *h5_true = (TH1*)h->Clone(Form("h5true_%s_G%d", sp.Data(), group));
      if (hSB_tot) h5_true->Add(hSB_tot, -1.0);

      // ---- SB total (Mix side) ----
      TH1 *hmixSB_tot = 0;
      if (hmixSBNeg_orig && hmixSBPos_orig) {
        hmixSB_tot = (TH1*)hmixSBNeg_orig->Clone(Form("hmixSBtot_%s_G%d", sp.Data(), group));
        if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
        hmixSB_tot->Add(hmixSBPos_orig);
        hmixSB_tot->Rebin(nRebin);
      } else if (hmixSBNeg_orig) {
        hmixSB_tot = (TH1*)hmixSBNeg_orig->Clone(Form("hmixSBtot_%s_G%d", sp.Data(), group));
        if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
        hmixSB_tot->Rebin(nRebin);
      } else if (hmixSBPos_orig) {
        hmixSB_tot = (TH1*)hmixSBPos_orig->Clone(Form("hmixSBtot_%s_G%d", sp.Data(), group));
        if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
        hmixSB_tot->Rebin(nRebin);
      }

      // ---- SB-subtracted Mixed ----
      TH1 *h5_mix = 0;
      double scaleFactor5 = 1.0;
      if (hmix_unscaled) {
        h5_mix = (TH1*)hmix_unscaled->Clone(Form("h5mix_%s_G%d", sp.Data(), group));
        if (hmixSB_tot) h5_mix->Add(hmixSB_tot, -1.0);
        int b1 = h5_true->FindBin(0.40001);
        int b2 = h5_true->FindBin(0.59999);
        double int5t = h5_true->Integral(b1, b2);
        double int5m = h5_mix->Integral(h5_mix->FindBin(0.40001), h5_mix->FindBin(0.59999));
        if (int5m > 0) {
          scaleFactor5 = int5t / int5m;
          h5_mix->Scale(scaleFactor5);
        }
      }

      // ---- CF (p.6) ----
      TH1 *hCF_sb = 0;
      if (h5_mix) {
        hCF_sb = (TH1*)h5_true->Clone(Form("hCFsb_%s_G%d", sp.Data(), group));
        hCF_sb->Divide(h5_mix);
      }

      // ============================================================
      // Output
      // ============================================================
      out << "Scale factor (True/Mixed norm, p.1) = " << scaleFactor << "\n";
      out << "Scale factor (h5_true/h5_mix norm, p.5) = " << scaleFactor5 << "\n\n";

      out << ">>> STEP A: True [h] after Rebin(" << nRebin << ")\n";
      printBins(out, h, "h (True)");

      out << ">>> STEP B: Mixed [hmix] after Rebin + Scale\n";
      printBins(out, hmix, "hmix (scaled)");

      out << ">>> STEP C: Mixed [hmix_unscaled] (before Scale)\n";
      printBins(out, hmix_unscaled, "hmix_unscaled");

      out << ">>> STEP D: SB Total True [hSB_tot]\n";
      printBins(out, hSB_tot, "hSB_tot");

      out << ">>> STEP E: SB Total Mix [hmixSB_tot]\n";
      printBins(out, hmixSB_tot, "hmixSB_tot");

      out << ">>> STEP F: SB-subtracted True [h5_true = h - hSB_tot]\n";
      printBins(out, h5_true, "h5_true");

      out << ">>> STEP G: SB-subtracted Mixed [h5_mix = hmix_unc - hmixSB_tot, scaled]\n";
      printBins(out, h5_mix, "h5_mix");

      out << ">>> STEP H: CF (p.2) = h / hmix_scaled\n";
      printBins(out, hCF, "CF_p2");

      out << ">>> STEP I: CF (p.6) = h5_true / h5_mix\n";
      printBins(out, hCF_sb, "CF_p6");

      // ---- Summary ----
      out << ">>> SUMMARY: CF_p2 vs CF_p6  (ratio = err_p6 / err_p2)\n";
      out << Form("  %-6s  %-12s  %-12s  %-12s  %-12s  %-8s\n",
                  "BinCtr", "CF_p2", "err_p2", "CF_p6", "err_p6", "ratio");
      if (hCF && hCF_sb) {
        int nb = hCF->GetNbinsX();
        for (int b = 1; b <= nb; ++b) {
          double x = hCF->GetBinCenter(b);
          if (x < 0.0 || x > 0.6) continue;
          double v2 = hCF->GetBinContent(b);
          double e2 = hCF->GetBinError(b);
          double v6 = hCF_sb->GetBinContent(b);
          double e6 = hCF_sb->GetBinError(b);
          double ratio = (e2 > 0) ? e6 / e2 : -1.0;
          out << Form("  %6.4f  %12.4f  %12.4f  %12.4f  %12.4f  %8.2f\n",
                      x, v2, e2, v6, e6, ratio);
        }
      }
      out << "\n";

      if (hSBNeg_orig)    delete hSBNeg_orig;
      if (hSBPos_orig)    delete hSBPos_orig;
      if (hmixSBNeg_orig) delete hmixSBNeg_orig;
      if (hmixSBPos_orig) delete hmixSBPos_orig;
    }

    out.close();
    std::cout << "Written: " << outTxt.Data() << std::endl;
  }

  fIn->Close();
}
