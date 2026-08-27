// run_anaK0XiFxtFemto.C - Load libs and run FXT K0s–Xi femtoscopy
// Usage: root4star -b -q 'run_anaK0XiFxtFemto.C("input.list","output.root","0",1000,"config/mainconf/main_auau3p9fxt_anaK0XiFemto.yaml")'

void run_anaK0XiFxtFemto(const Char_t* inputFile,
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
  if (gSystem->Load(TString(pwd) + "/lib/libStXiFxtMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStXiFxtMaker.so" << std::endl;
    return;
  }
  if (gSystem->Load(TString(pwd) + "/lib/libStK0shortFxtMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStK0shortFxtMaker.so" << std::endl;
    return;
  }
  if (gSystem->Load(TString(pwd) + "/lib/libStK0XiFxtFemtoMaker.so") < 0) {
    std::cerr << "ERROR: failed to load libStK0XiFxtFemtoMaker.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gInterpreter->AddIncludePath(TString::Format("%s/StMaker/common", pwd));
  gInterpreter->AddIncludePath("$STAR/StRoot");
  gSystem->AddLinkedLibs(TString::Format(
      "-L%s/lib -lStarAnaConfig -lStRefMultCorr -lStCommon -lStXiFxtMaker -lStK0shortFxtMaker -lStK0XiFxtFemtoMaker -Wl,-rpath,%s/lib",
      pwd, pwd));

  TString acliCBuildDir = TString::Format("%s/.build/aclic", pwd);
  gSystem->mkdir(acliCBuildDir.Data(), kTRUE);
  gSystem->SetBuildDir(acliCBuildDir.Data());

  gROOT->ProcessLine(TString::Format(".L %s/analysis/anaK0XiFxtFemto.C+", pwd));
  anaK0XiFxtFemto(inputFile, outputFile, jobid, nEventsMax, configPath);
}
