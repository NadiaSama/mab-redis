#include "redismodule.h"
#include "multiarm.h"

#define MABREDIS_ENCODING_VERSION   0
#define MABREDIS_METHOD_VERSION     1
#define MABREDIS_TYPE_NAME          "mab-nadia"

static RedisModuleType *mabType;

struct sstr_s{
    uint8_t     *data;
    size_t      len;
};
typedef struct sstr_s sstr_t;

struct mab_type_obj_s {
    sstr_t              *choices;
    multi_arm_t         *ma;
    size_t              choice_num;
};
typedef struct mab_type_obj_s mab_type_obj_t;

static void *mabTypeRdbLoad(RedisModuleIO *rdb, int event);
static void mabTypeRdbSave(RedisModuleIO *rdb, void *value);
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


static mab_type_obj_t * mab_type_obj_new(RedisModuleString *type,
        RedisModuleString **choices, int choice_num);

static void mab_type_obj_free(mab_type_obj_t *);

int
RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if(RedisModule_Init(ctx, MABREDIS_TYPE_NAME, 1,
                REDISMODULE_APIVER_1) == REDISMODULE_ERR){
        return REDISMODULE_ERR;
    }

    RedisModuleTypeMethods  tm = {
        .version = MABREDIS_METHOD_VERSION,
        .rdb_load = mabTypeRdbLoad,
        .rdb_save = mabTypeRdbSave,
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
    
    return REDISMODULE_OK;
}


/* mab.set $key $type $choice_num $choice1 $choice2 $choice3 ... */
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

    RedisModuleKey  *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ|REDISMODULE_WRITE);

    int type = RedisModule_KeyType(key);
    if(type != REDISMODULE_KEYTYPE_EMPTY){
        return RedisModule_ReplyWithError(ctx, "ERR key already exist");
    }

    mab_type_obj_t    *mabobj = mab_type_obj_new(argv + 2, argv + 4,
            (int)choice_num);

    RedisModule_ModuleTypeSetValue(key, mabType, mabobj);
    RedisModule_ReplyWithLongLong(key, choice_num);
    return REDISMODULE_OK;
}


static int
mabTypeChoice_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
        int argc)
{
    RedisModule_AutoMemory(ctx);
    if(argc != 2){
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey  *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    if(key == NULL){
        return RedisModule_ReplyWithError(ctx, "mab key not exist");
    }

    if(RedisModule_ModuleTypeGetType(key) != mabType){
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    mab_type_obj_t  *mabobj = RedisModule_ModuleTypeGetValue(key);

    int             idx;
    sstr_t          *choice = mabobj->ma->choice(mabobj->ma, &idx);

    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithLongLong(ctx, idx);
    RedisModule_ReplyWithStringBuffer(ctx, (char *)choice->data, choice->len);

    return REDISMODULE_OK;
}

static mab_type_obj_t *
mab_type_obj_new(RedisModuleString *type, RedisModuleString **choice_strs,
        int choice_num)
{
    mab_type_obj_t  *ret = RedisModule_Alloc(sizeof(*ret));
    size_t          len;
    sstr_t          **choices = RedisModule_Calloc(choice_num, sizeof(sstr_t *));
    int             i;

    for(i = 0; i < choice_num; i++){
        choices[i] = RedisModule_Alloc(sizeof(sstr_t));

        choices[i]->data = (uint8_t *)RedisModule_Strdup(RedisModule_StringDMA(type,
                    &len, REDISMODULE_READ));
        choices[i]->len = len;
    }

    ret->ma = multi_arm_new(RedisModule_StringDMA(type, &len, REDISMODULE_READ),
            choices, choice_num);
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
