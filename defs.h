#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdatomic.h>
#include "include/c11threads.h"

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#define MAX_THREADS 8
#define LIMIT 20

// COLORS:
#define GREEN "32"

#endif
