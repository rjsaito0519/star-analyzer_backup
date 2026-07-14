import sys

def append_mult_to_yaml(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    if "hMult_d:" in content:
        print(f"Already updated {filepath}")
        return

    particles = ["d", "t", "3He", "4He"]
    percents = [
        "70-80%", "60-70%", "50-60%", "40-50%", "30-40%", "20-30%", "10-20%", "5-10%", "0-5%"
    ]

    additions = ""
    for sp in particles:
        additions += f"""  hMult_{sp}:
    axis: {{ nBins: 20, min: -0.5, max: 19.5 }}
    title: "{sp} multiplicity per event; multiplicity; counts"
"""
        for i, perc in enumerate(percents):
            additions += f"""  hMult_{sp}_CentBin{i}:
    axis: {{ nBins: 20, min: -0.5, max: 19.5 }}
    title: "{sp} multiplicity per event cent9={i} ({perc}); multiplicity; counts"
"""
    with open(filepath, 'a') as f:
        f.write("\n" + additions)
    print(f"Updated {filepath}")

for f in ["config/hist/hist_anaNuclearId.yaml", "config/hist/hist_auau13p5_anaNuclearId.yaml"]:
    append_mult_to_yaml(f)
