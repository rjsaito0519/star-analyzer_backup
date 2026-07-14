// Print histogram entries for phi-KK QA smoke checks.
// Usage: root4star -b -q 'script/print_hist_entries.C("path/to/file.root")'

void print_hist_entries(const Char_t* rootPath = 0) {
  if (!rootPath) {
    std::cerr << "Usage: print_hist_entries.C(\"file.root\")" << std::endl;
    return;
  }
  TFile fin(rootPath, "READ");
  if (!fin.IsOpen()) {
    std::cerr << "Cannot open " << rootPath << std::endl;
    return;
  }
  const char* names[] = {
      "hPhiPair_Mass_stage0", "hPhiPair_Mass_tofStrict", "hMKK_BothCuts",
      "hPhi_MKK", "hKstarSE_phi_proton", 0};
  for (Int_t i = 0; names[i]; ++i) {
    TH1* h = (TH1*)fin.Get(names[i]);
    std::cout << names[i] << ": " << (h ? h->GetEntries() : -1) << std::endl;
  }
  fin.Close();
}
