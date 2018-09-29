#include <iostream>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <limits.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#define PROF

#include "FuncProf.h"

#define ERROR(info)\
{\
    printf("%s:%d [%s]\n", __FUNCTION__, __LINE__, info);\
}

struct hash_t {
    const char *name_ptr;
    int index;
    hash_t *next;
};

int init = 1;
pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
int slot[PROF_MAX_THREADS];
pthread_mutex_t slot_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t slot_key;
pthread_key_t clock_stack_key;
pthread_key_t hash_table_key;

int _shm_id;
void *_shm_ptr;

int prof_mod;

inline uint64_t get_current_clock() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp
    );
    return (uint64_t) (tp.tv_sec * (1000 * 1000 * 1000) + tp.tv_nsec);
}

void __profile_sigprof_handler(int sig) {
    exit(1);
}

void __profile_finalize(void) {
    if (shmdt(_shm_ptr) == -1) {
        ERROR("shmdt()");
    }
    if (shmctl(_shm_id, IPC_RMID, 0) == -1) {
        ERROR("shmctl()");
    }
}

void __profile_slot_cleanup(void *ptr) {
    printf("slot slot slot\n");
    pthread_mutex_lock(&slot_mutex);
    printf("111111slot %d\n", (int) ((char *) ptr - (char *) _shm_ptr) / (PROF_MAX_FUNC * sizeof(prof_t)));
    slot[(int) ((char *) ptr - (char *) _shm_ptr) / (PROF_MAX_FUNC * sizeof(prof_t))] = 0;
    pthread_mutex_unlock(&slot_mutex);
    free(ptr);
}

void __profile_clock_stack_cleanup(void *ptr) {
    free(ptr);
}

void __profile_hash_table_cleanup(void *ptr) {
    hash_t *cur, *nxt, *hash_table = (hash_t *) ptr;
    for (int i = 0; i < PROF_MAX_FUNC; ++i) {
        cur = hash_table[i].next;
        while (cur != NULL) {
            nxt = cur->next;
            free(cur);
            cur = nxt;
        }
    }
}

void __profile_initialize() {
    atexit(__profile_finalize);
    signal(SIGTERM, __profile_sigprof_handler);
    signal(SIGINT, __profile_sigprof_handler);
    signal(SIGQUIT, __profile_sigprof_handler);

    pthread_key_create(&slot_key, __profile_slot_cleanup);
    pthread_key_create(&clock_stack_key, __profile_clock_stack_cleanup);
    pthread_key_create(&hash_table_key, __profile_hash_table_cleanup);

    key_t _shm_key;
    char path_name[256] = {0};
    snprintf(path_name, sizeof(path_name), "/proc/%d", getpid());
    _shm_key = ftok(path_name, 2);
    printf("_shm_key %d path_name %s\n", _shm_key, path_name);
    if (_shm_key == -1) {
        ERROR("ftok()");
        exit(1);
    }
    _shm_id = shmget(_shm_key, sizeof(prof_t) * PROF_MAX_THREADS * PROF_MAX_FUNC,
                     IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (_shm_id == -1) {
        ERROR("shmget()");
        exit(1);
    }
    _shm_ptr = shmat(_shm_id, NULL, 0);
    if (_shm_ptr == (void *) (-1)) {
        ERROR("shmget()");
        exit(1);
    }
    prof_mod = getenv("PROF_MOD") ? 1 : 0;
    printf("\n033[31;49;1mShare Memory ID: %d\nShare Memory Size: %dbytes\033[31;49;0m\n", _shm_id,
           sizeof(prof_t) * PROF_MAX_THREADS * PROF_MAX_FUNC);
}

void __profile_entry_func(const char *name_ptr) {
    //printf("__profile_entry_func_begin\n");
    prof_t **prof_ptr;
    prof_t *prof;
    uint64_t *clock_stack;
    hash_t *hash_table;

    if (init == 1) {
        pthread_mutex_lock(&init_mutex);
        if (init == 1) {
            __profile_initialize();
            init = 0;
        }
        pthread_mutex_unlock(&init_mutex);
    }

    clock_stack = (uint64_t *) pthread_getspecific(clock_stack_key);
    if (clock_stack == NULL) {
        int i;
        pthread_mutex_lock(&slot_mutex);
        for (int i = 0; i < PROF_MAX_THREADS; i++) {
            if (!slot[i]) {
                slot[i] = 1;
                break;
            }
        }
        pthread_mutex_unlock(&slot_mutex);
        if (i == PROF_MAX_THREADS) {
            ERROR("Too Many Threads");
            exit(1);
        }
        prof_ptr = (prof_t **) malloc(sizeof(prof_t *));
        *prof_ptr = (prof_t *) (_shm_ptr) + i * PROF_MAX_FUNC;
        (*prof_ptr)[0].prof_size = 0;
        pthread_setspecific(slot_key, (void *) prof_ptr);

        clock_stack = (uint64_t *) malloc(sizeof(uint64_t) * PROF_MAX_DEEP);
        clock_stack[0] = 1;
        pthread_setspecific(slot_key, (void *) clock_stack);

        hash_table = (hash_t *) malloc(sizeof(hash_t) * PROF_MAX_FUNC);
        memset(hash_table, 0x00, sizeof(hash_t) * PROF_MAX_FUNC);
        pthread_setspecific(hash_table_key, (void *) hash_table);
    }

    clock_stack[clock_stack[0]++] = get_current_clock();

    //printf("__profile_entry_func_end\n");
}

void __profile_exit_func(const char *name_ptr) {
    prof_t *prof;
    prof_t *prof_entity = NULL;
    uint64_t *clock_stack;
    hash_t *hash_table;
    hash_t *hash_entry;

    prof = *((prof_t **) pthread_getspecific(slot_key));
    clock_stack = (uint64_t *) pthread_getspecific(clock_stack_key);
    hash_table = (hash_t *) pthread_getspecific(hash_table_key);

    uint64_t addr = (uint64_t) (name_ptr);
    while (!(addr && 0x1))
        addr >>= 1;
    addr &= (PROF_MAX_FUNC - 1);
    hash_entry = &hash_table[addr];
    if (hash_entry->name_ptr != NULL) {
        while (hash_entry != NULL) {
            if (hash_entry->name_ptr == name_ptr) {
                prof_entity = &prof[hash_entry->index];
                break;
            }
            if (hash_entry->next != NULL) {
                hash_entry = hash_entry->next;
            } else {
                hash_entry->next = (hash_t *) malloc(sizeof(hash_t));
                hash_entry = hash_entry->next;
                break;
            }
        }
    }

    if (prof_entity == NULL) {
        hash_entry->name_ptr = name_ptr;
        hash_entry->index = ++prof[0].prof_size;
        hash_entry->next = NULL;
        prof_entity = &prof[hash_entry->index];
        (prof_entity->prof_body).begin = 0;
        (prof_entity->prof_body).end = 0;
        (prof_entity->prof_body).call_times = 0;
        (prof_entity->prof_body).exec_time = 0;
        strncpy((prof_entity->prof_body).func_name, name_ptr, PROF_MAX_FUNC_NAME);
        (prof_entity->prof_body).func_name[PROF_MAX_FUNC_NAME] = '\0';
    }

    uint64_t func_exec_time = get_current_clock() - clock_stack[--clock_stack[0]];

    (prof_entity->prof_body).call_times += 1;
    (prof_entity->prof_body).exec_time += func_exec_time;
    if (prof_mod) {
        for (int i = 0; i < clock_stack[0]; ++i) {
            clock_stack[i] += func_exec_time;
        }
    }
}