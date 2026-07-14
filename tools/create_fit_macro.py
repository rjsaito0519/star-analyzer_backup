import sys

def main():
    with open("tools/Draw_LLFormula.C", "r") as f:
        ll_lines = f.readlines()
    
    # Extract math functions from Draw_LLFormula.C
    math_funcs = []
    in_funcs = True
    for line in ll_lines:
        if "void Draw_LLFormula()" in line:
            in_funcs = False
            break
        if not line.startswith("#include") and in_funcs:
            math_funcs.append(line)

    math_funcs_str = "".join(math_funcs)

    with open("tools/draw_LambdaNuclearId_relamom_sum.C", "r") as f:
        sum_lines = f.readlines()

    # Extract the main body of draw_LambdaNuclearId_relamom_sum
    # But we want to modify it heavily:
    # 1. only 4He
    # 2. only create one Canvas per groupSet for the fit results
    # 3. Add TF1 fits
    
    new_macro = """#include "TFile.h"
#include "TH1.h"
#include "TCanvas.h"
#include "TString.h"
#include "TLegend.h"
#include "TSystem.h"
#include "TF1.h"
#include "TMath.h"
#include "TStyle.h"
#include <iostream>

""" + math_funcs_str + """

// Wrapper for TF1: a0 and reff are fixed
double fitLL_fixed(double *x, double *p) {
    double q_GeV = x[0];
    double q_MeV = q_GeV * 1000.0;
    if (q_MeV < 1e-3) q_MeV = 1e-3;
    
    double R = p[0];
    double norm = p[1];
    
    double a0 = 4.01;   // fixed
    double reff = 1.84; // fixed
    
    return norm * LL_Formula(q_MeV, R, a0, reff);
}

// Wrapper for TF1: a0, reff, R are free
double fitLL_free(double *x, double *p) {
    double q_GeV = x[0];
    double q_MeV = q_GeV * 1000.0;
    if (q_MeV < 1e-3) q_MeV = 1e-3;
    
    double R = p[0];
    double a0 = p[1];
    double reff = p[2];
    double norm = p[3];
    
    return norm * LL_Formula(q_MeV, R, a0, reff);
}

void Fit_LambdaNuclearId_relamom_sum() {
  gStyle->SetOptFit(1111); // Show fit parameters in stat box

  TString inputFile =
      "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
      "auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all.root";
  TFile *fIn = TFile::Open(inputFile, "READ");
  if (!fIn || fIn->IsZombie()) {
    std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
    return;
  }

  TString outDir = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
                   "auau13p5_anaLambdaNuclearId/pdf";
  gSystem->mkdir(outDir, kTRUE);

  TString sp = "4He";
  int nRebin = 10;

  TDirectory *dirNuc = (TDirectory *)fIn->Get("true");
  TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

  TString outFile = Form("%s/Fit_LL_%s.pdf", outDir.Data(), sp.Data());

  for (int groupSet = 0; groupSet < 2; ++groupSet) {
    int nGroups = (groupSet == 0) ? 2 : 3;

    TCanvas *cFit = new TCanvas(
        Form("cFit_%s_groupSet%d", sp.Data(), groupSet),
        Form("LL Fit CF (SB Subtracted) for Lambda - %s", sp.Data()), 1200, 600);
    cFit->Divide(nGroups, 1);

    for (int group = 0; group < nGroups; ++group) {
      TString centLabel;
      int jStart, jEnd;
      if (groupSet == 0) {
        centLabel = (group == 0) ? "20-80%" : "0-20%";
        jStart = (group == 0) ? 0 : 6;
        jEnd = (group == 0) ? 5 : 8;
      } else {
        if (group == 0) { centLabel = "0-10%"; jStart = 7; jEnd = 8; }
        else if (group == 1) { centLabel = "10-20%"; jStart = 6; jEnd = 6; }
        else { centLabel = "20-60%"; jStart = 2; jEnd = 5; }
      }

      TH1 *h = 0;
      TH1 *hmix = 0;
      TH1 *hSBNeg_orig = 0;
      TH1 *hSBPos_orig = 0;
      TH1 *hmixSBNeg_orig = 0;
      TH1 *hmixSBPos_orig = 0;

      for (int j = jStart; j <= jEnd; ++j) {
        TString histName = Form("hKstar_%s_CentBin%d", sp.Data(), j);
        TH1 *h_tmp = dirNuc ? (TH1 *)dirNuc->Get(histName) : (TH1 *)fIn->Get(histName);
        if (h_tmp) {
          if (!h) {
            h = (TH1 *)h_tmp->Clone(Form("hKstar_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!h->GetSumw2N()) h->Sumw2();
          } else {
            TH1 *h_add = (TH1 *)h_tmp->Clone();
            if (!h_add->GetSumw2N()) h_add->Sumw2();
            h->Add(h_add);
            delete h_add;
          }
        }

        TString mixName = Form("hKstar_Mixed_%s_CentBin%d", sp.Data(), j);
        TH1 *hmix_tmp = dirMix ? (TH1 *)dirMix->Get(mixName) : (TH1 *)fIn->Get(mixName);
        if (hmix_tmp) {
          if (!hmix) {
            hmix = (TH1 *)hmix_tmp->Clone(Form("hKstar_Mixed_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmix->GetSumw2N()) hmix->Sumw2();
          } else {
            TH1 *hmix_add = (TH1 *)hmix_tmp->Clone();
            if (!hmix_add->GetSumw2N()) hmix_add->Sumw2();
            hmix->Add(hmix_add);
            delete hmix_add;
          }
        }

        TString nameSBNeg = Form("hKstar_%s_SBNeg_CentBin%d", sp.Data(), j);
        TH1 *hSBNeg_tmp = dirNuc ? (TH1 *)dirNuc->Get(nameSBNeg) : (TH1 *)fIn->Get(nameSBNeg);
        if (hSBNeg_tmp) {
          if (!hSBNeg_orig) {
            hSBNeg_orig = (TH1 *)hSBNeg_tmp->Clone(Form("hSBNeg_orig_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hSBNeg_orig->GetSumw2N()) hSBNeg_orig->Sumw2();
          } else {
            TH1 *hSBNeg_add = (TH1 *)hSBNeg_tmp->Clone();
            if (!hSBNeg_add->GetSumw2N()) hSBNeg_add->Sumw2();
            hSBNeg_orig->Add(hSBNeg_add);
            delete hSBNeg_add;
          }
        }

        TString nameSBPos = Form("hKstar_%s_SBPos_CentBin%d", sp.Data(), j);
        TH1 *hSBPos_tmp = dirNuc ? (TH1 *)dirNuc->Get(nameSBPos) : (TH1 *)fIn->Get(nameSBPos);
        if (hSBPos_tmp) {
          if (!hSBPos_orig) {
            hSBPos_orig = (TH1 *)hSBPos_tmp->Clone(Form("hSBPos_orig_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hSBPos_orig->GetSumw2N()) hSBPos_orig->Sumw2();
          } else {
            TH1 *hSBPos_add = (TH1 *)hSBPos_tmp->Clone();
            if (!hSBPos_add->GetSumw2N()) hSBPos_add->Sumw2();
            hSBPos_orig->Add(hSBPos_add);
            delete hSBPos_add;
          }
        }

        TString mixNameSBNeg = Form("hKstar_Mixed_%s_SBNeg_CentBin%d", sp.Data(), j);
        TH1 *hmixSBNeg_tmp = dirMix ? (TH1 *)dirMix->Get(mixNameSBNeg) : (TH1 *)fIn->Get(mixNameSBNeg);
        if (hmixSBNeg_tmp) {
          if (!hmixSBNeg_orig) {
            hmixSBNeg_orig = (TH1 *)hmixSBNeg_tmp->Clone(Form("hmixSBNeg_orig_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSBNeg_orig->GetSumw2N()) hmixSBNeg_orig->Sumw2();
          } else {
            TH1 *hmixSBNeg_add = (TH1 *)hmixSBNeg_tmp->Clone();
            if (!hmixSBNeg_add->GetSumw2N()) hmixSBNeg_add->Sumw2();
            hmixSBNeg_orig->Add(hmixSBNeg_add);
            delete hmixSBNeg_add;
          }
        }

        TString mixNameSBPos = Form("hKstar_Mixed_%s_SBPos_CentBin%d", sp.Data(), j);
        TH1 *hmixSBPos_tmp = dirMix ? (TH1 *)dirMix->Get(mixNameSBPos) : (TH1 *)fIn->Get(mixNameSBPos);
        if (hmixSBPos_tmp) {
          if (!hmixSBPos_orig) {
            hmixSBPos_orig = (TH1 *)hmixSBPos_tmp->Clone(Form("hmixSBPos_orig_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSBPos_orig->GetSumw2N()) hmixSBPos_orig->Sumw2();
          } else {
            TH1 *hmixSBPos_add = (TH1 *)hmixSBPos_tmp->Clone();
            if (!hmixSBPos_add->GetSumw2N()) hmixSBPos_add->Sumw2();
            hmixSBPos_orig->Add(hmixSBPos_add);
            delete hmixSBPos_add;
          }
        }
      }

      TH1 *hmix_unscaled = 0;

      if (h) {
        h->Rebin(nRebin);

        if (hmix) {
          hmix->Rebin(nRebin);
          hmix_unscaled = (TH1 *)hmix->Clone(Form("hmix_unscaled_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));

          int bin1 = h->FindBin(0.40001);
          int bin2 = h->FindBin(0.59999);
          double intTrue = h->Integral(bin1, bin2);
          double intMix = hmix->Integral(hmix->FindBin(0.40001), hmix->FindBin(0.59999));

          if (intMix > 0) {
            hmix->Scale(intTrue / intMix);
          }
        }

        // Sideband Subtraction
        TH1 *h5_true = (TH1 *)h->Clone(Form("h5_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));

        TH1 *hSB_tot_true = 0;
        if (hSBNeg_orig && hSBPos_orig) {
          hSB_tot_true = (TH1 *)hSBNeg_orig->Clone(Form("hSB_tot_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          if (!hSB_tot_true->GetSumw2N()) hSB_tot_true->Sumw2();
          hSB_tot_true->Add(hSBPos_orig);
          hSB_tot_true->Rebin(nRebin);
        } else if (hSBNeg_orig) {
          hSB_tot_true = (TH1 *)hSBNeg_orig->Clone(Form("hSB_tot_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          if (!hSB_tot_true->GetSumw2N()) hSB_tot_true->Sumw2();
          hSB_tot_true->Rebin(nRebin);
        } else if (hSBPos_orig) {
          hSB_tot_true = (TH1 *)hSBPos_orig->Clone(Form("hSB_tot_true_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          if (!hSB_tot_true->GetSumw2N()) hSB_tot_true->Sumw2();
          hSB_tot_true->Rebin(nRebin);
        }

        if (hSB_tot_true) {
          h5_true->Add(hSB_tot_true, -1.0);
        }

        TH1 *h5_mix = 0;
        if (hmix_unscaled) {
          h5_mix = (TH1 *)hmix_unscaled->Clone(Form("h5_mix_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));

          TH1 *hmixSB_tot = 0;
          if (hmixSBNeg_orig && hmixSBPos_orig) {
            hmixSB_tot = (TH1 *)hmixSBNeg_orig->Clone(Form("hmixSB_tot_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
            hmixSB_tot->Add(hmixSBPos_orig);
            hmixSB_tot->Rebin(nRebin);
          } else if (hmixSBNeg_orig) {
            hmixSB_tot = (TH1 *)hmixSBNeg_orig->Clone(Form("hmixSB_tot_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
            hmixSB_tot->Rebin(nRebin);
          } else if (hmixSBPos_orig) {
            hmixSB_tot = (TH1 *)hmixSBPos_orig->Clone(Form("hmixSB_tot_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
            if (!hmixSB_tot->GetSumw2N()) hmixSB_tot->Sumw2();
            hmixSB_tot->Rebin(nRebin);
          }

          if (hmixSB_tot) {
            h5_mix->Add(hmixSB_tot, -1.0);
          }

          // Scale
          int bin1 = h5_true->FindBin(0.40001);
          int bin2 = h5_true->FindBin(0.59999);
          double int5_true = h5_true->Integral(bin1, bin2);
          double int5_mix = h5_mix->Integral(h5_mix->FindBin(0.40001), h5_mix->FindBin(0.59999));

          if (int5_mix > 0) {
            h5_mix->Scale(int5_true / int5_mix);
          }
        }

        // Generate CF
        cFit->cd(group + 1);
        if (h5_mix) {
          TH1 *hCF_sb = (TH1 *)h5_true->Clone(Form("hCF_sb_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          hCF_sb->Divide(h5_mix);

          TH1 *hCF_sb_zoom = (TH1 *)hCF_sb->Clone(Form("hCF_sb_zoom_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          hCF_sb_zoom->SetTitle(Form("CF Zoom (SB Sub) %s (%s)", sp.Data(), centLabel.Data()));
          hCF_sb_zoom->GetXaxis()->SetRangeUser(0.0, 0.15);
          hCF_sb_zoom->GetYaxis()->SetRangeUser(0.0, 10.0);
          hCF_sb_zoom->GetYaxis()->SetTitle("C(k*)");
          hCF_sb_zoom->GetXaxis()->SetTitle("k* (GeV/c)");
          hCF_sb_zoom->SetLineColor(kBlack);
          hCF_sb_zoom->SetMarkerColor(kBlack);
          hCF_sb_zoom->SetMarkerStyle(20);
          hCF_sb_zoom->Draw("E1");

          // --- FITTING ---
          
          double fit_max = 0.15; // Fitting range up to 0.15 GeV/c
          
          // 1. Fit fixed a0, reff
          TF1 *fit1 = new TF1(Form("fit1_%d_%d", groupSet, group), fitLL_fixed, 0.0, fit_max, 2);
          fit1->SetParameters(3.0, 1.0); // Init R=3.0 fm, norm=1.0
          fit1->SetParNames("R", "Norm");
          fit1->SetParLimits(0, 0.1, 20.0); // Limit R > 0
          fit1->SetLineColor(kBlue);
          hCF_sb_zoom->Fit(fit1, "R"); // Fit within range
          
          // 2. Fit free a0, reff, R
          TF1 *fit2 = new TF1(Form("fit2_%d_%d", groupSet, group), fitLL_free, 0.0, fit_max, 4);
          fit2->SetParameters(3.0, 4.01, 1.84, 1.0); // Init R=3, a0=4.01, reff=1.84, norm=1
          fit2->SetParNames("R", "a_{0}", "r_{eff}", "Norm");
          fit2->SetParLimits(0, 0.1, 20.0); // Limit R > 0
          fit2->SetLineColor(kRed);
          hCF_sb_zoom->Fit(fit2, "R+"); // "R+" means draw on top of existing fit
          
          TLegend *leg = new TLegend(0.4, 0.7, 0.9, 0.9);
          leg->SetBorderSize(0);
          leg->AddEntry(hCF_sb_zoom, "Data", "pe");
          leg->AddEntry(fit1, "Fit (a0, reff fixed)", "l");
          leg->AddEntry(fit2, "Fit (all free)", "l");
          leg->Draw();
        }

      } else {
        std::cerr << "Warning: No histogram found for group " << group << std::endl;
      }

      if (hSBNeg_orig) delete hSBNeg_orig;
      if (hSBPos_orig) delete hSBPos_orig;
      if (hmixSBNeg_orig) delete hmixSBNeg_orig;
      if (hmixSBPos_orig) delete hmixSBPos_orig;
    }

    if (groupSet == 0) {
      cFit->Print(outFile + "(");
    } else {
      cFit->Print(outFile + ")");
    }
    
    delete cFit;
  }

  std::cout << "Fit drawing completed. PDF file saved to: " << outFile << std::endl;
  fIn->Close();
}
"""
    with open("tools/Fit_LambdaNuclearId_relamom_sum.C", "w") as f:
        f.write(new_macro)
    
    print("Macro created successfully.")

if __name__ == "__main__":
    main()
