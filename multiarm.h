#ifndef MAB_REDIS_POLICY_H
#define MAB_REDIS_POLICY_H

#include <stdint.h>

struct choice_s {
    double  value;
    void    *choice;
};
typedef struct choice_s choice_t;

struct multi_arm_s;
typedef struct multi_arm_s multi_arm_t;

typedef void * (*malloc_ptr)(size_t);
typedef void (*free_ptr)(void *);
typedef void * (*realloc_ptr)(void *, size_t s);
int multi_arm_alloc_set(malloc_ptr m, fee_ptr f, realloc_ptr r);

multi_arm_t * multi_arm_new(const char *policy, choice_t *, size_t l);
void multi_arm_free(multi_arm_t *);

void * multi_arm_choice(multi_arm_t *, int *idx);
int multi_arm_reward(multi_arm_t *, int idx, double reward);
#endif
