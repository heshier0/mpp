#include <sqlite3.h>
#include "utils.h"
#include "common.h"
#include "db.h"

sqlite3 *g_hxt_service_db;

static BOOL create_params_tables()
{
    int result;
    char *err_msg;
    char line[1024] = {0};
    FILE *pFile = NULL;
    if ((pFile = fopen(INIT_SQL, "rb")) == NULL)
    {
        utils_print("Open file init.sql faild!\n");
        return FALSE;
    }
    
    while((fgets(line, 1024, pFile)) != NULL)
    {
        result = sqlite3_exec(g_hxt_service_db, line, NULL, NULL, &err_msg);
        if (result != SQLITE_OK)
        {
            utils_print("%s:%s \n",line, err_msg);
            sqlite3_free(err_msg);
            fclose(pFile);
            return FALSE;
        }
        bzero(line, 1024);
    }
    fclose(pFile);

    return TRUE;
}

BOOL open_hxt_service_db()
{
    int result = sqlite3_open_v2(DB_PATH, &g_hxt_service_db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL);
    if (result != 0)
    {
        utils_print("open database %s failed, errcode=%d\n", DB_PATH, result);
        return FALSE;
    }

    utils_print("open database %s OK\n", DB_PATH);

    return create_params_tables();
}

BOOL close_hxt_service_db()
{
    if (NULL == g_hxt_service_db)
    {
        utils_print("database is NULL\n");
        return TRUE;
    }

    int result = sqlite3_close(g_hxt_service_db);
    if (result != 0)
    {
        utils_print("close database failed, errcode=%d\n", result);
        return FALSE;
    }

    return TRUE;
}

void deinit_hxt_service_db()
{
    deinit_connect_params();
    deinit_desk_params();
    deinit_iflyos_params();
    deinit_running_params();
    deinit_server_params();
    deinit_update_params();
    deinit_user_params();
    deinit_wifi_params();
}

BOOL delete_table(const char* table_name)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (NULL == table_name)
    {
        return FALSE;
    }

    sql = sqlite3_mprintf("delete from %s", table_name);
    result = sqlite3_exec(g_hxt_service_db, sql, NULL, NULL, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s failed:%s\n", sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        return FALSE;    
    }

    sqlite3_free(sql);
    
    return TRUE;
}

int get_number_value_from_table(const char* table_name, const char* col_name)
{
    int result;
    char *err_msg;
    char **db_result;
    int row_count, col_count;
    int value = 0;

    char* sql = sqlite3_mprintf("select %s from %s", col_name, table_name);
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

char* get_string_value_from_table(const char* table_name, const char* col_name)
{
    int result;
    char *err_msg;
    char **db_result;
    int row_count, col_count;
    char* out_value = NULL;

    if (NULL == table_name || NULL == col_name)
    {
        return NULL;
    }

    char* sql = sqlite3_mprintf("select %s from %s", col_name, table_name);
    utils_print("%s\n", sql);
    result = sqlite3_get_table(g_hxt_service_db, sql, &db_result, &row_count, &col_count, &err_msg);
    if (result != SQLITE_OK)
    {
        utils_print("%s:%s\n", sql, err_msg);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_free_table(db_result);
        return NULL;        
    }
    
    if (row_count < 1)
    {
        return NULL;
    }

    char* item = db_result[col_count];
    if (item != NULL)
    {
        out_value = utils_malloc(strlen(item)+1);
        strcpy(out_value, item);
        utils_print("%s\n", out_value);
    }

    sqlite3_free_table(db_result);
    sqlite3_free(sql);

    return out_value;
}

BOOL set_number_value_into_table(const char* table_name, const char* col_name, const int value)
{
    int result;
    char *err_msg;
    char *sql = NULL;
    
    if (NULL == table_name || NULL == col_name)
    {
        return FALSE;
    }

    sql = sqlite3_mprintf("update %s set %s=%d", table_name, col_name, value);
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

BOOL set_string_value_into_table(const char* table_name, const char* col_name, const char* value)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (NULL == table_name || NULL == col_name || NULL == value)
    {
        return FALSE;
    }
    
    sql = sqlite3_mprintf("update %s set %s = '%s'", table_name, col_name, value);
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
