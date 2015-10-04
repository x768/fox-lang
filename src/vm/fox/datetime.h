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

void Timestamp_to_DateTime(Date *dt, Time *tm, int64_t ts);
int64_t DateTime_to_Timestamp(const Date *dt, const Time *tm);
void JulianDay_to_Date(Date *dt, int32_t jd);
int64_t Date_ymd_to_JulianDay(const Date *dt);
int64_t Date_week_to_JulianDay(const Date *dt);

void DateTime_adjust(Date *dt, Time *tm);
void Date_adjust_month(Date *dt);

#endif /* DATETIME_H_ */
