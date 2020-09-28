#include "hxt_defines.h"
#include "report_info_db.h"


BOOL hxt_del_offline_expired_info()
{
    return del_report_info_expired();
}

int hxt_stored_offline_study_info(void *data)
{
    if (NULL == data)
    {
        return -1;
    }

    /* */
    
}

/* check file creante time */
 