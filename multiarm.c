#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdilb.h>

#include "policy.h"
#include "log.h"

struct policy_s;
typedef struct policy_s policy_t;

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

typedef void *  (*policy_new)();
typedef void    (*policy_free)(policy_t *);
typedef void *  (*policy_choice)(policy_t *, multi_arm_t *, int *idx);
typedef int     (*policy_reward)(policy_t *， multi_arm_t *, int idx, double reward);

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

static void * policy_ucb1_new();
static void * policy_ucb1_choice(policy_t *, multi_arm_t *mab, int *idx);
static int policy_ucb1_reward(policy_t *, multi_arm_t *mab, int idx, double);
static polic_t policy_ucb1 = {"ucb1", policy_ucb1_choice, policy_ucb1_reward};

struct policy_elem_t {
    const char      *name;
    policy_op_t     *op;
};

static policy_elem_t policies[] = {
    {"ucb1", policy_ucb1}
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
multi_arm_new(const char *policy, choice_t *choices, size_t len)
{
    multi_arm_r *ret = _malloc(sizeof(*ret));
    if(ret == NULL){
        log_error("process run out of memory");
        exit(1);
    }

    ret->arms = _malloc(sizeof(arm_t) * len);
    if(ret == NULL){
        log_error("process run out of memory");
        exit(1);
    }

    policy_t    *policy = NULL;
    int         i = 0;
    for(i = 0; i < len; i++){
        ret->arms[i].count = 0;
        ret->arms[i].reward = 0.0;
        ret->arms[i].choice = choices[i].choice;
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
    _free(arm->arms);
    _free(arm);
}

void *
multi_arm_choice(multi_arm_t *mab, int *idx)
{
    void * ret = mab->policy.op->choice(mab->policy, mab, idx);
    mab->total_count++;

    return ret;
}

int
multi_arm_reward(multi_arm_t *mab, int idx, double reward)
{
    return mab->policy.op->reward(mab->policy, mab, idx, reward);
}

static int
policy_init(const char *policy, policy_t *dst)
{
    policy_t    *policy = NULL;
    int         i = 0;

    for(i = 0; i < sizeof(policies)/sizeof(polies[0]); i++){
        if(strcmp(policies[i].name, policy) == 0){
            dst->op = policies[i].op;
            dst->data = policies[i].op->new();
            return 0;
        }
    }

    log_error("invalid policy name %s", policy);
    return 1;
}


static void *
policy_ucb1_choice(policy_t *policy, multi_arm_t *ma, int *idx)
{
    int     i;
    for(i = 0; i < ma->len; i++){
        if(ma->arms[i].count == 0){
            *idx = i;
            return ma->arms[i].choice;
        }
    }
}
