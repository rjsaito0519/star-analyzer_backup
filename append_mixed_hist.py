import sys

species_list = ['d', 't', '3He', '4He']
cent_labels = ['70-80', '60-70', '50-60', '40-50', '30-40', '20-30', '10-20', '5-10', '0-5']

lines = []
lines.append('')
lines.append('  # --- k* and q_lab Mixed, inclusive ---')
for sp in species_list:
    lines.append('  hKstar_Mixed_%s:' % sp)
    lines.append('    axis: *Kstar')
    lines.append('    title: "Relative momentum k* in pair rest frame (Lambda - %s, Mixed);k* (GeV/c);Counts"' % sp)
    lines.append('  hQlab_Mixed_%s:' % sp)
    lines.append('    axis: *Qlab')
    lines.append('    title: "Relative momentum q_lab in lab frame (Lambda - %s, Mixed);q_lab (GeV/c);Counts"' % sp)

lines.append('')
lines.append('  # --- k* Mixed, per-centrality ---')
for sp in species_list:
    for c, pct in enumerate(cent_labels):
        lines.append('  hKstar_Mixed_%s_CentBin%d:' % (sp, c))
        lines.append('    axis: *Kstar')
        lines.append('    title: "k* (Lambda - %s, Mixed) cent9=%d (%s%%%%);k* (GeV/c);Counts"' % (sp, c, pct))

lines.append('')
lines.append('  # --- k* and q_lab Mixed Sideband (SBPos / SBNeg), inclusive ---')
for sp in species_list:
    for sb, sb_label in [('SBPos', 'pos. sideband'), ('SBNeg', 'neg. sideband')]:
        lines.append('  hKstar_Mixed_%s_%s:' % (sp, sb))
        lines.append('    axis: *Kstar')
        lines.append('    title: "k* (Lambda - %s, Mixed, %s);k* (GeV/c);Counts"' % (sp, sb_label))
        lines.append('  hQlab_Mixed_%s_%s:' % (sp, sb))
        lines.append('    axis: *Qlab')
        lines.append('    title: "q_lab (Lambda - %s, Mixed, %s);q_lab (GeV/c);Counts"' % (sp, sb_label))

lines.append('')
for sb, sb_label in [('SBPos', 'pos. SB'), ('SBNeg', 'neg. SB')]:
    lines.append('  # --- k* Mixed Sideband (%s), per-centrality ---' % sb)
    for sp in species_list:
        for c, pct in enumerate(cent_labels):
            lines.append('  hKstar_Mixed_%s_%s_CentBin%d:' % (sp, sb, c))
            lines.append('    axis: *Kstar')
            lines.append('    title: "k* (Lambda - %s, Mixed, %s) cent9=%d (%s%%%%);k* (GeV/c);Counts"' % (sp, sb_label, c, pct))
    lines.append('')

with open('config/hist/hist_auau19_anaLambdaNuclearId.yaml', 'a') as f:
    f.write('\n'.join(lines) + '\n')

print("Successfully appended Mixed histograms to config/hist/hist_auau19_anaLambdaNuclearId.yaml")
