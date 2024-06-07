#include "defs.h"

void initialize(void);
int find(void *arg);
void thread_pool_add(thrd_t thread);
void thread_pool_wait();

atomic_int count = 0;
int active_threads = 0;
mtx_t mutex;
cnd_t cond;

typedef struct {
    char dir_path[PATH_MAX];
    char match[PATH_MAX];
} ARGS;

int main(void) {
    char current[PATH_MAX], buffer[PATH_MAX];

    printf("Please enter either a partial or full filename: ");
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (buffer[0] == '\n') {
          printf("Please enter a valid string!\n");
          exit(1);
        }
        size_t len = strlen(buffer);
        if (len > 1 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    } 

    if (getcwd(current, PATH_MAX) == NULL) {
        perror("Unable to get current working directory!");
        exit(1);
    }

    printf("Searching for '%s'\n", buffer);

    ARGS args;
    strncpy(args.dir_path, current, PATH_MAX);
    strncpy(args.match, buffer, PATH_MAX);

    initialize();

    find(&args);
    thread_pool_wait();

    mtx_destroy(&mutex);
    cnd_destroy(&cond);

    const int res = atomic_load(&count);
    if (res < LIMIT) {
        printf(
            "Found %d match%s\n", res, 
            (res != 1) ? "es" : ""
        );
    } else {
        printf(
            "The number of matches for the searched string '%s' has exceeded the limit of %d.",
             buffer, LIMIT
        );
    }
    putchar('\n');

    return 0;
}

void color_print(const char *full_string, const char *partial_string, 
                 const char *match, const char *color) 
{
    const char *match_start = strstr(partial_string, match);
    if (match_start) {
        const char *file_name_start = strstr(full_string, partial_string);
        int path_len = file_name_start - full_string;

        printf("%.*s", path_len, full_string);
        printf("%.*s", (int)(match_start - partial_string), partial_string);
        printf("\033[%sm%.*s\033[0m", color, (int)strlen(match), match_start);
        printf("%s\n", match_start + strlen(match));
    } else {
        printf("%s\n", full_string);
    }
} 

int find(void *arg) {
    ARGS *args = malloc(sizeof(ARGS)); 
    if (args == NULL) {
        perror("Unable to allocate memory!");
        return thrd_error;
    }
    memcpy(args, arg, sizeof(ARGS)); 
    DIR *dp = opendir(args->dir_path);

    if (!dp) {
        fprintf(stderr, "Unable to read directory '%s'!\n", args->dir_path);
        free(args);
        return thrd_error;
    }

    const struct dirent *entry;

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char sub_dir_path[PATH_MAX];
        snprintf(sub_dir_path, sizeof(sub_dir_path), "%s/%s", args->dir_path, entry->d_name);

        struct stat fs;
        if (lstat(sub_dir_path, &fs) == -1) {
            perror("Unable to get file parameters!");
            continue;
        }

        if (atomic_load(&count) >= LIMIT) {
            closedir(dp);
            free(args);
            return thrd_success;
        }

        if (strstr(entry->d_name, args->match) != NULL) {
            color_print(sub_dir_path, entry->d_name, args->match, GREEN);
            atomic_fetch_add(&count, 1);
        }

        if (S_ISDIR(fs.st_mode)) {
            ARGS *new_args = malloc(sizeof(ARGS));
            if (new_args == NULL) {
                perror("Unable to allocate memory!");
                closedir(dp); free(args);
                return thrd_error;
            }

            strncpy(new_args->dir_path, sub_dir_path, sizeof(new_args->dir_path));
            strncpy(new_args->match, args->match, sizeof(new_args->match));

            mtx_lock(&mutex);
            while (active_threads >= MAX_THREADS) {
                cnd_wait(&cond, &mutex);
            }
            active_threads++;
            mtx_unlock(&mutex);

            thrd_t curr_thrd;
            if (thrd_create(&curr_thrd, find, new_args) != 0) {
                perror("Could not create a new thread!");
                free(new_args);
                continue;
            }

            thread_pool_add(curr_thrd);
        }
    }

    closedir(dp);
    free(args);

    return thrd_success;
}
 
void initialize(void) {
    if (mtx_init(&mutex, mtx_plain) != thrd_success || 
        cnd_init(&cond) != thrd_success) {
        perror("Initialization has failed!");
        exit(1);
    }
}

void thread_pool_add(thrd_t thread) {
    thrd_detach(thread);
    mtx_lock(&mutex);
    active_threads--;
    cnd_signal(&cond);
    mtx_unlock(&mutex);
}

void thread_pool_wait() {
    mtx_lock(&mutex);
    while (active_threads > 0) {
        cnd_wait(&cond, &mutex);
    }
    mtx_unlock(&mutex);
}