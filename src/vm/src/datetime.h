#ifndef DATETIME_H_
#define DATETIME_H_

#include "fox.h"
#include <stdint.h>

enum {
	MSECS_PER_DAY = 86400000,
	MSECS_PER_HOUR = 3600000,
	MSECS_PER_MINUTE = 60000,
	MSECS_PER_SECOND = 1000,

	MAX_TZ_LEN = 64,
};

struct TimeOffset {
	int64_t tm;   // millisec
	int offset;   // millisec
	int is_dst;
	RefStr *name;   // ex. JST
};

struct RefTimeZone {
	RefHeader rh;

	RefStr *name;   // ex. Asia/Tokyo
	int num;
	TimeOffset off[0];
};

RefTimeZone *load_timezone(const char *name_p, int name_size);
RefTimeZone *get_machine_localtime(void);
TimeOffset *TimeZone_offset_utc(RefTimeZone *tz, int64_t tm);
TimeOffset *TimeZone_offset_local(RefTimeZone *tz, int64_t tm);

void Time_to_Calendar(Calendar *cal, int64_t timer);
void Calendar_to_Time(int64_t *timer, const Calendar *cal);
void Calendar_adjust(Calendar *cal);
void Calendar_adjust_month(Calendar *cal);
void Calendar_set_isoweek(Calendar *cal, int year, int week);
void Calendar_get_isoweek(int *year, int *week, Calendar *cal);

#endif /* DATETIME_H_ */
