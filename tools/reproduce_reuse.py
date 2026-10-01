#!/usr/bin/env python3
"""Optional bounded rerun of two limited checks; never starts full-cube science."""
import argparse, json, resource, subprocess, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args();out=args.output_dir.resolve()
    out.mkdir(parents=True,exist_ok=False)
    for script,result in [('verify_fixed_witness.py','witness.json'),('check_geometric_lemma.py','geometric_lemma.json')]:
        command=[sys.executable,str(ROOT/'checks/reuse/src'/script),'--output-dir',str(out)]
        start=time.monotonic()
        with (out/(script+'.stdout')).open('xb') as stdout,(out/(script+'.stderr')).open('xb') as stderr:
            try:code=subprocess.run(command,stdout=stdout,stderr=stderr,timeout=30).returncode
            except subprocess.TimeoutExpired:code=124
        peak=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
        # Linux reports KiB; Darwin reports bytes. This is the cumulative direct-child peak.
        if sys.platform!='darwin':peak*=1024
        if peak>268435456:code=125
        record={'command':command,'exit_code':code,'wall_seconds':time.monotonic()-start,'direct_child_peak_rss_bytes':peak,'wall_timeout_seconds':30,'post_run_peak_rss_limit_bytes':268435456,'memory_limit_kind':'Completed-child acceptance check, not enforced address-space limit'}
        (out/(script+'.receipt.json')).write_text(json.dumps(record,indent=2)+'\n')
        if code:raise SystemExit(code)
        fresh=json.loads((out/result).read_text());saved=json.loads((ROOT/'results/reuse'/result).read_text())
        # Only environment-dependent path/hash provenance differs between historical and portable runs.
        if result=='witness.json':
            fresh.pop('input_hashes');saved.pop('input_hashes')
        if fresh!=saved:raise ValueError('Fresh scientific fields differ: '+result)
    print(json.dumps({'status':'PASS','saved_scientific_fields_matched':True,'full_cube_executions':0}))
if __name__=='__main__':main()
