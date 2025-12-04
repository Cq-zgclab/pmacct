#!/usr/bin/env python3
import sys,struct,ipaddress,codecs

def read_msg(b):
    if len(b)<16:
        return None
    version,msg_len,export_time,seq,obs = struct.unpack('!HHIII', b[:16])
    return {'version':version,'msg_len':msg_len,'export_time':export_time,'seq':seq,'obs':obs,'body':b[16:]} 


def parse_templates(body):
    templates = {}
    offset=0
    while offset+4<=len(body):
        set_id,set_len = struct.unpack('!HH', body[offset:offset+4])
        if set_len<4: break
        inner = body[offset+4: offset+set_len]
        if set_id==2:
            pos=0
            while pos+4<=len(inner):
                tid,fcount = struct.unpack('!HH', inner[pos:pos+4])
                pos+=4
                fields=[]
                for _ in range(fcount):
                    if pos+4>len(inner): break
                    fid,flen = struct.unpack('!HH', inner[pos:pos+4])
                    pos+=4
                    enterprise = bool(fid & 0x8000)
                    eid = fid & 0x7fff
                    pen=None
                    if enterprise:
                        if pos+4>len(inner): break
                        pen = struct.unpack('!I', inner[pos:pos+4])[0]
                        pos+=4
                    fields.append((eid,flen,enterprise,pen))
                templates[tid]=fields
        offset+=set_len
    return templates


def parse_data(body,templates):
    offset=0
    data_sets=[]
    while offset+4<=len(body):
        set_id,set_len = struct.unpack('!HH', body[offset:offset+4])
        inner = body[offset+4: offset+set_len]
        if set_id!=2:
            tpl_id=set_id
            recs=[]
            if tpl_id in templates:
                fields = templates[tpl_id]
                pos=0
                while pos < len(inner):
                    rec={}
                    for (eid,flen,enterprise,pen) in fields:
                        if flen==0xFFFF:
                            if pos+1>len(inner):
                                rec['__truncated']=True
                                break
                            first = inner[pos]
                            pos+=1
                            if first<255:
                                vlen=first
                            else:
                                if pos+2>len(inner): rec['__truncated']=True; break
                                vlen = struct.unpack('!H', inner[pos:pos+2])[0]; pos+=2
                            val = inner[pos:pos+vlen]; pos+=vlen
                            rec[eid]=('var',vlen,val.hex())
                        else:
                            if pos+flen>len(inner): rec['__truncated']=True; break
                            val = inner[pos:pos+flen]; pos+=flen
                            if flen==4 and eid in (8,12):
                                rec[eid]=str(ipaddress.IPv4Address(val))
                            elif flen==8 and eid in (1,2):
                                rec[eid]=struct.unpack('!Q', val)[0]
                            else:
                                rec[eid]=val.hex()
                    recs.append(rec)
            else:
                recs.append({'__unknown_template':tpl_id,'raw':inner.hex()})
            data_sets.append({'template':tpl_id,'records':recs})
        offset+=set_len
    return data_sets

if __name__=='__main__':
    if len(sys.argv)<2:
        print('usage: parse_ipfix_minimal.py file1 [file2 ...]')
        sys.exit(1)
    for fn in sys.argv[1:]:
        print('==',fn)
        b=open(fn,'rb').read()
        msg = read_msg(b)
        if not msg:
            print('not an ipfix message or too short')
            continue
        print('version',msg['version'],'msg_len',msg['msg_len'],'obs',msg['obs'])
        templates = parse_templates(msg['body'])
        print('templates found:')
        for tid,fields in templates.items():
            print(' ',tid,':',[(f[0],f[1],f[2],f[3]) for f in fields])
        ds = parse_data(msg['body'],templates)
        for d in ds:
            print('DataSet template',d['template'])
            for r in d['records']:
                for k,v in r.items():
                    if k.startswith('__'): continue
                    print('  IE',k,':',v)
                if '__truncated' in r:
                    print('  record truncated')
        print()
