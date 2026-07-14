// run_anaKXiFemto.C - Wrapper to load libraries and run K0-Xi Femtoscopy analysis
// Usage: root4star -b -q 'run_anaKXiFemto.C("input.list","output.root","0",100,"config/mainconf/main_auau19_anaKXiFemto.yaml")'

void run_anaKXiFemto(const Char_t* inputFile,
                     const Char_t* outputFile,
                     const Char_t* jobid = "0",
                     Long64_t nEventsMax = -1,
                     const Char_t* configPath = 0)
{
  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  TString starDir = gSystem->Getenv("STAR");
  if (starDir.IsNull()) {
    starDir = "/star/nfs4/AFS/star/packages/SL24y";
  }
  gROOT->LoadMacro(starDir + "/StRoot/StMuDSTMaker/COMMON/macros/loadSharedLibraries.C");
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
  if (gSystem->Load(TString(pwd) + "/lib/libStK0shortMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStK0shortMaker.so" << std::endl;
    return;
  }
  if (gSystem->Load(TString(pwd) + "/lib/libStXiMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStXiMaker.so" << std::endl;
    return;
  }
  if (gSystem->Load(TString(pwd) + "/lib/libStKXiFemtoMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStKXiFemtoMaker.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gInterpreter->AddIncludePath(TString::Format("%s/StMaker/common", pwd));
  gInterpreter->AddIncludePath("$STAR/StRoot");
  gSystem->AddLinkedLibs(TString::Format("-L%s/lib -lStarAnaConfig -lStRefMultCorr -lStCommon -lStK0shortMaker -lStXiMaker -lStKXiFemtoMaker -Wl,-rpath,%s/lib", pwd, pwd));

  gROOT->ProcessLine(TString::Format(".L %s/analysis/anaKXiFemto.C+", pwd));
  anaKXiFemto(inputFile, outputFile, jobid, nEventsMax, configPath);
}
