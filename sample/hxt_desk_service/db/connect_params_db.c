#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

BOOL set_connect_params(const char* token, const int tokenExpiredTime)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    if (NULL == token)
    {
        return FALSE;
    }

    sql = sqlite3_mprintf("replace into %s (id, token, tokenExpiredTime) values (1, '%s', %d)", CONNECT_PARAMS_TABLE, token, tokenExpiredTime);
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

char* get_server_token()
{
    return get_string_value_from_table(CONNECT_PARAMS_TABLE, "token");
}

int get_server_token_expired_time()
{
    return get_number_value_from_table(CONNECT_PARAMS_TABLE, "tokenExpiredTime");
}

BOOL deinit_connect_params()
{
    return delete_table(CONNECT_PARAMS_TABLE);
}