#include <string.h>

#include "redismodule.h"
#include "multiarm.h"

#define MABREDIS_ENCODING_VERSION   0
#define MABREDIS_METHOD_VERSION     1
#define MABREDIS_TYPE_NAME          "mab-nadia"
#define MABREDIS_STATBUF_SIZE       1024
#define MABREDIS_MAXCHOICE_NUM      64

static RedisModuleType *mabType;

struct sstr_s{
    uint8_t     *data;
    size_t      len;
};
typedef struct sstr_s sstr_t;

struct mab_type_obj_s {
    sstr_t              **choices;
    int                 choice_num;
    multi_arm_t         *ma;
};
typedef struct mab_type_obj_s mab_type_obj_t;

static void *mabTypeRDBLoad(RedisModuleIO *rdb, int encv);
static void mabTypeRDBSave(RedisModuleIO *rdb, void *value);
static void mabTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key,
        void *value);
static size_t mabTypeMemUsage(const void *value);
static void mabTypeFree(void *value);
static void mabTypeDigest(RedisModuleDigest *md, void *value);

static int mabTypeSet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **,
        int);
static int mabTypeChoice_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **,
        int);
static int mabTypeReward_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **,
        int);
static int mabTypeConfig_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **,
        int);
static int mabTypeStatJson_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **,
        int );
static RedisModuleKey * mabType_OpenKey(RedisModuleCtx *ctx, RedisModuleString *);

static mab_type_obj_t * mab_type_obj_new(const char *type, size_t type_len,
        void **choices, int choice_num, int rdb_load);

static void mab_type_obj_free(mab_type_obj_t *);

int
RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    multi_arm_alloc_set(RedisModule_Alloc, RedisModule_Free, RedisModule_Realloc);

    if(RedisModule_Init(ctx, MABREDIS_TYPE_NAME, 1,
                REDISMODULE_APIVER_1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }

    RedisModuleTypeMethods  tm = {
        .version = MABREDIS_METHOD_VERSION,
        .rdb_load = mabTypeRDBLoad,
        .rdb_save = mabTypeRDBSave,
        .aof_rewrite = mabTypeAofRewrite,
        .mem_usage = mabTypeMemUsage,
        .free = mabTypeFree,
        .digest = mabTypeDigest
    };

    mabType = RedisModule_CreateDataType(ctx, MABREDIS_TYPE_NAME,
            MABREDIS_ENCODING_VERSION, &tm);
    if(mabType == NULL){
        return REDISMODULE_ERR;
    }

    if(RedisModule_CreateCommand(ctx, "mab.set", mabTypeSet_RedisCommand, 
                "write deny-oom", 1, 1, 1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }

    if(RedisModule_CreateCommand(ctx, "mab.choice", mabTypeChoice_RedisCommand,
                "random", 1, 1, 1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }

    if(RedisModule_CreateCommand(ctx, "mab.reward", mabTypeReward_RedisCommand,
                "write fast deny-oom", 1, 1, 1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }

    if(RedisModule_CreateCommand(ctx, "mab.config", mabTypeConfig_RedisCommand,
                "write fast", 1, 1, 1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }

    if(RedisModule_CreateCommand(ctx, "mab.statjson", mabTypeStatJson_RedisCommand,
                "readonly", 1, 1, 1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }
    
    return REDISMODULE_OK;
}


/* 
 * command:
 *
 * mab.set $key $type $choice_num $choice1 $choice2 $choice3 ...
 * 
 * return:
 *
 * $choice_num
 */

static int
mabTypeSet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString *argv[], int argc)
{
    RedisModule_AutoMemory(ctx); 

    if(argc < 5){
        return RedisModule_WrongArity(ctx);
    }

    long long   choice_num = 0;
    if(RedisModule_StringToLongLong(argv[3], &choice_num) == REDISMODULE_ERR){
        return RedisModule_ReplyWithError(ctx,
                "ERR choice number must be a interger");
    }
    if(choice_num > MABREDIS_MAXCHOICE_NUM){
        return RedisModule_ReplyWithError(ctx, "ERR choice_num too big");
    }

    if(choice_num != argc - 4){
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey  *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ|REDISMODULE_WRITE);
    if(key != REDISMODULE_KEYTYPE_EMPTY){
        return RedisModule_ReplyWithError(ctx, "ERR key already exist");
    }

    size_t      l;
    const char  *type = RedisModule_StringPtrLen(argv[2], &l);

    mab_type_obj_t    *mabobj = mab_type_obj_new(type, l, (void **)argv + 4, (int)choice_num, 0);
    if(mabobj == NULL){
        return RedisModule_ReplyWithError(ctx, "ERR mab obj create failed");
    }

    RedisModule_ModuleTypeSetValue(key, mabType, mabobj);
    RedisModule_ReplyWithLongLong(ctx, choice_num);
    return REDISMODULE_OK;
}


/* 
 * command:
 * mab.choice $key
 *
 * 
 * return:
 * (idx, reward)
 */
static int
mabTypeChoice_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
        int argc)
{
    RedisModule_AutoMemory(ctx);
    if(argc != 2){
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey  *key = mabType_OpenKey(ctx, argv[1]);
    if(key == NULL){
        return REDISMODULE_OK;
    }

    mab_type_obj_t  *mabobj = RedisModule_ModuleTypeGetValue(key);

    int             idx;
    sstr_t          *choice = multi_arm_choice(mabobj->ma, &idx);

    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithLongLong(ctx, idx);
    RedisModule_ReplyWithStringBuffer(ctx, (char *)choice->data, choice->len);

    return REDISMODULE_OK;
}

/* 
 * command
 * mab.reward $key $idx $reward
 *
 * return 0
 *
 */
static int
mabTypeReward_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
        int argc)
{
    RedisModule_AutoMemory(ctx);

    if(argc != 4){
        return RedisModule_WrongArity(ctx);
    }

    long long idx;
    if(RedisModule_StringToLongLong(argv[2], &idx) == REDISMODULE_ERR){
        return RedisModule_ReplyWithError(ctx,
                "ERR invalid idx value must be a integer");
    }

    double reward;
    if(RedisModule_StringToDouble(argv[3], &reward) == REDISMODULE_ERR){
        return RedisModule_ReplyWithError(ctx,
                "ERR invalid reward value must be double");
    }

    RedisModuleKey  *key = mabType_OpenKey(ctx, argv[1]);
    if(key == NULL){
        return REDISMODULE_OK;
    }

    mab_type_obj_t  *mabobj = RedisModule_ModuleTypeGetValue(key);

    if(multi_arm_reward(mabobj->ma, (int)idx, reward) != 0){
        return RedisModule_ReplyWithError(ctx,
                "ERR invalid idx exceed choice range");
    }

    RedisModule_ReplyWithLongLong(ctx, 0);
    return REDISMODULE_OK;
}


/*
 * reconfig specific bandit arm count reward value. used by redis aof
 *
 * command:
 * mab.config $key $arm_idx1 $count1 $reward1 ....
 *
 * return:
 * 0
 */
static int
mabTypeConfig_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);

    if(argc - 2 % 3 != 0){
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey  *key = mabType_OpenKey(ctx, argv[1]);
    if(key == NULL){
        return REDISMODULE_OK;
    }

    mab_type_obj_t  *mabobj = RedisModule_ModuleTypeGetValue(key);
    size_t          arm_len = mabobj->choice_num;

    long long       tmp1;
    double          tmp2;
    int             i;
    for(i = 2; i < argc;){
        //for $arm_idx
        if(RedisModule_StringToLongLong(argv[i++], &tmp1) != REDISMODULE_OK){
            return RedisModule_ReplyWithError(ctx, "ERR expect a integer for arm idx");
        }
        if(tmp1 < 0 || tmp1 >= (long long)arm_len){
            return RedisModule_ReplyWithError(ctx, "ERR invalid idx value");
        }

        //for $count
        if(RedisModule_StringToLongLong(argv[i++], &tmp1) != REDISMODULE_OK){
            return RedisModule_ReplyWithError(ctx, "ERR expect a integer for count");
        }
        if(tmp1 < 0){
            return RedisModule_ReplyWithError(ctx, "ERR invalid count value");
        }

        //for reward
        if(RedisModule_StringToDouble(argv[i++], &tmp2) != REDISMODULE_OK){
            return RedisModule_ReplyWithError(ctx, "ERR expect a double for reward");
        }
        if(tmp2 < 0){
            return RedisModule_ReplyWithError(ctx, "ERR invalid reward value");
        }
    }


    arm_t   *arm;
    for(i = 2; i < argc;){
        //for $arm_idx
        RedisModule_StringToLongLong(argv[i++], &tmp1);
        arm = mabobj->ma->arms + tmp1;

        RedisModule_StringToLongLong(argv[i++], &tmp1);
        arm->count = tmp1;

        RedisModule_StringToDouble(argv[i++], &tmp2);
        arm->reward = tmp2;
    }

    return REDISMODULE_OK;
}


/*
 * command:
 * mab.statjson $key
 *
 * return:
 * a json string
 */
static int
mabTypeStatJson_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);

    if(argc != 2){
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = mabType_OpenKey(ctx, argv[1]);
    if(key == NULL){
        return REDISMODULE_OK;
    }

    mab_type_obj_t  *mabobj = RedisModule_ModuleTypeGetValue(key);
    char    buf[MABREDIS_STATBUF_SIZE];

    if(multi_arm_stat_json(mabobj->ma, buf, sizeof(buf)) != 0){
        return RedisModule_ReplyWithError(ctx, "buf size not enough");
    }

    return RedisModule_ReplyWithSimpleString(ctx, buf);
}

static RedisModuleKey *
mabType_OpenKey(RedisModuleCtx *ctx, RedisModuleString *key)
{
    RedisModuleKey *ret = RedisModule_OpenKey(ctx, key, REDISMODULE_READ);
    if(ret == NULL){
        RedisModule_ReplyWithError(ctx, "ERR mab key not exist");
        return NULL;
    }

    if(RedisModule_ModuleTypeGetType(ret) != mabType){
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return NULL;
    }
    return ret;
}


static mab_type_obj_t *
mab_type_obj_new(const char *type, size_t type_len, void **choice_strs, int choice_num, int rdb_load)
{
    mab_type_obj_t  *ret = RedisModule_Alloc(sizeof(*ret));

    sstr_t          **choices;

    if(rdb_load == 0){
        size_t          len;
        int             i;
        choices = RedisModule_Calloc(choice_num, sizeof(sstr_t *));

        for(i = 0; i < choice_num; i++){
            choices[i] = RedisModule_Alloc(sizeof(sstr_t));

            choices[i]->data = (uint8_t *)RedisModule_Strdup(RedisModule_StringPtrLen(
                        ((RedisModuleString **)choice_strs)[i], &len));
            choices[i]->len = len;
        }
    }else{
        choices = (sstr_t **)choice_strs;
    }

    char *t = RedisModule_Alloc(type_len + 1);
    memcpy(t, type, type_len);
    t[type_len] = '\0';

    ret->ma = multi_arm_new(t, (void **)choices, choice_num);
    if(ret->ma == NULL){
        if(rdb_load == 0){
            int i;
            for(i = 0; i < choice_num; i++){
                RedisModule_Free(choices[i]->data);
                RedisModule_Free(choices[i]);
            }
            RedisModule_Free(choices);
        }
        RedisModule_Free(ret);
        ret = NULL;
    }else{
        ret->choice_num = choice_num;
        ret->choices = choices;
    }

    RedisModule_Free(t);
    return ret;
}

static void
mab_type_obj_free(mab_type_obj_t *mabobj)
{
    int     i = 0;
    for(i = 0; i < (int)mabobj->choice_num; i++){
        RedisModule_Free(mabobj->choices[i]->data);
        RedisModule_Free(mabobj->choices[i]);
    }

    multi_arm_free(mabobj->ma);
    RedisModule_Free(mabobj->choices);
    RedisModule_Free(mabobj);
}


static void
mabTypeRDBSave(RedisModuleIO *rdb, void *value)
{
    mab_type_obj_t  *mabobj = value;
    multi_arm_t     *ma = mabobj->ma;
    int             i = 0;
    //save choices array
    RedisModule_SaveUnsigned(rdb, mabobj->choice_num);
    for(i = 0; i < mabobj->choice_num; i++){
        RedisModule_SaveStringBuffer(rdb, (char *)mabobj->choices[i]->data,
                mabobj->choices[i]->len);
    }

    //save policy earlier handy for reload
    RedisModule_SaveStringBuffer(rdb, ma->policy.name,
            strlen(ma->policy.name));

    //save multi_arm_t arms
    RedisModule_SaveUnsigned(rdb, ma->len);
    for(i = 0; i < ma->len; i++){
        RedisModule_SaveUnsigned(rdb, ma->arms[i].count);
        RedisModule_SaveDouble(rdb, ma->arms[i].reward);
    }

    RedisModule_SaveUnsigned(rdb, ma->total_count);
}


static void *
mabTypeRDBLoad(RedisModuleIO *rdb, int encv)
{
    if(encv != 0){
        RedisModule_LogIOError(rdb, "warning", "can not load with version %d", encv);
        return NULL;
    }

    long long   choice_num;

    choice_num = RedisModule_LoadUnsigned(rdb);

    sstr_t   **strs = RedisModule_Calloc(choice_num, sizeof(void *));
    int         i;
    for(i = 0; i < choice_num; i++){
        strs[i] = RedisModule_Alloc(sizeof(sstr_t));
        strs[i]->data = (uint8_t *)RedisModule_LoadStringBuffer(rdb, &(strs[i]->len));
    }

    mab_type_obj_t  *mabobj = NULL;
    long long       arm_len;
    size_t          type_len;
    //load policy name
    char            *type = RedisModule_LoadStringBuffer(rdb, &type_len);

    //load arms->len
    arm_len = RedisModule_LoadUnsigned(rdb);
    if(arm_len != choice_num){
        RedisModule_LogIOError(rdb, "warning", "choice %lu != arms %lu", choice_num, arm_len);
        goto error1;
    }

    mabobj = mab_type_obj_new(type, type_len, (void **)strs, choice_num, 1);
    if(mabobj == NULL){
        RedisModule_LogIOError(rdb, "warning", "mab obj create failed");
        goto error1;
    }

    //reload arms info
    for(i = 0; i < choice_num; i++){
        mabobj->ma->arms[i].count = RedisModule_LoadUnsigned(rdb);
        mabobj->ma->arms[i].reward = RedisModule_LoadDouble(rdb);
    }
    mabobj->ma->total_count = RedisModule_LoadUnsigned(rdb);
    goto error0;

error1:
    for(i = 0; i < choice_num; i++){
        RedisModule_Free(strs[i]);
        RedisModule_Free(strs);
    }

error0:
    RedisModule_Free(type);
    return mabobj;
}

static void
mabTypeFree(void *value)
{
    mab_type_obj_free((mab_type_obj_t *)value);
}

static void
mabTypeDigest(RedisModuleDigest *md, void *value)
{
    mab_type_obj_t  *mabobj = (mab_type_obj_t *)value;
    int             i;

    for(i = 0; i < mabobj->choice_num; i++){
        RedisModule_DigestAddStringBuffer(md, mabobj->choices[i]->data, mabobj->choices[i]->len);
    }
    RedisModule_DigestAddLongLong(md, mabobj->choice_num);

    multi_arm_t     *ma = mabobj->ma;
    for(i = 0; i < ma->len; i++){
        RedisModule_DigestAddLongLong(md, ma->arms[i].count);
    }
    RedisModule_DigestAddLongLong(md, ma->len);

    RedisModule_DigestAddLongLong(md, ma->total_count);

    RedisModule_DigestEndSequence(md);
}


static size_t
mabTypeMemUsage(const void *value)
{
    mab_type_obj_t  *mabobj = (mab_type_obj_t *)value;
    multi_arm_t     *ma = mabobj->ma;
    size_t          ret = 0;
    int             i;

    //size of choices;
    for(i = 0; i < mabobj->choice_num; i++){
        ret += sizeof(sstr_t) + mabobj->choices[i]->len;
    }

    //size of ma
    for(i = 0; i < ma->len; i++){
        ret += sizeof(ma->arms[i]);
    }
    ret += sizeof(*ma);

    ret += sizeof(*mabobj);

    return ret;
}

static void
mabTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value)
{
    mab_type_obj_t  *mabobj = value;
    arm_t           *arms = mabobj->ma->arms;
    char            reward_str[128];
    int             i, len = mabobj->ma->len;
    for(i = 0; i < len; i++){
        //redis does not support double format specifier. convert to string
        snprintf(reward_str, sizeof(reward_str), "%.4f", arms[i].reward);

        RedisModule_EmitAOF(aof, "mab.config", "cllc", key, i, arms[i].count, reward_str);
    }
}
