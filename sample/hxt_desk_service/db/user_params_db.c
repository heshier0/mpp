#include "common.h"
#include "db.h"

/*
create table if not exists UserParams(id integer primary key asc not null,
childID integer unique, studyMode integer, alarmType integer, isSelect integer);
*/
extern sqlite3 *g_hxt_service_db;

BOOL set_user_params(int child_id, int study_mode, int alarm_type, int selected)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    sql = sqlite3_mprintf("insert or replace into %s (id,childID,studyMode,alarmType,isSelect) \
                            values ((select id from %s where childID=%d),%d,%d,%d,%d)", 
                            USER_PARAMS_TABLE, USER_PARAMS_TABLE, 
                            child_id,
                            child_id, study_mode, alarm_type, selected);
    utils_print("%s\n", sql);
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

BOOL set_all_unselected()
{
    int result;
    char *err_msg;
    char *sql = NULL;
    /* first set unselect every child*/
    sql = sqlite3_mprintf("update %s set isSelect=0 where isSelect=1", USER_PARAMS_TABLE);
    utils_print("%s\n", sql);
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


BOOL update_select_child(int child_id)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    set_all_unselected();

    /* first set unselect every child*/
    sql = sqlite3_mprintf("update %s set isSelect=1 where childID=%d", USER_PARAMS_TABLE, child_id);
    utils_print("%s\n", sql);
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

BOOL update_study_mode(int child_id, int study_mode)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    set_all_unselected();

    /* first set unselect every child*/
    sql = sqlite3_mprintf("update %s set studyMode=%d where childID=%d", USER_PARAMS_TABLE, study_mode, child_id);
    utils_print("%s\n", sql);
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

BOOL update_alarm_type(int child_id, int alarm_type)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    // set_all_unselected();

    /* first set unselect every child*/
    sql = sqlite3_mprintf("update %s set alarmType=%d where childID=%d", USER_PARAMS_TABLE, alarm_type, child_id);
    utils_print("%s\n", sql);
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

int get_select_child_id()
{
    int result;
    char *err_msg;
    char *sql = NULL;
    char **db_result;
    int row_count, col_count;
    int value = 0;

    sql = sqlite3_mprintf("select childID from %s where isSelect=1", USER_PARAMS_TABLE);
    // utils_print("%s\n", sql);
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s:%s\n", sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return -1;        
    }
    
    if (row_count < 1)
    {
        return -1;
    }

    value = atoi(db_result[col_count]);

    sqlite3_free_table(db_result);
    sqlite3_free(sql);

    return value;
}

int get_study_mode(int child_unid)
{
    int result;
    char *err_msg;
    char *sql = NULL;
    char **db_result;
    int row_count, col_count;
    int value = 0;

    sql = sqlite3_mprintf("select studyMode from %s where childID=%d", USER_PARAMS_TABLE, child_unid);
    utils_print("%s\n", sql);
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s:%s\n", sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return 3;        
    }
    
    if (row_count < 1)
    {
        return 3;
    }

    value = atoi(db_result[col_count]);

    sqlite3_free_table(db_result);
    sqlite3_free(sql);

    return value;
}

int get_alarm_type(int child_unid)
{
    int result;
    char *err_msg;
    char *sql = NULL;
    char **db_result;
    int row_count, col_count;
    int value = 0;

    sql = sqlite3_mprintf("select alarmType from %s where childID=%d", USER_PARAMS_TABLE, child_unid);
    utils_print("%s\n", sql);
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s:%s\n", sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return 2;        
    }
    
    if (row_count < 1)
    {
        return 2;
    }

    value = atoi(db_result[col_count]);

    sqlite3_free_table(db_result);
    sqlite3_free(sql);

    return value;
}

BOOL delete_child(int child_unid)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    sql = sqlite3_mprintf("delete from %s where childID=%d", USER_PARAMS_TABLE, child_unid);
    utils_print("%s\n", sql);
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

BOOL deinit_user_params()
{
    return delete_table(USER_PARAMS_TABLE);
}