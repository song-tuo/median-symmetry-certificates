"""Bounded subprocess execution with raw output and wait4 resource provenance."""
import argparse, pathlib, json, os, signal, time, subprocess, resource, hashlib, datetime, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--tag',required=True);p.add_argument('--timeout',type=float,default=180);p.add_argument('--science',action='store_true');p.add_argument('command',nargs=argparse.REMAINDER);a=p.parse_args()
    cmd=a.command[1:] if a.command and a.command[0]=='--' else a.command
    assert cmd and all(c not in a.tag for c in '/\\'), 'unsafe tag'
    log=ROOT/'logs'/a.tag;assert not log.with_suffix('.json').exists(),'never overwrite a recorded run'
    prior=[]
    for f in (ROOT/'logs').glob('*.json'):
        try:
            r=json.loads(f.read_text())
            if r.get('science'): prior.append(r)
        except (ValueError,AttributeError): pass
    remaining=1800-sum(r['wall_seconds'] for r in prior)
    limit=min(a.timeout,180,remaining) if a.science else a.timeout
    assert limit>0, 'total scientific time budget already exhausted'
    started=datetime.datetime.now(datetime.timezone.utc).isoformat();t=time.monotonic();stopped=False;stop_reason=None;samples=[];last_sample=-1
    with log.with_suffix('.stdout').open('wb') as out,log.with_suffix('.stderr').open('wb') as err:
        child=subprocess.Popen(cmd,cwd=ROOT,stdout=out,stderr=err,start_new_session=True)
        while True:
            pid,status,usage=os.wait4(child.pid,os.WNOHANG)
            if pid: break
            elapsed=time.monotonic()-t
            if elapsed-last_sample>=.2:
                try:probe=subprocess.run(['/bin/ps','-o','rss=','-p',str(child.pid)],capture_output=True,text=True)
                except OSError as exc:
                    err.write(('RESOURCE_MONITOR_FAILED: '+repr(exc)+'\n').encode());stop_reason='RESOURCE_MONITOR_FAILED';probe=None
                if probe is not None and probe.returncode==0 and probe.stdout.strip():
                    try:
                        rss=int(probe.stdout.strip())*1024
                        if rss<0:raise ValueError('negative RSS')
                        samples.append([elapsed,rss])
                        if rss>4*1024**3:stop_reason='RSS_LIMIT'
                    except ValueError as exc:
                        err.write(('RESOURCE_MONITOR_FAILED: '+repr(exc)+'\n').encode());stop_reason='RESOURCE_MONITOR_FAILED'
                elif probe is not None:
                    pid2,status2,usage2=os.wait4(child.pid,os.WNOHANG)
                    if pid2:status,usage=status2,usage2;break
                    err.write(('RESOURCE_MONITOR_FAILED: '+probe.stderr+' '+probe.stdout+'\n').encode());stop_reason='RESOURCE_MONITOR_FAILED'
                last_sample=elapsed
            if elapsed>limit:stop_reason='WALL_LIMIT'
            if stop_reason:
                stopped=True;os.killpg(child.pid,signal.SIGKILL);_,status,usage=os.wait4(child.pid,0);break
            time.sleep(.03)
        code=os.waitstatus_to_exitcode(status);child.returncode=code
    wall=time.monotonic()-t
    peak_rss=usage.ru_maxrss if sys.platform=='darwin' else usage.ru_maxrss*1024
    if peak_rss>4*1024**3:stopped=True;stop_reason='PEAK_RSS_LIMIT_RECORDED_AT_EXIT'
    result={'command':cmd,'cwd':str(ROOT),'utc_started':started,'exit_code':code,'status':'RESOURCE_MONITOR_FAILED' if stop_reason=='RESOURCE_MONITOR_FAILED' else ('BUDGET_STOP' if stopped else ('COMPLETED' if code==0 else 'FAILED')),'stop_reason':stop_reason,'science':a.science,'wall_seconds':wall,'cpu_user_seconds':usage.ru_utime,'cpu_system_seconds':usage.ru_stime,'child_peak_rss_bytes':usage.ru_maxrss if sys.platform=='darwin' else usage.ru_maxrss*1024,'rss_scope':'OS wait4 child maximum RSS; ps child-only RSS monitored at approximately 0.2s; no scientific child processes allowed','rss_limit_bytes':4*1024**3,'rss_samples':samples,'address_space_limit_bytes':None,'wall_limit_seconds':limit,'total_scientific_seconds_after':1800-remaining+wall if a.science else None,'stdout':str(log.with_suffix('.stdout').relative_to(ROOT)),'stderr':str(log.with_suffix('.stderr').relative_to(ROOT))}
    log.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='rss_samples'}));return 124 if stopped else code
if __name__=='__main__': sys.exit(main())
