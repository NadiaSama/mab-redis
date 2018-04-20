#ifndef MAB_REDIS_POLICY_H
#define MAB_REDIS_POLICY_H

#include <stdint.h>

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
typedef struct multi_arm_s multi_arm_t;

typedef void *  (*policy_choice)(multi_arm_t *, int *idx);
typedef int     (*policy_reward)(multi_arm_t *, int idx, double reward);

struct policy_s {
    policy_choice   choice;
    policy_reward   reward;
};

#endif
