#!/usr/bin/env python3
"""Home Assistant resident, bounded RadPro log archive; no serial dependency."""
import datetime,hashlib,html,http.server,json,os,pathlib,signal,threading,time,urllib.request
ROOT=pathlib.Path(os.environ.get('RADPRO_LOG_DIR','/share/radpro-logs'));ROOT.mkdir(parents=True,exist_ok=True)
os.umask(0o077)
OPTS=json.loads(pathlib.Path('/data/options.json').read_text()) if pathlib.Path('/data/options.json').exists() else {}
HOST=OPTS.get('bridge_host',os.environ.get('RADPRO_HOST','192.168.1.40'));INTERVAL=max(2,int(OPTS.get('interval_seconds',5)))
BASE='http://'+HOST;STOP=threading.Event();LOCK=threading.Lock();MAX_FILE=4*1024*1024;MAX_TOTAL=256*1024*1024
STATE_PATH=ROOT/'status.json';STATE=json.loads(STATE_PATH.read_text()) if STATE_PATH.exists() else {'cursor':0,'lines_archived':0,'outages':0,'crashes_archived':0,'buffer_gaps':0,'start_epoch':time.time(),'soak_duration_seconds':48*3600}
STATE['process_starts']=STATE.get('process_starts',0)+1;STATE['running']=True;STATE['host']=HOST
STATE['capture_type']='Runtime logs over HTTP plus reboot-persistent panic records; not a raw serial stream.'
def stamp():return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='milliseconds')
def persist():
 STATE['heartbeat_utc']=stamp();STATE['heartbeat_epoch']=time.time()
 tmp=ROOT/'status.tmp';tmp.write_text(json.dumps(STATE,indent=2));os.replace(tmp,STATE_PATH)
def append(kind,rows):
 day=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d');files=sorted(ROOT.glob(kind+'-'+day+'-*.jsonl'));f=files[-1] if files and files[-1].stat().st_size<MAX_FILE else ROOT/(kind+'-'+day+'-'+str(time.time_ns())+'.jsonl')
 with f.open('a') as out:
  for row in rows:out.write(json.dumps(row,ensure_ascii=False)+'\n')
  out.flush();os.fsync(out.fileno())
 files=sorted(ROOT.glob('*.jsonl'),key=lambda f:f.stat().st_mtime);total=sum(f.stat().st_size for f in files)
 for old in files:
  if total<=MAX_TOTAL:break
  if old==f:continue
  size=old.stat().st_size;old.unlink();total-=size;STATE['retention_files_removed']=STATE.get('retention_files_removed',0)+1

def event(name,**args):
 append('events',[{'received_utc':stamp(),'event':name,**args}]);print(json.dumps({'event':name,**args}),flush=True)
def get(path):
 with urllib.request.urlopen(BASE+path,timeout=4) as r:
  data=r.read(1024*1024+1)
  if len(data)>1024*1024:raise ValueError('Response too large')
  return json.loads(data)
def poll():
 old=int(STATE['cursor']);data=get('/logs.json?after='+str(old));latest=int(data['latest']);reset=latest<old
 if reset:
  event('bridge_log_counter_reset',previous=old,latest=latest);STATE['boot_observations']=STATE.get('boot_observations',0)+1
  data=get('/logs.json');latest=int(data['latest'])
 lines=data.get('lines',[])
 if int(data.get('returnedCount',len(lines)))!=len(lines):raise ValueError('Incomplete log batch')
 if data.get('reset'):
  STATE['buffer_gaps']+=1;event('log_buffer_overrun',previous=old,oldest=data.get('oldest'),latest=latest)
 if latest>old and not reset and latest-old>len(lines) and not data.get('reset'):
  STATE['buffer_gaps']+=1;event('log_batch_gap',previous=old,latest=latest,returned=len(lines))
 if lines:
  first=latest-len(lines)+1;append('runtime',[{'received_utc':stamp(),'id':first+i,'line':line} for i,line in enumerate(lines)]);STATE['lines_archived']+=len(lines)
 STATE['cursor']=latest;STATE['last_success_utc']=stamp();STATE['last_success_epoch']=time.time();STATE['last_error']=None
 if STATE.get('unreachable_since'):
  event('bridge_reachable_again',unreachable_since=STATE.pop('unreachable_since'))
 if time.time()-STATE.get('last_telemetry_epoch',0)>60:
  telemetry=get('/device.json');bridge=get('/bridge.json')
  try:bridge['safecastSampling']=get('/diagnostics/safecast.json')
  except Exception as exc:bridge['safecastSampling']={'unavailable':type(exc).__name__}
  append('telemetry',[{'received_utc':stamp(),'device':telemetry,'bridge':bridge}]);STATE['last_telemetry_epoch']=time.time();STATE['device_id']=telemetry.get('deviceId');STATE['measurement_age_ms']=telemetry.get('measurementAgeMs');STATE['bridge_firmware']=telemetry.get('bridgeFirmware')
 if time.time()-STATE.get('last_crash_check_epoch',0)>10:
  try:
   crash=get('/diagnostics/crash.json')
   if crash.get('available'):
    identity={k:crash.get(k) for k in ('sequence','captureBootId','elfSha256','text')}
    digest=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()
    target=ROOT/('crash-'+digest+'.json')
    if not target.exists():
     tmp=target.with_suffix('.tmp');tmp.write_text(json.dumps({'archived_utc':stamp(),'record':crash},indent=2));os.replace(tmp,target);STATE['crashes_archived']+=1;event('panic_record_archived',file=target.name)
   previousBoot=STATE.get('device_boot_id');currentBoot=crash.get('bootId')
   if previousBoot is not None and currentBoot is not None and previousBoot!=currentBoot:
    event('bridge_boot_id_changed',previous=previousBoot,current=currentBoot);STATE['boot_observations']=STATE.get('boot_observations',0)+1
   STATE['device_boot_id']=currentBoot
   STATE['panic_endpoint_available']=True
  except urllib.error.HTTPError as e:
   if e.code==404:STATE['panic_endpoint_available']=False
   else:raise
  STATE['last_crash_check_epoch']=time.time()

def loop():
 event('collector_started',host=HOST,interval_seconds=INTERVAL)
 while not STOP.is_set():
  t=time.monotonic()
  with LOCK:
   try:poll()
   except Exception as exc:
    STATE['last_error']=type(exc).__name__+': '+str(exc)
    if not STATE.get('unreachable_since'):STATE['unreachable_since']=stamp();STATE['outages']+=1;event('bridge_unreachable',error=STATE['last_error'])
   STATE['soak_deadline_epoch']=STATE['start_epoch']+STATE['soak_duration_seconds'];STATE['soak_deadline_utc']=datetime.datetime.fromtimestamp(STATE['soak_deadline_epoch'],datetime.timezone.utc).isoformat()
   baseline=STATE.get('soak_baseline',{})
   STATE['soak_counters']={key:STATE.get(key,0)-baseline.get(key,0) for key in ('lines_archived','outages','crashes_archived','buffer_gaps','boot_observations')}
   STATE['soak_complete']=time.time()>=STATE['soak_deadline_epoch'];persist()
   if STATE['soak_complete'] and not (ROOT/'soak-summary.json').exists():
    summary={**STATE,'note':'48-hour wall-clock observation window; outages/buffer gaps are included. Capture continues with bounded retention.'};(ROOT/'soak-summary.json').write_text(json.dumps(summary,indent=2));event('soak_window_complete')
  STOP.wait(max(.1,INTERVAL-(time.monotonic()-t)))
 with LOCK:STATE['running']=False;persist();event('collector_stopped')
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  name=self.path.split('?',1)[0]
  if name in ('/','/index.html'):
   files=sorted([f.name for f in ROOT.iterdir() if f.is_file() and f.suffix in ('.json','.jsonl')],reverse=True)
   with LOCK:s=dict(STATE)
   b=('<!doctype html><meta charset="utf-8"><title>RadPro logs</title><h1>RadPro log archive</h1><p>Stored on Home Assistant; capture continues without a laptop.</p><pre>'+html.escape(json.dumps(s,indent=2))+'</pre><h2>Files</h2><ul>'+''.join('<li><a href="files/'+html.escape(n)+'">'+html.escape(n)+'</a></li>' for n in files)+'</ul>').encode();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b);return
  if name=='/health':self.send_response(200);self.end_headers();self.wfile.write(b'ok');return
  if name.startswith('/files/'):
   filename=name[len('/files/'):];f=ROOT/filename
   if pathlib.Path(filename).name==filename and f.is_file() and f.suffix in ('.json','.jsonl'):
    self.send_response(200);self.send_header('Content-Type','text/plain; charset=utf-8');self.send_header('Content-Length',str(f.stat().st_size));self.end_headers()
    with f.open('rb') as source:
     while data:=source.read(65536):self.wfile.write(data)
    return
  self.send_error(404)
 def log_message(self,*args):pass
if __name__=='__main__':
 signal.signal(signal.SIGTERM,lambda *a:STOP.set());signal.signal(signal.SIGINT,lambda *a:STOP.set());server=http.server.ThreadingHTTPServer(('0.0.0.0',8099),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();loop();server.shutdown()
