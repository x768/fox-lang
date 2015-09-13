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
    int64_t begin;    // sec
    int offset;       // sec
    int is_dst;
    const char *abbr; // ex. JST
};

struct RefTimeZone {
    RefHeader rh;

    const char *name;   // ex. Asia/Tokyo
    int count;
    TimeOffset off[0];
};

RefTimeZone *load_timezone(const char *name_p, int name_size);
RefTimeZone *get_machine_localtime(void);
TimeOffset *TimeZone_offset_utc(RefTimeZone *tz, int64_t tm);
TimeOffset *TimeZone_offset_local(RefTimeZone *tz, int64_t tm);

void Timestamp_to_DateTime(DateTime *cal, int64_t timer);
void DateTime_to_Timestamp(int64_t *timer, const DateTime *cal);
void DateTime_adjust(DateTime *cal);
void Date_adjust_month(Date *dt);
void Date_set_isoweek(Date *dt, int year, int week, int day_of_week);
void Date_get_isoweek(int *pyear, int *pweek, const Date *dt);

#endif /* DATETIME_H_ */
