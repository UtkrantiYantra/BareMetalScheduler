#!/usr/bin/env python3
"""Generate C headers/source from YAML system config.

Usage:
    python3 python/generate_from_yaml.py config/system.yaml --out include/generated_config.h --platform stm32f4
"""
import sys, os, argparse
try:
    import yaml
except Exception as e:
    print('PyYAML is required. Install: pip install pyyaml')
    raise

parser = argparse.ArgumentParser()
parser.add_argument('yaml', help='config yaml')
parser.add_argument('--out', default='include/generated_config.h')
parser.add_argument('--platform', default='host')
args = parser.parse_args()

with open(args.yaml) as f:
    cfg = yaml.safe_load(f)

# generate header content
lines = []
lines.append('/* generated file - do not edit */')
lines.append('#ifndef GENERATED_CONFIG_H')
lines.append('#define GENERATED_CONFIG_H')
lines.append('')
lines.append('/* System configuration generated from YAML */')
if 'system' in cfg:
    sysc = cfg['system']
    if 'tick_ms' in sysc:
        lines.append('#define SYS_TICK_MS %d' % int(sysc['tick_ms']))
    if 'max_tasks' in sysc:
        lines.append('#define MAX_TASKS %d' % int(sysc['max_tasks']))
if 'events' in cfg:
    lines.append('enum {')
    for e in cfg['events']:
        lines.append('    EVT_%s,' % e.upper())
    lines.append('    EVT_COUNT')
    lines.append('};')
lines.append('')
lines.append('#endif')

out = '\n'.join(lines)
os.makedirs(os.path.dirname(args.out), exist_ok=True)
with open(args.out,'w') as f:
    f.write(out)

print('Wrote', args.out)
