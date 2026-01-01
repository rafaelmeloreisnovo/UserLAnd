# RAFAELIA VM — General Purpose Computation Unit (GPCU) v1.2 (FIXED)
# Correções aplicadas: handlers omitidos reimplementados (ADD/SUB/MUL/DIV/MOD/INC/DEC/NEG/CLR),
# handlers de matriz MAT_ADD e MAT_T implementados, pequenos ajustes em contagem de ciclos,
# correções no boot/context switch/irq handling e proteção de acesso.
#
# Mantenha este arquivo em um repositório seguro. Execute localmente para testes unitários.
#
import json
import collections
import os
import sys
import math
import array
from typing import List, Tuple, Any, Dict, Callable, Optional, Deque

# --- Configuração Arquitetural (V3/V7) ---
W = 31  # 31 bits de dados (0..30)
M = (1 << W) - 1  # máscara de dados (0x7FFFFFFF)
P_MASK = (1 << W)  # bit de paridade na posição W (bit 31)

# --- Opcodes (Numéricos) ---
HALT = 0; MOVI = 1; MOVR = 2; ADD = 3; SUB = 4; MUL = 5; DIV = 6; MOD = 7
LOAD = 8; STORE = 9; PUSH = 10; POP = 11; JMP = 12; JZ = 13; JNZ = 14
INC = 15; DEC = 16; CMP = 17; NEG = 18; CLR = 19; NOP = 20
# Opcodes de Kernel/Contexto (v1.1)
IRET = 21; SAVE_CTX = 22; RESTORE_CTX = 23; SYSCALL = 24; YIELD = 25
# Opcodes de Co-processador (Δ)
MAT_MUL = 50; MAT_ADD = 51; MAT_T = 52
# Opcodes de I/O (π)
STORE_DR = 60; LOAD_DR = 61
IRQ = 70

# --- Níveis de Privilégio (v1.1) ---
MODE_KERNEL = 0
MODE_USER = 1

# --- Vetores de Interrupção (V6) ---
VEC_BASE = 0xF000
VEC_RESET = 0xF000         # Boot
VEC_OPUNK = 0xF001         # Opcode Inválido
VEC_DIV0 = 0xF002          # Divisão por Zero
VEC_MA = 0xF003            # Falha de Acesso à Memória
VEC_PRIV = 0xF004          # Falha de Privilégio (v1.1)
VEC_SYSCALL = 0xF010       # v1.1
VEC_IRQ_TIMER = 0xF100     # v1.1 (WDT)
VEC_IRQ_MEMDRIVE = 0xF101

# --- Endereços MMIO (V8) ---
MMIO_WDT_PAT = 0xFFF0  # Watchdog "Pat" (Acariciar)
MMIO_SYSCALL_REG = 0xFFF1 # Registrador de ID da Syscall

# --- Process Control Block (PCB) (v1.1) ---
class PCB:
    def __init__(s, pid: int, base: int, size: int, mode: int):
        s.pid = pid
        s.pc = base  # Início do código
        s.regs = array.array('i', [0] * 8) # 8 registradores
        s.stack: List[int] = [] # Pilha do processo
        s.zf = 0
        s.mode = mode # MODE_KERNEL ou MODE_USER
        s.state = "READY" # READY, RUNNING, WAIT
        s.mem_base = base
        s.mem_size = size
        s.cy_ran = 0 # Ciclos executados

# --- Otimização de Cache (V3 / 50x) ---
class FastLRUCache:
    """Cache simples: dict + deque order. get/put atualizam ordem."""
    def __init__(s, size: int):
        s.size = size
        s.cache: Dict[int, int] = {}
        s.order: Deque[int] = collections.deque()

    def get(s, a: int) -> Optional[int]:
        if a not in s.cache:
            return None
        # atualizar ordem (remover e adicionar no final)
        try:
            s.order.remove(a)
        except ValueError:
            pass
        s.order.append(a)
        return s.cache[a]

    def put(s, a: int, v: int) -> Optional[int]:
        evicted_addr = None
        if a in s.cache:
            try:
                s.order.remove(a)
            except ValueError:
                pass
        elif len(s.cache) >= s.size:
            evicted_addr = s.order.popleft()
            del s.cache[evicted_addr]
        
        s.cache[a] = v
        s.order.append(a)
        return evicted_addr

class R:
    def __init__(s, memsz=4096, cache_sz=128, mdpath=None, mdlat=500):
        s.ms = memsz
        s.md = mdpath
        s.mdlat = mdlat
        s.mem = array.array('i', [0] * memsz)
        s.pc = 0
        s.zf = 0
        s.mode = MODE_KERNEL
        s.running = True
        s.wdt_patted = True
        s.cache = FastLRUCache(cache_sz)
        s.stash = None
        s.stash_max = 0
        s.pcb: List[PCB] = []
        s.pid_current = 0
        s.pid_next = 0
        s.irq_queue: Deque[Tuple[int, Any]] = collections.deque()
        s.irq_vector_table = {
            1: VEC_IRQ_MEMDRIVE,
            2: VEC_IRQ_TIMER,
        }
        s.cy = 0
        s.stat = {"ins": 0, "hits": 0, "miss": 0, "mda": 0, "irqs": 0, "faults": 0}
        s.dispatch: List[Optional[Callable]] = [None] * 128
        s._map_dispatch()

    def _map_dispatch(s):
        d = s.dispatch
        d[HALT]=s.op_HALT; d[MOVI]=s.op_MOVI; d[MOVR]=s.op_MOVR; d[ADD]=s.op_ADD
        d[SUB]=s.op_SUB; d[MUL]=s.op_MUL; d[DIV]=s.op_DIV; d[MOD]=s.op_MOD
        d[LOAD]=s.op_LOAD; d[STORE]=s.op_STORE; d[PUSH]=s.op_PUSH; d[POP]=s.op_POP
        d[JMP]=s.op_JMP; d[JZ]=s.op_JZ; d[JNZ]=s.op_JNZ; d[INC]=s.op_INC
        d[DEC]=s.op_DEC; d[CMP]=s.op_CMP; d[NEG]=s.op_NEG; d[CLR]=s.op_CLR
        d[NOP]=s.op_NOP
        d[IRET]=s.op_IRET; d[SAVE_CTX]=s.op_SAVE_CTX; d[RESTORE_CTX]=s.op_RESTORE_CTX
        d[SYSCALL]=s.op_SYSCALL; d[YIELD]=s.op_YIELD
        d[MAT_MUL]=s.op_MAT_MUL; d[MAT_ADD]=s.op_MAT_ADD; d[MAT_T]=s.op_MAT_T
        d[STORE_DR]=s.op_STORE_DR; d[LOAD_DR]=s.op_LOAD_DR; d[IRQ]=s.op_IRQ

    # --- V1.1: Gerenciador de Contexto e Processo (Kernel) ---
    def _switch_context(s, next_pid: int):
        """Salva o contexto atual e carrega o próximo."""
        if not s.pcb:
            return
        if next_pid < 0 or next_pid >= len(s.pcb):
            return
        if s.pid_current == next_pid:
            return

        p_old = s.pcb[s.pid_current]
        p_old.pc = s.pc
        p_old.zf = s.zf
        p_old.state = "READY"
        # Salva regs já estão no PCB

        p_new = s.pcb[next_pid]
        s.pc = p_new.pc
        s.zf = p_new.zf
        s.mode = p_new.mode
        p_new.state = "RUNNING"
        s.pid_current = next_pid
        s.cy += 50

    def _scheduler(s):
        """Scheduler cooperativo simples (Round-Robin)."""
        if not s.pcb:
            return
        # Encontra o próximo processo READY (simples)
        start = (s.pid_current + 1) % len(s.pcb)
        next_pid = start
        while s.pcb[next_pid].state != "READY":
             next_pid = (next_pid + 1) % len(s.pcb)
             if next_pid == start:
                 # nenhum outro READY encontrado
                 return
        
        if next_pid != s.pid_current:
            s._switch_context(next_pid)

    # --- V6: Sistema de Interrupção e Falha (π) ---
    def inject_irq(s, irq_id: int, payload: Any = None):
        s.irq_queue.append((irq_id, payload))

    def _fault(s, fault_code: int, vector_addr: int, op: int):
        """Gera uma Falha (Trap) interna."""
        s.stat["faults"] += 1
        
        # Processos de usuário (User) têm a falha tratada pelo Kernel
        if s.mode == MODE_USER and s.pcb:
            print(f"USER FAULT: PID={s.pid_current} Code={hex(fault_code)} (op={op}) at PC={s.pc-1}. Trapping to Kernel.", file=sys.stderr)
            # Salva o contexto do User
            p = s.pcb[s.pid_current]
            p.stack.append(s.pc)
            p.stack.append(s.zf)
            p.stack.append(s.mode)
            # Entra no Kernel
            s.mode = MODE_KERNEL
            s.pc = vector_addr
            s.cy += 10
        else:
            # Falha no Kernel (Modo 0) é fatal
            s.running = False
            print(f"KERNEL FAULT: Code={hex(fault_code)} (op={op}) at PC={s.pc-1}. VM Halted.", file=sys.stderr)

    def _handle_irq(s, vector_addr: int):
        """v1.1: Salva contexto e pula para o vetor IRQ (em Modo Kernel)."""
        s.stat["irqs"] += 1
        
        if not s.pcb:
            s.mode = MODE_KERNEL
            s.pc = vector_addr
            s.cy += 10
            return

        p = s.pcb[s.pid_current]
        p.stack.append(s.pc)
        p.stack.append(s.zf)
        p.stack.append(s.mode)
        
        s.mode = MODE_KERNEL
        s.pc = vector_addr
        s.cy += 10

    # --- V3 / V8: Sistema de Memória/Cache (φ) ---
    def _check_parity(s, v: int) -> bool:
        """V3: Simulação de Paridade (Simplista: conta bits)"""
        data = v & M
        par = (v & P_MASK) >> W
        count = bin(data).count('1')
        expected_par = count % 2
        return expected_par == par

    def _mget(s, a):
        # MMIO handling (read-only stub values)
        if a == MMIO_WDT_PAT: return 0
        if a == MMIO_SYSCALL_REG: return 0
        
        if not (0 <= a < s.ms):
            s._fault(VEC_MA, VEC_MA, LOAD); return 0
        
        v_cache = s.cache.get(a)
        if v_cache is not None:
            s.stat["hits"] += 1; s.cy += 1; return (v_cache & M)
        
        s.stat["miss"] += 1
        v_mem = s.mem[a]
        
        if not s._check_parity(v_mem):
             s._fault(VEC_MA, VEC_MA, LOAD); v_mem = 0

        s.cache.put(a, v_mem)
        s.cy += 5
        return (v_mem & M)

    def _mset(s, a, v):
        v = v & M
        
        if a == MMIO_WDT_PAT:
            s.wdt_patted = True; s.cy += 1; return
        if a == MMIO_SYSCALL_REG:
            # Essa escrita pode sinalizar uma syscall - tratamos como mem write por compatibilidade
            s.cy += 1
            # pass

        if not (0 <= a < s.ms):
            s._fault(VEC_MA, VEC_MA, STORE); return
        
        count = bin(v).count('1')
        par = count % 2
        v_final = (par << W) | v
        
        s.cache.put(a, v_final)
        s.mem[a] = v_final
        s.cy += 5

    # --- V3: Memdrive (I/O) ---
    def _md_write(s, a, v):
        if s.md is None: s._fault(VEC_MA, VEC_MA, STORE_DR); return
        s.cy += 2; s.stat["mda"] += 1
        # Simulação de I/O assíncrono (Stub)
        s.mem[a] = v & M # (Escrita direta por enquanto)
        s.inject_irq(1, payload=a) # ID 1 = MEMDRIVE_DONE

    def _md_read(s, a):
        if s.md is None: s._fault(VEC_MA, VEC_MA, LOAD_DR); return 0
        s.cy += s.mdlat; s.stat["mda"] += 1
        v = s.mem[a] # (Leitura direta)
        s._mset(a, v)
        return (v & M)

    # --- V4: Stash (φ) ---
    def build_stash(s, maxv=255):
        mm = maxv
        t = [[(i * j) & M for j in range(mm + 1)] for i in range(mm + 1)]
        s.stash = t; s.stash_max = mm
        s.cy += ((mm + 1) ** 2) // 20
        return t

    def off(s, base, i, j, nc, stride=None):
        if stride is None: stride = nc
        return base + i * stride + j

    # --- Loop de Execução Principal (V1 / v1.1) ---
    def _load_mem(s, code: list, addr: int):
        """Carrega bytecode na memória (ignora paridade no boot)."""
        if addr + len(code) > s.ms:
            raise Exception(f"ROM excede a memória (addr={hex(addr)})")
        for i, byte in enumerate(code):
            # Carrega sem paridade no boot
            s.mem[addr + i] = int(byte) & M

    def _boot(s, programs: dict):
        """V1.2: Bootloader. Carrega programas e cria PCBs."""
        
        for name, info in programs.items():
            s._load_mem(info["code"], info["addr"])
            
            if "pcb" in info and info["pcb"]:
                p = PCB(pid=len(s.pcb),
                        base=info["addr"],
                        size=info.get("size", 1024),
                        mode=info.get("mode", MODE_USER))
                s.pcb.append(p)
                print(f"GPCU v1.2: Criado PCB {p.pid} ({name}) em {hex(p.mem_base)}, Mode={p.mode}")

        if not s.pcb:
            raise Exception("Bootloader: Nenhum PCB criado.")
            
        s.pid_current = 0
        p0 = s.pcb[0]
        p0.state = "RUNNING"
        
        s.pc = p0.pc
        s.zf = p0.zf
        s.mode = p0.mode
        
        print(f"GPCU v1.2: Boot OK. PID 0 em {hex(p0.pc)}. Iniciando...")
        
    def run(s, programs: dict, max_cycles=1000000):
        
        try:
            s._boot(programs)
        except Exception as e:
            print(f"Bootloader Falhou: {e}", file=sys.stderr)
            return

        s.running = True
        
        while s.running:
            if s.cy >= max_cycles:
                print(f"VM Atingiu Limite de Ciclos ({max_cycles})")
                break

            # V6: Processa IRQs
            if s.irq_queue:
                irq_id, payload = s.irq_queue.popleft()
                
                # CORREÇÃO V1.2: interpretar IRQ timer/memdrive
                if irq_id == 2: # (VEC_IRQ_TIMER)
                    if not s.wdt_patted:
                        print("WDT FAULT: REBOOTING", file=sys.stderr)
                        s._fault(VEC_RESET, VEC_RESET, -1)
                        break
                    s.wdt_patted = False
                
                vector = s.irq_vector_table.get(irq_id, VEC_BASE)
                s._handle_irq(vector)
                if not s.running: break # Fault no IRQ

            # Leitura da instrução (PC aponta para endereço)
            op = s._mget(s.pc)
            
            handler = s.dispatch[op] if 0 <= op < len(s.dispatch) else None
            
            # Avança PC para o próximo byte/unidade
            s.pc = (s.pc + 1) & M
            s.stat["ins"] += 1
            if s.pcb:
                s.pcb[s.pid_current].cy_ran += 1
            
            if handler:
                handler()
            else:
                s._fault(VEC_OPUNK, VEC_OPUNK, op)
        
        print(f"VM Halted. Total Cycles: {s.cy}")

    # --- Handlers de Instrução (V1 / v1.1) ---
    def _fetch(s):
        """Busca o próximo byte/palavra e avança PC."""
        v = s._mget(s.pc)
        s.pc = (s.pc + 1) & M
        return v
        
    def _check_priv(s, op):
        """V1.1: Verifica se a instrução é privilegiada."""
        # Defina aqui a lista de opcodes que exigem modo Kernel
        privileged = {IRET, SAVE_CTX, RESTORE_CTX, SYSCALL, MAT_MUL, MAT_ADD, MAT_T}
        if op in privileged and s.mode == MODE_USER:
            s._fault(VEC_PRIV, VEC_PRIV, op); return False
        return True

    def op_HALT(s):
        if not s._check_priv(HALT): return
        s.running = False; s.cy += 1
    
    def op_NOP(s): s.cy += 0
    
    def op_MOVI(s):
        r = s._fetch(); v = s._fetch()
        p = s.pcb[s.pid_current]
        p.regs[r] = v & M; s.cy += 1
    
    def op_MOVR(s):
        r_dst = s._fetch(); r_src = s._fetch()
        p = s.pcb[s.pid_current]
        p.regs[r_dst] = p.regs[r_src]
        s.cy += 1
    
    # --- CORREÇÃO V1.2 (Handlers Omitidos) ---
    def op_ADD(s):
        r = s._fetch(); v = s._fetch()
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] + v) & M; s.cy += 1
    
    def op_SUB(s):
        r = s._fetch(); v = s._fetch()
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] - v) & M; s.cy += 1

    def op_MUL(s):
        r = s._fetch(); v = s._fetch()
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] * v) & M; s.cy += 2

    def op_DIV(s):
        r = s._fetch(); v = s._fetch()
        if v == 0: s._fault(VEC_DIV0, VEC_DIV0, DIV); return
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] // v) & M; s.cy += 3

    def op_MOD(s):
        r = s._fetch(); v = s._fetch()
        if v == 0: s._fault(VEC_DIV0, VEC_DIV0, MOD); return
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] % v) & M; s.cy += 3

    def op_INC(s):
        r = s._fetch()
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] + 1) & M; s.cy += 1
        
    def op_DEC(s):
        r = s._fetch()
        regs = s.pcb[s.pid_current].regs
        regs[r] = (regs[r] - 1) & M; s.cy += 1

    def op_NEG(s):
        r = s._fetch()
        regs = s.pcb[s.pid_current].regs
        regs[r] = (-regs[r]) & M; s.cy += 1

    def op_CLR(s):
        r = s._fetch()
        s.pcb[s.pid_current].regs[r] = 0; s.cy += 1

    # --- Handlers Padrão ---
    def op_LOAD(s):
        r = s._fetch(); a = s._fetch()
        s.pcb[s.pid_current].regs[r] = s._mget(a)
        s.cy += 2
    
    def op_STORE(s):
        a = s._fetch(); r = s._fetch()
        s._mset(a, s.pcb[s.pid_current].regs[r])
        s.cy += 2

    def op_PUSH(s):
        r = s._fetch()
        s.pcb[s.pid_current].stack.append(s.pcb[s.pid_current].regs[r])
        s.cy += 2
    
    def op_POP(s):
        r = s._fetch()
        p = s.pcb[s.pid_current]
        if not p.stack: s._fault(VEC_MA, VEC_MA, POP); return
        p.regs[r] = p.stack.pop(); s.cy += 2
    
    def op_JMP(s): s.pc = s._fetch(); s.cy += 1
    def op_JZ(s): d = s._fetch(); s.pc = d if s.zf == 1 else s.pc; s.cy += 1
    def op_JNZ(s): d = s._fetch(); s.pc = d if s.zf == 0 else s.pc; s.cy += 1
    
    def op_CMP(s):
        r1 = s._fetch(); v = s._fetch() # (Simples R, V)
        s.zf = 1 if s.pcb[s.pid_current].regs[r1] == v else 0; s.cy += 1

    # --- Handlers v1.1 (Kernel/Contexto) ---
    def op_IRET(s):
        if not s._check_priv(IRET): return
        p = s.pcb[s.pid_current]
        if len(p.stack) < 3: s._fault(VEC_MA, VEC_MA, IRET); return
        s.mode = p.stack.pop()
        s.zf = p.stack.pop()
        s.pc = p.stack.pop()
        s.cy += 10
        
    def op_SAVE_CTX(s):
        if not s._check_priv(SAVE_CTX): return
        base = s._fetch()
        p = s.pcb[s.pid_current]
        s._mset(base + 0, p.pc); s._mset(base + 1, p.zf); s._mset(base + 2, p.mode)
        for i, r_val in enumerate(p.regs):
            s._mset(base + 3 + i, r_val)
        s.cy += 20
        
    def op_RESTORE_CTX(s):
        if not s._check_priv(RESTORE_CTX): return
        base = s._fetch()
        p = s.pcb[s.pid_current]
        s.pc = s._mget(base + 0); s.zf = s._mget(base + 1); s.mode = s._mget(base + 2)
        for i in range(len(p.regs)):
            p.regs[i] = s._mget(base + 3 + i)
        s.cy += 20

    def op_SYSCALL(s):
        p = s.pcb[s.pid_current]
        p.stack.append(s.pc); p.stack.append(s.zf); p.stack.append(s.mode)
        s.mode = MODE_KERNEL
        s.pc = VEC_SYSCALL
        s.cy += 10
        
    def op_YIELD(s):
        s._scheduler()
        s.cy += 1
    
    # --- CORREÇÃO V1.2 (I/O) ---
    def op_STORE_DR(s):
        a = s._fetch(); r = s._fetch()
        s._md_write(a, s.pcb[s.pid_current].regs[r])
        s.cy += 2

    def op_LOAD_DR(s):
        r = s._fetch(); a = s._fetch()
        s.pcb[s.pid_current].regs[r] = s._md_read(a)
        s.cy += 2
        
    def op_IRQ(s):
        # Dispara uma IRQ (para teste)
        id = s._fetch()
        s.inject_irq(id)
        s.cy += 1

    # --- Handlers de Matriz (Δ) ---
    def op_MAT_MUL(s):
        if not s._check_priv(MAT_MUL): return
        bA=s._fetch(); bB=s._fetch(); bC=s._fetch()
        m=s._fetch(); n=s._fetch(); p=s._fetch()
        blk=s._fetch(); us=s._fetch()
        
        if blk < 1: blk = 8
        us = 1 if us != 0 else 0
        sm = s.stash if us and s.stash is not None else None
        smm = s.stash_max if sm is not None else -1
        
        # Lógica de MAT_MUL (Δ)
        for ii in range(0, m, blk):
            ib = min(blk, m - ii)
            for kk in range(0, n, blk):
                kb = min(blk, n - kk)
                A = [[0] * kb for _ in range(ib)]
                for i in range(ib):
                    for k in range(kb):
                        A[i][k] = int(s._mget(s.off(bA, ii + i, kk + k, n)))
                for jj in range(0, p, blk):
                    jb = min(blk, p - jj)
                    B = [[0] * jb for _ in range(kb)]
                    for k in range(kb):
                        for j in range(jb):
                            B[k][j] = int(s._mget(s.off(bB, kk + k, jj + j, p)))
                    C = [[int(s._mget(s.off(bC, ii + i, jj + j, p))) for j in range(jb)] for i in range(ib)]
                    
                    if sm is not None:
                        for i in range(ib):
                            for k in range(kb):
                                av = A[i][k]
                                if 0 <= av <= smm:
                                    row = sm[av]
                                    for j in range(jb):
                                        bv = B[k][j]
                                        if 0 <= bv <= smm:
                                            C[i][j] = (C[i][j] + row[bv]) & M
                                        else:
                                            C[i][j] = (C[i][j] + av * bv) & M
                                else:
                                    for j in range(jb):
                                        C[i][j] = (C[i][j] + av * B[k][j]) & M
                    else:
                        for i in range(ib):
                            for k in range(kb):
                                av = A[i][k]
                                for j in range(jb):
                                    C[i][j] = (C[i][j] + av * B[k][j]) & M
                    
                    for i in range(ib):
                        for j in range(jb):
                            s._mset(s.off(bC, ii + i, jj + j, p), int(C[i][j]) & M)
                            
        s.cy += max(1, (m * n * p) // 10)
    
    def op_MAT_ADD(s):
        """Element-wise matrix addition: C = A + B
           Operands: bA, bB, bC, rows, cols
        """
        if not s._check_priv(MAT_ADD): return
        bA = s._fetch(); bB = s._fetch(); bC = s._fetch()
        rows = s._fetch(); cols = s._fetch()
        blk = s._fetch()  # optional blocking
        if blk < 1: blk = cols
        # iterate by blocks to limit working set
        for i in range(0, rows, blk):
            rb = min(blk, rows - i)
            for j in range(0, cols, blk):
                cb = min(blk, cols - j)
                for ii in range(rb):
                    for jj in range(cb):
                        a_val = s._mget(s.off(bA, i + ii, j + jj, cols))
                        b_val = s._mget(s.off(bB, i + ii, j + jj, cols))
                        c_val = (a_val + b_val) & M
                        s._mset(s.off(bC, i + ii, j + jj, cols), c_val)
        s.cy += max(1, (rows * cols) // 20)

    def op_MAT_T(s):
        """Transpose matrix: write transpose of A into B
           Operands: bA, bB, rows, cols
        """
        if not s._check_priv(MAT_T): return
        bA = s._fetch(); bB = s._fetch()
        rows = s._fetch(); cols = s._fetch()
        blk = s._fetch()
        if blk < 1: blk = cols
        for i in range(0, rows, blk):
            rb = min(blk, rows - i)
            for j in range(0, cols, blk):
                cb = min(blk, cols - j)
                for ii in range(rb):
                    for jj in range(cb):
                        a_val = s._mget(s.off(bA, i + ii, j + jj, cols))
                        s._mset(s.off(bB, j + jj, i + ii, rows), a_val)
        s.cy += max(1, (rows * cols) // 20)

# EOF