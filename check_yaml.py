import yaml

with open('/star/u/yichikawa/pwg/git_STAR_3/star-analyzer/config/hist/hist_auau3p85_anaLambdaNuclearId.yaml') as f:
    doc = yaml.safe_load(f)

hists = doc.get('histograms', {})
# Check hKstar_d
for name in ['hKstar_d', 'hKstar_d_CentBin1', 'hKstar_t_SBNeg', 'hKstarMass_d_CentBin6']:
    if name in hists:
        print(f'FOUND {name}: {hists[name]}')
    else:
        print(f'MISSING {name}')
print('Total histograms:', len(hists))

# Show some hKstar related histograms
kstar_hists = [k for k in hists if 'Kstar' in k]
print(f'\nhKstar-related histograms ({len(kstar_hists)}):')
for k in sorted(kstar_hists)[:20]:
    print(f'  {k}: {hists[k]}')
