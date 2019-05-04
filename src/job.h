/* Definitions for managing subprocesses in GNU Make.
Copyright (C) 1992-2017 Free Software Foundation, Inc.
This file is part of GNU Make.

GNU Make is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

GNU Make is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "output.h"

/* Structure describing a running or dead child process.  */

struct child
  {
    struct child *next;         /* Link in the chain.  */

    struct file *file;          /* File being remade.  */

    char **environment;         /* Environment for commands.  */
    char *sh_batch_file;        /* Script file for shell commands */
    char **command_lines;       /* Array of variable-expanded cmd lines.  */
    char *command_ptr;          /* Ptr into command_lines[command_line].  */

#ifdef VMS
    char *comname;              /* Temporary command file name */
    int efn;                    /* Completion event flag number */
    int cstatus;                /* Completion status */
    int vms_launch_status;      /* non-zero if lib$spawn, etc failed */
#endif

    unsigned int  command_line; /* Index into command_lines.  */
    struct output output;       /* Output for this child.  */
    pid_t         pid;          /* Child process's ID number.  */
    unsigned int  remote:1;     /* Nonzero if executing remotely.  */
    unsigned int  noerror:1;    /* Nonzero if commands contained a '-'.  */
    unsigned int  good_stdin:1; /* Nonzero if this child has a good stdin.  */
    unsigned int  deleted:1;    /* Nonzero if targets have been deleted.  */
    unsigned int  recursive:1;  /* Nonzero for recursive command ('+' etc.)  */
    unsigned int  dontcare:1;   /* Saved dontcare flag.  */
  };

extern struct child *children;

/* A signal handler for SIGCHLD, if needed.  */
RETSIGTYPE child_handler (int sig);
int is_bourne_compatible_shell(const char *path);
void new_job (struct file *file);
void reap_children (int block, int err);
void start_waiting_jobs (void);

char **construct_command_argv (char *line, char **restp, struct file *file,
                               int cmd_flags, char** batch_file);

#ifdef VMS
int child_execute_job (struct child *child, char *argv);
#else
int child_execute_job (struct output *out, int good_stdin, char **argv, char **envp);
#endif

#ifdef _AMIGA
void exec_command (char **argv) __attribute__ ((noreturn));
#elif defined(__EMX__)
int exec_command (char **argv, char **envp);
#else
void exec_command (char **argv, char **envp) __attribute__ ((noreturn));
#endif

void unblock_all_sigs (void);

extern unsigned int job_slots_used;
extern unsigned int jobserver_tokens;
