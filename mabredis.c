#include "redismodule.h"
#include "multiarm.h"

#define MABREDIS_ENCODING_VERSION   0
#define MABREDIS_METHOD_VERSION     1
#define MABREDIS_TYPE_NAME          "mab-nadia"

#define MABREDIS_ERR_NOTEXISTS      "ERR mab key not exists"
static RedisModuleType *mabType;

struct sstr_s{
    uint8_t     *data;
    size_t      len;
};
typedef struct sstr_s sstr_t;

struct mab_type_obj_s {
    sstr_t              *choices;
    size_t              choice_num;
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

static RedisModuleKey * mabType_OpenKey(RedisModuleCtx *ctx, RedisModuleString *);

static mab_type_obj_t * mab_type_obj_new(const char *type,
        void **choices, int choice_num, int rdb_load);

static void mab_type_obj_free(mab_type_obj_t *);

int
RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
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

    if(choice_num != argc - 4){
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey  *key = mabType_OpenKey(ctx, argv[1]);
    if(key == NULL){
        return REDISMODULE_OK;
    }

    size_t  l;
    mab_type_obj_t    *mabobj = mab_type_obj_new(RedisModule_StringDMA(argv + 2,
                &l, REDISMODULE_READ), argv + 4, (int)choice_num, 0);

    RedisModule_ModuleTypeSetValue(key, mabType, mabobj);
    RedisModule_ReplyWithLongLong(key, choice_num);
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
    sstr_t          *choice = mabobj->ma->choice(mabobj->ma, &idx);

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

    if(mabobj->ma->reward(mabobj->ma, (int)idx, reward) != 0){
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


    int     i;
    for(i = 0; i < argc;){
    }
}


static RedisModuleKey *
mabType_OpenKey(RedisModuleCtx *ctx, RedisModuleString *key)
{
    RedisModule_KeyType *ret = RedisModule_OpenKey(key, REDISMODULE_READ);
    if(ret == NULL){
        RedisModule_ReplyWithError(ctx, MABREDIS_ERR_NOTEXISTS);
        return NULL;
    }

    if(RedisModule_ModuleTypeGetType(ret) != mabType){
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return NULL;
    }
    return ret;
}


static mab_type_obj_t *
mab_type_obj_new(const char *type, void **choice_strs, int choice_num, int rdb_load)
{
    mab_type_obj_t  *ret = RedisModule_Alloc(sizeof(*ret));

    sstr_t          **choices;

    if(rdb_load != 0){
        size_t          len;
        int             i;
        choices = RedisModule_Calloc(choice_num, sizeof(sstr_t *));

        for(i = 0; i < choice_num; i++){
            choices[i] = RedisModule_Alloc(sizeof(sstr_t));

            choices[i]->data = (uint8_t *)RedisModule_Strdup(RedisModule_StringDMA(
                        ((RedisModuleString **)choice_strs)[i], &len, REDISMODULE_READ));
            choices[i]->len = len;
        }
    }else{
        choices = choice_strs;
    }

    ret->ma = multi_arm_new(type, choices, choice_num);
    ret->choice_num = choice_num;

    return ret;
}

static void
mab_type_obj_free(mab_type_obj_t *mabobj)
{
    int     i = 0;
    for(i = 0; i < mabobj->choice_num; i++){
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
    int             i = 0;

    //save choices array
    RedisModule_SaveUnsigned(mabobj->choice_num);
    for(i = 0; i < mabobj->choice_num; i++){
        RedisModule_SaveStringBuffer(rdb, (char *)mabobj->choices[i]->data,
                mabobj->choices[i]->size);
    }

    //save policy earlier handy for reload
    RedisModule_SaveStringBuffer(rdb, ma->policy.name,
            strlen(ma->policy.name));

    //save multi_arm_t arms
    multi_arm_t *ma = mabobj->ma;
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
        return NULL;
    }

    long long   len;

    RedisModule_LoadUnsigned(rdb, &len);

    sstr_t   **strs = RedisModule_Calloc(len, sizeof(void *));
    double      val;
    int         i;
    for(i = 0; i < len; i++){
        strs[i] = RedisModule_Alloc(sizeof(sstr_t));
        strs[i]->data = (uint8_t *)RedisModule_LoadStringBuffer(rdb, &(strs[i]->len));
    }

    long long       l;
    //load policy name
    const char      *type = RedisModule_LoadStringBuffer(rdb, &l, REDISMODULE_READ);

    //load arms->len
    RedisModule_LoadUnsigned(rdb, &l);
    if(l != len){
        for(i = 0; i < len; i++){
            RedisModule_Free(strs[i]);
            RedisModule_Free(strs);
        }
        return NULL;
    }

    mab_type_obj_t  *mabobj = mab_type_obj_new(type, strs, len, 1);

    //reload arms info
    for(i = 0; i < len; i++){
        mabobj->ma->arms[i].count = RedisModule_LoadUnsigned(rdb);
        mabobj->ma->arms[i].reward = RedisModule_LoadDouble(rdb);
    }
    mabobj->ma->total_count = RedisModule_LoadUnsigned(rdb);

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
        RedisModule_DigestAddStringBuffer(md, mabobj->choices[i].data, mabobj->choices[i].len);
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
        ret += sizeof(sstr_t) + mabobj->choices[i].len;
    }

    //size of ma
    for(i = 0; i < ma->len; i++){
        ret += sizeof(ma->arms[i])
    }
    ret += sizeof(*ma);

    ret += sizeof(*mabobj);

    return ret;
}
