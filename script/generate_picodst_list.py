#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Generate picoDst file list under config/picoDstList/ using get_file_list.pl.
Arguments for get_file_list.pl are built from analysis_info's starTag (from mainconf).
Usage:
  python script/generate_pico_dst_list.py MAINCONF [--config-dir PATH] [--dry-run]
Example:
  python script/generate_pico_dst_list.py config/mainconf/main_auau19_anaPhi.yaml

Requires get_file_list.pl (STAR environment). Output path is taken from
analysis_info dataset.allPicoDstList (e.g. picoDstList/auau19GeV.list);
if unset, uses picoDstList/{anaName}.list.
"""
from __future__ import print_function
import os
import re
import subprocess
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


def get_analysis_path_from_mainconf(mainconf_path, config_base):
    """mainconf has 'analysis: path'; path is relative to config/."""
    data = load_yaml(mainconf_path)
    rel = data.get('analysis') or data.get('analysis info')
    if not rel:
        print("ERROR: mainconf has no 'analysis' key: {}".format(mainconf_path), file=sys.stderr)
        sys.exit(1)
    if isinstance(rel, str):
        rel = rel.strip()
    return os.path.join(config_base, rel)


def get_analysis_path_from_mainconf_no_yaml(mainconf_path, config_base):
    with open(mainconf_path, 'r') as f:
        for line in f:
            m = re.search(r'analysis\s*:\s*(\S+)', line)
            if m:
                return os.path.join(config_base, m.group(1).strip())
    return None


def star_tag_to_cond(star_tag):
    """Build get_file_list.pl -cond string from starTag (same mapping as catalog URL)."""
    parts = []
    if star_tag.get('triggerSets'):
        parts.append("trgsetupname={}".format(star_tag['triggerSets']))
    if star_tag.get('productionTag'):
        parts.append("production={}".format(star_tag['productionTag']))
    if star_tag.get('filetype'):
        parts.append("filetype={}".format(star_tag['filetype']))
    if star_tag.get('filenameFilter'):
        parts.append("filename~{}".format(star_tag['filenameFilter']))
    if star_tag.get('storage'):
        parts.append("storage={}".format(star_tag['storage']))
    return ','.join(parts)


def _parse_quoted_or_word(line, key_pattern):
    """Match 'key: value' or 'key: "value"'; return value or None."""
    m = re.search(key_pattern + r'\s*:\s*["\']([^"\']*)["\']', line)
    if m:
        return m.group(1).strip()
    m = re.search(key_pattern + r'\s*:\s*(\S+)', line)
    if m:
        return m.group(1).strip()
    return None


def get_star_tag_cond_no_yaml(analysis_path):
    """When PyYAML is missing: read starTag fields and build cond string."""
    trigger_sets = None
    production_tag = None
    filetype = None
    filename_filter = None
    storage = None
    in_star_tag = False
    with open(analysis_path, 'r') as f:
        for line in f:
            stripped = line.strip()
            if stripped == 'starTag:' or stripped.startswith('starTag:'):
                in_star_tag = True
                continue
            if in_star_tag and line and line[0] not in ' \t' and ':' in line:
                break
            if not in_star_tag:
                continue
            v = _parse_quoted_or_word(line, r'triggerSets')
            if v is not None:
                trigger_sets = v
                continue
            v = _parse_quoted_or_word(line, r'productionTag')
            if v is not None:
                production_tag = v
                continue
            v = _parse_quoted_or_word(line, r'filetype')
            if v is not None:
                filetype = v
                continue
            v = _parse_quoted_or_word(line, r'filenameFilter')
            if v is not None:
                filename_filter = v
                continue
            v = _parse_quoted_or_word(line, r'storage')
            if v is not None:
                storage = v
                continue
    star_tag = {
        'triggerSets': trigger_sets,
        'productionTag': production_tag,
        'filetype': filetype,
        'filenameFilter': filename_filter,
        'storage': storage,
    }
    return star_tag_to_cond({k: v for k, v in star_tag.items() if v})


def get_all_pico_dst_list_no_yaml(analysis_path):
    with open(analysis_path, 'r') as f:
        for line in f:
            m = re.search(r'allPicoDstList\s*:\s*["\']?([^"\'\s]+)["\']?', line)
            if m:
                return m.group(1).strip()
    return None


def get_ana_name_no_yaml(analysis_path):
    with open(analysis_path, 'r') as f:
        for line in f:
            if 'anaName' not in line:
                continue
            m = re.search(r'anaName\s*:\s*(?:&\w+\s+)?["\']([^"\']+)["\']', line)
            if m:
                return m.group(1).strip()
            m = re.search(r'anaName\s*:\s*(\S+)', line)
            if m:
                return m.group(1).strip()
    return None


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = get_project_root(script_dir)
    default_config = os.path.join(project_root, 'config')

    parser = argparse.ArgumentParser(
        description='Generate picoDst list with get_file_list.pl using starTag from mainconf.'
    )
    parser.add_argument(
        'mainconf',
        help='Main config path (e.g. config/mainconf/main_auau19_anaPhi.yaml)'
    )
    parser.add_argument(
        '--config-dir',
        default=default_config,
        help='Config directory (default: project_root/config)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Print get_file_list.pl command and output path, do not run'
    )
    args = parser.parse_args()

    config_base = os.path.abspath(args.config_dir)
    mainconf_path = args.mainconf
    if not os.path.isabs(mainconf_path):
        mainconf_path = os.path.join(project_root, mainconf_path)
    if not os.path.isfile(mainconf_path):
        print("ERROR: mainconf not found: {}".format(mainconf_path), file=sys.stderr)
        sys.exit(1)

    if yaml is not None:
        analysis_path = get_analysis_path_from_mainconf(mainconf_path, config_base)
    else:
        analysis_path = get_analysis_path_from_mainconf_no_yaml(mainconf_path, config_base)
        if not analysis_path:
            print("ERROR: mainconf has no 'analysis' key: {}".format(mainconf_path), file=sys.stderr)
            sys.exit(1)
    if not os.path.isfile(analysis_path):
        print("ERROR: analysis info not found: {}".format(analysis_path), file=sys.stderr)
        sys.exit(1)

    if yaml is not None:
        info = load_yaml(analysis_path)
        star_tag = info.get('starTag') or info.get('startag') or {}
        dataset = info.get('dataset') or {}
        analysis = info.get('analysis') or {}
        cond = star_tag_to_cond(star_tag)
        list_rel = dataset.get('allPicoDstList')
        ana_name = analysis.get('anaName') or analysis.get('name')
    else:
        cond = get_star_tag_cond_no_yaml(analysis_path)
        list_rel = get_all_pico_dst_list_no_yaml(analysis_path)
        ana_name = get_ana_name_no_yaml(analysis_path)

    if not cond:
        print("ERROR: starTag in analysis_info has no usable keys for -cond", file=sys.stderr)
        sys.exit(1)

    if list_rel:
        out_path = os.path.join(config_base, list_rel)
    elif ana_name:
        list_rel = "picoDstList/{}.list".format(ana_name)
        out_path = os.path.join(config_base, list_rel)
    else:
        print("ERROR: no dataset.allPicoDstList and no anaName in analysis info", file=sys.stderr)
        sys.exit(1)

    out_dir = os.path.dirname(out_path)
    if not os.path.isdir(out_dir):
        try:
            os.makedirs(out_dir)
        except OSError:
            if not os.path.isdir(out_dir):
                raise

    cmd = ['get_file_list.pl', '-keys', 'path,filename', '-cond', cond, '-delim', '/']
    print("Output: {}".format(out_path))
    print("Cond:   {}".format(cond))
    if args.dry_run:
        print("Would run: {}".format(' '.join(cmd)))
        return

    try:
        prefix = "root://xrdstar.rcf.bnl.gov:1095/"
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, universal_newlines=True)
        with open(out_path, 'w') as f:
            for line in p.stdout:
                line_stripped = line.strip()
                if line_stripped:
                    f.write(prefix + line_stripped + '\n')
        p.wait()
        ret = p.returncode
        if ret != 0:
            print("WARNING: get_file_list.pl exited with {}".format(ret), file=sys.stderr)
            sys.exit(ret)
        n_lines = sum(1 for _ in open(out_path))
        print("Wrote {} ({} files)".format(out_path, n_lines))
    except OSError as e:
        if e.errno == 2:
            print("ERROR: get_file_list.pl not found (set up STAR environment?)", file=sys.stderr)
        else:
            print("ERROR: {}".format(e), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
