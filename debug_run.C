void debug_run() {
  gROOT->LoadMacro("$STAR/StRoot/StMuDSTMaker/COMMON/macros/loadSharedLibraries.C");
  loadSharedLibraries();
  gSystem->Load("StPicoEvent");
  gSystem->Load("StPicoDstMaker");
  gSystem->Load("libStarAnaConfig");
  gSystem->Load("libStRefMultCorr");
  gSystem->Load("libStLambdaMaker");
  gSystem->Load("libStNuclearIdMaker");

  gROOT->ProcessLine(".L analysis/anaLambdaNuclearId.C+");
  anaLambdaNuclearId("config/picoDstList/auau13p5GeV.list", "rootfile/auau13p5_anaLambdaNuclearId/tmp_debug.root", "0", 100, "config/mainconf/main_auau13p5_anaLambdaNuclearId.yaml");
}
