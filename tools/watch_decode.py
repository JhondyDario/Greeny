# decodifica watch.txt: devuelve por sesion y vigilante la lista (t, addr, bytes, cambios)
import sys,struct
def load(path):
    sessions=[]; cur=None
    for l in open(path,errors='replace'):
        l=l.rstrip('\n')
        if l.startswith('=== sesion'): cur={}; sessions.append(cur); continue
        if not l.startswith('t=') or cur is None: continue
        p=l.split(' '); t=int(p[0][2:]); 
        if len(p)>1 and p[1]=='MARCA': cur.setdefault('marks',[]).append((t if t<1<<60 else 0,int(p[2]))); continue
        if t>1<<60: t=0
        w=int(p[1][1:]); ev=cur.setdefault(w,[])
        if p[2]=='NULL': ev.append((t,None,None,None)); continue
        if p[2].startswith('@'):
            ev.append((t,int(p[2][1:],16),bytearray.fromhex(p[4]),None)); continue
        prev=None
        for e in reversed(ev):
            if e[2] is not None: prev=e; break
        if prev is None: continue
        b=bytearray(prev[2]); ch=[]
        for d in p[2:]:
            if not d: continue
            off,hx=d[1:].split(':'); off=int(off,16); nb=bytes.fromhex(hx)
            ch.append((off,bytes(b[off:off+len(nb)]),nb)); b[off:off+len(nb)]=nb
        ev.append((t,prev[1],b,ch))
    return sessions
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
if __name__=='__main__':
    ss=load(sys.argv[1])
    for i,s in enumerate(ss): print('sesion',i,{w:len(e) for w,e in sorted(s.items())})
