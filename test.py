#!/usr/bin/python

'''
test mabredis module
'''

import redis

conn = redis.StrictRedis()

#conn.execute_command("module", "load", "../mabredis.so")

def EC(*args):
    try:
        ret = conn.execute_command(*args)
        print "command %s execue ok %s" % (args, ret)
    except redis.exceptions.ResponseError as e:
        print "command %s execue failed %s" % (args, e)


EC("module", "list")

#key not exist
EC("mab.choice", "not-exist")

#test mab.set
EC("mab.set", "test2", "ucb2", "a")
EC("mab.set", "test2", "ucb2", "a", "b")
EC("mab.set", "test2", "ucb2", 2, "a")
EC("mab.set", "test2", "ucb2", 2, "a", "b")
EC("mab.set", "test2", "ucb1", 2, "a", "b")
