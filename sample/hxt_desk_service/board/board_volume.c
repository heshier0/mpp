#include <hi_comm_aio.h>

#include "board_func.h"
#include "db.h"
#include "utils.h"


/* 实际音量对应表
user    100     90      80      70      60      50      40      30      20      10      MUTE
board   5       0       -5      -10     -15     -20     -25     -30     -35     -40     MUTE
*/
static AUDIO_DEV ao_dev = 0;

static int change_user_to_board(int vol)
{
    int board_vol = 0;
    if (vol < 15 && vol > 0)
    {
        board_vol = -40;
    }
    else if (vol < 25 && vol >= 15)
    {
        board_vol = -35;
    }
    else if (vol < 35 && vol >= 25)
    {
        board_vol = -30;
    }
    else if (vol < 45 && vol >= 35)
    {
        board_vol = -25;
    }
    else if (vol < 55 && vol >= 45)
    {
        board_vol = -20;
    }
    else if (vol < 65 && vol >= 55)
    {
        board_vol = -15;
    }
    else if (vol < 75 && vol >= 65)
    {
        board_vol = -10;
    }    
    else if (vol < 85 && vol >= 75)
    {
        board_vol = -5;
    }    
    else if (vol < 95 && vol >= 85)
    {
        board_vol = 0;
    }
    else if (vol >= 95)
    {
        board_vol = 5;
    }

    return board_vol;
}

void board_init_volume()
{
    int mute_status = check_mute();
    if (mute_status)
    {
        HI_MPI_AO_SetMute(ao_dev, HI_TRUE, NULL);
        return;
    }

    int stored_vol = get_volume();
    HI_MPI_AO_SetVolume(ao_dev, stored_vol);
    utils_print("last volume is %d\n", stored_vol);

    return;
}

BOOL board_set_volume(int vol)
{
    int stored_vol = get_volume();
    if (stored_vol == 5)
    {
        utils_send_local_voice(VOICE_VOLUME_MAX);
        return FALSE;
    }

    int board_vol = change_user_to_board(vol);
    HI_MPI_AO_SetVolume(ao_dev, board_vol);

    return set_volume(vol);
}

int board_inc_volume()
{
    int stored_vol = get_volume();
    if (stored_vol == 5)
    {
        utils_send_local_voice(VOICE_VOLUME_MAX);
        return -1;
    }

    int mute_status = check_mute();
    if (mute_status == 1)
    {
        HI_MPI_AO_SetMute(ao_dev, HI_FALSE, NULL);
        set_mute(FALSE);
    }

    int cur_vol = stored_vol + 5;
    HI_MPI_AO_SetVolume(ao_dev, cur_vol);
    set_volume(cur_vol);

    return cur_vol;
}

int board_dec_volume()
{
    int cur_vol = -1;

    int mute_status = check_mute();
    if (mute_status == 1)
    {
        return -1;
    }

    int stored_vol = get_volume();
    if (stored_vol == -40)
    {
        HI_MPI_AO_SetMute(ao_dev, HI_TRUE, NULL);
        set_mute(TRUE);
    }
    else
    {
        cur_vol = stored_vol - 5;
        HI_MPI_AO_SetVolume(ao_dev, cur_vol);
        set_volume(cur_vol);
    }
    
    return cur_vol;
}

BOOL board_set_mute(BOOL mute)
{
    if (mute)
    {
        HI_MPI_AO_SetMute(ao_dev, HI_TRUE, NULL);
    }
    else
    {
        HI_MPI_AO_SetMute(ao_dev, HI_FALSE, NULL);
        int stored_vol = get_volume();
        HI_MPI_AO_SetVolume(ao_dev, stored_vol);
    }
    
    return set_mute(mute);
}