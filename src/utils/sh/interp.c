//
// interp.c
//
// Shell command interpreter
//
// Copyright (C) 2012 Michael Ringgaard. All rights reserved.
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

#include "sh.h"

static int expand_glob(struct stkmark *mark, struct args *args, char *pattern) {
  glob_t globbuf;
  unsigned i;

  if (glob(pattern, GLOB_NOCHECK | GLOB_NOESCAPE, NULL, &globbuf) < 0) {
    fprintf(stderr, "errror expanding glob pattern %s\n", pattern);
    return 1;
  }

  if (args) {
    for (i = 0; i < globbuf.gl_pathc; ++i) {
      stputstr(mark, globbuf.gl_pathv[i]);
      add_arg(args, ststr(mark));
    }
  } else {
    for (i = 0; i < globbuf.gl_pathc; ++i) {
      if (i > 0) stputc(mark, ' ');
      stputstr(mark, globbuf.gl_pathv[i]);
    }
  }
  
  globfree(&globbuf);
  return 0;
}

static void expand_field(struct stkmark *mark, char *str, int quoted, struct args *args) {
  if (!str) return;
  if (quoted || !args) {
    stputstr(mark, str);
  } else {
    while (*str) {
      char *start = str;
      while (*str && is_space(*str)) str++;
      if (!*str) break;

      if (str != start && ststrlen(mark) > 0) add_arg(args, ststr(mark));
      start = str;
      while (*str && !is_space(*str)) str++;
      stputbuf(mark, start, str - start);
    }
  }
}

static char *expand_word(struct job *job, union node *node) {
  struct stkmark mark;
  int rc;
  char *word = NULL;
  
  pushstkmark(NULL, &mark);
  rc = expand_args(&mark, job, NULL, node);
  if (rc == 0) word = strdup(ststr(&mark));
  popstkmark(&mark);
  return word;
}

static char *remove_shortest_suffix(char *str, char *pattern) {
  char *s;

  if (!str || !pattern) return NULL;
  s = str + strlen(str);
  while (s >= str) {
    if (fnmatch(pattern, s, 0) == 0) {
      int len = s - str;
      char *buffer = malloc(len +1);
      memcpy(buffer, str, len);
      buffer[len] = 0;
      return buffer;
    }
    s--;
  }
  return NULL;
}

static char *remove_longest_suffix(char *str, char *pattern) {
  char *s;

  if (!str || !pattern) return NULL;
  s = str;
  while (*s) {
    if (fnmatch(pattern, s, 0) == 0) {
      int len = s - str;
      char *buffer = malloc(len +1);
      memcpy(buffer, str, len);
      buffer[len] = 0;
      return buffer;
    }
    s++;
  }
  return NULL;
}

static char *remove_shortest_prefix(char *str, char *pattern) {
  char *s;

  if (!str || !pattern) return str;
  s = str;
  while (*s) {
    char c = *s;
    *s = 0;
    if (fnmatch(pattern, str, 0) == 0) {
      *s = c;
      return s;
    }
    *s++ = c;
  }
  if (fnmatch(pattern, str, 0) == 0) return s;
  return str;
}

static char *remove_longest_prefix(char *str, char *pattern) {
  char *s;

  if (!str || !pattern) return str;
  s = str + strlen(str);
  while (s >= str) {
    char c = *s;
    *s = 0;
    if (fnmatch(pattern, str, 0) == 0) {
      *s = c;
      return s;
    }
    *s-- = c;
  }
  return str;
}

static int expand_param(struct stkmark *mark, struct job *job, struct args *args, struct nargparam *param) {
  char buffer[12];
  struct job *scope;
  char *value = NULL;
  int n;
  int special;
  int quoted;
  int varmod;
  struct arg *arg;

  // Expand special parameters
  quoted = (param->flags & S_TABLE) == S_DQUOTED;
  special = param->flags & S_SPECIAL;
  if (special) {
    switch (special) {
      case S_ARGC: // $#
        scope = get_arg_scope(job);
        value = itoa(scope->args.num - 1, buffer, 10);
        expand_field(mark, value, quoted, args);
        return 0;
      
      case S_ARGV: // $*
      case S_ARGVS: // $@
        scope = get_arg_scope(job);
        arg = scope->args.first;
        if (arg) arg = arg->next;
        while (arg) {
          if (args) {
            stputstr(mark, arg->value);
            add_arg(args, ststr(mark));
          } else {
            expand_field(mark, arg->value, quoted, args);
          }
          arg = arg->next;
        }
        return 0;

      case S_EXITCODE: // $?
        value = itoa(job->shell->lastrc, buffer, 10);
        expand_field(mark, value, quoted, args);
        return 0;

      case S_ARG: // $[0-9]
        scope = get_arg_scope(job);
        arg = scope->args.first;
        n = param->num;
        while (arg && n > 0) {
          n--;
          arg = arg->next;
        }
        if (param->flags & S_STRLEN) {
          value = itoa(arg ? strlen(arg->value) : 0, buffer, 10);
          expand_field(mark, value, quoted, args);
        } else {
          if (arg) expand_field(mark, arg->value, quoted, args);
        }
        return 0;

      case S_PID: // $$
        value = itoa(job->shell->lastpid, buffer, 10);
        expand_field(mark, value, quoted, args);
        return 0;

      case S_FLAGS: // $-
      case S_BGEXCODE:  // $!
        // Not implemented
        return 0;
    }
  }

  // Expand variable
  value = get_var(job, param->name);
  varmod = param->flags & S_VAR;
  if (!varmod) {
    if (value) expand_field(mark, value, quoted, args);
  } else {
    // Expand word
    char *word = expand_word(job, param->word);
    char *buf = NULL;
    int rc = 0;
    switch (varmod) {
      case S_DEFAULT: // ${parameter:-word}
        if (!value) value = word;
        break;

      case S_ASGNDEF: // ${parameter:=word}
        if (!value) {
          value = word;
          set_var(job, param->name, value);
          if (job->parent) set_var(job->parent, param->name, value);
        }
        break;

      case S_ERRNULL: // ${parameter:?[word]}
        if (!value) {
          if (word && *word) {
            fprintf(stderr, "%s: %s\n", param->name, word);
          } else {
            fprintf(stderr, "%s: variable not set\n", param->name);
          }
          rc = 1;
        }
        break;

      case S_ALTERNAT: // ${parameter:+word}
        if (value || *value) value = word;
        break;

      case S_RSSFX: // ${parameter%word}
        buf = remove_shortest_suffix(value, word);
        if (buf) value = buf;
        break;

      case S_RLSFX: // ${parameter%%word}
        buf = remove_longest_suffix(value, word);
        if (buf) value = buf;
        break;

      case S_RSPFX: // ${parameter#word}
        value = remove_shortest_prefix(value, word);
        break;

      case S_RLPFX: // ${parameter##word}
        value = remove_longest_prefix(value, word);
        break;
    }
    if (rc == 0 && value) expand_field(mark, value, quoted, args);
    free(word);
    free(buf);
    return rc;
  }

  return 0;
}

static int expand_command(struct stkmark *mark, struct job *parent, struct args *args, union node *node) {
  struct job *job;
  FILE *f;
  union node *n;
  char buf[512];
  int len;

  // Create job for command expansion
  job = create_job(parent);
  
  // Redirect output to temporary file
  f = tmpfile();
  if (!f) {
    remove_job(job);
    return 1;
  }
  set_fd(job, 1, fileno(f));
  
  // Interpret command
  for (n = node->nargcmd.list; n; n = n->list.next) {
    if (interp(job, n) != 0) {
      fclose(f);
      remove_job(job);
      return 1;
    }
  }
  
  // Add output from command to expansion
  fseek(f, 0, SEEK_SET);
  while (fgets(buf, sizeof(buf), f)) {
    expand_field(mark, buf, (node->nargcmd.flags & S_TABLE) == S_DQUOTED, args);
  }

  fclose(f);
  remove_job(job);
  return 0;
}

static int expand_args(struct stkmark *mark, struct job *job, struct args *args, union node *node) {
  union node *n;
  int before;
  int rc;
  char *value;

  switch (node->type) {
    case N_SIMPLECMD:
      for (n = node->ncmd.args; n; n = n->list.next) {
        rc = expand_args(mark, job, args, n);
        if (rc != 0) return rc;
      }
      break;

    case N_ARG:
      if (args) {
        before = args->num;
        for (n = node->narg.list; n; n = n->list.next) {
          rc = expand_args(mark, job, args, n);
          if (rc != 0) return rc;
        }
        if (args->num == before || ststrlen(mark) > 0) add_arg(args, ststr(mark));
      } else {
        for (n = node->narg.list; n; n = n->list.next) {
          rc = expand_args(mark, job, args, n);
          if (rc != 0) return rc;
        }
      }
      break;

    case N_ARGSTR:
      if (node->nargstr.flags & S_GLOB) {
        if (expand_glob(mark, args, node->nargstr.text) < 0) return 1;
      } else {
        stputstr(mark, node->nargstr.text);
      }
      break;

    case N_ARGPARAM:
      rc = expand_param(mark, job, args, &node->nargparam);
      if (rc != 0) return rc;
      break;

    case N_ARGCMD:
      rc = expand_command(mark, job, args, node);
      if (rc != 0) return rc;
      break;

    default:
      fprintf(stderr, "Unsupported argument type: %d\n", node->type);
      return 1;
  }
  
  return 0;
}

static int interp_vars(struct stkmark *mark, struct job *job, union node *node) {
  struct arg *arg;
  union node *n;
  int rc;

  // Expand variable assignments  
  struct args args;
  init_args(&args);
  for (n = node->ncmd.vars; n; n = n->list.next) {
    rc = expand_args(mark, job, &args, n);
    if (rc != 0) {
      delete_args(&args);
      return rc;
    }
  }

  // Assign variables to job
  arg = args.first;
  while (arg) {
    char *name = arg->value;
    char *value = strchr(arg->value, '=');
    if (value) *value++ = 0;
    set_var(job, name, value);
    arg = arg->next;
  }
  delete_args(&args);
  return 0;
}

static int interp_redir(struct stkmark *mark, struct job *job, union node *node) {
  union node *n;
  int rc, dir, act, fd, f, flags, oflags;
  char *arg;

  // Open files for redirection
  rc = 0;
  for (n = node; n; n = n->nredir.next) {
    fd = n->nredir.fd;
    flags = n->nredir.flags;
    dir = flags & R_DIR;
    act = flags & R_ACT;

    rc = expand_args(mark, job, NULL, n->nredir.list);
    if (rc != 0) break;
    arg = ststr(mark);
    if (!arg || fd < 0 || fd >= STD_HANDLES || act != R_OPEN) {
      rc = 1;
      break;
    }

    oflags = 0;
    if (dir == R_IN) {
      oflags |= O_RDONLY;
    } else if (dir == R_OUT) {
      oflags |= O_WRONLY;
    } else if (dir == (R_IN | R_OUT)) {
      oflags |= O_RDWR;
    }

    if (dir & R_OUT) {
      oflags |= (flags & R_APPEND) ? O_APPEND : O_TRUNC;
      oflags |= (flags & R_CLOBBER) ? (O_CREAT | O_EXCL) : O_CREAT;
    }

    f = open(arg, oflags, 0666);
    if (f < 0) {
      perror(arg);
      rc = 1;
      break;
    }
    set_fd(job, fd, f);
  }
  return rc;
}

static struct job *setup_command(struct job *parent, union node *node) {
  struct stkmark mark;
  struct job *job;

  // Create new job
  job = create_job(parent);

  // Assign variables
  pushstkmark(NULL, &mark);
  if (node->ncmd.vars) {
    if (interp_vars(&mark, job, node) != 0) {
      popstkmark(&mark);
      remove_job(job);
      return NULL;
    }
  }

  // Perform I/O redirection
  if (node->ncmd.rdir) {
    if (interp_redir(&mark, job, node->ncmd.rdir) != 0) {
      popstkmark(&mark);
      remove_job(job);
      return NULL;
    }
  }

  // Expand command arguments
  if (expand_args(&mark, job, &job->args, node) != 0) {
    popstkmark(&mark);
    remove_job(job);
    return NULL;
  }

  popstkmark(&mark);
  return job;
}

static int interp_simple_command(struct job *parent, union node *node) {
  int rc;
  struct job *job;
  int i;

  // If there are no arguments just assign variables to parent job
  if (!node->ncmd.args) {
    struct stkmark mark;
    pushstkmark(NULL, &mark);
    rc = interp_vars(&mark, parent, node);
    popstkmark(&mark);
    return rc;
  }
  
  // Setup job for command
  job = setup_command(parent, node);
  if (job == NULL) return 1;

  // Execute job
  rc = execute_job(job);
  if (rc != 0) {
    remove_job(job);
    return rc;
  }
  if (job->handle != -1) resume(job->handle);
  for (i = 0; i < STD_HANDLES; i++) {
    if (job->fd[i] != -1) {
      close(job->fd[i]);
      job->fd[i] = -1;
    }
  }

  if (node->ncmd.flags & S_BGND) {
    // Detach background job
    detach_job(job);
    return 0;
  } else {
    // Wait for command to complete
    rc = wait_for_job(job);
    remove_job(job);
    return rc;
  }
}

static int interp_pipeline(struct job *parent, union node *node) {
  union node *n;
  struct job *job;
  int pipefd[2];
  int out;

  // Create job for pipeline and get stdin and stdout for whole pipeline
  job = create_job(parent);
  out = get_fd(job, 1, 1);
  pipefd[0] = pipefd[1] = -1;

  for (n = node->npipe.cmds; n; n = n->list.next) {
    int first = (n == node->npipe.cmds);
    int last = (n->list.next == NULL);

    // Set input to be the output of the previous pipe
    if (!first) {
      set_fd(job, 0, pipefd[0]);
      pipefd[0] = -1;
    }

    // Create pipe for output
    if (last) {
      set_fd(job, 1, out);
      out = -1;
    } else {
      pipe(pipefd);
      set_fd(job, 1, pipefd[1]);
      pipefd[1] = -1;
      n->list.flags |= S_BGND;
    }

    // Interpret pipeline element
    if (interp(job, n) != 0) {
      if (out != -1) close(out);
      if (pipefd[0] != -1) close(pipefd[0]);
      if (pipefd[1] != -1) close(pipefd[1]);
      remove_job(job);
      return 1;
    }
  }

  remove_job(job);
  return 0;
}

int interp(struct job *parent, union node *node) {
  switch (node->type) {
    case N_SIMPLECMD: return interp_simple_command(parent, node);
    case N_PIPELINE: return interp_pipeline(parent, node);
     default:
       fprintf(stderr, "Error: not supported (node type %d)\n", node->type);
       return 1;
  }
}
