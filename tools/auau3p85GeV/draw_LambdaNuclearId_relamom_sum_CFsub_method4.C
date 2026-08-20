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

void draw_LambdaNuclearId_relamom_sum_CFsub_method4() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gStyle->SetOptFit(111);

    TString inputFile = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau3p85_anaLambdaNuclearId/auau3p85_all1.root";
    TFile *fIn = TFile::Open(inputFile, "READ");
    if (!fIn || fIn->IsZombie()) {
        std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
        return;
    }

    TString outDir = "/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/rootfile/auau3p85_anaLambdaNuclearId/pdf";
    gSystem->mkdir(outDir, kTRUE);
    
    const char *species[] = {"d", "t", "3He", "4He"};
    int nSpecies = 4;
    
    double meanLambda = 1.11596;
    double sigmaLambda = 2.96e-03;
    double minM = meanLambda - 3.0 * sigmaLambda;
    double maxM = meanLambda + 3.0 * sigmaLambda;

    TDirectory *dirNuc = (TDirectory *)fIn->Get("true");
    TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

    for (int i = 0; i < nSpecies; ++i) {
        TString sp = species[i];
        
        int nRebin = 1;
        if (sp == "t" || sp == "3He") nRebin = 4;
        else if (sp == "4He") nRebin = 10;
        
        TString outFile = Form("%s/kstar_sum_CFsub_method4_%s.pdf", outDir.Data(), sp.Data());
        
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
                
                // Coarse binning for Purity fit
                int nPurityRebin = 4;
                TH2D *h2True_purity = (TH2D*)h2True_sum->RebinX(nPurityRebin, Form("%s_purity", h2True_sum->GetName()));
                TH2D *h2Mix_purity = (TH2D*)h2Mix_sum->RebinX(nPurityRebin, Form("%s_purity", h2Mix_sum->GetName()));
                
                int nBinsX_coarse = h2True_purity->GetNbinsX();
                TH1D *hPTrue_coarse = new TH1D(Form("hPTrue_c_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s Coarse Purity", sp.Data(), centLabel.Data()), nBinsX_coarse, h2True_purity->GetXaxis()->GetXmin(), h2True_purity->GetXaxis()->GetXmax());
                TH1D *hPMix_coarse = new TH1D(Form("hPMix_c_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s Coarse Purity", sp.Data(), centLabel.Data()), nBinsX_coarse, h2True_purity->GetXaxis()->GetXmin(), h2True_purity->GetXaxis()->GetXmax());
                
                for (int bc = 1; bc <= nBinsX_coarse; ++bc) {
                    TH1D *hYTrue_c = h2True_purity->ProjectionY(Form("hYTrue_c_B%d", bc), bc, bc);
                    TH1D *hYMix_c = h2Mix_purity->ProjectionY(Form("hYMix_c_B%d", bc), bc, bc);
                    
                    double Pt, ePt, Nsigt, eNsigt, Ntott, eNtott;
                    double Pm, ePm, Nsigm, eNsigm, Ntotm, eNtotm;
                    
                    getPurity(hYTrue_c, minM, maxM, Pt, ePt, Nsigt, eNsigt, Ntott, eNtott);
                    getPurity(hYMix_c, minM, maxM, Pm, ePm, Nsigm, eNsigm, Ntotm, eNtotm);
                    
                    if (Pt > 0 && ePt < 0.2) { // Exclude very bad fits
                        hPTrue_coarse->SetBinContent(bc, Pt);
                        hPTrue_coarse->SetBinError(bc, ePt);
                    }
                    if (Pm > 0 && ePm < 0.2) {
                        hPMix_coarse->SetBinContent(bc, Pm);
                        hPMix_coarse->SetBinError(bc, ePm);
                    }
                    
                    delete hYTrue_c;
                    delete hYMix_c;
                }
                
                TF1 *fPTrue = new TF1(Form("fPTrue_%s_GS%d_G%d", sp.Data(), groupSet, group), "pol1", 0.0, 0.6);
                hPTrue_coarse->Fit(fPTrue, "Q 0", "", 0.0, 0.45);
                
                TF1 *fPMix = new TF1(Form("fPMix_%s_GS%d_G%d", sp.Data(), groupSet, group), "pol1", 0.0, 0.6);
                hPMix_coarse->Fit(fPMix, "Q 0", "", 0.0, 0.45);
                
                int nBinsX = h2True_sum->GetNbinsX();
                
                TH1D *hCF = new TH1D(Form("hCF_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s CF;k* (GeV/c);CF", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                
                TH1D *hSigTrue = new TH1D(Form("hSigTrue_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s N_{sig};k* (GeV/c);Counts", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                TH1D *hSigMix = new TH1D(Form("hSigMix_%s_GS%d_G%d", sp.Data(), groupSet, group), Form("%s %s N_{sig};k* (GeV/c);Counts", sp.Data(), centLabel.Data()), nBinsX, h2True_sum->GetXaxis()->GetXmin(), h2True_sum->GetXaxis()->GetXmax());
                
                for (int bx = 1; bx <= nBinsX; ++bx) {
                    double kstar = h2True_sum->GetXaxis()->GetBinCenter(bx);
                    
                    double P_t = fPTrue->Eval(kstar);
                    double P_m = fPMix->Eval(kstar);
                    
                    // Prevent purity from going wild outside fit range
                    if (P_t < 0.1) P_t = 0.1;
                    if (P_t > 1.0) P_t = 1.0;
                    if (P_m < 0.1) P_m = 0.1;
                    if (P_m > 1.0) P_m = 1.0;
                    
                    TH1D *hYTrue = h2True_sum->ProjectionY(Form("hYTrue_%s_GS%d_G%d_B%d", sp.Data(), groupSet, group, bx), bx, bx);
                    TH1D *hYMix = h2Mix_sum->ProjectionY(Form("hYMix_%s_GS%d_G%d_B%d", sp.Data(), groupSet, group, bx), bx, bx);
                    
                    int binMin = hYTrue->FindBin(minM);
                    int binMax = hYTrue->FindBin(maxM);
                    
                    double err_Ntott;
                    double Ntott = hYTrue->IntegralAndError(binMin, binMax, err_Ntott);
                    
                    double err_Ntotm;
                    double Ntotm = hYMix->IntegralAndError(binMin, binMax, err_Ntotm);
                    
                    double N_sig_t = Ntott * P_t;
                    double err_N_sig_t = err_Ntott * P_t; // ignoring fit parameter errors for simplicity in signal error
                    
                    double N_sig_m = Ntotm * P_m;
                    double err_N_sig_m = err_Ntotm * P_m;
                    
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
                hPTrue_coarse->SetMarkerStyle(20);
                hPTrue_coarse->SetMarkerColor(kRed);
                hPTrue_coarse->SetLineColor(kRed);
                hPTrue_coarse->SetMinimum(0.0);
                hPTrue_coarse->SetMaximum(1.2);
                hPTrue_coarse->GetXaxis()->SetRangeUser(0.0, 0.6);
                hPTrue_coarse->Draw("E1");
                fPTrue->SetLineColor(kRed);
                fPTrue->Draw("SAME");
                
                hPMix_coarse->SetMarkerStyle(24);
                hPMix_coarse->SetMarkerColor(kBlue);
                hPMix_coarse->SetLineColor(kBlue);
                hPMix_coarse->Draw("E1 SAME");
                fPMix->SetLineColor(kBlue);
                fPMix->SetLineStyle(2);
                fPMix->Draw("SAME");
                
                if (group == 0) {
                    TLegend *legP = new TLegend(0.4, 0.75, 0.88, 0.88);
                    legP->AddEntry(hPTrue_coarse, "True Purity (Coarse)", "pl");
                    legP->AddEntry(hPMix_coarse, "Mixed Purity (Coarse)", "pl");
                    legP->AddEntry(fPTrue, "pol1 Fit (True)", "l");
                    legP->AddEntry(fPMix, "pol1 Fit (Mixed)", "l");
                    legP->Draw();
                }
                
                cCF->cd(group + 1);
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
