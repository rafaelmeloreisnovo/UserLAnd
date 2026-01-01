#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static void* xaligned64(size_t bytes){
  void *p = NULL;
  if (posix_memalign(&p, 64, bytes) != 0) return NULL;
  return p;
}

static inline uint64_t nsec_now(){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(int argc, char** argv){
  int size_mb = (argc>1)? atoi(argv[1]) : 256;
  int reps    = (argc>2)? atoi(argv[2]) : 200;

  size_t bytes = (size_t)size_mb * 1024u * 1024u;
  uint8_t *buf = (uint8_t*)xaligned64(bytes);
  if(!buf){ fprintf(stderr,"alloc failed\n"); return 1; }

  for(size_t i=0;i<bytes;i++) buf[i]=(uint8_t)(i*131u);

  printf("sink=0\n");
  printf("# size_mb=%d reps=%d bytes=%zu\n", size_mb, reps, bytes);
  printf("# stride_bytes, ns_per_access\n");

  volatile uint64_t sink = 0;

  for(size_t stride=4; stride<=1024*1024; stride*=2){
    uint64_t t0 = nsec_now();
    for(int r=0;r<reps;r++){
      for(size_t off=0; off<bytes; off+=stride){
        sink += buf[off];
      }
    }
    uint64_t t1 = nsec_now();

    double accesses = (double)reps * (double)(bytes/stride);
    double ns_per = (double)(t1-t0) / accesses;
    printf("%zu, %.3f\n", stride, ns_per);
  }

  if(sink==0) printf("sink=0\n");
  free(buf);
  return 0;
}
