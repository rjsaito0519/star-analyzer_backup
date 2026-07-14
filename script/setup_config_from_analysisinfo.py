#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Create analysis-specific config files from templates using anaName from analysis_info.
Usage:
  python script/setup_config_from_analysisinfo.py ANALYSIS_INFO [--config-dir PATH] [--force]
Example:
  python script/setup_config_from_analysisinfo.py config/analysis/analysis_info_pp500_anaPhi.yaml

Reads analysis_info YAML to get anaName, then:
Copies:
  cuts/event/event.yaml       -> cuts/event/event_{anaName}.yaml
  cuts/mixing/mixing.yaml     -> cuts/mixing/mixing_{anaName}.yaml
  cuts/pid/pid.yaml           -> cuts/pid/pid_{anaName}.yaml
  cuts/track/track.yaml       -> cuts/track/track_{anaName}.yaml
  cuts/v0reco/v0.yaml         -> cuts/v0reco/v0_{anaName}.yaml
  maker/maker_anaPhi.yaml     -> maker/maker_{anaName}.yaml
Creates:
  mainconf/main_{anaName}.yaml  (from mainconf/mainconf.yaml; __ANANAME__ placeholder is replaced)
If a destination already exists, it is SKIPPED (not overwritten).
Use --force to overwrite existing files.
"""
from __future__ import print_function
import os
import re
import shutil
import sys
import argparse

try:
    import yaml
except ImportError:
    yaml = None


def get_project_root(script_dir):
    """Project root is parent of script/."""
    return os.path.dirname(script_dir)


def load_yaml(path):
    if yaml is None:
        print("ERROR: PyYAML is required. Install with: pip install pyyaml", file=sys.stderr)
        sys.exit(1)
    with open(path, 'r') as f:
        return yaml.safe_load(f)


def get_analysis_field_no_yaml(analysis_path, key):
    """When PyYAML is missing: grep for key and extract value."""
    with open(analysis_path, 'r') as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith('#'):
                continue
            # Ignore trailing inline comments before matching key/value.
            line = line.split('#', 1)[0].strip()
            if key not in line:
                continue
            m = re.search(r'{}\s*:\s*(?:&\w+\s+)?["\']([^"\']+)["\']'.format(key), line)
            if m:
                return m.group(1).strip()
            m = re.search(r'{}\s*:\s*(\S+)'.format(key), line)
            if m:
                return m.group(1).strip()
    return None


# (src relative to config/, dst subdir under config/, base filename for dst)
CUTS_COPIES = [
    ('cuts/event/event.yaml', 'cuts/event', 'event'),
    ('cuts/mixing/mixing.yaml', 'cuts/mixing', 'mixing'),
    ('cuts/pid/pid.yaml', 'cuts/pid', 'pid'),
    ('cuts/track/track.yaml', 'cuts/track', 'track'),
    ('cuts/v0reco/v0.yaml', 'cuts/v0reco', 'v0'),
    ('cuts/centrality/centrality.yaml', 'cuts/centrality', 'centrality'),
]
MAKER_SRC = 'maker/maker_anaPhi.yaml'
MAKER_DIR = 'maker'
MAKER_BASE = 'maker'
MAINCONF_TEMPLATE_REL = 'mainconf/mainconf.yaml'
MAINCONF_TEMPLATE_ANANAME = '__ANANAME__'


def write_mainconf(config_base, ana_name, analysis_rel, force=False):
    """Create mainconf/main_{anaName}.yaml from template, with analysis: -> analysis_rel."""
    template_path = os.path.join(config_base, MAINCONF_TEMPLATE_REL)
    mainconf_dir = os.path.join(config_base, 'mainconf')
    mainconf_dst = os.path.join(mainconf_dir, 'main_{}.yaml'.format(ana_name))
    if not force and os.path.isfile(mainconf_dst):
        print("Skipped (exists): {}".format(os.path.relpath(mainconf_dst, config_base)))
        return
    if not os.path.isfile(template_path):
        print("WARNING: mainconf template not found, skip: {}".format(MAINCONF_TEMPLATE_REL))
        return
    with open(template_path, 'r') as f:
        content = f.read()
    content = content.replace(MAINCONF_TEMPLATE_ANANAME, ana_name)
    is_lambda = 'lambda' in ana_name.lower()
    is_nuclearid = 'nuclearid' in ana_name.lower() or 'nucleusid' in ana_name.lower()
    is_phi = 'phi' in ana_name.lower()

    # Determine which keys are active for the current analysis type
    active_keys = {'event', 'track', 'pid', 'centrality', 'hist', 'analysis'}
    if is_lambda:
        active_keys.add('v0')
        active_keys.add('mixing')
        active_keys.add('lambda')
    if is_phi:
        active_keys.add('mixing')
        active_keys.add('phi')
    if is_nuclearid:
        active_keys.add('nuclearid')

    # Replace placeholders for active cuts
    for _subdir, _base in [('event', 'event'), ('track', 'track'), ('pid', 'pid'),
                           ('v0reco', 'v0'), ('mixing', 'mixing'), ('centrality', 'centrality'),
                           ('nuclearid', 'nuclearid')]:
        content = content.replace(
            'cuts/{}/{}.yaml'.format(_subdir, _base),
            'cuts/{}/{}_{}.yaml'.format(_subdir, _base, ana_name)
        )

    # For integrated Lambda and NuclearId analysis
    if 'lambdanucle' in ana_name.lower():
        # Parse system prefix (e.g. auau19, auau13p5, etc.)
        system_match = re.match(r'^([a-zA-Z0-9\.]+)_ana', ana_name)
        system = system_match.group(1) if system_match else 'auau19'
        
        # Replace single maker key with both lambda and nuclearid pointing to separate configs
        content = re.sub(
            r'^lambda\s*:\s*.*$',
            'lambda:        maker/maker_{system}_anaLambda.yaml'.format(system=system),
            content,
            flags=re.MULTILINE
        )
        content = re.sub(
            r'^nuclearid\s*:\s*.*$',
            'nuclearid:     cuts/nuclearid/nuclearid_{ana_name}.yaml'.format(ana_name=ana_name),
            content,
            flags=re.MULTILINE
        )
        # Also set separate hist configurations and generic hist configuration
        content = re.sub(
            r'^hist\s*:\s*.*$',
            'lambdaHist:    hist/hist_{system}_anaLambda.yaml\n'
            'nuclearidHist: hist/hist_{system}_anaNuclearId.yaml\n'
            'hist:          hist/hist_{system}_anaLambdaNuclearId.yaml'.format(system=system),
            content,
            flags=re.MULTILINE
        )
    else:
        # Standard replacements
        if is_nuclearid:
            # Map nuclearid key to the cuts file
            content = re.sub(
                r'^nuclearid\s*:\s*.*$',
                'nuclearid:     cuts/nuclearid/nuclearid_{ana_name}.yaml'.format(ana_name=ana_name),
                content,
                flags=re.MULTILINE
            )
        elif is_phi:
            # Map lambda key to phi key
            content = re.sub(
                r'^lambda\s*:\s*(maker/maker_{}\.yaml)'.format(re.escape(ana_name)),
                'phi:        \\1',
                content,
                flags=re.MULTILINE
            )
        elif is_lambda:
            # Keep lambda key
            pass

    content = re.sub(
        r'^analysis\s*:\s*\S+',
        'analysis:      {}'.format(analysis_rel),
        content,
        flags=re.MULTILINE
    )

    # Filter out lines that belong to inactive keys
    lines = content.split('\n')
    filtered_lines = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            filtered_lines.append(line)
            continue
        # Check if line starts with an inactive key followed by a colon
        parts = stripped.split(':', 1)
        key = parts[0].strip()
        if key in ['event', 'track', 'pid', 'v0', 'mixing', 'centrality', 'nuclearid', 'lambda', 'phi', 'lambdaHist', 'nuclearidHist', 'hist', 'analysis']:
            if key in active_keys or (is_lambda and is_nuclearid and key in ['lambdaHist', 'nuclearidHist']):
                filtered_lines.append(line)
        else:
            filtered_lines.append(line)

    content = '\n'.join(filtered_lines)

    if not os.path.isdir(mainconf_dir):
        try:
            os.makedirs(mainconf_dir)
        except OSError:
            if not os.path.isdir(mainconf_dir):
                raise
    with open(mainconf_dst, 'w') as f:
        f.write(content)
    print("Created: {}".format(os.path.relpath(mainconf_dst, config_base)))


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = get_project_root(script_dir)
    default_config = os.path.join(project_root, 'config')

    parser = argparse.ArgumentParser(
        description='Create analysis-specific config files from analysis_info (using anaName).'
    )
    parser.add_argument(
        'analysis_info',
        help='Analysis info YAML path (e.g. config/analysis/analysis_info_pp500_anaPhi.yaml)'
    )
    parser.add_argument(
        '--config-dir',
        default=default_config,
        help='Config directory (default: project_root/config)'
    )
    parser.add_argument(
        '--force',
        action='store_true',
        default=False,
        help='Overwrite existing config files (default: skip if already exists)'
    )
    args = parser.parse_args()
    force = args.force

    config_base = os.path.abspath(args.config_dir)
    analysis_path = args.analysis_info
    if not os.path.isabs(analysis_path):
        analysis_path = os.path.join(project_root, analysis_path)
    if not os.path.isfile(analysis_path):
        print("ERROR: analysis_info not found: {}".format(analysis_path), file=sys.stderr)
        sys.exit(1)
    analysis_rel = os.path.relpath(analysis_path, config_base).replace(os.sep, '/')

    if yaml is not None:
        info = load_yaml(analysis_path)
        analysis = info.get('analysis') or {}
        ana_name = analysis.get('anaName') or analysis.get('name')
        base_ana_macro = analysis.get('baseAnaMacro')
        base_run_macro = analysis.get('baseRunMacro')
        mode = analysis.get('mode')
    else:
        ana_name = get_analysis_field_no_yaml(analysis_path, 'anaName')
        base_ana_macro = get_analysis_field_no_yaml(analysis_path, 'baseAnaMacro')
        base_run_macro = get_analysis_field_no_yaml(analysis_path, 'baseRunMacro')
        mode = get_analysis_field_no_yaml(analysis_path, 'mode')
    if not ana_name:
        print("ERROR: anaName not found in analysis info: {}".format(analysis_path), file=sys.stderr)
        sys.exit(1)

    print("anaName: {}".format(ana_name))
    print("config base: {}".format(config_base))

    def copy_one(src_rel, subdir, base_name):
        src = os.path.join(config_base, src_rel)
        dst_name = "{}_{}.yaml".format(base_name, ana_name)
        dst = os.path.join(config_base, subdir, dst_name)
        if not os.path.isfile(src):
            print("WARNING: template not found, skip: {}".format(src_rel))
            return
        if not force and os.path.isfile(dst):
            print("Skipped (exists): {}".format(os.path.relpath(dst, config_base)))
            return
        d = os.path.dirname(dst)
        if not os.path.isdir(d):
            try:
                os.makedirs(d)
            except OSError:
                if not os.path.isdir(d):
                    raise
        shutil.copy2(src, dst)
        print("Created: {}".format(os.path.relpath(dst, config_base)))

    is_lambda = 'lambda' in ana_name.lower()
    is_nuclearid = 'nuclearid' in ana_name.lower() or 'nucleusid' in ana_name.lower()
    is_phi = 'phi' in ana_name.lower()

    active_cuts = [
        ('cuts/event/event.yaml', 'cuts/event', 'event'),
        ('cuts/track/track.yaml', 'cuts/track', 'track'),
        ('cuts/pid/pid.yaml', 'cuts/pid', 'pid'),
        ('cuts/centrality/centrality.yaml', 'cuts/centrality', 'centrality'),
    ]
    if is_lambda:
        active_cuts.append(('cuts/v0reco/v0.yaml', 'cuts/v0reco', 'v0'))
        active_cuts.append(('cuts/mixing/mixing.yaml', 'cuts/mixing', 'mixing'))
    if is_phi:
        active_cuts.append(('cuts/mixing/mixing.yaml', 'cuts/mixing', 'mixing'))
    if is_nuclearid:
        active_cuts.append(('cuts/nuclearid/nuclearid.yaml', 'cuts/nuclearid', 'nuclearid'))

    for src_rel, subdir, base_name in active_cuts:
        copy_one(src_rel, subdir, base_name)

    # Update centrality mode if specified
    if mode:
        centrality_dst = os.path.join(config_base, 'cuts/centrality', "centrality_{}.yaml".format(ana_name))
        if os.path.isfile(centrality_dst):
            with open(centrality_dst, 'r') as f:
                cent_content = f.read()
            cent_content = re.sub(
                r'^mode\s*:\s*\S+',
                'mode: {}'.format(mode),
                cent_content,
                flags=re.MULTILINE
            )
            with open(centrality_dst, 'w') as f:
                f.write(cent_content)
            print("Updated centrality mode to: {} in {}".format(mode, os.path.relpath(centrality_dst, config_base)))

        # Update event Vz cuts based on the mode
        event_dst = os.path.join(config_base, 'cuts/event', "event_{}.yaml".format(ana_name))
        if os.path.isfile(event_dst):
            with open(event_dst, 'r') as f:
                event_content = f.read()
            
            if mode.lower() == 'fxtmult':
                # Fixed-target: target is at z ~ 200 cm, so set Vz cuts to [198.0, 202.0]
                new_min, new_max = '198.0', '202.0'
            elif mode.lower() == 'refmult':
                # Collider: vertex centered at z ~ 0, so set Vz cuts to [-100.0, 100.0]
                new_min, new_max = '-100.0', '100.0'
            else:
                new_min, new_max = None, None

            if new_min and new_max:
                event_content = re.sub(
                    r'^minVz\s*:\s*\S+',
                    'minVz: {}'.format(new_min),
                    event_content,
                    flags=re.MULTILINE
                )
                event_content = re.sub(
                    r'^maxVz\s*:\s*\S+',
                    'maxVz: {}'.format(new_max),
                    event_content,
                    flags=re.MULTILINE
                )
                with open(event_dst, 'w') as f:
                    f.write(event_content)
                print("Updated event Vz cuts to [{}, {}] in {}".format(new_min, new_max, os.path.relpath(event_dst, config_base)))

    # maker: template based on analysis name -> maker/maker_{anaName}.yaml
    if is_phi or is_lambda:
        maker_template_rel = 'maker/maker_anaPhi.yaml'
        if is_lambda:
            maker_template_rel = 'maker/maker_anaLambda.yaml'
        elif is_phi:
            maker_template_rel = 'maker/maker_anaPhi.yaml'

        maker_src = os.path.join(config_base, maker_template_rel)
        maker_dst = os.path.join(config_base, MAKER_DIR, "{}_{}.yaml".format(MAKER_BASE, ana_name))
        if not os.path.isfile(maker_src):
            print("WARNING: template not found, skip: {}".format(maker_template_rel))
        elif not force and os.path.isfile(maker_dst):
            print("Skipped (exists): {}".format(os.path.relpath(maker_dst, config_base)))
        else:
            shutil.copy2(maker_src, maker_dst)
            print("Created: {}".format(os.path.relpath(maker_dst, config_base)))

    # hist: template based on analysis name -> hist/hist_{anaName}.yaml
    if 'lambdanucle' in ana_name.lower():
        system_match = re.match(r'^([a-zA-Z0-9\.]+)_ana', ana_name)
        system = system_match.group(1) if system_match else 'auau19'
        
        hist_files = [
            ('hist_anaLambda.yaml', 'hist_{}_anaLambda.yaml'.format(system)),
            ('hist_anaNuclearId.yaml', 'hist_{}_anaNuclearId.yaml'.format(system)),
            ('hist_anaLambdaNuclearId.yaml', 'hist_{}_anaLambdaNuclearId.yaml'.format(system))
        ]
        
        for src_name, dst_name in hist_files:
            hist_src = os.path.join(config_base, 'hist', src_name)
            hist_dst = os.path.join(config_base, 'hist', dst_name)
            if not os.path.isfile(hist_src):
                print("WARNING: hist template not found, skip: {}".format(src_name))
            elif not force and os.path.isfile(hist_dst):
                print("Skipped (exists): {}".format(os.path.relpath(hist_dst, config_base)))
            else:
                shutil.copy2(hist_src, hist_dst)
                print("Created: {}".format(os.path.relpath(hist_dst, config_base)))
    else:
        hist_template_rel = None
        if is_lambda:
            hist_template_rel = 'hist_anaLambda.yaml'
        elif is_phi:
            hist_template_rel = 'hist_anaPhi.yaml'
        elif is_nuclearid:
            hist_template_rel = 'hist_anaNuclearId.yaml'

        if hist_template_rel:
            hist_src = os.path.join(config_base, 'hist', hist_template_rel)
            hist_dst = os.path.join(config_base, 'hist', 'hist_{}.yaml'.format(ana_name))
            if not os.path.isfile(hist_src):
                print("WARNING: hist template not found, skip: {}".format(hist_template_rel))
            elif not force and os.path.isfile(hist_dst):
                print("Skipped (exists): {}".format(os.path.relpath(hist_dst, config_base)))
            else:
                shutil.copy2(hist_src, hist_dst)
                print("Created: {}".format(os.path.relpath(hist_dst, config_base)))

    write_mainconf(config_base, ana_name, analysis_rel, force=force)

    # generate macros if base macros are defined
    if base_ana_macro and base_run_macro:
        if 'lambdanucle' in base_ana_macro.lower():
            print("Integrated analysis detected; skipping macro generation to preserve custom C++ files.")
        else:
            maker_name = base_ana_macro[3:] if base_ana_macro.startswith('ana') else base_ana_macro
            maker_class = "St{}Maker".format(maker_name)
            maker_var = "{}Maker".format(maker_name.lower())
            
            # ana macro
            ana_src = os.path.join(project_root, 'analysis', 'anaPhi.C')
            ana_dst = os.path.join(project_root, 'analysis', '{}.C'.format(base_ana_macro))
            if os.path.isfile(ana_src):
                with open(ana_src, 'r') as f:
                    content = f.read()
                content = content.replace('anaPhi', base_ana_macro)
                content = content.replace('StPhiMaker', maker_class)
                content = content.replace('phiMaker', maker_var)
                content = content.replace('"phi"', '"{}"'.format(maker_name.lower()))
                content = content.replace('auau19_anaPhi', ana_name)
                content = content.replace('run_anaPhi', base_run_macro)
                with open(ana_dst, 'w') as f:
                    f.write(content)
                print("Created: {}".format(os.path.relpath(ana_dst, project_root)))
                
            # run macro
            run_src = os.path.join(project_root, 'analysis', 'run_anaPhi.C')
            run_dst = os.path.join(project_root, 'analysis', '{}.C'.format(base_run_macro))
            if os.path.isfile(run_src):
                with open(run_src, 'r') as f:
                    content = f.read()
                content = content.replace('run_anaPhi', base_run_macro)
                content = content.replace('anaPhi', base_ana_macro)
                content = content.replace('StPhiMaker', maker_class)
                content = content.replace('auau19_anaPhi', ana_name)
                with open(run_dst, 'w') as f:
                    f.write(content)
                print("Created: {}".format(os.path.relpath(run_dst, project_root)))


if __name__ == '__main__':
    main()
