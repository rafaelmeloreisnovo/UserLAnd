#define _POSIX_C_SOURCE 200809L
#include "gpcu.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Simple string instr memory; parse very small set */

static char *strdup_s(const char *s) {
    if(!s) return NULL;
    size_t n = strlen(s)+1;
    char *t = malloc(n);
    if(t) memcpy(t, s, n);
    return t;
}

GPCU *gpcu_create(int memory_size) {
    GPCU *g = (GPCU*)calloc(1, sizeof(GPCU));
    g->memory_size = memory_size;
    g->memory = (char**)calloc(memory_size, sizeof(char*));
    g->next_pid = 1;
    g->proc_capacity = 16;
    g->procs = (PCB**)calloc(g->proc_capacity, sizeof(PCB*));
    return g;
}
void gpcu_free(GPCU *g) {
    if(!g) return;
    for(int i=0;i<g->memory_size;i++) if(g->memory[i]) free(g->memory[i]);
    free(g->memory);
    for(int i=0;i<g->proc_capacity;i++) if(g->procs[i]) free(g->procs[i]);
    free(g->procs);
    free(g);
}

/* load program into memory at pid*block per earlier design */
PCB *gpcu_create_process(GPCU *g, const char **program, int prog_len) {
    if(!g || !program) return NULL;
    int pid = g->next_pid++;
    PCB *pcb = (PCB*)calloc(1, sizeof(PCB));
    pcb->pid = pid;
    pcb->state = 0;
    int base = pid * 256;
    for(int i=0;i<prog_len && base + i < g->memory_size;i++) {
        if(g->memory[base+i]) free(g->memory[base+i]);
        g->memory[base+i] = strdup_s(program[i]);
    }
    pcb->pc = base;
    if(pid-1 >= g->proc_capacity) {
        int newcap = g->proc_capacity * 2;
        g->procs = (PCB**)realloc(g->procs, newcap * sizeof(PCB*));
        for(int i=g->proc_capacity;i<newcap;i++) g->procs[i]=NULL;
        g->proc_capacity = newcap;
    }
    g->procs[pid-1] = pcb;
    return pcb;
}

/* fetch */
static const char *gpcu_fetch(GPCU *g, PCB *pcb) {
    int pc = pcb->pc;
    if(pc < 0 || pc >= g->memory_size) return NULL;
    return g->memory[pc];
}

/* decode/execute basic */
static void execute_instr(PCB *pcb, const char *instr) {
    if(!instr) { pcb->state = 2; return; }
    char op[32];
    int r1, r2;
    if(sscanf(instr, "%31s %n", op, (int[]){0})<0) { pcb->pc++; return; }
    if(strcmp(op,"NOP")==0) { pcb->pc++; return; }
    if(strcmp(op,"HALT")==0) { pcb->state = 2; return; }
    if(strcmp(op,"MOV")==0) {
        /* MOV rX imm */
        if(sscanf(instr, "MOV r%d %d", &r1, &r2)==2) { if(r1>=0 && r1<8) pcb->regs[r1]=r2; pcb->zero_flag = (pcb->regs[r1]==0); }
        pcb->pc++; return;
    }
    if(strcmp(op,"ADD")==0) {
        if(sscanf(instr, "ADD r%d r%d", &r1, &r2)==2) { if(r1>=0 && r1<8 && r2>=0 && r2<8) pcb->regs[r1]+=pcb->regs[r2]; pcb->zero_flag = (pcb->regs[r1]==0); }
        pcb->pc++; return;
    }
    if(strcmp(op,"SUB")==0) {
        if(sscanf(instr, "SUB r%d r%d", &r1, &r2)==2) { if(r1>=0 && r1<8 && r2>=0 && r2<8) pcb->regs[r1]-=pcb->regs[r2]; pcb->zero_flag = (pcb->regs[r1]==0); }
        pcb->pc++; return;
    }
    if(strcmp(op,"JMP")==0) {
        if(sscanf(instr, "JMP %d", &r1)==1) pcb->pc = r1;
        else pcb->pc++;
        return;
    }
    if(strcmp(op,"JZ")==0) {
        if(sscanf(instr, "JZ %d", &r1)==1) { if(pcb->zero_flag) pcb->pc = r1; else pcb->pc++; }
        return;
    }
    if(strcmp(op,"HASH")==0) {
        if(sscanf(instr, "HASH r%d", &r1)==1 && r1>=0 && r1<8) {
            uint32_t val = (uint32_t)pcb->regs[r1];
            uint32_t hashed = (uint32_t)((uint64_t)val * 2654435761u);
            pcb->regs[r1] = (int)hashed;
        }
        pcb->pc++; return;
    }
    /* unknown => NOP */
    pcb->pc++;
}

void gpcu_step(PCB *pcb, GPCU *g, int max_instructions) {
    if(!pcb || !g) return;
    int executed = 0;
    pcb->state = 1;
    while(executed < max_instructions && pcb->state != 2) {
        const char *instr = gpcu_fetch(g, pcb);
        execute_instr(pcb, instr);
        executed++;
    }
    if(pcb->state != 2) pcb->state = 0; /* back to ready */
}

PCB *gpcu_run_program(GPCU *g, const char **program, int prog_len, int max_steps) {
    PCB *pcb = gpcu_create_process(g, program, prog_len);
    if(!pcb) return NULL;
    int step = 0;
    while(pcb->state != 2 && step < max_steps) {
        gpcu_step(pcb, g, 10);
        step++;
    }
    return pcb;
}