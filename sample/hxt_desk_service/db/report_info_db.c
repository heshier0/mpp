#include "report_info_db.h"
#include "common.h"
#include "utils.h"


BOOL create_report_info_table()
{
    char *err_msg;
    char *sql;
    int result;
    sql = "CREATE TABLE IF NOT EXISTS report_table( id INTEGER PRIMARY KEY ASC NOT NULL,\
                                                    parent INTEGER NOT NULL,\
                                                    child INTEGER NOT NULL,\
                                                    type INTEGER NOT NULL,\
                                                    reportTime INTEGER,\
                                                    mode INTEGER,\
                                                    camera INTEGER,\
                                                    duration INTEGER,\
                                                    videoUrl TEXT, \
                                                    snapVrl TEXT)";
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("create table failed:%s\n", err_msg);
        sqlite3_free(err_msg);

        return FALSE;
    }
         
    return TRUE;
}

BOOL add_report_info(void* data)
{
    char *err_msg;
    char sql[1024] = {0};
    int result;

    if (NULL == data)
    {
        return FALSE;
    }
    ReportInfo *info = (ReportInfo *)data;

    /* change date from string to int */
    const char* insert_sql = "INSERT INTO report_table VALUES(NULL, %d, %d, %d, \"%s\", %d, %d, %d, \"%s\", \"%s\")";
    sprintf(sql, insert_sql, info->parent_unid, info->child_unid, info->report_type, info->report_time, 
                                info->study_mode, info->camera_status, info->duration, info->video_url, info->snap_url);
    utils_print("sql:[%s]\n", sql);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("insert data failed:%s\n", err_msg);
        sqlite3_free(err_msg);

        return FALSE;
    }
         
    return TRUE;
}

BOOL del_report_info(int id)
{
    char *err_msg;
    char sql[1024] = {0};
    int result;

    if (id < 0)
    {
        return FALSE;
    }
    const char *del_sql = "DELETE FROM report_table WHERE id=%d"; 
    sprintf(sql, del_sql, id);
    utils_print("sql:[%s]\n", sql);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("delete data failed:%s\n", err_msg);
        sqlite3_free(err_msg);

        return FALSE;
    }

    return TRUE;
}

BOOL del_report_info_expired()
{
    char *err_msg;
    int result;

    const char *del_sql = "DELETE FROM report_table WHERE DATE('NOW','-7 DAY') >= DATE(reportTime)"; 
    utils_print("sql:[%s]\n", del_sql);
    result = sqlite3_exec(g_hxt_service_db, del_sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("delete data failed:%s\n", err_msg);
        sqlite3_free(err_msg);

        return FALSE;
    }

    return TRUE;
}

int get_report_info_count()
{
    char *err_msg;
    int result;
    char **db_result;
    int row_count, col_count;

    const char *select_sql = "SELECT * from report_table";
    utils_print("sql:[%s]\n", select_sql);
    result = sqlite3_get_table(g_hxt_service_db, select_sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("select data failed:%s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_free_table(db_result);
        return -1;        
    }
    sqlite3_free_table(db_result);

    return row_count;
}

int get_amount_records_of_day()
{
    char *err_msg;
    int result;
    char **db_result;
    int row_count, col_count;

    const char* query_sql = "SELECT COUNT(*) FROM report_table WHERE reportTime>=datetime('now', 'start of day', '+0 day') and \
                             reportTime<datetime('now', 'start of day', '+1 day')";

    result = sqlite3_get_table(g_hxt_service_db, query_sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("select data failed:%s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_free_table(db_result);
        return -1;        
    }
    sqlite3_free_table(db_result);                         
   
   return row_count; 
}

BOOL update_mp4_url(int id, const char *url)
{
    char *err_msg;
    int result;
    char sql[1024] = {0};

    const char *update_sql = "UPDATE report_table SET videoUrl= %s WHERE ID=%d";
    sprintf(sql, update_sql, id, url);
    utils_print("sql:[%s]\n", sql);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("delete data failed:%s\n", err_msg);
        sqlite3_free(err_msg);

        return FALSE;
    }

    return TRUE;
}

BOOL update_snap_url(int id, const char *url)
{
        char *err_msg;
    int result;
    char sql[1024] = {0};

    const char *update_sql = "UPDATE report_table SET snapUrl= %s WHERE ID=%d";
    sprintf(sql, update_sql, id, url);
    utils_print("sql:[%s]\n", sql);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("delete data failed:%s\n", err_msg);
        sqlite3_free(err_msg);

        return FALSE;
    }

    return TRUE;
}

