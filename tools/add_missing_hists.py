import glob

files = glob.glob("config/hist/hist_auau3p85_anaFemtoLambda_*.yaml")
files.append("config/hist/hist_anaFemtoLambda.yaml")

append_text = """
  hNLambda_vs_Cent9:
    xAxis: *Cent9
    yAxis: *NLambdaPairs
    title: "Number of Lambda candidates vs Centrality; Centrality Bin; Number of Lambda Candidates"
    type: TH2F
  hNNuclear_vs_Cent9:
    xAxis: *Cent9
    yAxis: *NCand
    title: "Number of Nuclear candidates vs Centrality; Centrality Bin; Number of Nuclear Candidates"
    type: TH2F
"""

for f in files:
    with open(f, 'a') as file:
        file.write(append_text)
    print("Appended to " + f)
