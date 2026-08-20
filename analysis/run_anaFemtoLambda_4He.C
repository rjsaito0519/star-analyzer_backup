// run_anaFemtoLambda_4He.C
void run_anaFemtoLambda_4He(const Char_t* inputFile,
                           const Char_t* outputFile,
                           const Char_t* jobid = "0",
                           Long64_t nEventsMax = -1,
                           const Char_t* configPath = 0)
{
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
  if (gSystem->Load(TString(pwd) + "/lib/libStCommon.so") < 0) {
    std::cerr << "ERROR: failed to load libStCommon.so" << std::endl;
    return;
  }
  if (gSystem->Load(TString(pwd) + "/lib/libStLambdaNuclearMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStLambdaNuclearMaker.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gInterpreter->AddIncludePath(TString::Format("%s/StMaker/common", pwd));
  gInterpreter->AddIncludePath("$STAR/StRoot");
  gSystem->AddLinkedLibs(TString::Format("-L%s/lib -lStarAnaConfig -lStRefMultCorr -lStCommon -lStLambdaNuclearMaker -Wl,-rpath,%s/lib", pwd, pwd));

  gROOT->ProcessLine(TString::Format(".L %s/analysis/anaFemtoLambda_4He.C+", pwd));
  anaFemtoLambda_4He(inputFile, outputFile, jobid, nEventsMax, configPath);
}
