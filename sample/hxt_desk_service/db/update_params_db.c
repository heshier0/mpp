#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

/*
create table if not exists UpdateParams(id integer primary key asc not null,verID integer, verNO text, packUrl text);
*/

BOOL set_update_params(const int version_id, const char* version_no, const char* packUrl)
{
    int result;
    char *err_msg;
    char *sql = NULL;

    sql = sqlite3_mprintf("replace into %s (id,verID,verNO,packUrl) values (1,%d,'%s','%s')", UPDATE_PARAMS_TABLE, version_id, version_no, packUrl);
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

int get_update_version_id()
{
    return get_number_value_from_table(UPDATE_PARAMS_TABLE, "verID");
}

char* get_update_version_no()
{
    return get_string_value_from_table(UPDATE_PARAMS_TABLE, "verNO");
}

char* get_update_packUrl()
{
    return get_string_value_from_table(UPDATE_PARAMS_TABLE, "packUrl");
}

BOOL deinit_update_params()
{
    return delete_table(UPDATE_PARAMS_TABLE);
}