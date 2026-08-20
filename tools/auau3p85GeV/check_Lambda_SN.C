void check_Lambda_SN() {
  TFile *fIn = TFile::Open("/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau3p85_anaLambdaNuclearId/auau3p85_all1.root", "READ");
  TH1 *h = 0;
  TDirectory *dir = (TDirectory *)fIn->Get("lambda");
  if (dir) h = (TH1 *)dir->Get("hLambda_InvMass");
  if (!h) h = (TH1 *)fIn->Get("hLambda_InvMass");
  if (!h) {
    std::cout << "Histogram not found" << std::endl;
    return;
  }

  double mean = 1.11596;
  double sigma = 2.96e-03;
  double nsigma = 3.0;

  double massMin = mean - nsigma * sigma;
  double massMax = mean + nsigma * sigma;
  double massSB1Min = mean - 2.0 * nsigma * sigma;
  double massSB1Max = massMin;
  double massSB2Min = massMax;
  double massSB2Max = mean + 2.0 * nsigma * sigma;

  int binMin = h->FindBin(massMin);
  int binMax = h->FindBin(massMax) - 1; // Prevent double counting edge
  
  int binSB1Min = h->FindBin(massSB1Min);
  int binSB1Max = h->FindBin(massSB1Max) - 1;
  
  int binSB2Min = h->FindBin(massSB2Min);
  int binSB2Max = h->FindBin(massSB2Max) - 1;

  double sig_region = h->Integral(binMin, binMax);
  double sb1_region = h->Integral(binSB1Min, binSB1Max);
  double sb2_region = h->Integral(binSB2Min, binSB2Max);
  double sb_region = sb1_region + sb2_region;

  std::cout << "--- Inclusive Lambda S/N Check ---" << std::endl;
  std::cout << "Signal window counts (S+B): " << sig_region << std::endl;
  std::cout << "Sideband window counts (B): " << sb_region << " (SB1: " << sb1_region << ", SB2: " << sb2_region << ")" << std::endl;
  std::cout << "Estimated Signal (S): " << sig_region - sb_region << std::endl;
  std::cout << "S/N ratio: " << (sig_region - sb_region) / sb_region << std::endl;
  std::cout << "S/B height ratio (visual peak / background): " << sig_region / sb_region << std::endl;
}
EOF
root -l -b -q tools/check_Lambda_SN.C
