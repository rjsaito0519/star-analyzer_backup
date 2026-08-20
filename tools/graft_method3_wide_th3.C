// tools/graft_method3_wide_th3.C
// Copy an existing anaFemtoPhi merge and add/overwrite hPhiMKK_vs_Kstar{SE,ME}_*_wide
// with toy full-M_KK fills so Method 3 checkHist can be exercised without re-running Maker.
//
// Usage (inside singularity / root4star):
//   root4star -b -q 'tools/graft_method3_wide_th3.C("in_merge.root","out_pilot_merge.root")'

#include <TFile.h>
#include <TKey.h>
#include <TH3F.h>
#include <TRandom3.h>
#include <TString.h>
#include <iostream>
#include <cmath>

static TH3F* CloneEmptyLike(const TH3* src, const char* newName) {
  TH3F* h = new TH3F(newName, Form("%s;%s;%s;%s", newName,
                                   src->GetXaxis()->GetTitle(),
                                   src->GetYaxis()->GetTitle(),
                                   src->GetZaxis()->GetTitle()),
                     src->GetNbinsX(), src->GetXaxis()->GetXmin(), src->GetXaxis()->GetXmax(),
                     src->GetNbinsY(), src->GetYaxis()->GetXmin(), src->GetYaxis()->GetXmax(),
                     src->GetNbinsZ(), src->GetZaxis()->GetXmin(), src->GetZaxis()->GetXmax());
  h->Sumw2();
  return h;
}

static void FillToyWide(TH3F* h, Int_t nFill, Double_t weightScale, UInt_t seed) {
  TRandom3 rng(seed);
  for (Int_t i = 0; i < nFill; ++i) {
    Double_t m = 0.0;
    if (rng.Uniform() < 0.55) {
      m = rng.Gaus(1.0195, 0.0045);
    } else {
      // sideband-rich continuum across wide M_KK
      m = rng.Uniform(0.985, 1.15);
    }
    Double_t k = rng.Exp(0.10);
    if (k > 0.55) k = 0.55;
    // prefer central cent9 bins 7-8 (~0-10% / 10-20% depending on mapping)
    Double_t c = rng.Uniform(7.0, 8.999);
    h->Fill(m, k, c, weightScale);
  }
}

void graft_method3_wide_th3(const char* inPath, const char* outPath) {
  TFile* fin = TFile::Open(inPath, "READ");
  if (!fin || fin->IsZombie()) {
    std::cerr << "cannot open " << inPath << std::endl;
    return;
  }
  TFile* fout = TFile::Open(outPath, "RECREATE");
  if (!fout || fout->IsZombie()) {
    std::cerr << "cannot create " << outPath << std::endl;
    fin->Close();
    return;
  }

  // Copy all keys except existing wide MKK keys (we overwrite those).
  TIter next(fin->GetListOfKeys());
  TKey* key = 0;
  Int_t nCopied = 0;
  while ((key = (TKey*)next())) {
    TString name = key->GetName();
    if (name.Contains("hPhiMKK_vs_Kstar") && name.EndsWith("_wide")) continue;
    TObject* obj = key->ReadObj();
    if (!obj) continue;
    fout->cd();
    obj->Write(name);
    delete obj;
    ++nCopied;
  }

  const char* bases[] = {"phi_proton", "phi_deuteron", "phi_triton", "phi_he3", "phi_he4", 0};
  Int_t nWide = 0;
  for (Int_t ib = 0; bases[ib]; ++ib) {
    for (Int_t ime = 0; ime < 2; ++ime) {
      TString sigName = Form("hPhiMKK_vs_Kstar%s_%s_signal", ime ? "ME" : "SE", bases[ib]);
      TH3* hSig = dynamic_cast<TH3*>(fin->Get(sigName));
      TString wideName = Form("hPhiMKK_vs_Kstar%s_%s_wide", ime ? "ME" : "SE", bases[ib]);
      TH3F* hW = 0;
      if (hSig) {
        hW = CloneEmptyLike(hSig, wideName);
      } else {
        hW = new TH3F(wideName, wideName + ";M_{KK};k*;cent9",
                      200, 0.98, 1.18, 60, 0.0, 0.6, 9, -0.5, 8.5);
        hW->Sumw2();
      }
      const Int_t nFill = (ib == 0) ? 120000 : (40000 / (ib + 1));
      const Double_t w = ime ? 6.0 : 1.0;
      FillToyWide(hW, nFill, w, 1000u + 17u * ib + 3u * ime);

      // Audit sideband vs signal window on projection
      TH1D* px = hW->ProjectionX(Form("_px_%s", wideName.Data()));
      const Int_t bLo = px->FindBin(1.012 + 1e-9);
      const Int_t bHi = px->FindBin(1.026 - 1e-9);
      Double_t nSigWin = px->Integral(bLo, bHi);
      Double_t nSide = px->Integral(1, bLo - 1) + px->Integral(bHi + 1, px->GetNbinsX());
      std::cout << wideName << " entries=" << hW->GetEntries()
                << " nSigWin=" << nSigWin << " nSideband=" << nSide << std::endl;
      delete px;

      fout->cd();
      hW->Write();
      delete hW;
      ++nWide;
    }
  }

  fout->Close();
  fin->Close();
  delete fout;
  delete fin;
  std::cout << "Copied " << nCopied << " objects; wrote " << nWide
            << " wide TH3s -> " << outPath << std::endl;
}
