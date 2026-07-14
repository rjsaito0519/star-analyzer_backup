import re

with open('tools/draw_LambdaNuclearId_relamom_sum.C', 'r') as f:
    code = f.read()

# Replace the canvas definitions and loop start
old_canvas = r"""    // 2x1のキャンバスを作成 \(2つのCentrality Group用\)
    TCanvas \*c1 = new TCanvas\(Form\("c1_%s", sp.Data\(\)\),
                              Form\("k\* for Lambda - %s", sp.Data\(\)\), 1200, 600\);
    c1->Divide\(2, 1\);

    TCanvas \*c2 = new TCanvas\(
        Form\("c2_%s", sp.Data\(\)\),
        Form\("Correlation Function for Lambda - %s", sp.Data\(\)\), 1200, 1200\);
    c2->Divide\(2, 2\);

    TCanvas \*c3 = new TCanvas\(Form\("c3_%s", sp.Data\(\)\),
                              Form\("True Sidebands for Lambda - %s", sp.Data\(\)\),
                              1200, 600\);
    c3->Divide\(2, 1\);

    TCanvas \*c4 = new TCanvas\(
        Form\("c4_%s", sp.Data\(\)\),
        Form\("Mixed Sidebands for Lambda - %s", sp.Data\(\)\), 1200, 600\);
    c4->Divide\(2, 1\);

    TCanvas \*c5 = new TCanvas\(Form\("c5_%s", sp.Data\(\)\),
                              Form\("SB Subtracted for Lambda - %s", sp.Data\(\)\),
                              1200, 600\);
    c5->Divide\(2, 1\);

    TCanvas \*c6 = new TCanvas\(
        Form\("c6_%s", sp.Data\(\)\),
        Form\("CF \(SB Subtracted\) for Lambda - %s", sp.Data\(\)\), 1200, 1200\);
    c6->Divide\(2, 2\);

    TDirectory \*dirNuc = \(TDirectory \*\)fIn->Get\("true"\);
    TDirectory \*dirMix = \(TDirectory \*\)fIn->Get\("mix"\);

    for \(int group = 0; group < 2; \+\+group\) \{
      c1->cd\(group \+ 1\);

      TString centLabel = \(group == 0\) \? "30-80%" : "0-20%";
      int jStart = \(group == 0\) \? 0 : 6;
      int jEnd = \(group == 0\) \? 5 : 8;"""

new_canvas = """    TDirectory *dirNuc = (TDirectory *)fIn->Get("true");
    TDirectory *dirMix = (TDirectory *)fIn->Get("mix");

    TString outFile = Form("%s/kstar_sum_%s.pdf", outDir.Data(), sp.Data());

    for (int set = 0; set < 2; ++set) {
      int nGroups = (set == 0) ? 2 : 3;

      TCanvas *c1 = new TCanvas(Form("c1_%s_set%d", sp.Data(), set),
                                Form("k* for Lambda - %s", sp.Data()), 1200, 600);
      c1->Divide(nGroups, 1);

      TCanvas *c2 = new TCanvas(
          Form("c2_%s_set%d", sp.Data(), set),
          Form("Correlation Function for Lambda - %s", sp.Data()), 1200, 1200);
      c2->Divide(nGroups, 2);

      TCanvas *c3 = new TCanvas(Form("c3_%s_set%d", sp.Data(), set),
                                Form("True Sidebands for Lambda - %s", sp.Data()),
                                1200, 600);
      c3->Divide(nGroups, 1);

      TCanvas *c4 = new TCanvas(
          Form("c4_%s_set%d", sp.Data(), set),
          Form("Mixed Sidebands for Lambda - %s", sp.Data()), 1200, 600);
      c4->Divide(nGroups, 1);

      TCanvas *c5 = new TCanvas(Form("c5_%s_set%d", sp.Data(), set),
                                Form("SB Subtracted for Lambda - %s", sp.Data()),
                                1200, 600);
      c5->Divide(nGroups, 1);

      TCanvas *c6 = new TCanvas(
          Form("c6_%s_set%d", sp.Data(), set),
          Form("CF (SB Subtracted) for Lambda - %s", sp.Data()), 1200, 1200);
      c6->Divide(nGroups, 2);

      for (int group = 0; group < nGroups; ++group) {
        c1->cd(group + 1);

        TString centLabel;
        int jStart, jEnd;
        if (set == 0) {
          centLabel = (group == 0) ? "20-80%" : "0-20%";
          jStart = (group == 0) ? 0 : 6;
          jEnd = (group == 0) ? 5 : 8;
        } else {
          if (group == 0) { centLabel = "0-10%"; jStart = 7; jEnd = 8; }
          else if (group == 1) { centLabel = "10-20%"; jStart = 6; jEnd = 6; }
          else { centLabel = "20-60%"; jStart = 2; jEnd = 5; }
        }"""

code = re.sub(old_canvas, new_canvas, code)

# Fix c2 zoom panel index
code = code.replace("c2->cd(group + 3);", "c2->cd(group + 1 + nGroups);")

# Fix c6 zoom panel index
code = code.replace("c6->cd(group + 3);", "c6->cd(group + 1 + nGroups);")

# Fix PDF printing
old_print = """    // マルチページPDFとして保存
    TString outFile = Form("%s/kstar_sum_%s.pdf", outDir.Data(), sp.Data());
    c1->Print(outFile + "("); // 1ページ目 (Open)
    c2->Print(outFile);       // 2ページ目
    c3->Print(outFile);       // 3ページ目
    c4->Print(outFile);       // 4ページ目
    c5->Print(outFile);       // 5ページ目
    c6->Print(outFile + ")"); // 6ページ目 (Close)

    delete c1;
    delete c2;
    delete c3;
    delete c4;
    delete c5;
    delete c6;
  }"""

new_print = """      // マルチページPDFとして保存
      if (set == 0) {
        c1->Print(outFile + "("); // Open
      } else {
        c1->Print(outFile);
      }
      c2->Print(outFile);
      c3->Print(outFile);
      c4->Print(outFile);
      c5->Print(outFile);
      if (set == 1) {
        c6->Print(outFile + ")"); // Close
      } else {
        c6->Print(outFile);
      }

      delete c1;
      delete c2;
      delete c3;
      delete c4;
      delete c5;
      delete c6;
    }
  }"""

code = re.sub(old_print, new_print, code)

with open('tools/draw_LambdaNuclearId_relamom_sum.C', 'w') as f:
    f.write(code)
EOF
python3 tools/patch.py

