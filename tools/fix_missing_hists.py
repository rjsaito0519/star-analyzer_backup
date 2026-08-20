import glob
import yaml

files = glob.glob("config/hist/hist_auau3p85_anaFemtoLambda_*.yaml")
files.append("config/hist/hist_anaFemtoLambda.yaml")

for f in files:
    with open(f, 'r') as file:
        try:
            data = yaml.safe_load(file)
        except yaml.composer.ComposerError:
            # It already has the bad append. Let's read lines, strip the bad append.
            with open(f, 'r') as file_r:
                lines = file_r.readlines()
            
            # Remove from hNLambda_vs_Cent9 onwards
            new_lines = []
            for line in lines:
                if line.startswith("  hNLambda_vs_Cent9:"):
                    break
                new_lines.append(line)
            
            with open(f, 'w') as file_w:
                file_w.writelines(new_lines)
            
            with open(f, 'r') as file_r:
                data = yaml.safe_load(file_r)
    
    if data is None:
        continue

    if 'histograms' not in data:
        data['histograms'] = {}

    # Define histograms inline without aliases
    data['histograms']['hNLambda_vs_Cent9'] = {
        'xAxis': data['axes']['Cent9'],
        'yAxis': data['axes']['NLambdaPairs'],
        'title': "Number of Lambda candidates vs Centrality; Centrality Bin; Number of Lambda Candidates",
        'type': 'TH2F'
    }
    
    data['histograms']['hNNuclear_vs_Cent9'] = {
        'xAxis': data['axes']['Cent9'],
        'yAxis': data['axes']['NCand'],
        'title': "Number of Nuclear candidates vs Centrality; Centrality Bin; Number of Nuclear Candidates",
        'type': 'TH2F'
    }

    with open(f, 'w') as file:
        yaml.dump(data, file, default_flow_style=False, sort_keys=False)
    
    print("Fixed " + f)
