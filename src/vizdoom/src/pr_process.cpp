/* pr_process.c
 * by Dennis Chao
 * modified by David Koppenhofer
 * modified again by Gideon Redelinghuys
 */
// Copyright (C) 1999 by Dennis Chao
// Copyright (C) 2000 by David Koppenhofer
// Copyright (C) 2015 by Gideon Redelinghuys

#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <unistd.h>

#include "pr_process.h"

#if defined(SCOOS5) || defined(SCOUW2) || defined(SCOUW7)
#include "strcmp.h"
#endif

struct Process {
   int pid;
   FString pName;
};

std::vector<Process> processList;



int hash(char *str)
{
    int hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    if (hash < 0) {
        hash = 0 - hash;
    }

    return hash;
}

// pr_list
// This routine replaces both pr_init() and pr_poll().  I used some code
// from those routines to make this one, though.
// Called at the start of a game/level and periodically throughout the
// playing of the level.  It runs 'ps', capturing certain info about each
// process except for 'ps' itself.
void pr_list(void) {

// Don't open 'ps' right away; see if we're on the right level first,
// amongst other criteria.
  FILE *f = NULL;

  char buf[256];
  int pid = 0;
  char namebuf[256];
  char tty[256];


  f = popen("echo list | nc -U /app/socket/dockerdoom.socket", "r");

  if (!f) {
    fprintf(stderr, "ERROR: pr_check could not open ps\n");
    return;
  }

  /* Read in all process information.  Exclude the last process in the
     list. */

  while (fgets(buf, 255, f)) {
    int read_fields = sscanf(buf, "%s\n", namebuf);
    if (read_fields == 1 && namebuf) {
      pid = hash(namebuf);
      fprintf(stderr, "Demon: %s, %d\n", namebuf, pid);
      Process process;
      process.pid = pid;
      process.pName = namebuf;
      processList.push_back(process);
    }
  }
  pclose(f);
}

int pr_count_processes(void) {
   return processList.size();
}

int pr_get_pid(int index) {
   return processList.at(index).pid;
}

FString pr_get_pname(int index) {
   return processList.at(index).pName;
}

// pr_kill
// kills the process
void pr_kill(int pid) {
  char buf[256];

  sprintf(buf, "echo \"kill %d\" | nc -U /app/socket/dockerdoom.socket", pid);
  system(buf);
}

