// pr_process.h
// Copyright (C) 1999 by Dennis Chao
// Copyright (C) 2000 by David Koppenhofer

#include "zstring.h"

// Functions
void pr_list(void);

int pr_count_processes(void);

int pr_get_pid(int index);

FString pr_get_pname(int index);

void pr_kill(int pid);
