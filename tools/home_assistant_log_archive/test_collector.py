import importlib.util,json,os,pathlib,tempfile,urllib.error
path=pathlib.Path(__file__).parent/'collector.py'
with tempfile.TemporaryDirectory() as temp:
 os.environ['RADPRO_LOG_DIR']=temp
 spec=importlib.util.spec_from_file_location('collector',path);c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
 responses=[];crash={'available':False,'bootId':1}
 def get(route):
  if route.startswith('/logs.json'):return responses.pop(0)
  if route=='/device.json':return {'deviceId':'fixture','measurementAgeMs':0,'bridgeFirmware':'fixture'}
  if route=='/bridge.json':return {'heapFree':100}
  if route=='/diagnostics/safecast.json':return {'samples':0,'collecting':False,'elfSha256':'fixture'}
  if route=='/diagnostics/crash.json':return crash
  raise AssertionError(route)
 c.get=get
 responses.append({'latest':3,'oldest':1,'lines':['a','b','c'],'returnedCount':3,'reset':False});c.poll();assert c.STATE['cursor']==3 and c.STATE['lines_archived']==3
 responses.append({'latest':4,'oldest':1,'lines':['d'],'returnedCount':1,'reset':False});c.poll();assert c.STATE['lines_archived']==4
 responses.extend([{'latest':2,'oldest':1,'lines':[],'returnedCount':0,'reset':False},{'latest':2,'oldest':1,'lines':['boot','reading'],'returnedCount':2,'reset':False}]);c.poll();assert c.STATE['cursor']==2 and c.STATE['lines_archived']==6
 responses.append({'latest':1000,'oldest':998,'lines':['x','y','z'],'returnedCount':3,'reset':True});c.poll();assert c.STATE['buffer_gaps']==1
 crash={'available':True,'bootId':2,'captureBootId':1,'sequence':1,'elfSha256':'fixture','text':'FAKE PANIC FIXTURE'};c.STATE['last_crash_check_epoch']=0;responses.append({'latest':1000,'lines':[]});c.poll();assert c.STATE['crashes_archived']==1
 crash['bootId']=3;c.STATE['last_crash_check_epoch']=0;responses.append({'latest':1000,'lines':[]});c.poll();assert c.STATE['crashes_archived']==1
 telemetry=[json.loads(line) for f in pathlib.Path(temp).glob('telemetry-*.jsonl') for line in f.read_text().splitlines()];assert telemetry[0]['bridge']['safecastSampling']['samples']==0
 c.persist();saved=json.loads((pathlib.Path(temp)/'status.json').read_text());assert saved['cursor']==1000;assert saved['device_boot_id']==3
 print('PASS: durable cursor, reboot detection, ring overrun, panic archiving and cross-reboot deduplication')
