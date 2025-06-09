#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>    // uint32_t 등 쓸 경우
#include "threads/thread.h"  // tid_t 가 정의됨
#include "threads/synch.h"   // struct lock

void syscall_init (void);

void halt(void);
void exit(int status);
int write (int fd, const void *buffer, unsigned size);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
tid_t fork(const char *thread_name, struct intr_frame *f); 
int read(int fd, void *buffer, unsigned size);
int filesize(int fd) ;
int exec (const char *file_name);
void seek(int fd, unsigned position);
int tell(int fd);
void close (int fd);
int wait(tid_t pid);

void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);

struct lock filesys_lock;

#endif /* userprog/syscall.h */