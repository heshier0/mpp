#include <sqlite3.h>
#include "utils.h"
#include "common.h"
#include "db.h"



BOOL open_hxt_service_db()
{
    int result = sqlite3_open_v2(DB_PATH, &g_hxt_service_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL);
    if (result != 0)
    {
        utils_print("open database %s failed, errcode=%d\n", DB_PATH, result);
        return FALSE;
    }

    utils_print("open database %s OK\n", DB_PATH);

    return TRUE;
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
