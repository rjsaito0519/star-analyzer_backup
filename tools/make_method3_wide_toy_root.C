// tools/make_method3_wide_toy_root.C — create minimal ROOT with wide TH3 for Method3 checkHist smoke test.
// root -b -q 'tools/make_method3_wide_toy_root.C("rootfile/auau3p85fxt_anaFemtoPhi/auau3p85fxt_anaFemtoPhi_method3pilot_merge.root")'

#include <TFile.h>
#include <TH3F.h>
#include <TF1.h>
#include <TRandom3.h>
#include <TString.h>
#include <iostream>

void make_method3_wide_toy_root(const char* outPath =
                                    "rootfile/auau3p85fxt_anaFemtoPhi/auau3p85fxt_anaFemtoPhi_method3pilot_merge.root") {
  TFile* f = TFile::Open(outPath, "RECREATE");
  if (!f || f->IsZombie()) {
    std::cerr << "cannot create " << outPath << std::endl;
    return;
  }
  TRandom3 rng(42);
  const char* bases[] = {"phi_proton", "phi_deuteron", "phi_triton", "phi_he3", "phi_he4", 0};
  for (Int_t ib = 0; bases[ib]; ++ib) {
    for (Int_t ime = 0; ime < 2; ++ime) {
      TString name = Form("hPhiMKK_vs_Kstar%s_%s_wide", ime ? "ME" : "SE", bases[ib]);
      TH3F* h = new TH3F(name, name + ";M_{KK};k*;cent9", 200, 0.98, 1.18, 60, 0.0, 0.6, 9, -0.5, 8.5);
      const Double_t scale = ime ? 8.0 : 1.0;
      const Int_t nFill = (ib == 0) ? 80000 : (20000 / (ib + 1));
      for (Int_t i = 0; i < nFill; ++i) {
        Double_t m = rng.Gaus(1.0195, 0.0045);
        if (rng.Uniform() < 0.35) m = rng.Uniform(0.99, 1.06);
        Double_t k = rng.Exp(0.12);
        if (k > 0.59) k = 0.59;
        Double_t c = rng.Uniform(7.0, 8.999);  // central
        h->Fill(m, k, c, scale);
      }
      // Also fill signal-window-only twin for compatibility (optional)
      TString nameSig = Form("hPhiMKK_vs_Kstar%s_%s_signal", ime ? "ME" : "SE", bases[ib]);
      TH3F* hs = new TH3F(nameSig, nameSig, 200, 0.98, 1.18, 60, 0.0, 0.6, 9, -0.5, 8.5);
      for (Int_t ix = 1; ix <= h->GetNbinsX(); ++ix) {
        Double_t m = h->GetXaxis()->GetBinCenter(ix);
        if (m < 1.012 || m > 1.026) continue;
        for (Int_t iy = 1; iy <= h->GetNbinsY(); ++iy) {
          for (Int_t iz = 1; iz <= h->GetNbinsZ(); ++iz) {
            hs->SetBinContent(ix, iy, iz, h->GetBinContent(ix, iy, iz));
          }
        }
      }
      h->Write();
      hs->Write();
      delete hs;
      delete h;
    }
  }
  // Minimal stubs so checkHist does not crash on missing QA keys (optional empty).
  f->Close();
  delete f;
  std::cout << "Wrote toy Method3 wide ROOT: " << outPath << std::endl;
}
