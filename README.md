# mab-redis
an redis module which impolement multi-armed bandtis algorithm

## command
### mab.set
init a mutli-armed bandit

    mab.set $key $type $choice_num $choice1 $choice2 ... $choiceN

field|type|description
----|----|----
key|string| identified a `bandit` uniquely
type|string| the algorithm to choice arm. (Currently only supports `ucb1` algorithm)
choice_num|integer| the number of bandit arms
choiceN|string| 


### mab.choice

    mab.choie $key

RETURN

    (idx, choiceN)


### mab.reward

    mab.reward $key $idx $reward

字段|类型|描述
----|----|----
key|string| 老虎机的key用于唯一标识一个老虎机
idx|int| 老虎机臂的id
reward|double| 对于ucb1算法(0<=reward<=1)


### mab.statjson

    mab.statjson $key

RETURN

    {"arms": [{"count": .., "reward": ...}, {"count": ..., "reward": ...}, ...], "policy": ...}


### mab.config
