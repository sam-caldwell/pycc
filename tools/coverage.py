#!/usr/bin/env python3
import json, os, subprocess, sys

BUILD = os.environ.get('PYCC_BUILD_DIR', 'build')
PROFDB = os.path.join(BUILD, 'coverage.profdata')
MIN_PCT = float(os.environ.get('PYCC_COVERAGE_MIN', '95'))
# Optional filters: comma-separated phase names and/or path substrings
FILTER_PHASES = [p.strip() for p in os.environ.get('PYCC_COVERAGE_PHASES', '').split(',') if p.strip()]
FILTER_PATHS = [p.strip() for p in os.environ.get('PYCC_COVERAGE_ONLY_PATHS', '').split(',') if p.strip()]

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
        # Export JSON from all binaries (file-level granularity needed for filtering)
        objs = [os.path.join(BUILD, x) for x in ['pycc','test_unit','test_integration','test_e2e'] if os.path.exists(os.path.join(BUILD,x))]
        if not objs:
            print('[coverage] no binaries found')
            return 0
        # Some llvm-cov builds fail when exporting multiple objects in a single invocation.
        # Fall back to per-binary export and aggregate.
        data = {'data': [{'files': []}]}
        for obj in objs:
            try:
                p = run(['llvm-cov', 'export', '--format=json', f'--instr-profile={PROFDB}', obj])
                dobj = json.loads(p.stdout)
                for el in dobj.get('data', []):
                    data['data'][0]['files'].extend(el.get('files', []))
            except subprocess.CalledProcessError as e:
                print(f"[coverage] warn: llvm-cov export failed for {obj}: {e}")
        # Aggregate by top-level phase directories (prefer JSON export; fallback to text report parsing)
        groups = {k:{'files':[], 'cov':0, 'tot':0} for k in [
            'cli','lexer','parser','sema','codegen','optimizer','observability','runtime','compiler','ast','other']}
        total_cov = 0; total_tot = 0; files_accum = []

        if data['data'][0]['files']:
            for el in data.get('data', []):
                for f in el.get('files', []):
                    path = f.get('filename','')
                    summ = f.get('summary',{})
                    lines_cov = summ.get('lines',{}).get('covered',0)
                    lines_tot = summ.get('lines',{}).get('count',0)
                    total_cov += lines_cov; total_tot += lines_tot
                    grp = 'other'
                    for g in groups:
                        if f"/src/{g}/" in path or f"/include/{g}/" in path:
                            grp = g; break
                    groups[grp]['cov'] += lines_cov
                    groups[grp]['tot'] += lines_tot
                    groups[grp]['files'].append(path)
                    files_accum.append((path, lines_cov, lines_tot, grp))
        else:
            # Fallback: parse llvm-cov report text for each binary, summing by path buckets
            def parse_report(obj):
                try:
                    ptxt = run(['llvm-cov','report',f'--instr-profile={PROFDB}',obj,'--ignore-filename-regex','_deps|gtest'])
                except subprocess.CalledProcessError:
                    return []
                rows = []
                for line in ptxt.stdout.splitlines():
                    if not line or line.startswith('Filename') or line.startswith('-') or line.startswith('TOTAL'):
                        continue
                    parts = line.split()
                    if len(parts) < 12: continue
                    filename = parts[0]
                    try:
                        lines_total = int(parts[8])
                        lines_missed = int(parts[9])
                    except ValueError:
                        continue
                    rows.append((filename, lines_total - lines_missed, lines_total))
                return rows
            per_file = {}
            for obj in objs:
                for (path, cov, tot) in parse_report(obj):
                    prev = per_file.get(path, (0,0))
                    per_file[path] = (prev[0]+cov, prev[1]+tot)
            for path,(cov,tot) in per_file.items():
                total_cov += cov; total_tot += tot
                grp = 'other'
                for g in groups:
                    if f"/src/{g}/" in path or f"/include/{g}/" in path:
                        grp = g; break
                groups[grp]['cov'] += cov
                groups[grp]['tot'] += tot
                groups[grp]['files'].append(path)
                files_accum.append((path, cov, tot, grp))
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
        print(f"{'TOTAL':<15} {total_cov:>10} {total_tot:>10} {pct_tot:>8.1f}%")

        # Optional filtered gating
        filt_cov = 0
        filt_tot = 0
        def include(path, grp):
            if FILTER_PHASES and grp not in FILTER_PHASES:
                return False
            if FILTER_PATHS:
                return any(s in path for s in FILTER_PATHS)
            return True
        if FILTER_PHASES or FILTER_PATHS:
            for path, cov, tot, grp in files_accum:
                if include(path, grp):
                    filt_cov += cov
                    filt_tot += tot
            pct_filt = (100.0*filt_cov/filt_tot) if filt_tot else 0.0
            print(f"Filtered TOTAL: {filt_cov:>10} {filt_tot:>10} {pct_filt:>8.1f}%  (filters: phases={FILTER_PHASES}, paths={FILTER_PATHS})")
            gate_pct = pct_filt
            gate_cov, gate_tot = filt_cov, filt_tot
        else:
            gate_pct = pct_tot
            gate_cov, gate_tot = total_cov, total_tot
        print(f"Threshold: {MIN_PCT:.1f}%")
        ok = gate_pct >= MIN_PCT
        print('[coverage] PASS' if ok else '[coverage] FAIL')
        return 0 if ok else 2
    except Exception as e:
        print('[coverage] error:', e)
        return 1

if __name__ == '__main__':
    sys.exit(main())
