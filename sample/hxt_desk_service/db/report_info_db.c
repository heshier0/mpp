#include "report_info_db.h"
#include "common.h"
#include "utils.h"

extern sqlite3 *g_hxt_service_db;

BOOL add_report_info(void* data)
{
    int result;
    char *err_msg;
    char *sql = NULL;
    
    if (NULL == data)
    {
        return FALSE;
    }
    ReportInfo *info = (ReportInfo *)data;
    sql = sqlite3_mprintf("INSERT INTO %s VALUES(NULL, %d, %d, %d, '%s', %d, %d, %d, '%s', '%s')",
                                            REPORT_INFOS_TABLE,
                                            info->parent_unid, 
                                            info->child_unid,
                                            info->report_type, 
                                            info->report_time,
                                            info->study_mode,
                                            info->camera_status,
                                            info->duration,
                                            info->video_url,
                                            info->snap_url);
    utils_print("%s\n", sql);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s:%s\n",sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        return FALSE;
    }

    sqlite3_free(sql);  
    return TRUE;
}

BOOL del_report_info(int id)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (id < 0)
    {
        return FALSE;
    }

    sql = sqlite3_mprintf("DELETE FROM %s WHERE id=%d", REPORT_INFOS_TABLE, id);
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

BOOL del_report_info_expired()
{
    char *err_msg;
    int result;
    char* sql = NULL;

    sql = sqlite3_mprintf("DELETE FROM %s WHERE DATE('NOW','-7 DAY') >= DATE(reportTime)", REPORT_INFOS_TABLE);
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


int get_report_info(void *data)
{
    char *err_msg;
    char **db_result;
    char* sql = NULL;
    int result;
    int row_count, col_count;
    int id = -1;
    

    if (NULL == data)
    {
        return -1;
    }

    sql = sqlite3_mprintf("select * from %s order by reportTime asc limit 0,1", REPORT_INFOS_TABLE);
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("select data failed:%s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return -1;        
    }

    if (row_count < 1)
    {
        return -1;
    }

    ReportInfo* tmp = (ReportInfo *)data;
    int index = col_count;
    id = atoi(db_result[index++]);
    tmp->parent_unid = atoi(db_result[index++]);
    tmp->child_unid = atoi(db_result[index++]);
    tmp->report_type = atoi(db_result[index++]);
    strcpy(tmp->report_time, db_result[index++]);
    tmp->study_mode = atoi(db_result[index++]);
    tmp->camera_status = atoi(db_result[index++]);
    tmp->camera_status = atoi(db_result[index++]);
    strcpy(tmp->video_url, db_result[index++]);
    strcpy(tmp->snap_url, db_result[index++]);

    sqlite3_free(sql);
    sqlite3_free_table(db_result);

    return id;
}

int get_report_info_count()
{
    char *err_msg;
    int result;
    char **db_result;
    int row_count, col_count;
    char* sql = NULL;

    sql = sqlite3_mprintf("select * from %s", REPORT_INFOS_TABLE);
    utils_print("%s\n", sql);
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("select data failed:%s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return -1;        
    }

    sqlite3_free(sql);
    sqlite3_free_table(db_result);

    return row_count;
}

int get_amount_records_of_day()
{
    char *err_msg;
    int result;
    char **db_result;
    int row_count, col_count;
    char* sql = NULL;

    sql = sqlite3_mprintf("SELECT COUNT(*) FROM %s WHERE reportTime>=datetime('now', 'start of day', '+0 day') and \
                             reportTime<datetime('now', 'start of day', '+1 day')", REPORT_INFOS_TABLE);
    utils_print("%s\n");
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("select data failed:%s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return -1;        
    }
    sqlite3_free(sql);
    sqlite3_free_table(db_result);                         
   
   return row_count; 
}

BOOL update_mp4_url(int id, const char *url)
{
    char *err_msg;
    int result;
    char* sql = NULL;

    sql = sqlite3_mprintf("UPDATE %s SET videoUrl= %s WHERE ID=%d", REPORT_INFOS_TABLE, url, id);
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

BOOL update_snap_url(int id, const char *url)
{
    char *err_msg;
    int result;
    char *sql = NULL;

    sql = sqlite3_mprintf("UPDATE %s SET snapUrl= %s WHERE ID=%d", REPORT_INFOS_TABLE, url, id);
    utils_print("%s\n", sql);

    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("delete data failed:%s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);

        return FALSE;
    }

    sqlite3_free(sql);
    return TRUE;
}

