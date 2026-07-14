// run_anaPhi.C - Wrapper to load lib and call anaPhi
// ROOT -q treats only one macro; load STAR libs first, then libStarAnaConfig, then libStPhiMaker.
// Usage: root4star -b -q 'run_anaPhi.C("input.list","output.root","0",100)'
//        run_anaPhi.C("input.list","output.root","0",100,"config/mainconf/main_auau19_anaPhi.yaml")'

void run_anaPhi(const Char_t* inputFile,
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
  if (gSystem->Load(TString(pwd) + "/lib/libStPhiMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStPhiMaker.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gInterpreter->AddIncludePath(TString::Format("%s/StMaker/common", pwd));
  gInterpreter->AddIncludePath("$STAR/StRoot");
  gSystem->AddLinkedLibs(TString::Format("-L%s/lib -lStarAnaConfig -lStRefMultCorr -lStCommon -lStPhiMaker -Wl,-rpath,%s/lib", pwd, pwd));

  gROOT->ProcessLine(TString::Format(".L %s/analysis/anaPhi.C+", pwd));
  anaPhi(inputFile, outputFile, jobid, nEventsMax, configPath);
}
