#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "multiarm.h"
#include "log.h"

struct policy_s;
typedef struct policy_s policy_t;

typedef void *  (*policy_new)();
typedef void    (*policy_free)(policy_t *);
typedef void *  (*policy_choice)(policy_t *, multi_arm_t *, int *idx);
typedef void    (*policy_reward)(policy_t *, multi_arm_t *, int idx, double reward);

struct policy_op_s{
    policy_new      new;
    policy_free     free;
    policy_choice   choice;
    policy_reward   reward;
};
typedef struct policy_op_s policy_op_t;

struct policy_s {
    policy_op_t *op;
    void        *data;
};

struct arm_s{
    uint64_t    count;
    double      reward;
    void        *choice;
};
typedef struct arm_s arm_t;

struct multi_arm_s {
    arm_t       *arms;
    size_t      len;
    uint64_t    total_count;
    policy_t    policy;
};

static void * policy_ucb1_choice(policy_t *, multi_arm_t *mab, int *idx);
static void   policy_ucb1_reward(policy_t *, multi_arm_t *mab, int idx, double);
static policy_op_t policy_ucb1 = {NULL, NULL, policy_ucb1_choice, policy_ucb1_reward};

struct policy_elem_s {
    const char      *name;
    policy_op_t     *op;
};
typedef struct policy_elem_s policy_elem_t;

static policy_elem_t policies[] = {
    {"ucb1", &policy_ucb1}
};
static int policy_init(const char *, policy_t *dst);

static malloc_ptr  _malloc = malloc;
static free_ptr    _free = free;
static realloc_ptr _realloc = realloc;

int
multi_arm_alloc_set(malloc_ptr m, free_ptr f, realloc_ptr r)
{
    if(m == NULL || f == NULL || r == NULL){
        return EINVAL;
    }

    _malloc = m;
    _free = f;
    _realloc = r;

    return 0;
}

multi_arm_t *
multi_arm_new(const char *policy, void **choices, size_t len)
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

    if(policy_init(policy, &ret->policy) == 0){
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

void
multi_arm_reward(multi_arm_t *mab, int idx, double reward)
{
    mab->policy.op->reward(&mab->policy, mab, idx, reward);
    mab->total_count++;
}

int
multi_arm_stat_json(multi_arm_t *ma, char *obuf, size_t maxlen)
{
    int     len;
#define PRINTF(fmt, ...) do{                            \
    len = snprintf(obuf, maxlen, fmt, ##__VA_ARGS__);   \
    if(maxlen < len){                                   \
        return 1;                                       \
    }                                                   \
    maxlen -= len;                                      \
    obuf += len;                                        \
}while(0)

#define FMT "{\"count\": %lu, \"reward\": %f}"

    PRINTF("{\"arms\": [");
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
    PRINTF("]");
    PRINTF("}");
#undef PRINTF

    return 0;
}

static int
policy_init(const char *policy, policy_t *dst)
{
    int         i = 0;

    for(i = 0; i < sizeof(policies)/sizeof(policies[0]); i++){
        if(strcmp(policies[i].name, policy) == 0){
            dst->op = policies[i].op;
            if(policies[i].op->new == NULL){
                dst->data = NULL;
            }else{
                dst->data = policies[i].op->new();
            }
            return 0;
        }
    }

    log_error("invalid policy name %s", policy);
    return 1;
}


static void *
policy_ucb1_choice(policy_t *policy, multi_arm_t *ma, int *idx)
{
    double  ucb_max = 0.0, ucb;
    arm_t   *arm;
    int     i, ridx;
    for(i = 0; i < ma->len; i++){
        if(ma->arms[i].count == 0){
            ridx = i;
            goto find;
        }
    }

    for(i = 0; i < ma->len; i++){
        arm = ma->arms + i;

        ucb = (arm->reward / arm->count) + sqrt(2 * log(arm->count + 1) / ma->total_count);

        if(ucb > ucb_max){
            ucb_max = ucb;
            ridx = i;
        }
    }

find:
    *idx = ridx;
    return ma->arms[i].choice;
}

static void
policy_ucb1_reward(policy_t *policy, multi_arm_t *ma, int idx, double reward)
{
    arm_t   *arm = ma->arms + idx;

    arm->reward += reward;
    arm->count++;
}
