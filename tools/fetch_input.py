#!/usr/bin/env python3
"""Fetch only the pinned upstream netlist, with ordinary verified HTTPS."""
import argparse,hashlib,json,ssl,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--ca-file',help='Optional existing trusted CA bundle; validation remains enabled');a=p.parse_args()
    spec=json.loads((ROOT/'provenance/upstream_input.json').read_text())
    dst=ROOT/'inputs/mom25_v2.cha';dst.parent.mkdir(exist_ok=True)
    def valid(b):return len(b)==spec['bytes'] and hashlib.sha256(b).hexdigest()==spec['sha256']
    if dst.exists():
        if not valid(dst.read_bytes()):raise ValueError('Existing input hash mismatch; not overwritten')
        print(json.dumps({'status':'EXISTING_INPUT_VERIFIED','sha256':spec['sha256']}));return
    context=ssl.create_default_context(cafile=a.ca_file)
    class HTTPSOnly(urllib.request.HTTPRedirectHandler):
        def redirect_request(self,req,fp,code,msg,headers,newurl):
            if not newurl.startswith('https://'):raise ValueError('Refusing non-HTTPS redirect')
            return super().redirect_request(req,fp,code,msg,headers,newurl)
    opener=urllib.request.build_opener(urllib.request.HTTPSHandler(context=context),HTTPSOnly())
    request=urllib.request.Request(spec['url'],headers={'User-Agent':'median-symmetry-certificates/1.0.0'})
    with opener.open(request,timeout=25) as r:
        if r.status!=200 or not r.url.startswith('https://'):raise ValueError('Unexpected download response')
        body=r.read(spec['bytes']+1)
    if not valid(body):raise ValueError('Upstream input size or SHA-256 mismatch; no input saved')
    with dst.open('xb') as f:f.write(body)
    print(json.dumps({'status':'DOWNLOADED_INPUT_VERIFIED','bytes':len(body),'sha256':spec['sha256'],'url':spec['url'],'tls_certificate_and_hostname_validation':True}))
if __name__=='__main__':main()
