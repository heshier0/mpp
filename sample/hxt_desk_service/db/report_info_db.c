#include "report_info_db.h"
#include "common.h"

#define DB_PATH     ("/userdata/data/desk.db")

static sqlite3 *report_info_db;


BOOL open_report_info_db()
{
    int result = sqlite3_open(DB_PATH, &report_info_db);
    if (result != 0)
    {
        utils_print("open database %s failed, errcode=%d\n", DB_PATH, result);
        return FALSE;
    }

    utils_print("open database %s OK\n", DB_PATH);

    return TRUE;
}

BOOL close_report_info_db()
{
    if (NULL == report_info_db)
    {
        utils_print("database is NULL\n");
        return TRUE;
    }

    int result = sqlite3_close(report_info_db);
    if (result != 0)
    {
        utils_print("close database failed, errcode=%d\n", result);
        return FALSE;
    }

    return TRUE;
}

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
    result = sqlite3_exec(report_info_db, sql, NULL, NULL, &err_msg);
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

    ReportInfo* info = (ReportInfo *)data;
    
    /* change date from string to int */
    int report_time = 0;

    const char* insert_sql = "INSERT INTO report_table VALUES(%d, %d, %d, %d, %d, %d, %d, \"%s\", \"%s\")";
    sprintf(sql, insert_sql, info->parent_unid, info->child_unid, info->report_type, report_time, 
                                info->study_mode, info->camera_status, info->duration, info->video_url, info->snap_url);
    // result = sqlite3_prepare_v2(report_info_db, sql, strlen(sql), &stmt, NULL);


    return  0;
}

