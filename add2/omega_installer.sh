#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

APP_DIR="rafaelia_omega"
SRC_FILE="${APP_DIR}/omega_kernel.c"
EXE_FILE="${APP_DIR}/omega"

mkdir -p "${APP_DIR}"

cat > "${SRC_FILE}" << 'EOF'
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define DB_FILE       "omega_mem.raf"
#define MAX_ITEMS     50000
#define CONTENT_SIZE  256
#define VEC_BITS      1024
#define VEC_BYTES     (VEC_BITS/8)
#define MAGIC_SIG     0x52414641u

#define C_RST "\033[0m"
#define C_RED "\033[31m"
#define C_GRN "\033[32m"
#define C_YEL "\033[33m"
#define C_BLU "\033[34m"
#define C_CYN "\033[36m"
#define C_Neon "\033[38;5;82m"
#define C_Pink "\033[38;5;198m"

typedef struct __attribute__((packed)) {
  uint32_t id;
  uint64_t timestamp;
  uint32_t crc32;
  uint8_t  vector[VEC_BYTES];
  uint8_t  active;
  char     content[CONTENT_SIZE];
} OmegaCell;

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t version;
  uint64_t count;
  uint64_t capacity;
  uint64_t dirty_flag;
} OmegaHeader;

static int db_fd = -1;
static void* db_map = NULL;
static OmegaHeader* header = NULL;
static OmegaCell* cells = NULL;
static size_t db_size = 0;

static void r_print(const char* s){ write(1, s, (size_t)strlen(s)); }
static void r_print_int(long n){
  char buf[32]; int i=30; buf[31]=0;
  if(n==0){ r_print("0"); return; }
  int neg = (n<0); if(neg) n=-n;
  while(n>0){ buf[i--]=(char)('0'+(n%10)); n/=10; }
  if(neg) buf[i--]='-';
  r_print(&buf[i+1]);
}
static void r_cls(){ r_print("\033[2J\033[H"); }

static uint32_t calc_crc32(const char *data){
  uint32_t crc=0xFFFFFFFFu;
  while(*data){
    crc ^= (unsigned char)*data++;
    for(int k=0;k<8;k++) crc = (crc>>1) ^ (0xEDB88320u & (uint32_t)(-(int)(crc&1u)));
  }
  return ~crc;
}
static uint64_t hash_fnv1a(const char *data){
  uint64_t h=0xcbf29ce484222325ULL;
  while(*data){ h^=(unsigned char)*data++; h*=0x100000001b3ULL; }
  return h;
}
static void vector_project(const char *text, uint8_t *vec){
  uint64_t seed = hash_fnv1a(text);
  memset(vec,0,VEC_BYTES);
  for(int i=0;i<VEC_BITS;i++){
    seed = seed*6364136223846793005ULL + 1442695040888963407ULL;
    if(seed & 1ULL) vec[i>>3] |= (uint8_t)(1u << (i & 7));
  }
}
static int vector_distance(const uint8_t *v1, const uint8_t *v2){
  int dist=0;
  for(int i=0;i<VEC_BYTES;i++){
    uint8_t x = (uint8_t)(v1[i]^v2[i]);
    while(x){ dist++; x &= (uint8_t)(x-1); }
  }
  return dist;
}

static void db_init(){
  db_fd = open(DB_FILE, O_RDWR|O_CREAT, 0644);
  if(db_fd<0){ r_print("FATAL: open\n"); exit(1); }

  size_t head_sz = sizeof(OmegaHeader);
  size_t cell_sz = sizeof(OmegaCell) * (size_t)MAX_ITEMS;
  db_size = head_sz + cell_sz;

  struct stat st;
  if(fstat(db_fd,&st)<0){ r_print("FATAL: fstat\n"); exit(1); }

  if(st.st_size < (off_t)db_size){
    if(ftruncate(db_fd,(off_t)db_size)<0){ r_print("FATAL: ftruncate\n"); exit(1); }
  }

  db_map = mmap(NULL, db_size, PROT_READ|PROT_WRITE, MAP_SHARED, db_fd, 0);
  if(db_map==MAP_FAILED){ r_print("FATAL: mmap\n"); exit(1); }

  header = (OmegaHeader*)db_map;
  cells  = (OmegaCell*)((uint8_t*)db_map + head_sz);

  if(header->magic != MAGIC_SIG){
    memset(db_map,0,db_size);
    header->magic = MAGIC_SIG;
    header->version = 3;
    header->capacity = MAX_ITEMS;
    r_print(C_Neon "[KERNEL] Novo Universo Criado.\n" C_RST);
  }
}
static void db_close(){
  if(db_map && db_map!=MAP_FAILED) msync(db_map, db_size, MS_SYNC);
  if(db_map && db_map!=MAP_FAILED) munmap(db_map, db_size);
  if(db_fd>=0) close(db_fd);
}
static void op_ingest(const char* text){
  if(header->count >= (uint64_t)MAX_ITEMS){ r_print(C_RED "MEMORIA CHEIA\n" C_RST); return; }
  OmegaCell *c = &cells[header->count];
  c->id = (uint32_t)(header->count + 1);
  c->timestamp = (uint64_t)time(NULL);
  c->crc32 = calc_crc32(text);
  c->active = 1;
  vector_project(text, c->vector);
  size_t i=0;
  for(; i<CONTENT_SIZE-1 && text[i]; i++) c->content[i]=text[i];
  c->content[i]=0;
  header->count++;
  header->dirty_flag = 1;
  r_print(C_GRN "[OK] INGEST id="); r_print_int((long)c->id); r_print("\n" C_RST);
}
static void op_search(const char* query){
  uint8_t q[VEC_BYTES];
  vector_project(query,q);
  r_print(C_CYN "\n--- SEARCH ---\n" C_RST);
  int hits=0;
  for(uint64_t i=0;i<header->count;i++){
    if(!cells[i].active) continue;
    int dist = vector_distance(q, cells[i].vector);
    (void)dist;
    if(strstr(cells[i].content, query)){
      r_print(C_Neon "[MATCH] " C_RST);
      r_print(cells[i].content); r_print("\n");
      hits++;
    }
  }
  if(!hits) r_print(C_YEL "Nenhum traço encontrado.\n" C_RST);
}

static void ui_splash(){
  r_cls();
  r_print(C_Neon "RAFAELIA OMEGA v3.0\n" C_RST);
  r_print(C_CYN "MMAP DB | HDC 1024b | RAW IO\n\n" C_RST);
}

int main(int argc, char** argv){
  db_init();

  if(argc>1){
    if(!strcmp(argv[1],"--ingest") && argc>=3) op_ingest(argv[2]);
    else if(!strcmp(argv[1],"--search") && argc>=3) op_search(argv[2]);
    db_close();
    return 0;
  }

  ui_splash();
  char buf[256];
  for(;;){
    r_print(C_Pink "[1] INGEST  [2] SEARCH  [3] STATS  [9] EXIT\n" C_RST);
    r_print(C_Neon "OMEGA> " C_RST);
    int n = (int)read(0, buf, 255);
    if(n<=0) break;
    buf[(n>0)?(n-1):0]=0;

    if(buf[0]=='1'){
      r_print("INPUT: ");
      int k=(int)read(0,buf,255);
      if(k>0){ buf[k-1]=0; op_ingest(buf); }
    }else if(buf[0]=='2'){
      r_print("QUERY: ");
      int k=(int)read(0,buf,255);
      if(k>0){ buf[k-1]=0; op_search(buf); }
    }else if(buf[0]=='3'){
      r_print(C_BLU "STATS\n" C_RST);
      r_print("items="); r_print_int((long)header->count); r_print("\n");
      r_print("cap="); r_print_int((long)header->capacity); r_print("\n");
      r_print("db="); r_print_int((long)db_size); r_print(" bytes\n");
    }else if(buf[0]=='9'){ break; }
  }

  db_close();
  r_print("\n[SIGTERM] halted\n");
  return 0;
}
EOF

if command -v clang >/dev/null 2>&1; then
  CC=clang
elif command -v gcc >/dev/null 2>&1; then
  CC=gcc
else
  echo "Instale um compilador: pkg install -y clang"
  exit 1
fi

$CC -O3 -std=gnu11 -Wall -Wextra -Wno-unused-result "${SRC_FILE}" -o "${EXE_FILE}"
chmod +x "${EXE_FILE}"

echo "[OK] build: ${EXE_FILE}"
echo "Run: ./${EXE_FILE}"
EOF
