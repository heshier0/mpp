#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

BOOL set_wifi_params(const  char* ssid, const char* pwd, const char* check_code)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (NULL == ssid && NULL == pwd && NULL == check_code)
    {
        return FALSE;
    }

    sql = sqlite3_mprintf("replace into %s (id, ssid, pwd, checkCode) values (1, '%s', '%s', '%s')", WIFI_PARAMS_TABLE, ssid, pwd, check_code);
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

char* get_wifi_ssid()
{
    return get_string_value_from_table(WIFI_PARAMS_TABLE, "ssid");
}

char* get_wifi_pwd()
{
    return get_string_value_from_table(WIFI_PARAMS_TABLE, "pwd");
}

char* get_wifi_check_code()
{
    return get_string_value_from_table(WIFI_PARAMS_TABLE, "checkCode");
}

BOOL deinit_wifi_params()
{
    return delete_table(WIFI_PARAMS_TABLE);
}