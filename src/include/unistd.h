//
// unistd.h
//
// Standard symbolic constants and types
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.
// 

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef UNISTD_H
#define UNISTD_H

#include <sys/types.h>

#ifndef NOVFORK
#include <setjmp.h>
#include <alloca.h>
#endif

//
// Standard file descriptors
//

#define	STDIN_FILENO	0	// Standard input
#define	STDOUT_FILENO	1	// Standard output
#define	STDERR_FILENO	2	// Standard error output

//
// Values for the second argument to access
//

#ifndef R_OK

#define	R_OK	4		// Test for read permission
#define	W_OK	2		// Test for write permission
#define	X_OK	1		// Test for execute permission
#define	F_OK	0		// Test for existence

#endif

//
// Seek types
//

#ifndef SEEK_SET

#define SEEK_SET                0       // Seek relative to begining of file
#define SEEK_CUR                1       // Seek relative to current file position
#define SEEK_END                2       // Seek relative to end of file

#endif

//
// Context for vfork()
//

#ifndef NOVFORK

struct _forkctx
{
  struct _forkctx *prev;
  jmp_buf jmp;
  int fd[3];
  char **env;
};

#endif

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef LARGEFILES
#define lseek(f, offset, origin) lseek64((f), (offset), (origin))
#define ftruncate(f, size) ftruncate64((f), (size))
#else
osapi loff_t lseek(handle_t f, loff_t offset, int origin);
osapi int ftruncate(handle_t f, loff_t size);
#endif

osapi int ftruncate64(handle_t f, off64_t size);
osapi off64_t lseek64(handle_t f, off64_t offset, int origin);

osapi int access(const char *name, int mode);
osapi int close(handle_t h);
osapi int fsync(handle_t f);
osapi int read(handle_t f, void *data, size_t size);
osapi int write(handle_t f, const void *data, size_t size);
#ifdef LARGEFILES
osapi int pread(handle_t f, void *data, size_t size, off64_t offset);
osapi int pwrite(handle_t f, const void *data, size_t size, off64_t offset);
#endif
osapi int pipe(handle_t fildes[2]);
osapi int chdir(const char *name);
osapi char *getcwd(char *buf, size_t size);
osapi handle_t dup(handle_t h);
osapi handle_t dup2(handle_t h1, handle_t h2);
osapi int link(const char *oldname, const char *newname);
osapi int unlink(const char *name);
osapi int mkdir(const char *name, int mode);
osapi int rmdir(const char *name);
osapi int gethostname(char *name, int namelen);
osapi unsigned sleep(unsigned seconds);
osapi char *crypt(const char *key, const char *salt);
osapi int getuid();
osapi int getgid();
osapi int setuid(uid_t uid);
osapi int setgid(gid_t gid);
osapi int geteuid();
osapi int getegid();
osapi int seteuid(uid_t uid);
osapi int setegid(gid_t gid);
osapi int getgroups(int size, gid_t list[]);
osapi int chown(const char *name, int owner, int group);
osapi int fchown(handle_t f, int owner, int group);

int *_opterr();
int *_optind();
int *_optopt();
char **_optarg();

#define opterr (*_opterr())
#define optind (*_optind())
#define optopt (*_optopt())
#define optarg (*_optarg())

int getopt(int argc, char **argv, char *opts);

#ifndef NOVFORK

struct _forkctx *_vfork(struct _forkctx *fc);
int execve(const char *path, char *argv[], char *env[]);
int execv(const char *path, char *argv[]);
int execl(const char *path, char *arg0, ...);

#define vfork() (setjmp(_vfork((struct _forkctx *) alloca(sizeof(struct _forkctx)))->jmp))

#endif

#ifdef  __cplusplus
}
#endif

#endif
