#ifndef _MACRO_H
#define _MACRO_H

/*
 * Macro Functions
 */
#define BUG_EXIT()          ASSERT(false)
#define ASSERT(c)           assert(c)
#define CONST_STRLEN(s)     (sizeof(s) - 1)
#define IS_EMPTY_STR(s)     ((*s == '\0') ? true : false)
#define IS_EQUAL_STR(s1,s2) (!strcmp(s1,s2))
#define ENDING_CHAR(s)      (*(s + strlen(s) - 1))
#define NEW_OBJECT(t)       ((t *)calloc(1, sizeof(t)))

#define DYNAMIC_VSPRINTF(s, f) do {                                     \
        size_t _strsize;                                                \
        for(_strsize = 1024; ; _strsize *= 2) {                         \
            s = (char*)malloc(_strsize);                                \
            if(s == NULL) {                                             \
                DEBUG("DYNAMIC_VSPRINTF(): can't allocate memory.");    \
                break;                                                  \
            }                                                           \
            va_list _arglist;                                           \
            va_start(_arglist, f);                                      \
            int _n = vsnprintf(s, _strsize, f, _arglist);               \
            va_end(_arglist);                                           \
            if(_n >= 0 && _n < _strsize) break;                         \
            free(s);                                                    \
        }                                                               \
    } while(0)

/*
 * Error message
 */
extern int _ad_log_level;
#define ERROROUT    (stderr)
#define ERROR(fmt, args...) if (_ad_log_level > 0) {                    \
        fprintf(ERROROUT, "[ERROR] " fmt "\n", ##args);                 \
    }
#define WARN(fmt, args...) if (_ad_log_level >= 2) {                    \
        fprintf(ERROROUT, "[WARN] " fmt "\n", ##args);                  \
    }
#define INFO(fmt, args...) if (_ad_log_level >= 3) {                    \
        fprintf(ERROROUT, "[INFO] " fmt "\n", ##args);                  \
    }

/*
 * Debug Macros
 */
#ifdef BUILD_DEBUG
#define DEBUGOUT    (stdout)
#define DEBUG(fmt, args...) if (_ad_log_level >= 4) {                   \
        fprintf(DEBUGOUT, "[DEBUG] " fmt " [%s(),%s:%d]\n",             \
                ##args, __func__, __FILE__, __LINE__);                  \
    }
#else
#define DEBUG(fms, args...)
#endif  /* BUILD_DEBUG */

// debug timer
#include <sys/time.h>
#define TIMER_START()                                                   \
    int _swno = 0;                                                      \
    struct timeval _tv1, _tv2;                                          \
    gettimeofday(&_tv1, NULL)

#define TIMER_STOP(prefix)  {                                           \
        gettimeofday(&_tv2, NULL);                                      \
        _swno++;                                                        \
        struct timeval _diff;                                           \
        _diff.tv_sec = _tv2.tv_sec - _tv1.tv_sec;                       \
        if(_tv2.tv_usec >= _tv1.tv_usec) {                              \
            _diff.tv_usec = _tv2.tv_usec - _tv1.tv_usec;                \
        } else {                                                        \
            _diff.tv_sec -= 1;                                          \
            _diff.tv_usec = 1000000 - _tv1.tv_usec + _tv2.tv_usec;      \
        }                                                               \
        printf("TIMER(%d,%s,%d): %zus %dus (%s:%d)\n",                  \
               getpid(), prefix, _swno, _diff.tv_sec, (int)(_diff.tv_usec), \
               __FILE__, __LINE__);                                     \
        gettimeofday(&_tv1, NULL);                                      \
    }

/*
 * Q_MUTEX Macros
 */
#ifndef _MULTI_THREADED
#define _MULTI_THREADED
#endif

#include <unistd.h>
#include <pthread.h>

#define Q_MUTEX_NEW(x,r) do {                                           \
        x = (qmutex_t *)calloc(1, sizeof(qmutex_t));                    \
        if(x == NULL) break;                                            \
        memset((void*)x, 0, sizeof(qmutex_t));                          \
        pthread_mutexattr_t _mutexattr;                                 \
        pthread_mutexattr_init(&_mutexattr);                            \
        if(r == true) {                                                 \
            pthread_mutexattr_settype(&_mutexattr, PTHREAD_MUTEX_RECURSIVE); \
        }                                                               \
        int _ret = pthread_mutex_init(&(x->mutex), &_mutexattr);        \
        pthread_mutexattr_destroy(&_mutexattr);                         \
        if(_ret != 0) {                                                 \
            char _errmsg[64];                                           \
            strerror_r(_ret, _errmsg, sizeof(_errmsg));                 \
            DEBUG("Q_MUTEX: can't initialize mutex. [%d:%s]", _ret, _errmsg); \
            free(x);                                                    \
            x = NULL;                                                   \
        }                                                               \
    } while(0)

#define Q_MUTEX_LEAVE(x) do {                                           \
        if(x == NULL) break;                                            \
        if(!pthread_equal(x->owner, pthread_self())) {                  \
            DEBUG("Q_MUTEX: unlock - owner mismatch.");                 \
        }                                                               \
        if((x->count--) < 0) x->count = 0;                              \
        pthread_mutex_unlock(&(x->mutex));                              \
    } while(0)

#define MAX_MUTEX_LOCK_WAIT (5000)
#define Q_MUTEX_ENTER(x) do {                                           \
        if(x == NULL) break;                                            \
        while(true) {                                                   \
            int _ret, i;                                                \
            for(i = 0; (_ret = pthread_mutex_trylock(&(x->mutex))) != 0 \
                        && i < MAX_MUTEX_LOCK_WAIT; i++) {              \
                if(i == 0) {                                            \
                    DEBUG("Q_MUTEX: mutex is already locked - retrying"); \
                }                                                       \
                usleep(1);                                              \
            }                                                           \
            if(_ret == 0) break;                                        \
            char _errmsg[64];                                           \
            strerror_r(_ret, _errmsg, sizeof(_errmsg));                 \
            DEBUG("Q_MUTEX: can't get lock - force to unlock. [%d:%s]", \
                  _ret, _errmsg);                                       \
            Q_MUTEX_LEAVE(x);                                           \
        }                                                               \
        x->count++;                                                     \
        x->owner = pthread_self();                                      \
    } while(0)

#define Q_MUTEX_DESTROY(x) do {                                         \
        if(x == NULL) break;                                            \
        if(x->count != 0) DEBUG("Q_MUTEX: mutex counter is not 0.");    \
        int _ret;                                                       \
        while((_ret = pthread_mutex_destroy(&(x->mutex))) != 0) {       \
            char _errmsg[64];                                           \
            strerror_r(_ret, _errmsg, sizeof(_errmsg));                 \
            DEBUG("Q_MUTEX: force to unlock mutex. [%d:%s]", _ret, _errmsg); \
            Q_MUTEX_LEAVE(x);                                           \
        }                                                               \
        free(x);                                                        \
    } while(0)

#endif  /* _MACRO_H */
