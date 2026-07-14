void draw_multi() {
    // 開く対象のROOTファイル
    TString fileName = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau7p3_anaNuclearId/auau7p3_anaNuclearId_all.root";
    TFile *file = TFile::Open(fileName);
    
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open file " << fileName << std::endl;
        return;
    }

    // キャンバスを作成
    TCanvas *c1 = new TCanvas("c1", "Multiplicity", 1200, 1200);
    
    // PDFを開く（複数ページの保存を開始）
    TString outPdf = "rootfile/auau7p3_anaNuclearId/pdf/draw_multi.pdf";
    c1->Print(outPdf + "[");

    // 描画する対象のリスト
    const char* speciesList[] = {"d", "t", "3He", "4He"};
    int nSpecies = 4;

    for (int s = 0; s < nSpecies; ++s) {
        TString sp = speciesList[s];
        
        c1->Clear();
        c1->Divide(3, 3);
        
        for (int i = 0; i < 9; ++i) {
            c1->cd(i + 1);
            gPad->SetLogy(1); // Y軸（カウント数）をログスケールにする
            
            // ヒストグラム名を作成
            TString histName = TString::Format("hMult_%s_CentBin%d", sp.Data(), i);
            
            // ヒストグラムを取得して描画
            TH1D *h = (TH1D*)file->Get(histName);
            if (h) {
                TString title = h->GetTitle();
                title.ReplaceAll("%%", "%");
                TString duplicateStr = TString::Format("%s %s ", sp.Data(), sp.Data());
                if (title.BeginsWith(duplicateStr)) {
                    title.Replace(0, duplicateStr.Length(), TString::Format("%s ", sp.Data()));
                }
                h->SetTitle(title.Data());
                h->Draw();
            } else {
                std::cerr << "Warning: Histogram " << histName << " not found!" << std::endl;
            }
        }
        
        // 1ページ分としてPDFに保存
        c1->Print(outPdf);
    }

    // PDFを閉じる
    c1->Print(outPdf + "]");
}
