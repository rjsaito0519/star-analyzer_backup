void list_keys() {
  TFile* f = new TFile("rootfile/auau13p5_anaLambdaNuclearId/tmp_relamom.root", "READ");
  if (!f || f->IsZombie()) return;
  TIter next(f->GetListOfKeys());
  TKey *key;
  while ((key = (TKey*)next())) {
    TString name = key->GetName();
    short cycle = key->GetCycle();
    if (name == "hLambda_InvMass" || name == "hDedxP" || name == "hKstar_d") {
      TH1* h = (TH1*)key->ReadObj();
      if (h) {
        std::cout << name << ";" << cycle << " entries: " << h->GetEntries() << std::endl;
      }
    }
  }
  f->Close();
}
