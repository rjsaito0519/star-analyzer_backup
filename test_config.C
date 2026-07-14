void test_config() {
  gSystem->Load("lib/libStarAnaConfig.so");
  gInterpreter->AddIncludePath(".");
  gInterpreter->AddIncludePath("./include");
  gROOT->ProcessLine(".L src/cuts/NuclearIdCutConfig.cpp+");

  NuclearIdCutConfig cuts;
  cuts.LoadFromFile("config/cuts/nuclearid/nuclearid_auau13p5_anaLambdaNuclearId.yaml");
  std::cout << "MeanLambda = " << cuts.MeanLambda << std::endl;
  std::cout << "sigmaLambda = " << cuts.sigmaLambda << std::endl;
  std::cout << "nsigmaLambda = " << cuts.nsigmaLambda << std::endl;
}
