#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <limits.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <errno.h>

using namespace std;

#define PROF

#include "FuncProf.h"

#define ERROR(info)\
{\
    printf("%s:%d [%s] [%d][%s]\n", __FUNCTION__, __LINE__, info, errno, strerror(errno));\
}


int rflag = 1;
int refresh_time = 2;
char path_name[PATH_MAX];
char file_name[PATH_MAX] = "./out.dat";
int item_limit = 30;

int _shm_id;
void *_shm_ptr;

FILE *logfp;

int prof_list_size;
prof_t prof_list[PROF_MAX_FUNC];
struct hash_t {
    char str[PROF_MAX_FUNC];
    int idx;
    hash_t *nxt;
} hash_table[PROF_MAX_FUNC];

char head_txt[6][64] = {"rank", "function", "exec", "call", "average", "rate"};
char *head_fmt = "\033[33;49;1m%4.4s %-47.47s %16.16s%4.4s %12.12s %16.16s%4.4s %8.8s\033[31;49;0m\n";
char *prof_fmt = "\033[33;49;1m%4d %-47.47s %20lld %20lld %20.2f %7.2f%%\033[31;49;0m\n";
char *head_fmt_none = "%4.4s %-47.47s %16.16s%4.4s %12.12s %16.16s%4.4s %8.8s\n";
char *prof_fmt_none = "%4d %-47.47s %20lld %20lld %20.2f %7.2f%%\n";

bool (*cmp)(prof_t, prof_t);

int thread_numth = -1;
uint64_t g_time_base = 1000 * 1000;

void sigint_handler(int sig) {
    rflag = 0;
}

int time33(char *str) {
    int rtv = 0;
    for (int i = 0; str[i]; ++i) {
        rtv = (rtv << 5) + rtv + (int) str[i];
    }
    return rtv;
}

int search_fname(char *str) {
    hash_t *hash_ptr = &hash_table[time33(str) & (PROF_MAX_FUNC - 1)];
    while (hash_ptr->str[0] != '\0') {
        if (strcmp(hash_ptr->str, str) == 0) {
            return hash_ptr->idx;
        }
        if (hash_ptr->nxt != NULL) {
            hash_ptr = hash_ptr->nxt;
        } else {
            hash_ptr->nxt = (hash_t *) malloc(sizeof(hash_t));
            hash_ptr = hash_ptr->nxt;
            break;
        }
    }
    strncpy(prof_list[prof_list_size].prof_body.func_name, str, PROF_MAX_FUNC_NAME);
    prof_list[prof_list_size].prof_body.call_times = 0;
    prof_list[prof_list_size].prof_body.exec_time = 0;
    strncpy(hash_ptr->str, str, PROF_MAX_FUNC_NAME);
    hash_ptr->idx = prof_list_size;
    hash_ptr->nxt = NULL;
    return prof_list_size++;
}

bool average_cmp(const prof_t a, const prof_t b) {
    uint64_t a_ave = a.prof_body.exec_time * b.prof_body.call_times;
    uint64_t b_ave = b.prof_body.exec_time * a.prof_body.call_times;
    return a_ave > b_ave;
}

bool exectime_cmp(const prof_t a, const prof_t b) {
    return a.prof_body.exec_time > b.prof_body.exec_time;
}

bool calltimes_cmp(const prof_t a, const prof_t b) {
    return a.prof_body.call_times > b.prof_body.call_times;
}

int initialize() {
    key_t _shm_key;

    _shm_key = ftok(path_name, 2);
    if (_shm_key == -1) {
        ERROR("ftok()");
        return -1;
    }
    _shm_id = shmget(_shm_key, 0, 0);
    if (_shm_id == -1) {
        ERROR("shmget()");
        return -1;
    }
    _shm_ptr = shmat(_shm_id, NULL, 0);
    if (_shm_ptr == (void *) (-1)) {
        ERROR("shmat()");
        return -1;
    }
    logfp = fopen(file_name, "w");
    if (logfp == NULL) {
        ERROR("fopen()");
    }
    return 0;
}

int finalize(void) {
    if (shmdt(_shm_ptr) == -1) {
        ERROR("shmdt()");
        return -1;
    }
    if (logfp != NULL) {
        double total_exec_time = 0.0;
        const char *prec = g_time_base == 1 ? "(ns)" : (g_time_base == 1000 ? "(us)" : "(ms)");
        fprintf(logfp, head_fmt_none, head_txt[0], head_txt[1], head_txt[2], prec, head_txt[3], head_txt[4],
                head_txt[5]);
        sort(prof_list, prof_list + prof_list_size, cmp);
        for (int i = 0; i < prof_list_size; ++i) {
            total_exec_time += (double) (prof_list[i].prof_body.exec_time);
        }
        for (int i = 0; i < item_limit && i < prof_list_size; ++i) {
            fprintf(logfp, prof_fmt_none, i + 1, prof_list[i].prof_body.func_name, prof_list[i].prof_body.exec_time,
                    prof_list[i].prof_body.call_times,
                    (double) prof_list[i].prof_body.exec_time / (double) prof_list[i].prof_body.call_times,
                    (double) prof_list[i].prof_body.exec_time / total_exec_time * 100.0);
        }
    }
    return 0;
}

void print_profile_info() {
    static int linen = 0;
    static int head_tag = 0;
    double total_exec_time = 0.0;
    int rank_list_size;
    prof_t rank_list[PROF_MAX_FUNC];

    if (head_tag == 0) {
        system("clear");
        if (thread_numth != -1) {
            printf("[Monitoring the %dth thread of process]\n", thread_numth);
        } else {
            printf("[Monitoring all threads of process]\n");
        }
        const char *prec = g_time_base == 1 ? "(ns)" : (g_time_base == 1000 ? "(us)" : "(ms)");
        printf(head_fmt, head_txt[0], head_txt[1], head_txt[2], prec, head_txt[3], head_txt[4], prec, head_txt[5]);
        head_tag = 1;
    }
    while (linen) {
        printf("\033[1A");
        printf("\033[K");
        --linen;
    }
    rank_list_size = prof_list_size;
    memcpy(rank_list, prof_list, sizeof(rank_list[0]) * rank_list_size);
    sort(rank_list, rank_list + rank_list_size, cmp);
    for (int i = 0; i < rank_list_size; ++i) {
        total_exec_time += (double) (rank_list[i].prof_body.exec_time);
    }
    for (int i = 0; i < item_limit && i < rank_list_size; ++i) {
        printf(prof_fmt, i + 1, rank_list[i].prof_body.func_name, rank_list[i].prof_body.exec_time,
               rank_list[i].prof_body.call_times,
               (double) rank_list[i].prof_body.exec_time / (double) rank_list[i].prof_body.call_times,
               (double) rank_list[i].prof_body.exec_time / total_exec_time * 100.0);
        ++linen;
    }
}

int collect_data() {
    prof_t *prof;
    int index;

    int i;
    for (i = 0; i < prof_list_size; ++i) {
        prof_list[i].prof_body.call_times = 0;
        prof_list[i].prof_body.exec_time = 0;
    }
    for (i = 0; i < PROF_MAX_THREADS; ++i) {
        if (thread_numth == -1 || thread_numth == i) {
            prof = (prof_t *) _shm_ptr;
            prof += i * PROF_MAX_FUNC;
            for (int j = 0; j <= prof[0].prof_size; ++j) {
                index = search_fname(prof[j].prof_body.func_name);
                prof_list[index].prof_body.call_times += prof[j].prof_body.call_times;
                prof_list[index].prof_body.exec_time += prof[j].prof_body.exec_time / g_time_base;
            }
        }
    }
    print_profile_info();
    return 0;
}

void help() {
    printf("\tMonitor the cost of function calls\n");
    printf("\t-p [PID]\n\t\tmonitor process which pid is PID\n");
    printf("\t-t [NUM]\n\t\tmonitor the profile date of NUMth thread of process\n");
    printf("\t-k [PID]\n\t\tkill the process which pid is PID(SIGTERM is sent)\n");
    printf("\t-l [NUM]\n\t\tshow top NUM items, the default number is %d\n", item_limit);
    printf("\t-f [PATH]\n\t\twrite the result into PATH after monitor terminated, the default PATH is ./out.dat\n");
    printf("\t-a\n\t\tsort items by average execute time, this is the default mode\n");
    printf("\t-e\n\t\tsort items by total execute time\n");
    printf("\t-c\n\t\tsort items by call times\n");
    printf("\t-m\n\t\tshow execute time in millisecond(10^-3), this is the default mode\n");
    printf("\t-u\n\t\tshow execute time in millisecond(10^-6)\n");
    printf("\t-n\n\t\tshow execute time in millisecond(10^-9)\n");
}

int main(int argc, char **argv) {
    cmp = average_cmp;
    int opt;
    while ((opt = getopt(argc, argv, "p:t:l:k:f:acemunh")) != -1) {
        switch (opt) {
            case 'p':
                snprintf(path_name, PATH_MAX, "/proc/%s", optarg);
                break;
            case 't':
                thread_numth = atoi(optarg);
                thread_numth = thread_numth >= PROF_MAX_FUNC ? -1 : thread_numth;
                break;
            case 'k':
                kill(atoi(optarg), SIGTERM);
                return 0;
            case 'f':
                snprintf(file_name, PATH_MAX, "%s", optarg);
                break;
            case 'l':
                item_limit = atoi(optarg);
                break;
            case 'a':
                cmp = average_cmp;
                break;
            case 'c':
                cmp = calltimes_cmp;
                break;
            case 'e':
                cmp = exectime_cmp;
                break;
            case 'm':
                g_time_base = 1000 * 1000;
                break;
            case 'u':
                g_time_base = 1000;
                break;
            case 'n':
                g_time_base = 1;
                break;
            case 'h':
                help();
                break;
            default:
                break;
        }
    }
    signal(SIGINT, sigint_handler);
    if (initialize() != 0) {
        return 1;
    }
    while (rflag) {
        collect_data();
        sleep(refresh_time);
    }
    finalize();
    return 0;
}