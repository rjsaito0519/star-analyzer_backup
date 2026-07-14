// run_checkHistAnaKplusXiFemto.C - Load libStarAnaConfig, compile checkHist, call it.

void run_checkHistAnaKplusXiFemto(const Char_t* rootFile,
                                  const Char_t* anaName,
                                  const Char_t* mainconfPath = 0)
{
  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  if (gSystem->Load(TString(pwd) + "/lib/libStarAnaConfig.so") < 0) {
    std::cerr << "ERROR: failed to load libStarAnaConfig.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gSystem->AddLinkedLibs(TString::Format("-L%s/lib -lStarAnaConfig -Wl,-rpath,%s/lib", pwd, pwd));

  gROOT->ProcessLine(TString::Format(".L %s/common/macro/checkHistAnaKplusXiFemto.C+", pwd));
  checkHistAnaKplusXiFemto(rootFile, anaName, mainconfPath);
}
