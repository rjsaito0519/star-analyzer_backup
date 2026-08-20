// Toy closure for kstarMassFitCF: S = F - α B, C_raw = Y_SE / Y_ME.
// root -b -q tools/test_cf_kstarmassfit.C

#include <TH1D.h>
#include <TF1.h>
#include <TMath.h>
#include <iostream>
#include <vector>

static TH1D* makeBkg(const char* name, Double_t nBkg) {
  TH1D* h = new TH1D(name, name, 140, 0.99, 1.06);
  const Double_t perBin = nBkg / 140.0;
  for (Int_t b = 1; b <= 140; ++b) {
    h->SetBinContent(b, perBin);
    h->SetBinError(b, TMath::Sqrt(perBin));
  }
  return h;
}

static void addSignal(TH1D* h, Double_t nSig, Double_t mean, Double_t sigma) {
  TF1 g("gsig", "gaus", 0.99, 1.06);
  g.SetParameters(nSig / (sigma * TMath::Sqrt(2.0 * TMath::Pi())), mean, sigma);
  h->Add(&g, 1.0);
}

static Double_t alphaFromWindow(TH1* hF, TH1* hB, Double_t aMin, Double_t aMax) {
  const Int_t b0 = hF->GetXaxis()->FindBin(aMin + 1e-9);
  const Int_t b1 = hF->GetXaxis()->FindBin(aMax - 1e-9);
  const Double_t f = hF->Integral(b0, b1);
  const Double_t b = hB->Integral(b0, b1);
  if (b <= 0.0) return 0.0;
  return f / b;
}

static Bool_t fitGausYield(TH1* hS, Double_t sigMin, Double_t sigMax, Double_t& nSig) {
  nSig = 0.0;
  TF1 f("fgaus", "gaus", 0.99, 1.06);
  f.SetParameter(0, hS->GetMaximum());
  f.SetParameter(1, 1.0195);
  f.SetParameter(2, 0.004);
  f.SetParLimits(2, 0.002, 0.020);
  const Int_t st = hS->Fit(&f, "RQ0");
  if (st != 0) return kFALSE;
  nSig = f.Integral(sigMin, sigMax);
  return nSig > 0.0 && TMath::Finite(nSig);
}

void test_cf_kstarmassfit() {
  const Double_t mean = 1.0195;
  const Double_t sigma = 0.0045;
  const Double_t sigMin = 1.012;
  const Double_t sigMax = 1.026;
  const Double_t aMin = 1.04;
  const Double_t aMax = 1.06;
  const Double_t injectedCf[] = {1.40, 1.20, 1.05, 1.00};
  const Double_t kstar[] = {0.025, 0.075, 0.125, 0.40};
  const Int_t nBins = 4;
  const Double_t yMeTrue = 800.0;
  const Double_t nBkg = 4000.0;

  Double_t maxRel = 0.0;
  Int_t nOk = 0;
  for (Int_t i = 0; i < nBins; ++i) {
    const Double_t ySeTrue = injectedCf[i] * yMeTrue;
    TH1D* hBse = makeBkg(Form("Bse_%d", i), nBkg);
    TH1D* hBme = makeBkg(Form("Bme_%d", i), nBkg * 1.1);
    TH1D* hFse = (TH1D*)hBse->Clone(Form("Fse_%d", i));
    TH1D* hFme = (TH1D*)hBme->Clone(Form("Fme_%d", i));
    addSignal(hFse, ySeTrue, mean, sigma);
    addSignal(hFme, yMeTrue, mean, sigma);

    const Double_t aSE = alphaFromWindow(hFse, hBse, aMin, aMax);
    const Double_t aME = alphaFromWindow(hFme, hBme, aMin, aMax);
    TH1D* hSse = (TH1D*)hFse->Clone(Form("Sse_%d", i));
    TH1D* hSme = (TH1D*)hFme->Clone(Form("Sme_%d", i));
    TH1D* hBscSE = (TH1D*)hBse->Clone(Form("BscSE_%d", i));
    TH1D* hBscME = (TH1D*)hBme->Clone(Form("BscME_%d", i));
    hBscSE->Scale(aSE);
    hBscME->Scale(aME);
    hSse->Add(hBscSE, -1.0);
    hSme->Add(hBscME, -1.0);

    Double_t ySE = 0.0, yME = 0.0;
    const Bool_t okSE = fitGausYield(hSse, sigMin, sigMax, ySE);
    const Bool_t okME = fitGausYield(hSme, sigMin, sigMax, yME);
    if (!okSE || !okME || yME <= 0.0) {
      std::cerr << "FAIL bin k*=" << kstar[i] << " fit SE=" << okSE << " ME=" << okME << std::endl;
      continue;
    }
    const Double_t cf = ySE / yME;
    const Double_t rel = TMath::Abs(cf - injectedCf[i]) / injectedCf[i];
    std::cout << Form("k*=%.3f  injected=%.3f  recovered=%.3f  rel=%.3f  alphaSE=%.3f alphaME=%.3f\n",
                      kstar[i], injectedCf[i], cf, rel, aSE, aME);
    if (rel > maxRel) maxRel = rel;
    if (rel < 0.15) ++nOk;
    delete hBse;
    delete hBme;
    delete hFse;
    delete hFme;
    delete hSse;
    delete hSme;
    delete hBscSE;
    delete hBscME;
  }
  const Bool_t pass = (nOk == nBins) && (maxRel < 0.15);
  std::cout << "kstarMassFitCF toy closure: nOk=" << nOk << "/" << nBins << " maxRel=" << maxRel
            << (pass ? " PASS" : " FAIL") << std::endl;
  if (!pass) {
    std::cerr << "kstarMassFitCF toy closure FAILED" << std::endl;
  }
}
