#!/usr/bin/python

'''
test mabredis module
'''

import random
import redis

conn = redis.StrictRedis(port=6380)

def EC(*args):
    try:
        ret = conn.execute_command(*args)
        print "command %s execue ok %s" % (args, ret)
        return ret
    except redis.exceptions.ResponseError as e:
        print "command %s execue failed %s" % (args, e)


#EC("module", "load", "./mabredis.so")

EC("module", "list")


#test mab.set
EC("mab.set", "test2", "ucb2", "a")
EC("mab.set", "test2", "ucb2", "a", "b")
EC("mab.set", "test2", "ucb2", 2, "a")
EC("mab.set", "test2", "ucb2", 2, "a", "b")
EC("mab.set", "test2", "ucb1", 2, "a", "b")

#test mab.choice key not exist
EC("mab.choice", "not-exist")

#test mab.reward invalild arg
EC("mab.reward", "not-exist")

EC("mab.reward", "test2", "a", "1.3")
EC("mab.reward", "test2", "3", "1.3")
EC("mab.reward", "test2", "1", "1.001")

idx, elem = EC("mab.choice", "test2")
EC("mab.reward", "test2", idx, 0.0)

idx, elem = EC("mab.choice", "test2")
EC("mab.reward", "test2", idx, 0.1)

for i in xrange(0, 1000):
    idx, elem = EC("mab.choice", "test2")
    r = "%.2f" % random.random()

    EC("mab.reward", "test2", idx, float(r))

EC("mab.statjson", "test2")

#set arm1 value manualy
EC("mab.config", "test2")
EC("mab.config", "test2", 2, 34, 34.5)
EC("mab.config", "test2", 0, 100, 34.5, 3, 100, 34.5)
EC("mab.config", "test2", 0, 100, 34.5, 1, 100, 34.5, 23)
EC("mab.config", "test2", 0, 100, 34.5, 1, 100, 34.5)

EC("mab.statjson", "test2")

for i in xrange(0, 1000):
    idx, elem = EC("mab.choice", "test2")
    r = "%.2f" % random.random()

    EC("mab.reward", "test2", idx, float(r))

EC("mab.statjson", "test2")
