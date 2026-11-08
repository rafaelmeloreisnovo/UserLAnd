#ifndef RAF_LINUX_SYSCALL_H
#define RAF_LINUX_SYSCALL_H

/* Standalone Linux ABI boundary; no libc, Android JNI or external headers. */
typedef __SIZE_TYPE__ raf_size_t;
long raf_linux_write(unsigned int fd, const void *bytes, raf_size_t count);
__attribute__((noreturn)) void raf_linux_exit(unsigned int status);

#endif
