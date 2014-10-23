#include "datetime.h"
#include "fox_vm.h"
#include <stdio.h>


// 扱える時間
// -99999999-01-01 ～ 99999999-01-01


#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))
#define IS_LEAP(year) ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

static const uint16_t mon_yday[2][13] =
{
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/**
 * alnum
 * alnum/alnum
 * alnum/alnum/alnum
 * ...
 */
static int convert_timezone_name(char *dst, const char *s_p, int s_size)
{
    const char *p = s_p;
    const char *end = p + s_size;
    const char *pend = dst + MAX_TZ_LEN - 1;

    while (dst < pend) {
        if (p >= end || !(isalnum(*p) || *p == '_')) {
            return FALSE;
        }
        *dst++ = tolower(*p++);
        while (p < end && (isalnum(*p) || *p == '_')) {
            *dst++ = tolower(*p++);
        }
        if (p >= end) {
            *dst = '\0';
            return TRUE;
        }
        if (*p != '/') {
            return FALSE;
        }
        *dst++ = *p++;
    }
    *dst = '\0';
    return TRUE;
}
/**
 * ファイルが見つからない場合はNULLを返す
 */
static char *load_tztext(RefStr *name)
{
    char *path = str_printf("%r" SEP_S "tzdata" SEP_S "%r.txt", fs->fox_home, name);
    char *buf;
#ifdef WIN32
    int i;
    for (i = fs->fox_home->size + 8; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }
#endif
    buf = read_from_file(NULL, path, NULL);
    free(path);

    return buf;
}
static int count_line(const char *buf)
{
    int n = 0;
    while (*buf != '\0') {
        if (*buf == '\n') {
            n++;
        }
        buf++;
    }
    return n;
}

static int64_t parse_hex64(const char *src)
{
    uint64_t ret = 0;

    while (*src != '\0') {
        ret = (ret << 4) | char2hex(*src);
        src++;
    }
    return ret;
}
int parse_decimal(const char *src)
{
    int ret = 0;
    int sign = 1;

    if (*src == '-') {
        sign = -1;
        src++;
    }
    while (*src != '\0') {
        ret = ret * 10 + (*src - '0');
        src++;
    }
    return ret * sign;
}
static RefTimeZone *RefTimeZone_new(int count, const char *p, int size)
{
    RefTimeZone *ptz = Mem_get(&fg->st_mem, sizeof(RefTimeZone) + sizeof(TimeOffset) * count);

    ptz->rh.type = fs->cls_timezone;
    ptz->rh.nref = -1;
    ptz->rh.n_memb = 0;
    ptz->rh.weak_ref = NULL;
    ptz->name = intern(p, size);

    return ptz;
}

static int parse_tztext_line(TimeOffset *off, Str line)
{
    const char *tmp[4];
    Str_split(line, tmp, 4, ' ');

    off->tm = parse_hex64(tmp[0]);
    off->offset = parse_decimal(tmp[1]) * 1000;
    off->is_dst = (tmp[2][0] == '1');
    off->name = intern(tmp[3], -1);

    return TRUE;

}
static RefTimeZone *parse_tztext(const char *buf, RefStr **name, int *alias)
{
    Str line;
    RefTimeZone *tz = NULL;
    int pos = 0;

    while (*buf != '\0') {
        line.p = buf;
        while (*buf != '\0' && *buf != '\n') {
            buf++;
        }
        line.size = buf - line.p;
        if (*buf == '\n') {
            buf++;
        }

        if (line.size == 0) {
            continue;
        }
        if (line.p[0] == '>') {
            *alias = TRUE;
            *name = intern(line.p + 1, line.size - 1);
            return NULL;
        } else if (line.p[0] == '=') {
            if (tz == NULL) {
                int n = count_line(buf);
                tz = RefTimeZone_new(n, line.p + 1, line.size - 1);
            }
        } else {
            if (tz != NULL) {
                TimeOffset *off = &tz->off[pos];
                if (parse_tztext_line(off, line)) {
                    pos++;
                }
            }
        }
    }
    *alias = FALSE;
    tz->num = pos;

    return tz;
}
RefTimeZone *load_timezone(const char *name_p, int name_size)
{
    static Hash timezones;
    RefTimeZone *ptz;
    char *buf;
    char name2[MAX_TZ_LEN];
    RefStr *name_r;
    RefStr *alias_key = NULL;
    int i;

    if (name_size < 0) {
        name_size = strlen(name_p);
    }
    if (!convert_timezone_name(name2, name_p, name_size)) {
        return NULL;
    }
    name_r = intern(name2, -1);
    if (timezones.entry == NULL) {
        Hash_init(&timezones, &fg->st_mem, 32);
    }

    for (i = 0; i < 3; i++) {
        int alias = FALSE;

        ptz = Hash_get_p(&timezones, name_r);
        if (ptz != NULL) {
            if (alias_key != NULL) {
                // alias解決前の名前
                Hash_add_p(&timezones, &fg->st_mem, alias_key, ptz);
            }
            return ptz;
        }
        if (strcmp(name_r->c, "etc/utc") == 0) {
            ptz = RefTimeZone_new(1, "Etc/UTC", 7);
            ptz->num = 1;
            ptz->off[0].tm = 0x8000000000000000LL;
            ptz->off[0].offset = 0;
            ptz->off[0].is_dst = 0;
            ptz->off[0].name = intern("UTC", 3);
        } else {
            buf = load_tztext(name_r);
            if (buf == NULL) {
                return NULL;
            }
            ptz = parse_tztext(buf, &name_r, &alias);
            free(buf);
        }

        if (ptz != NULL) {
            // alias解決後の名前
            Hash_add_p(&timezones, &fg->st_mem, name_r, ptz);
            if (alias_key != NULL) {
                // alias解決前の名前
                Hash_add_p(&timezones, &fg->st_mem, alias_key, ptz);
            }
            return ptz;
        } else if (alias) {
            alias_key = name_r;
        } else {
            return NULL;
        }
    }
    return NULL;
}

RefTimeZone *get_machine_localtime(void)
{
    static RefTimeZone *tz = NULL;
    if (tz == NULL) {
        char cbuf[256];
        get_local_timezone_name(cbuf, sizeof(cbuf));
        tz = load_timezone(cbuf, -1);
    }
    return tz;
}

/**
 * tm(UTC)に対応するオフセットを返す
 */
TimeOffset *TimeZone_offset_utc(RefTimeZone *tz, int64_t tm)
{
    int i;

    for (i = tz->num - 1; i >= 0; i--) {
        TimeOffset *off = &tz->off[i];
        if (tm >= off->tm) {
            return off;
        }
    }
    return &tz->off[0];
}
/**
 * tm(local)に対応するオフセットを返す
 * 夏時間付近では正確ではない
 */
TimeOffset *TimeZone_offset_local(RefTimeZone *tz, int64_t tm)
{
    int i;

    for (i = tz->num - 1; i >= 0; i--) {
        TimeOffset *off = &tz->off[i];
        if (tm >= off->tm + off->offset) {
            return off;
        }
    }
    return &tz->off[0];
}

/////////////////////////////////////////////////////////////////////////////////////////////

// グレゴリオ歴を過去に遡って適用する
void Time_to_Calendar(Calendar *cal, int64_t timer)
{
    int64_t y = 1970;
    int64_t days = timer / MSECS_PER_DAY;
    int64_t rem = timer % MSECS_PER_DAY;
    const uint16_t *ip;
    int month;

    if (rem < 0) {
        days--;
        rem += MSECS_PER_DAY;
    }

    cal->day_of_week = (days + 4) % 7;
    if (cal->day_of_week < 0) {
        cal->day_of_week += 7;
    }
    // ISO 8601
    if (cal->day_of_week == 0) {
        cal->day_of_week = 7;
    }

    while (days < 0 || days >= (IS_LEAP(y) ? 366 : 365)) {
        int64_t yg = y + days / 365 - (days % 365 < 0);
        days -= ((yg - y) * 365
            + LEAPS_THRU_END_OF(yg - 1)
            - LEAPS_THRU_END_OF(y - 1));
        y = yg;
    }
    cal->year = y;
    cal->day_of_year = days;

    ip = mon_yday[IS_LEAP(y) ? 1 : 0];
    month = 11;
    while (days < (int)ip[month]) {
        month--;
    }

    days -= ip[month];
    cal->month = month + 1;
    cal->day_of_month = days + 1;

    cal->hour = rem / MSECS_PER_HOUR;
    rem %= MSECS_PER_HOUR;
    cal->minute = rem / MSECS_PER_MINUTE;
    rem %= MSECS_PER_MINUTE;
    cal->second = rem / MSECS_PER_SECOND;
    cal->millisec = rem % MSECS_PER_SECOND;
}

void Calendar_to_Time(int64_t *timer, const Calendar *cal)
{
    enum {
        JD_1969 = 719499,
    };

    int64_t julius;  // ユリウス通日（1969-01-01 == 0）
    int64_t y = cal->year;
    int64_t m = cal->month - 2;
    int64_t d = cal->day_of_month;

    // 月が1～12の範囲外の場合、正規化する
    if (m > 12) {
        m--;
        y += m / 12;
        m = m % 12;
        m++;
    } else if (m <= 0) {
        m--;
        y += (m + 1) / 12 - 1;
        m = m % 12;
        if (m != 0) {
            m += 12;
        }
        m++;
    }

    // TODO year < 0
    julius = (36525 * y / 100) + (y / 400) - (y / 100) + (3059 * m / 100) + d - JD_1969;
    *timer = julius * MSECS_PER_DAY + cal->hour * MSECS_PER_HOUR + cal->minute * MSECS_PER_MINUTE + cal->second * MSECS_PER_SECOND + cal->millisec;
}

static int get_days_of_month(int year, int month)
{
    switch (month) {
    case 2:
        if ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0)) {
            return 29;
        } else {
            return 28;
        }
    case 4: case 6: case 9: case 11:
        return 30;
    default:
        return 31;
    }
}
void Calendar_adjust(Calendar *cal)
{
    int64_t i_tm;
    Calendar_to_Time(&i_tm, cal);
    Time_to_Calendar(cal, i_tm);
}
// 2010-01-31 + 1months => 2010-02-28
void Calendar_adjust_month(Calendar *cal)
{
    int dom;

    if (cal->month < 1 || cal->month > 12) {
        int y = (cal->month - 1) / 12;
        int m = (cal->month - 1) % 12;
        if (cal->month < 1) {
            y--;
        }
        if (m < 0) {
            m += 12;
        }
        cal->year += y;
        cal->month = m + 1;
    }
    dom = get_days_of_month(cal->year, cal->month);
    if (cal->day_of_month > dom) {
        cal->day_of_month = dom;
    }
}
void Calendar_set_isoweek(Calendar *cal, int year, int week)
{
}
void Calendar_get_isoweek(int *year, int *week, Calendar *cal)
{
}

