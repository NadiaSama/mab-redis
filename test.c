#include <stdio.h>
#include <assert.h>

#include "multiarm.h"

#define LEN 4

int
main(void)
{
    void    *choice[LEN] = {(void *)0x1, (void *)0x2, (void *)0x3, (void *)0x4};

    multi_arm_t *ma = multi_arm_new("ucb1", choice, LEN);
    assert(ma != NULL);

    int     i, idx;
    for(i = 0; i < LEN; i++){
        void *p = multi_arm_choice(ma, &idx);
        printf("choice %p %d\n", p, idx);
        multi_arm_reward(ma, idx, 0.1);
    }

    char    obuf[1024];
    assert(multi_arm_stat_json(ma, obuf, 20) == 1);
    printf("%.*s\n", 20, obuf);

    multi_arm_stat_json(ma, obuf, 1024);
    printf("%s\n", obuf);

    multi_arm_free(ma);

    return 0;
}
