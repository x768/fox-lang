#include "datetime.h"
#include "fox_vm.h"
#include <stdio.h>


// 扱える時間
// -99999999-01-01 ～ 99999999-01-01
// BCE1 == year:0

#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV((y), 4) - DIV((y), 100) + DIV((y), 400))
#define IS_LEAP_YEAR(year) ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

typedef struct {
    RefTimeZone *tz;
    int index;
} TZDataAlias;

typedef struct {
    union {
        const char *name;
        int index;
    } u1;
    union {
        TZDataAlias *alias;
        int index;
    } u2;
} TZData;

static TZData *tzdata;
static int32_t tzdata_size;
static TZDataAlias *tzdata_alias;
static char *tzdata_abbr;
static char *tzdata_line;  // raw data (big endian)
static char *tzdata_data;  // raw data (big endian)

static const int16_t mon_yday[2][13] =
{
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

static uint32_t fread_uint32(FileHandle fh)
{
    char buf[4];
    if (read_fox(fh, buf, 4) == 4) {
        return ptr_read_uint32(buf);
    } else {
        return 0;
    }
}
static char *fread_buffer(FileHandle fh, int size, Mem *mem)
{
    char *p = Mem_get(mem, size);
    if (read_fox(fh, p, size) != size) {
        fatal_errorf("Failed to load file");
    }
    return p;
}
static uint64_t ptr_read_uint64(const char *pc)
{
    const uint8_t *p = (const uint8_t*)pc;
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] <<  8) | ((uint64_t)p[7]);
}

static void load_tzdata(void)
{
    char *path = str_printf("%r" SEP_S "data" SEP_S "tzdata.dat", fs->fox_home);
    FileHandle fh = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
    int32_t size;
    char *tzdata_names;
    int i;

    if (fh == -1) {
        fatal_errorf("Cannot load file %q", path);
    }
    free(path);


    tzdata_size = fread_uint32(fh);
    if (tzdata_size <= 0 || tzdata_size > fs->max_alloc / sizeof(TZData)) {
        goto ERROR_END;
    }
    tzdata = Mem_get(&fg->st_mem, tzdata_size * sizeof(TZData));
    for (i = 0; i < tzdata_size; i++) {
        TZData *p = &tzdata[i];
        p->u1.index = fread_uint32(fh);
        p->u2.index = fread_uint32(fh);
    }

    size = fread_uint32(fh);
    if (size <= 0 || size > fs->max_alloc) {
        goto ERROR_END;
    }
    tzdata_names = fread_buffer(fh, size, &fg->st_mem);

    size = fread_uint32(fh);
    if (size <= 0 || size > fs->max_alloc / sizeof(TZDataAlias)) {
        goto ERROR_END;
    }
    tzdata_alias = Mem_get(&fg->st_mem, size * sizeof(TZDataAlias));
    for (i = 0; i < size; i++) {
        TZDataAlias *p = &tzdata_alias[i];
        p->tz = NULL;
        p->index = fread_uint32(fh);
    }

    size = fread_uint32(fh);
    if (size <= 0 || size > fs->max_alloc) {
        goto ERROR_END;
    }
    tzdata_abbr = fread_buffer(fh, size, &fg->st_mem);

    size = fread_uint32(fh);
    if (size <= 0 || size > fs->max_alloc) {
        goto ERROR_END;
    }
    tzdata_line = fread_buffer(fh, size * 4, &fg->st_mem);

    size = fread_uint32(fh);
    if (size <= 0 || size > fs->max_alloc) {
        goto ERROR_END;
    }
    tzdata_data = fread_buffer(fh, size, &fg->st_mem);

    for (i = 0; i < tzdata_size; i++) {
        TZData *p = &tzdata[i];
        p->u1.name = tzdata_names + p->u1.index;
        p->u2.alias = tzdata_alias + p->u2.index;
    }
    close_fox(fh);
    return;

ERROR_END:
    fatal_errorf("Failed to load tzdata");
}
/**
 * nameは、全て小文字。別名でも構わない
 * 見つからなければNULL
 */
static TZDataAlias *TZDataAlias_find(const char *name)
{
    int low, high;

    if (tzdata == NULL) {
        load_tzdata();
    }
    low = 0;
    high = tzdata_size;

    while (low <= high) {
        int mid = (low + high) / 2;
        int cmp = strcmp(name, tzdata[mid].u1.name);
        if (cmp == 0) {
            return tzdata[mid].u2.alias;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    
    return NULL;
}

/**
 * alnum
 * alnum/alnum
 * alnum/alnum/alnum
 * ...
 */
static int validate_and_tolower_tzname(char *dst, const char *s_p, int s_size)
{
    const char *p = s_p;
    const char *end;
    const char *dst_end = dst + MAX_TZ_LEN - 1;

    if (s_size < 0) {
        s_size = strlen(s_p);
    }
    end = p + s_size;

    while (dst < dst_end) {
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
 * count: TimeZoneの要素数
 * name:  TimeZone#name
 */
static RefTimeZone *RefTimeZone_new(int count, const char *name)
{
    RefTimeZone *ptz = Mem_get(&fg->st_mem, sizeof(RefTimeZone) + sizeof(TimeOffset) * count);

    ptz->rh.type = fs->cls_timezone;
    ptz->rh.nref = -1;
    ptz->rh.n_memb = 0;
    ptz->rh.weak_ref = NULL;
    ptz->name = name;
    ptz->count = count;

    return ptz;
}

RefTimeZone *load_timezone(const char *name_p, int name_size)
{
    char name2[MAX_TZ_LEN];
    TZDataAlias *alias;

    if (!validate_and_tolower_tzname(name2, name_p, name_size)) {
        return NULL;
    }
    alias = TZDataAlias_find(name2);
    if (alias == NULL) {
        return NULL;
    }

    if (alias->tz == NULL) {
        RefTimeZone *tz;
        const char *tzname = tzdata_data + alias->index;
        const char *data = tzname + strlen(tzname) + 1;
        int n = 0;
        int i;

        while (ptr_read_uint64(data + n * 8) != 0xFFFFffffFFFFffffUL) {
            n++;
        }
        tz = RefTimeZone_new(n, tzname);

        for (i = 0; i < n; i++) {
            TimeOffset *off = &tz->off[i];
            uint64_t line = ptr_read_uint64(data + i * 8);
            uint32_t line_data = ptr_read_uint32(tzdata_line + (line >> 48));
            int64_t time_begin = line & 0xffffFFFFffffUL;

            if (time_begin == 0x800000000000) {
                off->begin = INT64_MIN;
            } else if ((time_begin & 0x800000000000) != 0) {
                off->begin = time_begin | 0xFFFF000000000000ULL;
                off->begin *= 1000;
            } else {
                off->begin = time_begin * 1000;
            }

            off->offset = line_data & 0x3FFFF;
            if ((off->offset & 0x20000) != 0) {
                off->offset -= 0x20000;
            }
            off->offset *= 1000;
            off->is_dst = line_data >> 31;
            off->abbr = tzdata_abbr + ((line_data >> 18) & 0x1FFF);
        }

        alias->tz = tz;
    }
    return alias->tz;
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

    for (i = tz->count - 1; i > 0; i--) {
        TimeOffset *off = &tz->off[i];
        if (tm >= off->begin) {
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

    for (i = tz->count - 1; i > 0; i--) {
        TimeOffset *off = &tz->off[i];
        if (tm >= off->begin + off->offset) {
            return off;
        }
    }
    return &tz->off[0];
}
// i2 > 0
static int64_t d_div(int64_t i1, int64_t i2)
{
    if (i1 < 0) {
        return (i1 + 1) / i2 - 1;
    } else {
        return i1 / i2;
    }
}
// i2 > 0
static int64_t d_mod(int64_t i1, int64_t i2)
{
    int64_t m = i1 % i2;
    if (m < 0) {
        return m + i2;
    }
    return m;
}
// 指定した年の最初のisoweek(月曜)のJulianDay
static int64_t get_jd_isoweek_begin(int32_t y)
{
    enum {
        JD_1969_1_4 = 719524,
    };
    int64_t jd = d_div(36525 * y, 100) + d_div(y, 400) - d_div(y, 100) - JD_1969_1_4;
    return jd - d_mod(jd + 3, 7);
}

/////////////////////////////////////////////////////////////////////////////////////////////

// グレゴリオ歴を過去に遡って適用する
void Timestamp_to_DateTime(Date *dt, Time *tm, int64_t ts)
{
    int64_t jd = d_div(ts, MSECS_PER_DAY);
    int64_t rem = d_mod(ts, MSECS_PER_DAY);

    JulianDay_to_Date(dt, jd);

    // rem >= 0
    tm->hour = rem / MSECS_PER_HOUR;
    rem %= MSECS_PER_HOUR;
    tm->minute = rem / MSECS_PER_MINUTE;
    rem %= MSECS_PER_MINUTE;
    tm->second = rem / MSECS_PER_SECOND;
    tm->millisec = rem % MSECS_PER_SECOND;
}

// (year != INT32_MIN) year, month, day_of_month
// (year == INT32_MIN) isoweek_year, isoweek, day_of_week
int64_t DateTime_to_Timestamp(const Date *dt, const Time *tm)
{
    int64_t days_s;
    if (dt->year != INT32_MIN) {
        days_s = Date_ymd_to_JulianDay(dt) * MSECS_PER_DAY;
    } else {
        days_s = Date_week_to_JulianDay(dt) * MSECS_PER_DAY;
    }
    return days_s + tm->hour * MSECS_PER_HOUR + tm->minute * MSECS_PER_MINUTE + tm->second * MSECS_PER_SECOND + tm->millisec;
}

void JulianDay_to_Date(Date *dt, int32_t jd)
{
    int64_t y = 1970;
    const int16_t *ip;
    int month;
    int64_t jd_a = jd;
    int64_t jd_week_begin;

    dt->day_of_week = d_mod(jd + 4, 7);
    // ISO 8601
    if (dt->day_of_week == 0) {
        dt->day_of_week = 7;
    }

    while (jd < 0 || jd >= (IS_LEAP_YEAR(y) ? 366 : 365)) {
        int64_t yg = y + jd / 365 - (jd % 365 < 0);
        jd -= ((yg - y) * 365
            + LEAPS_THRU_END_OF(yg - 1)
            - LEAPS_THRU_END_OF(y - 1));
        y = yg;
    }
    dt->year = y;
    dt->day_of_year = jd + 1;

    ip = mon_yday[IS_LEAP_YEAR(y) ? 1 : 0];
    month = 11;
    while (jd < ip[month]) {
        month--;
    }

    jd -= ip[month];
    dt->month = month + 1;
    dt->day_of_month = jd + 1;

    // 1月4日の週の月曜日
    jd_week_begin = get_jd_isoweek_begin(dt->year);
    if (jd_a < jd_week_begin) {
        // 前年
        jd_week_begin = get_jd_isoweek_begin(dt->year - 1);
        dt->isoweek = (jd_a - jd_week_begin) / 7 + 1;
        dt->isoweek_year = dt->year - 1;
    } else {
        dt->isoweek = (jd_a - jd_week_begin) / 7 + 1;
        if (dt->isoweek > 52) {
            // 翌年の可能性
            int64_t jd_week_next = get_jd_isoweek_begin(dt->year + 1);
            if (jd_a >= jd_week_next) {
                dt->isoweek = (jd_a - jd_week_next) / 7 + 1;
                dt->isoweek_year = dt->year + 1;
                return;
            }
        }
        dt->isoweek_year = dt->year;
    }
}
/*
 * in: dt->isoweek_year, dt->isoweek, dt->day_of_week
 */
int64_t Date_week_to_JulianDay(const Date *dt)
{
    int64_t jd = get_jd_isoweek_begin(dt->isoweek_year);
    return jd + (dt->isoweek - 1) * 7 + dt->day_of_week - 1;
}
/*
 * in: dt->year, dt->month, dt->day_of_month
 */
int64_t Date_ymd_to_JulianDay(const Date *dt)
{
    enum {
        JD_1969 = 719499,
    };

    int64_t y = dt->year;
    int64_t m = dt->month - 2;
    int64_t d = dt->day_of_month;

    // 1～12 → 11,12(last year), 1, 2, .. 10
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
    // ユリウス通日 (1969-01-01 == 0)
    return d_div(36525 * y, 100) + d_div(y, 400) - d_div(y, 100) + (3059 * m / 100) + d - JD_1969;
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
void DateTime_adjust(Date *dt, Time *tm)
{
    int64_t ts = DateTime_to_Timestamp(dt, tm);
    Timestamp_to_DateTime(dt, tm, ts);
}
// 2010-01-31 + 1months => 2010-02-28
void Date_adjust_month(Date *dt)
{
    if (dt->month < 1 || dt->month > 12) {
        int y = (dt->month - 1) / 12;
        int m = (dt->month - 1) % 12;
        if (dt->month < 1) {
            y--;
        }
        if (m < 0) {
            m += 12;
        }
        dt->year += y;
        dt->month = m + 1;
    }
    {
        int dom = get_days_of_month(dt->year, dt->month);
        if (dt->day_of_month > dom) {
            dt->day_of_month = dom;
        }
    }
}
