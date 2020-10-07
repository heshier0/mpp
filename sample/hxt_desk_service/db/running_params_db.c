#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

/*
* create table if not exists RunningParams(id integer primary key asc not null,judgeTime integer, videoLength integer, 
                                    videoRatio integer, videoCnt integer, snapCnt integer, offlineStorage integer,attachRatio integer);
*/

BOOL set_running_params(int judge_time, int video_length, int video_ratio, int video_cnt, 
                            int snap_count, int offline_storage, int attach_ratio)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    sql = sqlite3_mprintf("replace into %s (id,judgeTime,videoLength,videoRatio,videoCnt,snapCnt,offlineStorage,attachRatio) \
                            values (1,%d,%d,%d,%d,%d,%d,%d)", RUNNING_PARAMS_TABLE,
                            judge_time,
                            video_length,
                            video_ratio,
                            video_cnt,
                            snap_count,
                            offline_storage,
                            attach_ratio);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s:%s\n", sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        return FALSE;    
    }
    sqlite3_free(sql);

    return TRUE;
}

int get_judge_time()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "judgetime");
}

int get_video_length()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "videoLength");
}

int get_video_ratio()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "videoRatio");
}

int get_video_count()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "videoCnt");
}

int get_snap_count()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "snapCnt");
}

int get_offline_storage()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "offlineStorage");
}

int get_attach_ratio()
{
    return get_number_value_from_table(RUNNING_PARAMS_TABLE, "attachRatio");
}

BOOL deinit_running_params()
{
    return delete_table(RUNNING_PARAMS_TABLE);
}