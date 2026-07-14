#include "TAxis.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TLegend.h"
#include "TMath.h"
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

void Draw_LLFormula() {
  // Parameters from Table III (Chi3 model)
  double a0 = 4.01;   // fm
  double reff = 1.84; // fm

  int nPoints = 300;
  double q_max = 0.3; // GeV/c

  TGraph *gr_R1 = new TGraph(nPoints);
  TGraph *gr_R3 = new TGraph(nPoints);

  for (int i = 0; i < nPoints; i++) {
    double q_GeV = i * (q_max / (nPoints - 1));
    double q_MeV = q_GeV * 1000.0; // convert to MeV/c for LL_Formula
    if (q_MeV == 0)
      q_MeV = 1e-3; // avoid exact 0

    double c_R1 = LL_Formula(q_MeV, 1.0, a0, reff);
    double c_R3 = LL_Formula(q_MeV, 3.0, a0, reff);

    gr_R1->SetPoint(i, q_GeV, c_R1);
    gr_R3->SetPoint(i, q_GeV, c_R3);
  }

  TCanvas *c1 = new TCanvas("c1", "LL Formula Reproduction", 1000, 500);
  c1->Divide(2, 1);

  c1->cd(1);
  gr_R1->SetTitle("R = 1 fm; k* (GeV/c); C(k*)");
  gr_R1->SetLineColor(kGreen);
  gr_R1->SetLineStyle(2); // dashed line
  gr_R1->SetLineWidth(2);
  gr_R1->Draw("AL");
  gr_R1->GetYaxis()->SetRangeUser(0.0, 1.1);
  gr_R1->GetXaxis()->SetRangeUser(0.0, 0.3);

  TLegend *leg1 = new TLegend(0.5, 0.2, 0.85, 0.3);
  leg1->SetBorderSize(0);
  leg1->AddEntry(gr_R1, "LL (a_{0}=4.01, r_{eff}=1.84)", "l");
  leg1->Draw();

  c1->cd(2);
  gr_R3->SetTitle("R = 3 fm; k* (GeV/c); C(k*)");
  gr_R3->SetLineColor(kGreen);
  gr_R3->SetLineStyle(2); // dashed line
  gr_R3->SetLineWidth(2);
  gr_R3->Draw("AL");
  gr_R3->GetYaxis()->SetRangeUser(0.0, 1.1);
  gr_R3->GetXaxis()->SetRangeUser(0.0, 0.3);

  TLegend *leg3 = new TLegend(0.5, 0.2, 0.85, 0.3);
  leg3->SetBorderSize(0);
  leg3->AddEntry(gr_R3, "LL (a_{0}=4.01, r_{eff}=1.84)", "l");
  leg3->Draw();

  c1->SaveAs("tools/ref/LL_Formula_Fig5_reproduction.pdf");
  std::cout << "Saved LL_Formula_Fig5_reproduction.pdf" << std::endl;
}
