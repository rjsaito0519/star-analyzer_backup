#include "TFile.h"
#include "TH1.h"
#include "TCanvas.h"
#include "TString.h"
#include "TLegend.h"
#include "TSystem.h"
#include "TF1.h"
#include "TMath.h"
#include "TStyle.h"
#include <iostream>


const double hbarc = 197.3269804; // MeV fm

// Dawson integral: D(x) = exp(-x^2) * integral_0^x exp(t^2) dt
// Computed using a stable series expansion to avoid overflow
double Dawson(double x) {
  // For small x: use Taylor series D(x) = x - 2x^3/3 + 4x^5/15 - ...
  // For large x: use asymptotic D(x) ~ 1/(2x) * (1 + 1/(2x^2) + ...)
  // For intermediate x: numerical integration with substitution t = x*u
  //   D(x) = x * integral_0^1 exp(x^2*(u^2-1)) du
  //   integrand is always <= 1, numerically stable!
  if (x < 0)
    return -Dawson(-x);

  if (x < 1e-6)
    return x;

  if (x > 20.0) {
    // Asymptotic: D(x) = 1/(2x) + 1/(4x^3) + ...
    return 0.5 / x * (1.0 + 0.5 / (x * x));
  }

  // Use substitution t = x*u, so exp(t^2 - x^2) = exp(x^2*(u^2-1))
  // which is always in (-inf, 0] -> exp value in [0,1]
  int n = 2000;
  double h = 1.0 / n;
  double sum = 0.0;
  // Simpson's rule on integrand exp(x^2*(u^2-1))
  for (int i = 0; i <= n; i++) {
    double u = i * h;
    double y = exp(x * x * (u * u - 1.0));
    double weight = (i == 0 || i == n) ? 1.0 : ((i % 2 != 0) ? 4.0 : 2.0);
    sum += weight * y;
  }
  sum *= h / 3.0;
  return x * sum;
}

// F1(x) = D(x)/x  where D(x) is the Dawson integral
// = e^{-x^2} * integral_0^x e^{t^2} dt / x
double F1(double x) {
  if (x < 1e-6)
    return 1.0;
  return Dawson(x) / x;
}

// F2(x) = (1 - e^{-x^2}) / x
double F2(double x) {
  if (x < 1e-6)
    return x;
  return (1.0 - exp(-x * x)) / x;
}

// F3 correction: 1 - r_eff / (2*sqrt(pi)*R)
// Called as F3(reff/R) = 1 - (reff/R)/(2*sqrt(pi))
double F3(double x) { return 1.0 - x / (2.0 * sqrt(TMath::Pi())); }

// Lednicky-Lyuboshits Formula
double LL_Formula(double q_MeV, double R, double a0, double reff) {
  double q = q_MeV / hbarc; // convert to fm^-1

  double qcotdelta = -1.0 / a0 + 0.5 * reff * q * q;
  double denom = qcotdelta * qcotdelta + q * q;

  double mag_f_sq = (denom > 0) ? 1.0 / denom : a0 * a0;
  double re_f = (denom > 0) ? qcotdelta / denom : -a0;
  double im_f = (denom > 0) ? q / denom : 0.0;

  double term1 = mag_f_sq / (2.0 * R * R) * F3(reff / R);
  double term2 = 2.0 * re_f / (sqrt(TMath::Pi()) * R) * F1(2.0 * q * R);
  double term3 = im_f / R * F2(2.0 * q * R);

  return 1.0 + term1 + term2 - term3;
}



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

#include "TLatex.h"

void Fit_LambdaNuclearId_relamom_sum_CFsub() {
  gStyle->SetOptFit(0); // Hide fit parameters in stat box
  gStyle->SetOptStat(0); // Hide stat box

  TString inputFile =
    "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/"
    //"auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all.root";
    "auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all_LambdaImprove_WideSide.root";
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

  TString outFile = Form("%s/Fit_LL_CFsub_%s.pdf", outDir.Data(), sp.Data());

  for (int groupSet = 0; groupSet < 2; ++groupSet) {
    int nGroups = (groupSet == 0) ? 2 : 3;

    TCanvas *cFit = new TCanvas(
        Form("cFit_%s_groupSet%d", sp.Data(), groupSet),
        Form("LL Fit CF (Purity Corrected) for Lambda - %s", sp.Data()), 1200, 600);
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

      if (hSBNeg_orig) hSBNeg_orig->Scale(1.0 / 3.0);
      if (hSBPos_orig) hSBPos_orig->Scale(1.0 / 3.0);
      if (hmixSBNeg_orig) hmixSBNeg_orig->Scale(1.0 / 3.0);
      if (hmixSBPos_orig) hmixSBPos_orig->Scale(1.0 / 3.0);

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

        TH1 *hCF_sig = 0;
        if (hmix) {
          hCF_sig = (TH1 *)h->Clone(Form("hCF_sig_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          hCF_sig->Divide(hmix);
        }

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

        // Generate CF
        cFit->cd(group + 1);
        if (hCF_sig && hCF_bg && h && hSB_tot_true) {
          double sn = 5.217;
          double purity = sn / (sn + 1.0);
          
          TH1 *hCF_sub = (TH1 *)hCF_sig->Clone(Form("hCF_sub_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          
          if (purity > 0) {
            TH1 *hTerm2 = (TH1*)hCF_bg->Clone("hTerm2");
            hTerm2->Scale(1.0 - purity);
            hCF_sub->Add(hTerm2, -1.0);
            hCF_sub->Scale(1.0 / purity);
          } else {
            hCF_sub->Reset();
          }

          TH1 *hCF_sb_zoom = (TH1 *)hCF_sub->Clone(Form("hCF_sb_zoom_%s_groupSet%d_Group%d", sp.Data(), groupSet, group));
          hCF_sb_zoom->SetTitle(Form("CF Zoom (Purity Corrected) %s (%s)", sp.Data(), centLabel.Data()));
          hCF_sb_zoom->GetXaxis()->SetRangeUser(0.0, 0.40);
          hCF_sb_zoom->GetYaxis()->SetRangeUser(0.0, 7.0);
          hCF_sb_zoom->GetYaxis()->SetTitle("C(k*)");
          hCF_sb_zoom->GetXaxis()->SetTitle("k* (GeV/c)");
          hCF_sb_zoom->SetLineColor(kBlack);
          hCF_sb_zoom->SetMarkerColor(kBlack);
          hCF_sb_zoom->SetMarkerStyle(20);
          hCF_sb_zoom->Draw("E1");

          // --- FITTING ---
          
          double fit_max = 0.40; // Fitting range up to 0.40 GeV/c
          
          // 1. Fit fixed a0, reff
          TF1 *fit1 = new TF1(Form("fit1_%d_%d", groupSet, group), fitLL_fixed, 0.0, fit_max, 2);
          fit1->SetParameters(3.0, 1.0); // Init R=5.0 fm, norm=1.0
          fit1->SetParNames("R", "Norm");
          fit1->SetParLimits(0, 0.1, 6.); // Limit R > 0
          fit1->SetLineColor(kBlue);
          hCF_sb_zoom->Fit(fit1, "R"); // Fit within range
	  hCF_sb_zoom->Fit(fit1, "R"); // Fit within range
          
          // 2. Fit free a0, reff, R
          TF1 *fit2 = new TF1(Form("fit2_%d_%d", groupSet, group), fitLL_free, 0.0, fit_max, 4);
          fit2->SetParameters(3.0, 4.01, 1.84, 1.0); // Init R=5.0, a0=4.01, reff=1.84, norm=1
          fit2->SetParNames("R", "a_{0}", "r_{eff}", "Norm");
          fit2->SetParLimits(0, 0.1, 6.); // Limit R > 0
          fit2->SetLineColor(kRed);
          hCF_sb_zoom->Fit(fit2, "R+"); // "R+" means draw on top of existing fit
	  hCF_sb_zoom->Fit(fit2, "R+"); // "R+" means draw on top of existing fit
          
          TLegend *leg = new TLegend(0.4, 0.7, 0.9, 0.9);
          leg->SetBorderSize(0);
          leg->AddEntry(hCF_sb_zoom, "Data", "pe");
          leg->AddEntry(fit1, "Fit (a0, reff fixed)", "l");
          leg->AddEntry(fit2, "Fit (all free)", "l");
          leg->Draw();

          TLatex tex;
          tex.SetNDC();
          tex.SetTextSize(0.04);
          
          tex.SetTextColor(kBlue);
          tex.DrawLatex(0.4, 0.65, Form("Fixed: #chi^{2}/ndf = %.1f/%d = %.2f", fit1->GetChisquare(), fit1->GetNDF(), fit1->GetChisquare()/(fit1->GetNDF() > 0 ? fit1->GetNDF() : 1)));
          tex.DrawLatex(0.4, 0.60, Form("R = %.2f #pm %.2f fm", fit1->GetParameter(0), fit1->GetParError(0)));
          
          tex.SetTextColor(kRed);
          tex.DrawLatex(0.4, 0.53, Form("Free: #chi^{2}/ndf = %.1f/%d = %.2f", fit2->GetChisquare(), fit2->GetNDF(), fit2->GetChisquare()/(fit2->GetNDF() > 0 ? fit2->GetNDF() : 1)));
          tex.DrawLatex(0.4, 0.48, Form("R = %.2f #pm %.2f fm", fit2->GetParameter(0), fit2->GetParError(0)));
          tex.DrawLatex(0.4, 0.43, Form("a_{0} = %.2f #pm %.2f fm", fit2->GetParameter(1), fit2->GetParError(1)));
          tex.DrawLatex(0.4, 0.38, Form("r_{eff} = %.2f #pm %.2f fm", fit2->GetParameter(2), fit2->GetParError(2)));
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
