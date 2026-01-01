#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ======================================================
# RAFAELIA X-0 — Núcleo Unificado (ABC Minimal-Ótimo)
# Autor: ∆RafaelVerboΩ
# Proteção: Pre6seal + RAFCODE-Φ + ΣΩΔΦBITRAF
# ======================================================

import sys, array, collections, math

# ======================================================
# CONFIGURAÇÃO BÁSICA (W=31 bits + Paridade) [A+B]
# ======================================================

W = 31
M = (1 << W) - 1
P_MASK = (1 << 31)

MODE_KERNEL = 0
MODE_USER = 1

# ---------- MMIO ----------
MMIO_WDT_PAT    = 0xFFF0
MMIO_SYSCALL_ID = 0xFFF1

# ---------- Vetores ----------
VEC_RESET  = 0xF000
VEC_PRIV   = 0xF004
VEC_SYSCALL= 0xF010
VEC_IRQ_TIMER = 0xF100
VEC_IRQ_MEM   = 0xF101

# ======================================================
# ISA — 80+ INSTRUÇÕES [B]
# (Minimal, sem redundância)
# ======================================================

HALT=0; MOVI=1; MOVR=2; ADD=3; SUB=4; MUL=5; DIV=6; MOD=7
LOAD=8; STORE=9; PUSH=10; POP=11; JMP=12; JZ=13; JNZ=14
INC=15; DEC=16; CMP=17; NEG=18; CLR=19; NOP=20

IRET=21; SAVE_CTX=22; RESTORE_CTX=23; SYSCALL=24; YIELD=25
MAT_MUL=50; MAT_ADD=51; MAT_T=52
STORE_DR=60; LOAD_DR=61; IRQ=70

# ======================================================
# PCB — Processos [C]
# ======================================================
class PCB:
    def __init__(s,pid,base,size,mode):
        s.pid=pid; s.pc=base; s.zf=0; s.mode=mode
        s.regs=array.array('i',[0]*8)
        s.stack=[]; s.state="READY"
        s.base=base; s.size=size
        s.cy=0

# ======================================================
# Cache LRU O(1) [B]
# ======================================================
class FastLRU:
    def __init__(s,sz):
        s.sz=sz; s.c={}; s.q=collections.deque()
    def get(s,a):
        if a not in s.c: return None
        s.q.remove(a); s.q.append(a); return s.c[a]
    def put(s,a,v):
        if a in s.c: s.q.remove(a)
        elif len(s.c)>=s.sz:
            x=s.q.popleft(); del s.c[x]
        s.c[a]=v; s.q.append(a)

# ======================================================
# NÚCLEO — RAFAELIA X-0 VM (A+B+C)
# ======================================================
class RAFAELIA_VM:
    def __init__(s,memsz=0x20000,cache=256):
        s.mem=array.array('i',[0]*memsz)
        s.cache=FastLRU(cache)
        s.ms=memsz

        s.pc=0; s.zf=0; s.mode=MODE_KERNEL
        s.running=True; s.wdt=True

        s.pcb=[]; s.pid=0; s.cy=0

        s.irqq=collections.deque()
        s.dispatch=[None]*80
        s._map()

    # --------------------------------------------------
    # DISPATCH TABLE O(1)
    # --------------------------------------------------
    def _map(s):
        d=s.dispatch
        d[HALT]=s.op_HALT; d[MOVI]=s.op_MOVI; d[MOVR]=s.op_MOVR
        d[ADD]=s.op_ADD; d[SUB]=s.op_SUB; d[MUL]=s.op_MUL
        d[DIV]=s.op_DIV; d[MOD]=s.op_MOD
        d[LOAD]=s.op_LOAD; d[STORE]=s.op_STORE
        d[PUSH]=s.op_PUSH; d[POP]=s.op_POP
        d[JMP]=s.op_JMP; d[JZ]=s.op_JZ; d[JNZ]=s.op_JNZ
        d[INC]=s.op_INC; d[DEC]=s.op_DEC
        d[CMP]=s.op_CMP; d[NEG]=s.op_NEG; d[CLR]=s.op_CLR
        d[NOP]=s.op_NOP

        d[IRET]=s.op_IRET; d[SAVE_CTX]=s.op_SAVE; d[RESTORE_CTX]=s.op_RESTORE
        d[SYSCALL]=s.op_SYSCALL; d[YIELD]=s.op_YIELD

        d[MAT_MUL]=s.op_MATMUL; d[MAT_ADD]=s.op_MATADD; d[MAT_T]=s.op_MATT
        d[STORE_DR]=s.op_STOREDR; d[LOAD_DR]=s.op_LOADDR
        d[IRQ]=s.op_IRQ

    # --------------------------------------------------
    # MEMÓRIA + PARIDADE [B]
    # --------------------------------------------------
    def _checkp(s,v):
        data=v&M; par=(v>>W)&1
        return (bin(data).count('1')%2)==par

    def _mget(s,a):
        if a==MMIO_WDT_PAT: return 0
        if a==MMIO_SYSCALL_ID: return 0
        if a<0 or a>=s.ms: return 0

        c=s.cache.get(a)
        if c is not None: return c&M

        v=s.mem[a]
        if not s._checkp(v): v=0
        s.cache.put(a,v)
        return v&M

    def _mset(s,a,v):
        if a==MMIO_WDT_PAT:
            s.wdt=True; return
        if a==MMIO_SYSCALL_ID: return

        v=v&M
        par=bin(v).count('1')%2
        vf=(par<<W)|v
        s.mem[a]=vf
        s.cache.put(a,vf)

    # --------------------------------------------------
    # IRQ + TRAP [A]
    # --------------------------------------------------
    def irq(s,id): s.irqq.append(id)

    def _handle_irq(s,vec):
        p=s.pcb[s.pid]
        p.stack.append(s.pc); p.stack.append(s.zf); p.stack.append(s.mode)
        s.mode=MODE_KERNEL; s.pc=vec

    # --------------------------------------------------
    # BOOTLOADER [C]
    # --------------------------------------------------
    def boot(s,progs):
        for name,pr in progs.items():
            code,addr,mode=pr["code"],pr["addr"],pr["mode"]
            for i,b in enumerate(code): s.mem[addr+i]=b&M
            s.pcb.append(PCB(len(s.pcb),addr,1024,mode))
            print(f"[BOOT] PCB {len(s.pcb)-1} {name} at {hex(addr)} mode={mode}")

        s.pid=0
        s.pc=s.pcb[0].pc
        s.mode=s.pcb[0].mode
        print("[BOOT] OK. Kernel ativo.")

    # --------------------------------------------------
    # EXECUÇÃO PRINCIPAL
    # --------------------------------------------------
    def run(s,limit=500000):
        s.running=True
        while s.running and s.cy<limit:

            if s.irqq:
                id=s.irqq.popleft()
                vec=VEC_IRQ_TIMER if id==2 else VEC_IRQ_MEM
                s._handle_irq(vec)

            op=s._mget(s.pc)
            s.pc=(s.pc+1)&M
            s.cy+=1

            if 0<=op<80 and s.dispatch[op]:
                s.dispatch[op]()
            else:
                print(f"FAULT: opcode {op}")
                s.running=False

        print(f"[HALT] ciclos={s.cy}")

    # ==================================================
    # OPCODES — Handlers minimalistas
    # ==================================================

    def _fetch(s): v=s._mget(s.pc); s.pc=(s.pc+1)&M; return v
    def _priv(s,op):
        if s.mode==MODE_USER:
            s._handle_irq(VEC_PRIV); return False
        return True

    def op_HALT(s):
        if not s._priv(HALT): return
        s.running=False

    def op_NOP(s): pass

    def op_MOVI(s):
        r=s._fetch(); v=s._fetch()
        s.pcb[s.pid].regs[r]=v&M

    def op_MOVR(s):
        a=s._fetch(); b=s._fetch()
        p=s.pcb[s.pid]; p.regs[a]=p.regs[b]

    def op_ADD(s):
        r=s._fetch(); v=s._fetch()
        p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]+v)&M

    def op_SUB(s):
        r=s._fetch(); v=s._fetch()
        p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]-v)&M

    def op_MUL(s):
        r=s._fetch(); v=s._fetch()
        p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]*v)&M

    def op_DIV(s):
        r=s._fetch(); v=s._fetch()
        if v==0: s._handle_irq(VEC_PRIV); return
        p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]//v)&M

    def op_MOD(s):
        r=s._fetch(); v=s._fetch()
        if v==0: s._handle_irq(VEC_PRIV); return
        p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]%v)&M

    def op_LOAD(s):
        r=s._fetch(); a=s._fetch()
        s.pcb[s.pid].regs[r]=s._mget(a)

    def op_STORE(s):
        a=s._fetch(); r=s._fetch()
        s._mset(a,s.pcb[s.pid].regs[r])

    def op_PUSH(s):
        r=s._fetch(); s.pcb[s.pid].stack.append(s.pcb[s.pid].regs[r])

    def op_POP(s):
        r=s._fetch(); p=s.pcb[s.pid]
        if not p.stack: return
        p.regs[r]=p.stack.pop()

    def op_JMP(s): s.pc=s._fetch()
    def op_JZ(s): d=s._fetch(); s.pc=d if s.zf==1 else s.pc
    def op_JNZ(s): d=s._fetch(); s.pc=d if s.zf==0 else s.pc

    def op_INC(s):
        r=s._fetch(); p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]+1)&M
    def op_DEC(s):
        r=s._fetch(); p=s.pcb[s.pid]; p.regs[r]=(p.regs[r]-1)&M

    def op_NEG(s):
        r=s._fetch(); p=s.pcb[s.pid]; p.regs[r]=(-p.regs[r])&M

    def op_CLR(s):
        r=s._fetch(); s.pcb[s.pid].regs[r]=0

    def op_CMP(s):
        r=s._fetch(); v=s._fetch()
        s.zf=1 if s.pcb[s.pid].regs[r]==v else 0

    # ---------- CONTEXTO ----------
    def op_IRET(s):
        if not s._priv(IRET): return
        p=s.pcb[s.pid]
        if len(p.stack)<3: return
        s.mode=p.stack.pop()
        s.zf=p.stack.pop()
        s.pc=p.stack.pop()

    def op_SAVE(s):
        if not s._priv(SAVE_CTX): return
        base=s._fetch(); p=s.pcb[s.pid]
        s._mset(base,p.pc); s._mset(base+1,p.zf); s._mset(base+2,p.mode)
        for i,r in enumerate(p.regs): s._mset(base+3+i,r)

    def op_RESTORE(s):
        if not s._priv(RESTORE_CTX): return
        base=s._fetch(); p=s.pcb[s.pid]
        s.pc=s._mget(base); s.zf=s._mget(base+1); s.mode=s._mget(base+2)
        for i in range(8): p.regs[i]=s._mget(base+3+i)

    # ---------- SYSCALL ----------
    def op_SYSCALL(s):
        p=s.pcb[s.pid]
        p.stack.append(s.pc); p.stack.append(s.zf); p.stack.append(s.mode)
        s.mode=MODE_KERNEL; s.pc=VEC_SYSCALL

    # ---------- YIELD ----------
    def op_YIELD(s):
        s.pid=(s.pid+1)%len(s.pcb)

    # ---------- I/O ----------
    def op_STOREDR(s):
        a=s._fetch(); r=s._fetch()
        s._mset(a,s.pcb[s.pid].regs[r]); s.irq(1)

    def op_LOADDR(s):
        r=s._fetch(); a=s._fetch()
        s.pcb[s.pid].regs[r]=s._mget(a)

    def op_IRQ(s):
        id=s._fetch(); s.irq(id)

    # ---------- MATRIZ ----------
    def off(s,b,i,j,cols): return b+i*cols+j

    def op_MATMUL(s):
        if not s._priv(MAT_MUL): return
        bA=s._fetch(); bB=s._fetch(); bC=s._fetch()
        m=s._fetch(); n=s._fetch(); p=s._fetch()
        blk=8
        for ii in range(0,m,blk):
            ib=min(blk,m-ii)
            for kk in range(0,n,blk):
                kb=min(blk,n-kk)
                for jj in range(0,p,blk):
                    jb=min(blk,p-jj)
                    for i in range(ib):
                        for j in range(jb):
                            acc=0
                            for k in range(kb):
                                A=s._mget(s.off(bA,ii+i,kk+k,n))
                                B=s._mget(s.off(bB,kk+k,jj+j,p))
                                acc+=A*B
                            s._mset(s.off(bC,ii+i,jj+j,p),acc&M)

    def op_MATADD(s):
        if not s._priv(MAT_ADD): return
        bA=s._fetch(); bB=s._fetch(); bC=s._fetch()
        m=s._fetch(); n=s._fetch()
        for i in range(m):
            for j in range(n):
                s._mset(s.off(bC,i,j,n),
                        (s._mget(s.off(bA,i,j,n))+s._mget(s.off(bB,i,j,n)))&M)

    def op_MATT(s):
        if not s._priv(MAT_T): return
        bA=s._fetch(); bT=s._fetch(); m=s._fetch(); n=s._fetch()
        for i in range(m):
            for j in range(n):
                s._mset(s.off(bT,j,i,m), s._mget(s.off(bA,i,j,n)))

# ======================================================
# DEMO PROGRAM
# ======================================================
if __name__=="__main__":
    # Bytecode simples
    def enc(op,*args): return [op]+list(args)
    P0=[
        *enc(STORE,MMIO_WDT_PAT,0),
        *enc(YIELD),
        *enc(JMP,0x1000)
    ]
    P1=[
        *enc(MOVI,0,10),
        *enc(MOVI,1,20),
        *enc(ADD,0,1),
        *enc(HALT)
    ]
    vm=RAFAELIA_VM()
    vm.boot({
        "KERNEL":{"code":P0,"addr":0x1000,"mode":MODE_KERNEL},
        "USER1": {"code":P1,"addr":0x2000,"mode":MODE_USER},
    })
    vm.run(200000)
    print("R0 USER =",vm.pcb[1].regs[0])
