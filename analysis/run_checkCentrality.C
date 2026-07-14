// run_checkCentrality.C - Load libs, compile checkCentrality.C+, and run.
// Usage: root4star -b -q 'analysis/run_checkCentrality.C("input.list","config/mainconf/main_*.yaml","out.root",100)'

void run_checkCentrality(const Char_t* inputFile,
                         const Char_t* mainconfPath,
                         const Char_t* outputFile = "centrality_qa.root",
                         Long64_t nEventsMax = -1) {
  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  gROOT->LoadMacro("$STAR/StRoot/StMuDSTMaker/COMMON/macros/loadSharedLibraries.C");
  loadSharedLibraries();
  gSystem->Load("StPicoEvent");
  gSystem->Load("StPicoDstMaker");

  if (gSystem->Load(TString(pwd) + "/lib/libStarAnaConfig.so") < 0) {
    std::cerr << "ERROR: failed to load libStarAnaConfig.so" << std::endl;
    return;
  }
  if (gSystem->Load(TString(pwd) + "/lib/libStRefMultCorr.so") < 0) {
    std::cerr << "ERROR: failed to load libStRefMultCorr.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gInterpreter->AddIncludePath(TString::Format("%s/StRoot", pwd));
  gInterpreter->AddIncludePath(TString::Format("%s/StRoot/StRefMultCorr", pwd));
  gInterpreter->AddIncludePath("$STAR/StRoot");
  gInterpreter->AddIncludePath("$STAR/StRoot/StPicoDstMaker");
  gInterpreter->AddIncludePath("$STAR/StRoot/StPicoEvent");
  gSystem->AddLinkedLibs(TString::Format("-L%s/lib -lStarAnaConfig -lStRefMultCorr -Wl,-rpath,%s/lib", pwd, pwd));

  gROOT->ProcessLine(TString::Format(".L %s/common/macro/checkCentrality.C+", pwd));
  checkCentrality(inputFile, mainconfPath, outputFile, nEventsMax);
}
