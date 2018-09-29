#ifndef FUNCPROF_FUNCPROF_H
#define FUNCPROF_FUNCPROF_H

#include <stdint.h>

#ifdef PROF
#define PROF_FUNC_ENTRY() __profile_entry_func(__FUNCTION__);
#define PROF_FUNC_EXIT() __profile_exit_func(__FUNCTION__);
#define PROF_NF_BEGIN(tag) __profile_entry_func(tag);
#define PROF_NF_END(tag) __profile_entry_func(tag);
#else
#define PROF_FUNC_ENTRY()
#define PROF_FUNC_EXIT()
#define PROF_NF_BEGIN(tag)
#define PROF_NF_END(tag)
#endif

#define PROF_MAX_THREADS 32
#define PROF_MAX_FUNC 4096
#define PROF_MAX_DEEP 4096

#define PROF_MAX_FUNC_NAME 47

union prof_t{
    int prof_size;
    struct {
        uint16_t begin,end;
        uint32_t call_times;
        uint64_t exec_time;
        char func_name[PROF_MAX_FUNC_NAME+1];
    }prof_body;
};

void __profile_entry_func(const char *name_ptr);
void __profile_exit_func(const char *name_ptr);

#endif //FUNCPROF_FUNCPROF_H
