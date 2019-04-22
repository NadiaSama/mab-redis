# mab-redis
an redis module which implement multi-armed bandtis algorithm

currently **ucb1**, **egreey(epsilon-greedy)** algorithm was implemented

## example
```python
#!/usr/bin/python

import redis
import random
conn = redis.StrictRedis(port=6381)

#load module
conn.execute_command("module load", "/path/to/mabredis.so")

# creat a ucb1 bandit
conn.execute_command("mab.set", "key-ucb1", "ucb1", 3, "choice_1", "choice_2", "choice_3")

# {"total_count": 0, "arms": [{"count": 0, "reward": 0.000000},{"count": 0, "reward": 0.000000},{"count": 0, "reward": 0.000000}], "policy": "ucb1"}
print conn.execute_command("mab.statjson", "key-ucb1")


for i in xrange(0, 3):
    idx, choice = conn.execute_command("mab.choice", "key-ucb1")
    conn.execute_command("mab.reward", "key-ucb1", idx, random.random())

# {"total_count": 3, "arms": [{"count": 1, "reward": 0.857870},{"count": 1, "reward": 0.886606},{"count": 1, "reward": 0.106418}], "policy": "ucb1"}
print conn.execute_command("mab.statjson", "key-ucb1")

#reconfig bandit
conn.execute_command("mab.config", "key-ucb1", 0, 10, 9.3, 2, 23, 13.4)

# {"total_count": 3, "arms": [{"count": 10, "reward": 9.300000},{"count": 1, "reward": 0.886606},{"count": 23, "reward": 13.400000}], "policy": "ucb1"}
print conn.execute_command("mab.statjson", "key-ucb1")


#create a egreedy bandit with epsilon 0.1
conn.execute_command("mab.set", "key-egreedy", "egreedy", 3, "choice_1", "choice_2", "choice_3", 0.1)
for i in xrange(0, 3):
    idx, choice = conn.execute_command("mab.choice", "key-egreedy")
    conn.execute_command("mab.reward", "key-egreedy", idx, random.random())

# {"total_count": 3, "arms": [{"count": 0, "reward": 0.000000},{"count": 0, "reward": 0.000000},{"count": 3, "reward": 1.928970}], "policy": "egreedy", "epsilon": 0.1000}
print conn.execute_command("mab.statjson", "key-egreedy")


conn.execute_command("del", "key-ucb1")
conn.execute_command("del", "key-egreedy")
```

## command
### mab.set
init a mutli-armed bandit

    mab.set $key $type $choice_num $choice1 $choice2 ... $choiceN [$option]

field|type|description
----|----|----
key|string| identified a `bandit` uniquely
type|string| the algorithm to choice arm. (`ucb1`, `egreedy`)
choice_num|integer| the number of bandit arms
choiceN|string| 
option||only set for `egreedy` algorithm. specific `epsilon` value


### mab.choice

    mab.choie $key

RETURN

    (idx, choiceN)

field|type|description
----|----|----
idx|int| the index of arm which been choiced by the algorithm.(0 based)
choiceN|| corresponding choice



### mab.reward

    mab.reward $key $idx $reward

field|type|description
----|----|----
key|string| identified a `bandit` uniquely
idx|int| the index of arm which been rewarded
reward|double| 0<=reward<=1


### mab.statjson

    mab.statjson $key

RETURN

    {"arms": [{"count": .., "reward": ...}, {"count": ..., "reward": ...}, ...], "policy": ...}


### mab.config
manualy set arm value and reward. (mainly used by redis aof rewrite procedure.)

    mab.config $idx1 $value1 $reward1 ......
