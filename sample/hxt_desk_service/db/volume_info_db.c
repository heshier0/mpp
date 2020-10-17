#include "common.h"
#include "db.h"

extern sqlite3 *g_hxt_service_db;

int check_mute()
{
    return get_number_value_from_table(VOLUME_INFOS_TABLE, "isMute");
}

BOOL set_mute(BOOL mute)
{
    return set_number_value_into_table(VOLUME_INFOS_TABLE, "isMute", mute);
} 

BOOL set_volume(int vol)
{
    return set_number_value_into_table(VOLUME_INFOS_TABLE, "vol", vol);
}

int get_volume()
{
    return get_number_value_from_table(VOLUME_INFOS_TABLE, "vol");
}