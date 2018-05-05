#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "multiarm.h"
#include "pcg.h"
#include "log.h"

typedef void *  (*policy_new)(const char *option);
typedef void    (*policy_free)(policy_t *);
typedef void *  (*policy_choice)(policy_t *, multi_arm_t *, int *idx);
typedef int     (*policy_reward)(policy_t *, multi_arm_t *, int idx, double reward);
typedef int     (*policy_stat_json)(policy_t *, char *obuf, size_t maxlen);

#ifdef MABREDIS_MODULE
typedef struct RedisModuleIO RedisModuleIO;

extern void (*RedisModule_SaveUnsigned)(RedisModuleIO *io, uint64_t value);
extern void (*RedisModule_SaveDouble)(RedisModuleIO *io, double value);
extern void (*RedisModule_SaveStringBuffer)(RedisModuleIO *io, const char *str, size_t len);
extern uint64_t (*RedisModule_LoadUnsigned)(RedisModuleIO *io);
extern double (*RedisModule_LoadDouble)(RedisModuleIO *io);
extern char *(*RedisModule_LoadStringBuffer)(RedisModuleIO *io, size_t *lenptr);
extern void (*RedisModule_Free)(void *ptr);
extern void (*RedisModule_LogIOError)(RedisModuleIO *io, const char *levelstr, const char *fmt, ...);


typedef void    (*policy_rdb_save)(policy_t *, RedisModuleIO *); 
typedef void *  (*policy_rdb_load)(policy_t *, RedisModuleIO *);
#endif

struct policy_op_s{
    policy_new          new;
    policy_free         free;
    policy_choice       choice;
    policy_reward       reward;
    policy_stat_json    sj;

#ifdef MABREDIS_MODULE
    policy_rdb_save save;
    policy_rdb_load load;
#endif
};

static void * policy_ucb1_choice(policy_t *, multi_arm_t *mab, int *idx);
static int    policy_ucb1_reward(policy_t *, multi_arm_t *mab, int idx, double);
static policy_op_t policy_ucb1 = {
    .new = NULL,
    .free = NULL,
    .choice = policy_ucb1_choice, 
    .reward = policy_ucb1_reward,
    .sj = NULL,

#ifdef MABREDIS_MODULE
    .save = NULL,
    .load = NULL,
#endif
};

static void * policy_egreedy_new(const char *option);
static void   policy_egreedy_free(policy_t *);
static void * policy_egreedy_choice(policy_t *, multi_arm_t *, int *idx);
#define policy_egreedy_reward policy_ucb1_reward
static int    policy_egreedy_stat_json(policy_t *, char *, size_t maxlen);

#ifdef MABREDIS_MODULE
static void   policy_egreedy_save(policy_t *, RedisModuleIO *);
static void * policy_egreedy_load(policy_t *, RedisModuleIO *);
#endif
static policy_op_t policy_egreedy = {
    .new = policy_egreedy_new,
    .free = policy_egreedy_free,
    .choice = policy_egreedy_choice,
    .reward = policy_egreedy_reward,
    .sj = policy_egreedy_stat_json,

#ifdef MABREDIS_MODULE
    .save = policy_egreedy_save,
    .load = policy_egreedy_load,
#endif
};

struct policy_elem_s {
    const char      *name;
    policy_op_t     *op;
};
typedef struct policy_elem_s policy_elem_t;

static policy_elem_t policies[] = {
    {"ucb1", &policy_ucb1},
    {"egreedy", &policy_egreedy}
};
static int policy_init(const char *policy, policy_t *dst, const char *option);

static malloc_ptr  _malloc = malloc;
static free_ptr    _free = free;
static realloc_ptr _realloc = realloc;

int
multi_arm_init(malloc_ptr m, free_ptr f, realloc_ptr r)
{
    if(m){
        _malloc = m;
    }

    if(f){
        _free = f;
    }

    if(r){
        _realloc = r;
    }

    pcg32_srandom(time(NULL) ^ (intptr_t)&printf, (intptr_t)&sprintf);
    return 0;
}

multi_arm_t *
multi_arm_new(const char *policy, void **choices, int len, const char *option)
{
    multi_arm_t *ret = _malloc(sizeof(*ret));
    if(ret == NULL){
        log_error("process run out of memory");
        exit(1);
    }

    ret->arms = _malloc(sizeof(arm_t) * len);
    if(ret == NULL){
        log_error("process run out of memory");
        exit(1);
    }

    int         i = 0;
    for(i = 0; i < len; i++){
        ret->arms[i].count = 0;
        ret->arms[i].reward = 0.0;
        ret->arms[i].choice = choices[i];
    }
    ret->len = len;
    ret->total_count = 0;

    if(policy_init(policy, &ret->policy, option) == 0){
        return ret;
    }

    _free(ret->arms);
    _free(ret);
    return NULL;
}

void
multi_arm_free(multi_arm_t *arm)
{
    if(arm->policy.op->free != NULL){
        arm->policy.op->free(arm->policy.data);
    }

    _free(arm->arms);
    _free(arm);
}

void *
multi_arm_choice(multi_arm_t *mab, int *idx)
{
    return mab->policy.op->choice(&mab->policy, mab, idx);
}


int
multi_arm_reward(multi_arm_t *mab, int idx, double reward)
{
    if(idx > mab->len - 1 || idx < 0){
        return 1;
    }

    int ret = mab->policy.op->reward(&mab->policy,
            mab, idx, reward);

    if(ret){
        return ret;
    }

    mab->total_count++;
    return ret;
}


int
multi_arm_stat_json(multi_arm_t *ma, char *obuf, size_t maxlen)
{
    size_t     len;
#define PRINTF(fmt, ...) do{                            \
    len = snprintf(obuf, maxlen, fmt, ##__VA_ARGS__);   \
    if(maxlen < len){                                   \
        return 1;                                       \
    }                                                   \
    maxlen -= len;                                      \
    obuf += len;                                        \
}while(0)

#define FMT "{\"count\": %lu, \"reward\": %f}"

    PRINTF("{\"total_count\": %lu, \"arms\": [", ma->total_count);
    const char  *fmt;
    int         i;
    for(i = 0; i < ma->len; i++){
        if(i + 1 == ma->len){
            fmt = FMT;
        }else{
            fmt = FMT",";
        }

        PRINTF(fmt, ma->arms[i].count, ma->arms[i].reward);
    }
    PRINTF("], ");
    
    if(ma->policy.op->sj){
        len = ma->policy.op->sj(&ma->policy, obuf, maxlen);
        if(maxlen < len){
            return 1;
        }
        maxlen -= len;
        obuf += len;
    }else{
        PRINTF("\"policy\": \"%s\"", ma->policy.name);
    }
    PRINTF("}");
#undef PRINTF

    return 0;
}

#ifdef MABREDIS_MODULE
void
multi_arm_rdb_save(multi_arm_t *ma, struct RedisModuleIO *rdb)
{
    //save multi_arm_t arms
    RedisModule_SaveUnsigned(rdb, ma->len);

    int i;
    for(i = 0; i < ma->len; i++){
        RedisModule_SaveUnsigned(rdb, ma->arms[i].count);
        RedisModule_SaveDouble(rdb, ma->arms[i].reward);
    }

    RedisModule_SaveUnsigned(rdb, ma->total_count);

    RedisModule_SaveStringBuffer(rdb, ma->policy.name,
            strlen(ma->policy.name));
    if(ma->policy.op->save){
        ma->policy.op->save(&ma->policy, rdb);
    }
}

multi_arm_t *
multi_arm_rdb_load(struct RedisModuleIO  *rdb, int encv)
{
    (void)encv;
    multi_arm_t     *ma = _malloc(sizeof(*ma));
    int             i;

    ma->len = RedisModule_LoadUnsigned(rdb);
    ma->arms = _malloc(ma->len * sizeof(arm_t));
    for(i = 0; i < ma->len; i++){
        ma->arms[i].count = RedisModule_LoadUnsigned(rdb);
        ma->arms[i].reward = RedisModule_LoadDouble(rdb);
    }

    ma->total_count = RedisModule_LoadUnsigned(rdb);

    size_t  policy_len;
    char    *policy = RedisModule_LoadStringBuffer(rdb, &policy_len);

    for(i = 0; i < (int)(sizeof(policies)/sizeof(policies[0])); i++){
        if(strlen(policies[i].name) == policy_len &&
                strncmp(policies[i].name, policy, policy_len) == 0){
            ma->policy.op = policies[i].op;
            ma->policy.name = policies[i].name;
            break;
        }
    }

    if(i == sizeof(policies)/sizeof(policies[0])){
        RedisModule_LogIOError(rdb, "warning", "unsupport multi_arm_policy %.*s", (int)policy_len, policy);
        goto error;
    }

    if(ma->policy.op->load){
        ma->policy.data = ma->policy.op->load(&ma->policy, rdb);
        if(ma->policy.data == NULL){
            goto error;
        }
    }else{
        ma->policy.data = NULL;
    }
    goto done;

error:
    _free(ma->arms);
    _free(ma);
    ma = NULL;

done:
    RedisModule_Free(policy);
    return ma;
}
#endif

static int
policy_init(const char *policy, policy_t *dst, const char *option)
{
    int         i = 0, len = (int)(sizeof(policies)/sizeof(policies[0]));

    for(i = 0; i < len; i++){
        if(strcmp(policies[i].name, policy) == 0){
            dst->op = policies[i].op;
            dst->name = policies[i].name;
            if(policies[i].op->new == NULL){
                dst->data = NULL;
            }else{
                dst->data = policies[i].op->new(option);
                if(dst->data == NULL){
                    return 1;
                }
            }
            return 0;
        }
    }

    return 1;
}


static void *
policy_ucb1_choice(policy_t *policy, multi_arm_t *ma, int *idx)
{
    (void)policy;
    double  ucb_max = 0.0, ucb;
    arm_t   *arm;
    int     i, ridx = 0;
    for(i = 0; i < ma->len; i++){
        if(ma->arms[i].count == 0){
            ridx = i;
            goto find;
        }
    }

    for(i = 0; i < ma->len; i++){
        arm = ma->arms + i;

        ucb = (arm->reward / arm->count) + sqrt(2 * log(ma->total_count + 1) / arm->count);

        if(ucb > ucb_max){
            ucb_max = ucb;
            ridx = i;
        }
    }

find:
    *idx = ridx;
    return ma->arms[ridx].choice;
}

static int
policy_ucb1_reward(policy_t *policy, multi_arm_t *ma, int idx, double reward)
{
    (void)policy;
    if(reward < 0 || reward > 1.0){
        return 1;
    }
    arm_t   *arm = ma->arms + idx;

    arm->reward += reward;
    arm->count++;

    return 0;
}


static void *
policy_egreedy_new(const char *option)
{
    if(option == NULL){
        return NULL;
    }

    char    *eptr = NULL;
    double  d = strtod(option, &eptr);
    if(eptr == option || d < 0 || d > 1.00000000001){
        printf("conver fail %s\n", eptr);
        return NULL;
    }

    double  *ret = _malloc(sizeof(double));
    *ret = d;
    return (void *)ret;
}


static void
policy_egreedy_free(policy_t *policy)
{
    _free(policy->data);
}

static void *
policy_egreedy_choice(policy_t *policy, multi_arm_t *ma, int *idx)
{
    double  r = randnumber(), epsilon = *((double *)policy->data);
    int     i, ridx = -1;
    if(r < epsilon || ma->total_count == 0){
        i = randint(ma->len);
        ridx = i;
        goto find;
    }

    double  max_avg = -0.1, avg;
    for(i = 0; i < ma->len; i++){
        if(ma->arms[i].count){
            avg = ma->arms[i].reward / ma->arms[i].count;
        }else{
            avg = 0.0;
        }

        if(avg > max_avg){
            ridx = i;
            max_avg = avg;
        }
    }

find:
    *idx = ridx;
    return ma->arms[ridx].choice;
}

static int
policy_egreedy_stat_json(policy_t *p, char *obuf, size_t maxlen)
{
    return snprintf(obuf, maxlen, "\"policy\": \"%s\", \"epsilon\": %0.4f", p->name,
            *((double *)p->data));
}

#ifdef MABREDIS_MODULE
static void
policy_egreedy_save(policy_t *p, RedisModuleIO *rdb)
{
    RedisModule_SaveDouble(rdb, *((double *)p->data));
}

static void *
policy_egreedy_load(policy_t *p, RedisModuleIO *rdb)
{
    (void)p;

    double  val = RedisModule_LoadDouble(rdb);
    double  *ret = _malloc(sizeof(double));

    *ret = val;
    return ret;
}
#endif
