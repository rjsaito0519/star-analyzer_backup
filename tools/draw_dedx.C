void draw_dedx() {
    // 開く対象のROOTファイル
    TString fileName = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau7p3_anaNuclearId/auau7p3_anaNuclearId_all.root";
    TFile *file = TFile::Open(fileName);
    
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open file " << fileName << std::endl;
        return;
    }

    // キャンバスを作成
    TCanvas *c1 = new TCanvas("c1", "dE/dx vs p", 1200, 1200);
    
    // PDFを開く（複数ページの保存を開始）
    c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_dedx.pdf[");

    // 描画する対象のリスト (空文字はInclusive, それ以外は各粒子)
    const char* speciesList[] = {"", "d", "t", "3He", "4He"};
    int nSpecies = 5;

    for (int s = 0; s < nSpecies; ++s) {
        TString sp = speciesList[s];
        
        c1->Clear();
        c1->Divide(3, 3);
        
        for (int i = 0; i < 9; ++i) {
            c1->cd(i + 1);
            gPad->SetLogz(1); // z軸をログスケールにする
            
            // ヒストグラム名を作成
            TString histName;
            TString speciesLabel = sp;
            if (sp == "") {
                histName = TString::Format("hDedxP_CentBin%d", i);
                speciesLabel = "Inclusive";
            } else {
                histName = TString::Format("hDedxP_%s_CentBin%d", sp.Data(), i);
            }
            
            // ヒストグラムを取得して描画 (2次元ヒストグラムを想定し COLZ オプションを使用)
            TH2 *h = (TH2*)file->Get(histName);
            if (h) {
                TString title = h->GetTitle();
                title.ReplaceAll("%%", "%");
                h->SetTitle(TString::Format("%s %s", speciesLabel.Data(), title.Data()));
                h->Draw("COLZ");
            } else {
                std::cerr << "Warning: Histogram " << histName << " not found!" << std::endl;
            }
        }
        
        // 1ページ分としてPDFに保存
        c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_dedx.pdf");
    }

    // PDFを閉じる
    c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_dedx.pdf]");
}
