#include "defs.h"

int find(void *arg);
void thread_pool_add(thrd_t thread);
void thread_pool_wait();

int count = 0, active_threads = 0;
mtx_t count_mutex, thread_count_mutex;
cnd_t thread_available;

typedef struct {
    char dir_path[PATH_MAX];
    char match[PATH_MAX];
} ARGS;

int main(int argc, char *argv[]) {
    char current[PATH_MAX];

    if (argc < 2) {
        fprintf(stderr, "Format: ff filename\n");
        exit(1);
    }

    if (getcwd(current, PATH_MAX) == NULL) {
        perror("Unable to get current working directory!");
        exit(1);
    }

    int states[3] = {mtx_init(&count_mutex, mtx_plain),
                     mtx_init(&thread_count_mutex, mtx_plain),
                     cnd_init(&thread_available)
    };

    for (int i = 0; i < 3; ++i) {
        if (states[i] != thrd_success) {
            perror("Initialization has failed!");
            exit(1);
        }
    }

    printf("Searching for '%s'\n", argv[1]);

    ARGS args;
    strncpy(args.dir_path, current, PATH_MAX);
    strncpy(args.match, argv[1], PATH_MAX);

    find(&args);
    thread_pool_wait();

    printf("Found %d match%s\n", count, (count != 1) ? "es" : "");
    putchar('\n');

    mtx_destroy(&count_mutex);
    mtx_destroy(&thread_count_mutex);
    cnd_destroy(&thread_available);

    return 0;
}

void color_print(const char *text, const char *match, const char *color) {
    const char *pos = strstr(text, match);
    if (pos) {
        printf("%.*s\033[%sm%s\033[0m%s\n",
               (int)(pos - text), text, color, match, pos + strlen(match));
    } else printf("%s\n", text);
}

int find(void *arg) {
    ARGS *args = (ARGS *) arg;
    DIR *dp = opendir(args->dir_path);

    if (!dp) {
        fprintf(stderr, "Unable to read directory '%s'!\n", args->dir_path);
        return thrd_error;
    }

    struct dirent *entry;
    char sub_dir_path[PATH_MAX];

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(sub_dir_path, sizeof(sub_dir_path), "%s/%s", args->dir_path, entry->d_name);

        struct stat fs;
        if (lstat(sub_dir_path, &fs) == -1) {
            perror("Unable to get file parameters!");
            continue;
        }

        if (strstr(entry->d_name, args->match) != NULL) {
            mtx_lock(&count_mutex);
            color_print(sub_dir_path, args->match, GREEN);
            count++;
            mtx_unlock(&count_mutex);
        }

        if (S_ISDIR(fs.st_mode)) {
            ARGS *new_args;
            new_args = malloc(sizeof(ARGS));
            if (new_args == NULL) {
                perror("Unable to allocate memory!");
                closedir(dp);
                return thrd_error;
            }

            strncpy(new_args->dir_path, sub_dir_path, sizeof(new_args->dir_path));
            strncpy(new_args->match, args->match, sizeof(new_args->match));

            mtx_lock(&thread_count_mutex);
            while (active_threads >= MAX_THREADS) {
                cnd_wait(&thread_available, &thread_count_mutex);
            }
            active_threads++;
            mtx_unlock(&thread_count_mutex);

            thrd_t curr_thrd;
            if (thrd_create(&curr_thrd, find, new_args) != thrd_success) {
                perror("Could not create a new thread!");
                free(new_args);
                continue;
            }

            thread_pool_add(curr_thrd);
        }
    }

    closedir(dp);
    return thrd_success;
}

void thread_pool_add(thrd_t thread) {
    thrd_detach(thread);
    mtx_lock(&thread_count_mutex);
    active_threads--;
    cnd_signal(&thread_available);
    mtx_unlock(&thread_count_mutex);
}

void thread_pool_wait() {
    mtx_lock(&thread_count_mutex);
    while (active_threads > 0) {
        cnd_wait(&thread_available, &thread_count_mutex);
    }
    mtx_unlock(&thread_count_mutex);
}
