#!/usr/bin/env python3
import json, os, subprocess, sys

BUILD = os.environ.get('PYCC_BUILD_DIR', 'build')
PROFDB = os.path.join(BUILD, 'coverage.profdata')

def run(cmd):
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

def main():
    try:
        # Merge raw profiles
        profraws = [os.path.join(BUILD, f) for f in os.listdir(BUILD) if f.endswith('.profraw')]
        if not profraws:
            print('[coverage] no .profraw found — run tests first')
            return 0
        run(['llvm-profdata', 'merge', '-sparse', '-o', PROFDB] + profraws)
        # Export JSON from all binaries
        objs = [os.path.join(BUILD, x) for x in ['pycc','test_unit','test_integration','test_e2e'] if os.path.exists(os.path.join(BUILD,x))]
        if not objs:
            print('[coverage] no binaries found')
            return 0
        p = run(['llvm-cov', 'export', '--summary-only', '--format=text', f'--instr-profile={PROFDB}'] + objs)
        data = json.loads(p.stdout)
        # Aggregate by top-level phase directories
        groups = {
            'cli':  {'files':[], 'cov':0, 'tot':0},
            'lexer':{'files':[], 'cov':0, 'tot':0},
            'parser':{'files':[], 'cov':0, 'tot':0},
            'sema': {'files':[], 'cov':0, 'tot':0},
            'codegen':{'files':[], 'cov':0, 'tot':0},
            'optimizer':{'files':[], 'cov':0, 'tot':0},
            'observability':{'files':[], 'cov':0, 'tot':0},
            'compiler':{'files':[], 'cov':0, 'tot':0},
            'ast': {'files':[], 'cov':0, 'tot':0},
            'other':{'files':[], 'cov':0, 'tot':0},
        }
        total_cov = 0
        total_tot = 0
        for el in data.get('data', []):
            for f in el.get('files', []):
                path = f.get('filename','')
                summ = f.get('summary',{})
                lines_cov = summ.get('lines',{}).get('covered',0)
                lines_tot = summ.get('lines',{}).get('count',0)
                total_cov += lines_cov
                total_tot += lines_tot
                # crude path grouping
                grp = 'other'
                for g in groups:
                    if f"/src/{g}/" in path or f"/include/{g}/" in path:
                        grp = g; break
                groups[grp]['cov'] += lines_cov
                groups[grp]['tot'] += lines_tot
                groups[grp]['files'].append(path)
        # Print ASCII table
        print('\n== Coverage by Phase ==')
        print(f"{'Phase':<15} {'Covered':>10} {'Total':>10} {'Percent':>9}")
        print('-'*48)
        for g, v in groups.items():
            cov, tot = v['cov'], v['tot']
            pct = (100.0*cov/tot) if tot else 0.0
            print(f"{g:<15} {cov:>10} {tot:>10} {pct:>8.1f}%")
        print('-'*48)
        pct_tot = (100.0*total_cov/total_tot) if total_tot else 0.0
        print(f"{'TOTAL':<15} {total_cov:>10} {total_tot:>10} {pct_tot:>8.1f}%\n")
        return 0
    except Exception as e:
        print('[coverage] error:', e)
        return 1

if __name__ == '__main__':
    sys.exit(main())
