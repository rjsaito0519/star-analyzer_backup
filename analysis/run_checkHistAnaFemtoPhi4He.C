// run_checkHistAnaFemtoPhi4He.C - Load lib and compile checkHist macro
// Usage: root4star -b -q 'run_checkHistAnaFemtoPhi4He.C("rootfile/...","anaName","config/mainconf/...")'

void run_checkHistAnaFemtoPhi4He(const Char_t* rootFile,
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

  gROOT->ProcessLine(TString::Format(".L %s/common/macro/checkHistAnaFemtoPhi4He.C+", pwd));
  checkHistAnaFemtoPhi4He(rootFile, anaName, mainconfPath);
}
