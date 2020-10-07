#include "common.h"
#include "db.h"

BOOL set_device_id(const char* uuid)
{
    return set_string_value_into_table(DESK_PARAMS_TABLE, "deskCode", uuid);
}

BOOL set_parent_id(const int parent_id)
{
    return set_number_value_into_table(DESK_PARAMS_TABLE, "parentID", parent_id);
}

BOOL set_desk_bind_status(int status)
{
    return set_number_value_into_table(DESK_PARAMS_TABLE, "isBind", status);
}

char* get_device_id()
{
    return get_string_value_from_table(DESK_PARAMS_TABLE, "deskCode");
}

int get_desk_bind_status()
{
    return get_number_value_from_table(DESK_PARAMS_TABLE, "isBind");
}

int get_parent_id()
{
    return get_number_value_from_table(DESK_PARAMS_TABLE, "parentID");
}

BOOL deinit_desk_params()
{
    BOOL ret = FALSE;
    ret |= set_desk_bind_status(FALSE);
    ret |= set_parent_id(-1);

    return ret;
}