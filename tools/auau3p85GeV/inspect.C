void printKeys(TDirectory* dir, const TString& path) {
    TIter next(dir->GetListOfKeys());
    TKey *key;
    while ((key = (TKey*)next())) {
        TString name = key->GetName();
        if (name.Contains("hKstar")) {
            cout << "Found in " << path << ": " << name << endl;
        }
        if (key->IsFolder()) {
            TDirectory* subdir = (TDirectory*)key->ReadObj();
            printKeys(subdir, path + "/" + name);
        }
    }
}
void inspect() {
    TFile* f = TFile::Open("/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau3p85_anaLambdaNuclearId/auau3p85_all1.root");
    if (!f) return;
    printKeys(f, "");
}
