#ifndef __REPORT_INFO_DB_H__
#define __REPORT_INFO_DB_H__

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "hxt_defines.h"



BOOL create_report_info_table();

BOOL add_report_info(void* data);

BOOL del_report_info(int id);
BOOL del_report_info_expired();

int get_report_info_count();
int get_amount_records_of_day();

BOOL update_mp4_url(int id, const char *url);
BOOL update_snap_url(int id, const char *url);



#endif // !__REPORT_INFO_DB_H__