#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

#include <cJSON.h>

#include "utils.h"
#include "iflyos_common_def.h"

#define IFLYOS_CFG          "/user/config/iflyos_config.json"
#define DEVICE_PARAMS       "device_params"
#define DEVICE_AUTH         "device_auth"
#define PLATFORM_PARAMS     "platform_params"
#define SYSTEM_PARAMS       "system_params"
#define AUDIO_PLAYER        "audio_player"
#define SPEAKER             "speaker"


static cJSON* g_cfg_root  = NULL;     //指向配置文件的Object

void iflyos_load_cfg()
{
    g_cfg_root = utils_load_cfg(IFLYOS_CFG);
    if (g_cfg_root == NULL)
    {
        utils_print("root node is null\n");
    }
}

BOOL iflyos_reload_cfg()
{
    return utils_reload_cfg(IFLYOS_CFG, g_cfg_root);
}

void iflyos_unload_cfg()
{
    utils_unload_cfg(g_cfg_root);
}


/*********get params********/
char* iflyos_get_client_id()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_PARAMS, "client_id");
}

char* iflyos_get_grant_type()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_PARAMS, "grant_type");
}

char* iflyos_get_device_code()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_PARAMS, "device_code");
}

char* iflyos_get_device_id()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_PARAMS, "device_id");
}   

char* iflyos_get_token_type()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_AUTH, "token_type");
}

char* iflyos_get_token()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_AUTH, "access_token");
}

char* iflyos_get_refresh_token()
{
    return utils_get_cfg_str_value(g_cfg_root, DEVICE_AUTH, "refresh_token");
}

BOOL iflyos_check_token()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    long expires = (long)utils_get_cfg_number_value(g_cfg_root, DEVICE_AUTH, "expires_in");
    long create_time = (long)utils_get_cfg_number_value(g_cfg_root, DEVICE_AUTH, "created_at");

    utils_print("%ld, %ld\n", expires, create_time);

    return (tv.tv_sec - create_time) < expires;
    
}

char* iflyos_get_platform_name()
{
    return utils_get_cfg_str_value(g_cfg_root, PLATFORM_PARAMS, "name");
}

char* iflyos_get_platform_version()
{
    return utils_get_cfg_str_value(g_cfg_root, PLATFORM_PARAMS, "version");
}

char* iflyos_get_system_version()
{
    return utils_get_cfg_str_value(g_cfg_root, SYSTEM_PARAMS, "version");
}

BOOL iflyos_get_system_sw_updater_state()
{
    return (BOOL)utils_get_cfg_number_value(g_cfg_root, SYSTEM_PARAMS, "software_updater");
}

BOOL iflyos_get_system_power_ctrl_state()
{
    return (BOOL)utils_get_cfg_number_value(g_cfg_root, SYSTEM_PARAMS, "power_controller");
}

BOOL iflyos_get_system_device_modes_state()
{
    return (BOOL)utils_get_cfg_number_value(g_cfg_root, SYSTEM_PARAMS, "device_modes");
}

BOOL iflyos_get_system_reboot_state()
{
    return (BOOL)utils_get_cfg_number_value(g_cfg_root, SYSTEM_PARAMS, "reboot");
}

char* iflyos_get_audio_version()
{
    return utils_get_cfg_str_value(g_cfg_root, AUDIO_PLAYER, "version");
}

char* iflyos_get_audio_state()
{    
    return utils_get_cfg_str_value(g_cfg_root, AUDIO_PLAYER, "state");
}

char* iflyos_get_speaker_version()
{
    return utils_get_cfg_str_value(g_cfg_root, SPEAKER, "version");
}

int iflyos_get_speaker_volume()
{
    return (int)utils_get_cfg_number_value(g_cfg_root, SPEAKER, "volume");
}

char* iflyos_get_speaker_type()
{
    return utils_get_cfg_str_value(g_cfg_root, SPEAKER, "type");
}
/********set params***********/

BOOL iflyos_set_client_id(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_PARAMS, "client_id", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_grant_type(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_PARAMS, "grant_type", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_device_code(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_PARAMS, "device_code", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_device_id(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_PARAMS, "device_id", value);
    return iflyos_reload_cfg();
}   

BOOL iflyos_set_token_type(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }    
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_AUTH, "token_type", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_token(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    } 
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_AUTH, "access_token", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_refresh_token(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    } 
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, DEVICE_AUTH, "refresh_token", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_expired_time(const long times)
{
    if (0 == times)
    {
        return FALSE;
    }
    utils_set_cfg_number_value(g_cfg_root, IFLYOS_CFG, DEVICE_AUTH, "expires_in", times);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_created_time(const long times)
{
    if (0 == times || NULL == g_cfg_root)
    {
        return FALSE;
    }
    utils_set_cfg_number_value(g_cfg_root, IFLYOS_CFG, DEVICE_AUTH, "created_at", times);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_platform_name(const char* value)
{
    if(NULL == value || NULL == g_cfg_root)
    {
        return FALSE;
    }     
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, PLATFORM_PARAMS, "name", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_platform_version(const char* value)
{
    if(NULL == value || NULL == g_cfg_root)
    {
        return FALSE;
    }   
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, PLATFORM_PARAMS, "version", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_system_version(const char* value)
{
    if(NULL == value || NULL == g_cfg_root)
    {
        return FALSE;
    }       
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, SYSTEM_PARAMS, "version", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_system_sw_updater_state(const BOOL value)
{
    utils_set_cfg_number_value(g_cfg_root, IFLYOS_CFG, SYSTEM_PARAMS, "software_updater", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_system_power_ctrl_state(const BOOL value)
{
    utils_set_cfg_number_value(g_cfg_root, IFLYOS_CFG, SYSTEM_PARAMS, "power_controller", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_system_device_modes_state(const BOOL value)
{
    utils_set_cfg_number_value(g_cfg_root, IFLYOS_CFG, SYSTEM_PARAMS, "device_modes", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_system_reboot_state(const BOOL value)
{
    utils_set_cfg_number_value(g_cfg_root, IFLYOS_CFG, SYSTEM_PARAMS, "reboot", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_audio_version(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }   
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, AUDIO_PLAYER, "version", value);
    return iflyos_reload_cfg();
}

BOOL iflyos_set_audio_state(const char* value)
{    
    if(NULL == value)
    {
        return FALSE;
    }   
    utils_set_cfg_str_value(g_cfg_root, IFLYOS_CFG, AUDIO_PLAYER, "state", value);
    return iflyos_reload_cfg();
}