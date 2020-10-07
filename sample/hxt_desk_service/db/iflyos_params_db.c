#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

BOOL set_iflyos_params(const char* token, const char* sn)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (NULL == token && NULL == sn)
    {
        return FALSE;
    }

    if (NULL == token)
    {
        sql = sqlite3_mprintf("update %s set sn = '%s'", IFLYOS_DEVICE_TABLE, sn);
    } 
    else if (NULL == sn)
    {
        sql = sqlite3_mprintf("update %s set token = '%s'", IFLYOS_DEVICE_TABLE, token);
    }
    else
    {
        sql = sqlite3_mprintf("replace into %s (id, token, sn) values (1, '%s', '%s')", IFLYOS_DEVICE_TABLE, token, sn);
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

char* get_iflyos_token()
{
    return get_string_value_from_table(IFLYOS_DEVICE_TABLE, "token");
}

char* get_iflyos_sn()
{
    return get_string_value_from_table(IFLYOS_DEVICE_TABLE, "sn");
}

BOOL deinit_iflyos_params()
{
    delete_table(IFLYOS_DEVICE_TABLE);
}