#!/usr/bin/python3

import os
import sys
import time
import random
import argparse
import subprocess

import redis

REDIS_CONF = "mab_test.conf"
REDIS_ADDR = "/tmp/mab_test.sock"

class RedisServer:
    def __init__(self, executable, module):

        path = os.path.abspath(executable)
        if not os.path.exists(path):
            raise IOError("redis server not exist {}".format(path))

        self.__cmd = [path, REDIS_CONF, "--loadmodule", module]
        self.__srv = None

    def start(self):
        self.__srv = subprocess.Popen(self.__cmd, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        retcode = self.__srv.poll()
        if retcode != None:
            raise OSError("redis start fail")

        #wait redis server start create sock file
        time.sleep(0.1)

    def stop(self):
        self.__srv.terminate()
        self.__srv.wait()

    def restart(self):
        self.stop()
        self.start()


class MabCmd:
    TYPE = None

    @staticmethod
    def newconn():
        return redis.from_url("unix://@{}".format(REDIS_ADDR))

    def __init__(self, *args):
        choices = args[0]
        self._key = "mab-test.{:.3}.{:.3}".format(time.time(), random.random())
        self._rates = [random.random() * 0.2 + 0.1 for _ in range(len(choices))]
        try:
            self._conn = MabCmd.newconn()
            cmd = self._buildsetcmd(choices, args[1:])
            self._conn.execute_command(*cmd)
        except redis.exceptions.RedisError as e:
            print("redis command execute fail {}".format(e))
            raise e


    def _buildsetcmd(self, choices, *args):
        return ("mab.set", self._key, self.TYPE, len(choices), *choices)

    def exec(self):
        idx, _ = self._conn.execute_command("mab.choice", self._key)
        #if random.random() < self._rates[idx]:
        self._conn.execute_command("mab.reward", self._key, idx, 0.1)
    

    def statjson(self):
        return self._conn.execute_command("mab.statjson", self._key)

    def clean(self):
        self._conn.execute_command("del", self._key)

    def info(self):
        print("{}: {}".format(self._key, self._rates))
        print(self.statjson())


class EgreedyCmd(MabCmd):
    TYPE = "egreedy"

    def _buildsetcmd(self, choices, *args):
        epsilon = args[0][0]
        return ("mab.set", self._key, self.TYPE, len(choices), *choices, epsilon)


class Ucb1Cmd(MabCmd):
    TYPE = "ucb1"


class ThompsenCmd(MabCmd):
    TYPE = "thompsen"


#restart redis server
#test dump.rdb serialize
def test_rdb(srv):
    cmds = (
        EgreedyCmd(("choice1", "choice2", "choice3"), 0.1),
        Ucb1Cmd(("choice1, choice2", "choice3", "choice4")),
        ThompsenCmd(("choice1", "choice2", "choice3"))
    )

    oldstats = []
    for cmd in cmds:
        for _ in range(0, 1000):
            cmd.exec()

        oldstats.append(cmd.statjson())

    # restart server
    srv.restart()

    newstats = []
    for cmd in cmds:
        newstats.append(cmd.statjson())
        cmd.clean()

    for i in range(0, len(oldstats)):
        if oldstats[i] != newstats[i]:
            raise ValueError("RDB TEST FAIL!")

    print("TEST OK!")

def test_cmd(srv):
    cmd = EgreedyCmd(("choice1", "choice2", "choice3"), 0.12)
    for _ in  range(0, 1000):
        cmd.exec()

    cmd.info()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--redis", type=str, required=True, help="specific redis-server path")
    parser.add_argument("--mab", type=str, required=True, help="specific mabredis.so path")
    parser.add_argument("--count", type=int, default=1000, help="specific test loop count")
    conf = parser.parse_args()

    srv = RedisServer(conf.redis, conf.mab)
    srv.start()

    try:
        test_rdb(srv)
        test_cmd(srv)
    except IOError as e:
        pass

    srv.stop()

    print("server stop")
