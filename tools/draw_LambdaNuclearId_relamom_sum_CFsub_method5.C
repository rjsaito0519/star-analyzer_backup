#include "TCanvas.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLegend.h"
#include "TMath.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TF1.h"
#include "TLine.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"
#include "TMatrixDSym.h"
#include "TPaveText.h"
#include <iostream>

void getPurity(TH1D* hY, double minM, double maxM, double &P, double &err_P, double &N_sig, double &err_N_sig, double &N_tot, double &err_N_tot) {
    if (!hY || hY->GetEntries() < 10) {
        P = 0; err_P = 0; N_sig = 0; err_N_sig = 0; N_tot = 0; err_N_tot = 0;
        return;
    }
    
    TF1 *fTotal = new TF1("fTotal", "gaus(0) + pol2(3)", 1.08, 1.15);
    fTotal->SetParameters(hY->GetMaximum(), 1.11596, 0.003, 0, 0, 0);
    fTotal->SetParLimits(1, 1.11, 1.12);
    fTotal->SetParLimits(2, 0.001, 0.008);
    
    TFitResultPtr r = hY->Fit(fTotal, "S Q N", "", 1.08, 1.15);
    
    int binMin = hY->FindBin(minM);
    int binMax = hY->FindBin(maxM);
    double exactMinM = hY->GetBinLowEdge(binMin);
    double exactMaxM = hY->GetBinLowEdge(binMax) + hY->GetBinWidth(binMax);
    
    N_tot = hY->IntegralAndError(binMin, binMax, err_N_tot);
    
    if (r->IsValid() && r->Status() == 0) {
        TF1 *fBg = new TF1("fBg", "pol2", 1.08, 1.15);
        fBg->SetParameters(fTotal->GetParameter(3), fTotal->GetParameter(4), fTotal->GetParameter(5));
        
        double covBg[9];
        TMatrixDSym cov = r->GetCovarianceMatrix();
        covBg[0] = cov(3,3); covBg[1] = cov(3,4); covBg[2] = cov(3,5);
        covBg[3] = cov(4,3); covBg[4] = cov(4,4); covBg[5] = cov(4,5);
        covBg[6] = cov(5,3); covBg[7] = cov(5,4); covBg[8] = cov(5,5);
        
        double binW = hY->GetBinWidth(binMin);
        double N_bg = fBg->Integral(exactMinM, exactMaxM) / binW;
        double err_N_bg = fBg->IntegralError(exactMinM, exactMaxM, fBg->GetParameters(), covBg) / binW;
        
        N_sig = N_tot - N_bg;
        err_N_sig = TMath::Sqrt(err_N_tot*err_N_tot + err_N_bg*err_N_bg);
        
        if (N_tot > 0) {
            P = N_sig / N_tot;
            if (N_bg > 0 && N_tot > 0) {
                err_P = (N_bg / N_tot) * TMath::Sqrt( TMath::Power(err_N_bg/N_bg, 2) + TMath::Power(err_N_tot/N_tot, 2) );
            } else {
                err_P = 0;
            }
        } else {
            P = 0; err_P = 0;
        }
        delete fBg;
    } else {
        P = 0; err_P = 0; N_sig = 0; err_N_sig = 0;
    }
    delete fTotal;
}

void draw_LambdaNuclearId_relamom_sum_CFsub_method5() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);

    TString inputFile = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau13p5_anaLambdaNuclearId/auau13p5_anaLambdaNuclearId_all_LambdaImprove_WideSide_2D.root";
    TFile *fIn = TFile::Open(inputFile, "READ");
    if (!fIn || fIn->IsZombie()) {
        std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
        return;
    }

    TString outDir = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau13p5_anaLambdaNuclearId/pdf";
    gSystem->mkdir(outDir, kTRUE);
    
    const char *species[] = {"d", "t", "3He", "4He"};
    int nSpecies = 4;
    
    double meanLambda = 1.11596;
    double sigmaLambda = 2.96e-03;
    double minM = meanLambda - 3.0 * sigmaLambda;
    double maxM = meanLambda + 3.0 * sigmaLambda;

    TDirectory *dirNuc = (TDirectory *)fIn->Get("nuclearid");
    TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

    for (int i = 0; i < nSpecies; ++i) {
        TString sp = species[i];
        
        int nRebin = 1;
        if (sp == "t" || sp == "3He") nRebin = 4;
        else if (sp == "4He") nRebin = 10;
        
        TString outFile = Form("%s/kstar_sum_CFsub_method5_%s.pdf", outDir.Data(), sp.Data());
        
        TCanvas *c_start = new TCanvas("c_start");
        c_start->Print(outFile + "[");

        for (int groupSet = 0; groupSet < 2; ++groupSet) {
            int nGroups = (groupSet == 0) ? 2 : 3;

            TCanvas *cCF = new TCanvas(Form("cCF_%s_groupSet%d", sp.Data(), groupSet), Form("CF(k*) - %s", sp.Data()), 1200, 600);
            cCF->Divide(nGroups, 1);
            
            TCanvas *cPurity = new TCanvas(Form("cPurity_%s_groupSet%d", sp.Data(), groupSet), Form("Purity(k*) - %s", sp.Data()), 1200, 600);
            cPurity->Divide(nGroups, 1);
            
            TCanvas *cNSig = new TCanvas(Form("cNSig_%s_groupSet%d", sp.Data(), groupSet), Form("NSig(k*) - %s", sp.Data()), 1200, 600);
            cNSig->Divide(nGroups, 1);

            for (int group = 0; group < nGroups; ++group) {
                TString centLabel;
                int jStart, jEnd;
                if (groupSet == 0) {
                    centLabel = (group == 0) ? "20-80%" : "0-20%";
                    jStart = (group == 0) ? 0 : 6;
                    jEnd = (group == 0) ? 5 : 8;
                } else {
                    if (group == 0) { centLabel = "0-10%"; jStart = 7; jEnd = 8; }
                    else if (group == 1) { centLabel = "10-20%"; jStart = 6; jEnd = 6; }
                    else if (group == 2) { centLabel = "20-80%"; jStart = 0; jEnd = 5; }
                }

                TH2D *h2True_sum = 0;
                TH2D *h2Mix_sum = 0;

                for (int j = jStart; j <= jEnd; ++j) {
                    TString hNameTrue = Form("hKstarMass_%s_CentBin%d", sp.Data(), j);
                    TString hNameMix = Form("hKstarMass_Mixed_%s_CentBin%d", sp.Data(), j);
                    
                    TH2D *h2True = 0;
                    TH2D *h2Mix = 0;
                    
                    if (dirNuc) h2True = (TH2D*)dirNuc->Get(hNameTrue);
                    if (dirMix) h2Mix = (TH2D*)dirMix->Get(hNameMix);
                    
                    if (!h2True) h2True = (TH2D*)fIn->Get(hNameTrue);
                    if (!h2Mix) h2Mix = (TH2D*)fIn->Get(hNameMix);

                    if (h2True) {
                        if (!h2True_sum) {
                            h2True_sum = (TH2D*)h2True->Clone(Form("h2True_sum_%s_GS%d_G%d", sp.Data(), groupSet, group));
                        } else {
                            h2True_sum->Add(h2True);
                        }
                    }
                    if (h2Mix) {
                        if (!h2Mix_sum) {
                            h2Mix_sum = (TH2D*)h2Mix->Clone(Form("h2Mix_sum_%s_GS%d_G%d", sp.Data(), groupSet, group));
                        } else {
                            h2Mix_sum->Add(h2Mix);
                        }
                    }
                }

                if (!h2True_sum || !h2Mix_sum) continue;
                
                h2True_sum->RebinX(nRebin);
                h2Mix_sum->RebinX(nRebin);
                
                int nBinsX = h2True_sum->GetNbinsX();
                
                TH1D *hCF = new TH1D(Form("hCF_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s CF;k* (GeV/c);CF", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                TH1D *hPTrue = new TH1D(Form("hPTrue_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s Purity;k* (GeV/c);Purity", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                TH1D *hPMix = new TH1D(Form("hPMix_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s Purity;k* (GeV/c);Purity", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                
                TH1D *hSigTrue = new TH1D(Form("hSigTrue_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s N_{sig};k* (GeV/c);Counts", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                TH1D *hSigMix = new TH1D(Form("hSigMix_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s N_{sig};k* (GeV/c);Counts", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                
                int W = 2; // Sliding window half-width (merges 2*W + 1 bins)
                
                for (int bx = 1; bx <= nBinsX; ++bx) {
                    int bx_start = TMath::Max(1, bx - W);
                    int bx_end = TMath::Min(nBinsX, bx + W);
                    
                    // Sliding window projection for Purity
                    TH1D *hYTrue_window = h2True_sum->ProjectionY(Form("hYTrue_w_GS%d_G%d_B%d", groupSet, group, bx), bx_start, bx_end);
                    TH1D *hYMix_window = h2Mix_sum->ProjectionY(Form("hYMix_w_GS%d_G%d_B%d", groupSet, group, bx), bx_start, bx_end);
                    
                    double Pt, ePt, Nsigt_w, eNsigt_w, Ntott_w, eNtott_w;
                    double Pm, ePm, Nsigm_w, eNsigm_w, Ntotm_w, eNtotm_w;
                    
                    getPurity(hYTrue_window, minM, maxM, Pt, ePt, Nsigt_w, eNsigt_w, Ntott_w, eNtott_w);
                    getPurity(hYMix_window, minM, maxM, Pm, ePm, Nsigm_w, eNsigm_w, Ntotm_w, eNtotm_w);
                    
                    // Fallback to 0 if fit failed horribly
                    if (Pt < 0 || Pt > 1) { Pt = 0; ePt = 0; }
                    if (Pm < 0 || Pm > 1) { Pm = 0; ePm = 0; }
                    
                    hPTrue->SetBinContent(bx, Pt);
                    hPTrue->SetBinError(bx, ePt);
                    hPMix->SetBinContent(bx, Pm);
                    hPMix->SetBinError(bx, ePm);
                    
                    delete hYTrue_window;
                    delete hYMix_window;
                    
                    // Single fine bin projection for N_tot
                    TH1D *hYTrue = h2True_sum->ProjectionY(Form("hYTrue_GS%d_G%d_B%d", groupSet, group, bx), bx, bx);
                    TH1D *hYMix = h2Mix_sum->ProjectionY(Form("hYMix_GS%d_G%d_B%d", groupSet, group, bx), bx, bx);
                    
                    int binMin = hYTrue->FindBin(minM);
                    int binMax = hYTrue->FindBin(maxM);
                    
                    double err_Ntott;
                    double Ntott = hYTrue->IntegralAndError(binMin, binMax, err_Ntott);
                    
                    double err_Ntotm;
                    double Ntotm = hYMix->IntegralAndError(binMin, binMax, err_Ntotm);
                    
                    double N_sig_t = Ntott * Pt;
                    double err_N_sig_t = TMath::Sqrt(TMath::Power(err_Ntott * Pt, 2) + TMath::Power(Ntott * ePt, 2));
                    
                    double N_sig_m = Ntotm * Pm;
                    double err_N_sig_m = TMath::Sqrt(TMath::Power(err_Ntotm * Pm, 2) + TMath::Power(Ntotm * ePm, 2));
                    
                    hSigTrue->SetBinContent(bx, N_sig_t);
                    hSigTrue->SetBinError(bx, err_N_sig_t);
                    hSigMix->SetBinContent(bx, N_sig_m);
                    hSigMix->SetBinError(bx, err_N_sig_m);
                    
                    delete hYTrue;
                    delete hYMix;
                }
                
                int bin1 = hSigTrue->FindBin(0.40001);
                int bin2 = hSigTrue->FindBin(0.59999);
                double intTrue = hSigTrue->Integral(bin1, bin2);
                double intMix = hSigMix->Integral(bin1, bin2);
                
                if (intMix > 0) {
                    hSigMix->Scale(intTrue / intMix);
                }
                
                for (int bx = 1; bx <= nBinsX; ++bx) {
                    double nt = hSigTrue->GetBinContent(bx);
                    double et = hSigTrue->GetBinError(bx);
                    double nm = hSigMix->GetBinContent(bx);
                    double em = hSigMix->GetBinError(bx);
                    
                    if (nm > 0) {
                        double cf = nt / nm;
                        double err_cf = cf * TMath::Sqrt( TMath::Power(et/nt, 2) + TMath::Power(em/nm, 2) );
                        hCF->SetBinContent(bx, cf);
                        hCF->SetBinError(bx, err_cf);
                    }
                }
                
                cNSig->cd(group + 1);
                hSigTrue->SetTitle(Form("%s (%s) N_{sig}", sp.Data(), centLabel.Data()));
                hSigTrue->SetMarkerStyle(20);
                hSigTrue->SetMarkerColor(kRed);
                hSigTrue->SetLineColor(kRed);
                hSigTrue->GetXaxis()->SetRangeUser(0.0, 0.6);
                double maxH = hSigTrue->GetMaximum();
                double maxMh = hSigMix->GetMaximum();
                hSigTrue->SetMaximum(TMath::Max(maxH, maxMh) * 1.2);
                hSigTrue->SetMinimum(0);
                hSigTrue->Draw("E1");
                
                hSigMix->SetMarkerStyle(24);
                hSigMix->SetMarkerColor(kBlue);
                hSigMix->SetLineColor(kBlue);
                hSigMix->Draw("E1 SAME");
                
                if (group == 0) {
                    TLegend *legSig = new TLegend(0.4, 0.75, 0.88, 0.88);
                    legSig->AddEntry(hSigTrue, "True N_{sig}", "pl");
                    legSig->AddEntry(hSigMix, "Mixed N_{sig} (Scaled)", "pl");
                    legSig->Draw();
                }
                
                cPurity->cd(group + 1);
                hPTrue->SetTitle(Form("%s (%s) Purity", sp.Data(), centLabel.Data()));
                hPTrue->SetMarkerStyle(20);
                hPTrue->SetMarkerColor(kRed);
                hPTrue->SetLineColor(kRed);
                hPTrue->SetMinimum(0.0);
                hPTrue->SetMaximum(1.2);
                hPTrue->GetXaxis()->SetRangeUser(0.0, 0.6);
                hPTrue->Draw("E1");
                
                hPMix->SetMarkerStyle(24);
                hPMix->SetMarkerColor(kBlue);
                hPMix->SetLineColor(kBlue);
                hPMix->Draw("E1 SAME");
                
                if (group == 0) {
                    TLegend *legP = new TLegend(0.4, 0.75, 0.88, 0.88);
                    legP->AddEntry(hPTrue, "True Purity", "pl");
                    legP->AddEntry(hPMix, "Mixed Purity", "pl");
                    legP->Draw();
                }
                
                cCF->cd(group + 1);
                hCF->SetTitle(Form("%s (%s) CF", sp.Data(), centLabel.Data()));
                hCF->SetMarkerStyle(20);
                hCF->SetMarkerColor(kBlack);
                hCF->SetLineColor(kBlack);
                hCF->SetMinimum(0.0);
                hCF->SetMaximum(3.0);
                hCF->GetXaxis()->SetRangeUser(0.0, 0.6);
                hCF->Draw("E1");
                
                TLine *l1 = new TLine(0.0, 1.0, 0.6, 1.0);
                l1->SetLineStyle(2);
                l1->Draw("SAME");
            } // end group loop
            
            cNSig->Print(outFile);
            cPurity->Print(outFile);
            cCF->Print(outFile);
        } // end groupSet loop
        
        TCanvas *c_end = new TCanvas("c_end");
        c_end->Print(outFile + "]");
        
        std::cout << "Successfully created " << outFile << std::endl;
    } // end species loop
}
