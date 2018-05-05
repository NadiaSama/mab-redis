#ifndef MAB_REDIS_POLICY_H
#define MAB_REDIS_POLICY_H

#include <stdint.h>

//export multi_arm_t, policy_t definition. redis rdb-load,rdb-save reuqire
struct policy_s;
typedef struct policy_s policy_t;
struct policy_op_s;
typedef struct policy_op_s policy_op_t;

struct policy_s {
    policy_op_t *op;
    //name field is used by redis rdb-load, rdb-save procedure
    const char  *name;
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
    int         len;

    uint64_t    total_count;
    policy_t    policy;
};

typedef void * (*malloc_ptr)(size_t);
typedef void (*free_ptr)(void *);
typedef void * (*realloc_ptr)(void *, size_t s);
int multi_arm_init(malloc_ptr m, free_ptr f, realloc_ptr r);


struct multi_arm_s;
typedef struct multi_arm_s multi_arm_t;

multi_arm_t * multi_arm_new(const char *policy, void **choices, int l, const char *option);
void multi_arm_free(multi_arm_t *);
void * multi_arm_choice(multi_arm_t *, int *idx);
int multi_arm_reward(multi_arm_t *, int idx, double reward);

int multi_arm_stat_json(multi_arm_t *, char *, size_t maxlen);


#ifdef MABREDIS_MODULE
/*
 * #include "redismodule.h"
 *
 * "redismodule.h" export some global function so can not include twice
 */
struct RedisModuleIO;

void multi_arm_rdb_save(multi_arm_t *, struct RedisModuleIO *rdb);
multi_arm_t * multi_arm_rdb_load(struct RedisModuleIO *, int encv);
#endif

#endif
