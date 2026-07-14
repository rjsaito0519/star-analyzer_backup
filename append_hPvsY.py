import sys

def append_to_yaml(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    if "hPvsY_d_CentBin0:" in content:
        print(f"Already updated {filepath}")
        return

    particles = ["d", "t", "3He", "4He"]
    percents = [
        "70-80%", "60-70%", "50-60%", "40-50%", "30-40%", "20-30%", "10-20%", "5-10%", "0-5%"
    ]

    additions = ""
    for sp in particles:
        for i, perc in enumerate(percents):
            additions += f"""  hPvsY_{sp}_CentBin{i}:
    xAxis: *P
    yAxis: *Rapidity
    title: "p vs y (2#sigma) cent9={i} ({perc}); p (GeV/c); y"
"""
    with open(filepath, 'a') as f:
        f.write("\n" + additions)
    print(f"Updated {filepath}")

for f in ["config/hist/hist_anaNuclearId.yaml", "config/hist/hist_auau13p5_anaNuclearId.yaml"]:
    append_to_yaml(f)
