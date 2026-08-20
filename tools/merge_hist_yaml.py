import yaml
import sys

def merge_yaml(source_file, target_file):
    with open(source_file, 'r') as f_src:
        src = yaml.safe_load(f_src)
    
    with open(target_file, 'r') as f_tgt:
        tgt = yaml.safe_load(f_tgt)
        
    # Merge axes
    if 'axes' in src and 'axes' in tgt:
        for k, v in src['axes'].items():
            if k not in tgt['axes']:
                tgt['axes'][k] = v
                
    # Merge histograms
    if 'histograms' in src and 'histograms' in tgt:
        for k, v in src['histograms'].items():
            if k not in tgt['histograms']:
                tgt['histograms'][k] = v

    # Write back to target
    with open(target_file, 'w') as f_out:
        yaml.dump(tgt, f_out, sort_keys=False, default_flow_style=False)
        print(f"Merged {source_file} into {target_file}")

if __name__ == '__main__':
    targets = [
        'config/hist/hist_auau3p85_anaFemtoLambda_d.yaml',
        'config/hist/hist_auau3p85_anaFemtoLambda_t.yaml',
        'config/hist/hist_auau3p85_anaFemtoLambda_3He.yaml',
        'config/hist/hist_auau3p85_anaFemtoLambda_4He.yaml',
        'config/hist/hist_anaFemtoLambda.yaml'
    ]
    
    source = 'config/hist/hist_auau3p85_anaLambda.yaml'
    
    for t in targets:
        merge_yaml(source, t)
