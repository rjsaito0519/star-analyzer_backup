// tools/audit_method3_wide_th3.C
// root4star -b -q 'tools/audit_method3_wide_th3.C("merge.root")'
#include <TFile.h>
#include <TH3.h>
#include <TH1D.h>
#include <TString.h>
#include <iostream>

void audit_method3_wide_th3(const char* path) {
  TFile* f = TFile::Open(path);
  if (!f || f->IsZombie()) {
    std::cerr << "FAIL open " << path << std::endl;
    return;
  }
  const char* bases[] = {"phi_proton", "phi_deuteron", "phi_triton", "phi_he3", "phi_he4", 0};
  Int_t nWide = 0, nSideOk = 0;
  for (Int_t ib = 0; bases[ib]; ++ib) {
    for (Int_t ime = 0; ime < 2; ++ime) {
      TString nm = Form("hPhiMKK_vs_Kstar%s_%s_wide", ime ? "ME" : "SE", bases[ib]);
      TH3* h = dynamic_cast<TH3*>(f->Get(nm));
      if (!h) {
        std::cout << nm << " MISSING" << std::endl;
        continue;
      }
      ++nWide;
      TH1D* px = h->ProjectionX("_px_audit");
      const Int_t bLo = px->FindBin(1.012 + 1e-9);
      const Int_t bHi = px->FindBin(1.026 - 1e-9);
      const Double_t nSig = px->Integral(bLo, bHi);
      const Double_t nSide = px->Integral(1, bLo - 1) + px->Integral(bHi + 1, px->GetNbinsX());
      std::cout << nm << " entries=" << h->GetEntries()
                << " nSigWin=" << nSig << " nSide=" << nSide << std::endl;
      if (nSide > 0) ++nSideOk;
      delete px;
    }
  }
  std::cout << "SUMMARY nWide=" << nWide << " nSideOk=" << nSideOk << std::endl;
  f->Close();
}
