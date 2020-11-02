#include "common.h"
#include "utils.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

BOOL create_upload_count_info()
{
    int result;
    char *err_msg;
    char *sql = NULL;

    sql = sqlite3_mprintf("insert into %s (id,upload_day,upload_count) values (NULL, '%s', 0)", UPLOAD_COUNT_TABLE, utils_date_to_string());
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

BOOL init_upload_count()
{
    int result;
    char *err_msg;
    char *sql = NULL;
   
    sql = sqlite3_mprintf("update %s set upload_day=\"\" update_count=0", UPLOAD_COUNT_TABLE);
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

BOOL inc_upload_count()
{
    int result;
    char *err_msg;
    char *sql = NULL;
    
    sql = sqlite3_mprintf("update %s set upload_count=upload_count+1 where upload_day=%s", UPLOAD_COUNT_TABLE, utils_date_to_string());
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

int get_upload_count_of_day()
{
    int result;
    char *err_msg;
    char **db_result;
    int row_count, col_count;
    int value = 0;

    char* sql = sqlite3_mprintf("select upload_count from %s where upload_day=%s", UPLOAD_COUNT_TABLE, utils_date_to_string());
    utils_print("%s\n", sql);
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

BOOL deinit_upload_count_params()
{
    return delete_table(UPLOAD_COUNT_TABLE);
}