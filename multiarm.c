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
    double      value;
    void        *choice;
};
typedef struct arm_s arm_t;

struct multi_arm_s {
    arm_t       *arms;
    size_t      len;
    policy_t    *policy;
};

typedef void *  (*policy_choice)(multi_arm_t *, int *idx);
typedef int     (*policy_reward)(multi_arm_t *, int idx, double reward);
struct policy_s {
    const char      *name;
    policy_choice   choice;
    policy_reward   reward;
};

static policy_ucb1_choice(multi_arm_t *arm, int *idx);
static policy_ucb1_reward(multi_arm_t *arm, int idx, double);
static polic_t policy_ucb1 = {"ucb1", policy_ucb1_choice, policy_ucb1_reward};

static policy_t policies[] = {
    policy_ucb1
};

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
        ret->arms[i].value = choices[i].value;
        ret->arms[i].choice = choices[i].choice;
    }
    ret->len = len;

    for(i = 0; i < sizeof(policies)/sizeof(polies[0]); i++){
        if(strcmp(policies[i].name, policy) == 0){
            ret->policy = policies + i;
            return ret;
        }
    }

    log_error("invalid policy name %s", policy);

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
multi_arm_choice(multi_arm_t *arm, int *idx)
{
    return arm->policy->choice(arm, idx);
}

int
multi_arm_reward(multi_arm_t *arm, int idx, double reward)
{
    return arm->policy->reward(arm, idx, reward);
}
