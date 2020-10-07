#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

BOOL set_server_params(const char* wscUrl, const char* uploadUrl)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (NULL == wscUrl && NULL == uploadUrl)
    {
        return FALSE;
    }

    if (NULL == wscUrl)
    {
        sql = sqlite3_mprintf("update %s set wscUrl = '%s'", SERVER_PARAMS_TABLE, wscUrl);
    } 
    else if (NULL == uploadUrl)
    {
        sql = sqlite3_mprintf("update %s set uploadUrl = '%s'", SERVER_PARAMS_TABLE, uploadUrl);
    }
    else
    {
        sql = sqlite3_mprintf("update %s set wscUrl='%s', uploadUrl='%s'", SERVER_PARAMS_TABLE, wscUrl, uploadUrl);
    }
    
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

char* get_server_url()
{
    return get_string_value_from_table(SERVER_PARAMS_TABLE, "serverUrl");
}

char* get_api_version()
{
    return get_string_value_from_table(SERVER_PARAMS_TABLE, "apiVer");
}

char* get_websocket_url()
{
    return get_string_value_from_table(SERVER_PARAMS_TABLE, "wscUrl");
}

char* get_upload_url()
{
    return get_string_value_from_table(SERVER_PARAMS_TABLE, "uploadUrl");
}

BOOL deinit_server_params()
{
    return set_server_params("", "");
}