#ifndef __REPORT_INFO_DB_H__
#define __REPORT_INFO_DB_H__

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "hxt_defines.h"


BOOL open_report_info_db();

BOOL close_report_info_db();

BOOL create_report_info_table();

BOOL add_report_info(void* data);

int del_report_info(int id);

int get_report_info_count();





#endif // !__REPORT_INFO_DB_H__