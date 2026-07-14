void draw_p_pt() {
    // 開く対象のROOTファイル
    TString fileName = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau7p3_anaNuclearId/auau7p3_anaNuclearId_all.root";
    TFile *file = TFile::Open(fileName);
    
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open file " << fileName << std::endl;
        return;
    }

    // キャンバスを作成
    TCanvas *c1 = new TCanvas("c1", "p vs pT", 1200, 1200);
    
    // PDFを開く（複数ページの保存を開始）
    c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_p_pt.pdf[");

    // 描画する対象のリスト (各粒子のみ)
    const char* speciesList[] = {"d", "t", "3He", "4He"};
    int nSpecies = 4;

    for (int s = 0; s < nSpecies; ++s) {
        TString sp = speciesList[s];
        
        c1->Clear();
        c1->Divide(3, 3);
        
        for (int i = 0; i < 9; ++i) {
            c1->cd(i + 1);
            gPad->SetLogz(1); // z軸をログスケールにする
            
            // ヒストグラム名を作成
            TString histName = TString::Format("hPvsPt_%s_CentBin%d", sp.Data(), i);
            
            // ヒストグラムを取得して描画 (2次元ヒストグラムを想定し COLZ オプションを使用)
            TH2 *h = (TH2*)file->Get(histName);
            if (h) {
                TString title = h->GetTitle();
                title.ReplaceAll("%%", "%");
                h->SetTitle(TString::Format("%s %s", sp.Data(), title.Data()));
                h->Draw("COLZ");
            } else {
                std::cerr << "Warning: Histogram " << histName << " not found!" << std::endl;
            }
        }
        
        // 1ページ分としてPDFに保存
        c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_p_pt.pdf");
        
        // --- 1D Projection (Pt) のページを追加 ---
        c1->Clear();
        c1->Divide(3, 3);
        for (int i = 0; i < 9; ++i) {
            c1->cd(i + 1);
            gPad->SetLogy(1); // 1DヒストのY軸（カウント数）をログスケールにする
            
            TString histName = TString::Format("hPvsPt_%s_CentBin%d", sp.Data(), i);
            TH2 *h2 = (TH2*)file->Get(histName);
            if (h2) {
                TString projName = TString::Format("hPt_%s_CentBin%d", sp.Data(), i);
                double pMaxCut = (sp == "3He" || sp == "4He") ? 4.9999 : 2.4999;
                double pMaxLabel = (sp == "3He" || sp == "4He") ? 5.0 : 2.5;
                // X軸 (p) が指定値以下の範囲のみ射影する
                int maxXbin = h2->GetXaxis()->FindBin(pMaxCut); 
                TH1D *h1 = h2->ProjectionY(projName, 1, maxXbin);
                
                TString title = h2->GetTitle();
                title.ReplaceAll("%%", "%");
                h1->SetTitle(TString::Format("%s %s (Pt Proj. p < %.1f GeV/c)", sp.Data(), title.Data(), pMaxLabel));
                h1->Draw();
            }
        }
        // 1ページ分としてPDFに保存 (1D Projection)
        c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_p_pt.pdf");
    }

    // PDFを閉じる
    c1->Print("rootfile/auau7p3_anaNuclearId/pdf/draw_p_pt.pdf]");
}
