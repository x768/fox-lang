#include "fox_vm.h"
#include "datetime.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

const char *time_month_str[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
const char *time_week_str[] = {
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",
};

enum {
    GET_YEAR,
    GET_MONTH,
    GET_DAY_OF_YEAR,
    GET_DAY_OF_MONTH,
    GET_DAY_OF_WEEK,
    GET_ISOWEEK,
    GET_ISOWEEK_YEAR,
    GET_HOUR,
    GET_MINUTE,
    GET_SECOND,
    GET_MILLISEC,
    GET_ERR,
};
enum {
    PARSE_ABS,
    PARSE_POSITIVE,
    PARSE_NEGATIVE,
    PARSE_DELTA,
};
enum {
    PARSETYPE_NONE,
    PARSETYPE_OFFSET,
    PARSETYPE_NUM,
};

typedef struct {
    const char *name;
    int hours;
} TimeZoneAbbrName;

static RefNode *cls_date;
static RefNode *cls_timedelta;

#define VALUE_INT64(v) (((RefInt64*)(intptr_t)(v))->u.i)

#define TIMESTAMP_MIN (-100000000.0 * 31557600000.0)
#define TIMESTAMP_MAX (100000000.0 * 31557600000.0)

////////////////////////////////////////////////////////////////////////////////////////////////////

void timestamp_to_RFC2822_UTC(int64_t ts, char *dst)
{
    Date dt;
    Time tm;
    Timestamp_to_DateTime(&dt, &tm, ts);

    sprintf(dst, "%s, %02d %s %d %02d:%02d:%02d +0000",
            time_week_str[dt.day_of_week - 1], dt.day_of_month, time_month_str[dt.month - 1], dt.year,
            tm.hour, tm.minute, tm.second);
}
void timestamp_to_cookie_date(int64_t ts, char *dst)
{
    Date dt;
    Time tm;
    Timestamp_to_DateTime(&dt, &tm, ts);

    sprintf(dst, "%s, %02d-%s-%d %02d:%02d:%02d GMT",
            time_week_str[dt.day_of_week - 1], dt.day_of_month, time_month_str[dt.month - 1], dt.year,
            tm.hour, tm.minute, tm.second);
}

/**
 * 環境変数FOX_TZが設定されていれば優先
 */
RefTimeZone *get_local_tz()
{
    static RefTimeZone *tz_local = NULL;

    if (tz_local == NULL) {
        const char *p = Hash_get(&fs->envs, "FOX_TZ", -1);
        if (p != NULL) {
            tz_local = load_timezone(p, -1);
        }
        if (tz_local == NULL) {
            tz_local = get_machine_localtime();
            if (tz_local == NULL) {
                tz_local = fs->tz_utc;
            }
        }
    }

    return tz_local;
}
/**
 * 指定したCalendarからTimeOffsetとtm(int64)を決定
 */
void adjust_timezone(RefDateTime *dt)
{
    if (dt->tz != NULL) {
        int64_t ts = DateTime_to_Timestamp(&dt->d, &dt->t);
        // TimeOffsetを推定
        dt->off = TimeZone_offset_local(dt->tz, ts);
        dt->ts = ts - dt->off->offset;
        // タイムスタンプを戻して、もう一度TimeOffsetを計算
        dt->off = TimeZone_offset_utc(dt->tz, dt->ts);
        // もう一度計算
        Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts + dt->off->offset);
    } else {
        dt->ts = DateTime_to_Timestamp(&dt->d, &dt->t);
        Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts);
    }
}
/**
 * 指定したタイムスタンプとRefTimeZoneから、DateTimeを決定
 */
void adjust_datetime(RefDateTime *dt)
{
    if (dt->tz != NULL) {
        dt->off = TimeZone_offset_local(dt->tz, dt->ts);
        Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts + dt->off->offset);
    }
}

RefTimeZone *Value_to_tz(Value v, int argn)
{
    RefNode *v_type = Value_type(v);

    if (v_type == fs->cls_timezone) {
        return Value_vp(v);
    } else if (v_type == fs->cls_str) {
        RefStr *name = Value_vp(v);
        RefTimeZone *tz;

        if (str_eqi(name->c, name->size, "localtime", -1)) {
            tz = get_local_tz();
        } else {
            tz = load_timezone(name->c, name->size);
            if (tz == NULL) {
                throw_errorf(fs->mod_lang, "ValueError", "RefTimeZone %q not found", name->c);
                return NULL;
            }
        }
        return tz;
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_timezone, fs->cls_str, v_type, argn + 1);
        return NULL;
    }
}

/*
 * vがdateの場合、時差を加算したtimestampを返す
 * vがtimeの場合、ローカル時間に変換したtimestampを返す
 */
int64_t Value_timestamp(Value v, RefTimeZone *tz)
{
    RefNode *type = Value_type(v);

    if (type == fs->cls_datetime) {
        RefDateTime *dt = Value_vp(v);
        return dt->ts + dt->off->offset;
    } else if (type == fs->cls_timestamp) {
        RefInt64 *r = Value_vp(v);
        TimeOffset *off = TimeZone_offset_local(tz != NULL ? tz : get_local_tz(), r->u.i);
        return r->u.i + off->offset;
    } else {
        return 0LL;
    }
}
/*
 * i_tm:   timestamp
 * return: FOX_TZで設定したTimeZoneを持つDateTime
 */
Value time_Value(int64_t ts, RefTimeZone *tz)
{
    RefDateTime *dt = buf_new(fs->cls_datetime, sizeof(RefDateTime));

    dt->tz = (tz != NULL ? tz : get_local_tz());
    dt->ts = ts;
    dt->off = TimeZone_offset_local(dt->tz, ts);
    Timestamp_to_DateTime(&dt->d, &dt->t, ts + dt->off->offset);

    return vp_Value(dt);
}

static void DateTime_init(Date *dt, Time *tm)
{
    dt->year = 1;
    dt->month = 1;
    dt->day_of_month = 1;
    if (tm != NULL) {
        tm->hour = 0;
        tm->minute = 0;
        tm->second = 0;
        tm->millisec = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int int64_hash(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *r = Value_vp(*v);
    int32_t hash = (r->u.u >> 32) ^ (r->u.u & 0xFFFFffff);
    *vret = int32_Value(hash & INT32_MAX);
    return TRUE;
}
static int int64_eq(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *r1 = Value_vp(*v);
    RefInt64 *r2 = Value_vp(v[1]);
    *vret = bool_Value(r1->u.i == r2->u.i);
    return TRUE;
}
static int int64_cmp(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *r1 = Value_vp(*v);
    RefInt64 *r2 = Value_vp(v[1]);
    int cmp;

    if (r1->u.i < r2->u.i) {
        cmp = -1;
    } else if (r1->u.i > r2->u.i) {
        cmp = 1;
    } else {
        cmp = 0;
    }
    *vret = int32_Value(cmp);

    return TRUE;
}

static int int64_marshal_read(Value *vret, Value *v, RefNode *node)
{
    uint8_t data[8];
    int rd_size = 8;
    int i;

    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefNode *cls = FUNC_VP(node);
    RefInt64 *rt = buf_new(cls, sizeof(RefInt64));
    *vret = vp_Value(rt);

    if (!stream_read_data(r, NULL, (char*)data, &rd_size, FALSE, TRUE)) {
        return TRUE;
    }
    for (i = 0; i < 8; i++) {
        rt->u.u |= ((uint64_t)data[i]) << ((7 - i) * 8);
    }

    return TRUE;
}
static int int64_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefInt64 *rt = Value_vp(*v);
    char data[8];
    int i;

    for (i = 0; i < 8; i++) {
        data[i] = (rt->u.u >> ((7 - i) * 8)) & 0xFF;
    }
    if (!stream_write_data(w, data, 8)) {
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int timestamp_new(Value *vret, Value *v, RefNode *node)
{
    int num[] = {
        1, 1, 1, 0, 0, 0, 0,
    };
    int i;
    Date dt;
    Time tm;
    RefInt64 *rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
    *vret = vp_Value(rt);

    for (i = 1; v + i < fg->stk_top; i++) {
        num[i - 1] = Value_int32(v[i]);
        if (num[i - 1] == INT32_MIN) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Out of range (argument #%d)", i);
        }
    }
    dt.year = num[0];
    dt.month = num[1];
    dt.day_of_month = num[2];
    tm.hour = num[3];
    tm.minute = num[4];
    tm.second = num[5];
    tm.millisec = num[6];
    rt->u.i = DateTime_to_Timestamp(&dt, &tm);

    return TRUE;
}
static int timestamp_now(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
    *vret = vp_Value(rt);
    rt->u.i = get_now_time();
    return TRUE;
}

static int parse_digits_sub(int *ret, const char **pp, const char *end, int width)
{
    const char *p = *pp;
    const char *top = p;
    int i;

    for (i = 0; i < width; i++) {
        if (p >= end) {
            break;
        }
        if (!isdigit_fox(*p)) {
            break;
        }
        p++;
    }
    if (p == top) {
        return FALSE;
    }
    *ret = parse_int(top, p - top, 99999999);

    *pp = p;
    return TRUE;
}

static void skip_space(const char **pp, const char *end)
{
    const char *p = *pp;
    while (p < end && isspace_fox(*p)) {
        p++;
    }
    *pp = p;
}

/**
 * ISO8601
 */
static int parse_iso_sub(Date *dt, Time *tm, const char *src_p, int src_size)
{
    const char *p = src_p;
    const char *end = p + src_size;
    int neg = FALSE;
    int year_n = 4;

    skip_space(&p, end);
    if (p < end) {
        if (*p == '-') {
            neg = TRUE;
            year_n = 8;
            p++;
        } else if (*p == '+') {
            year_n = 8;
            p++;
        }
    }

    if (!parse_digits_sub(&dt->year, &p, end, year_n)) {
        return FALSE;
    }
    if (p < end && (*p == 'w' || *p == 'W')) {
        p++;
        // ISO週番号
        if (!parse_digits_sub(&dt->isoweek, &p, end, 2)) {
            return FALSE;
        }
        dt->isoweek_year = dt->year;
        dt->year = INT32_MIN;
    } else {
        if (p < end && *p == '-') {
            p++;
        }
        if (!parse_digits_sub(&dt->month, &p, end, 2)) {
            return FALSE;
        }
        if (p < end && *p == '-') {
            p++;
        }
        if (!parse_digits_sub(&dt->day_of_month, &p, end, 2)) {
            return FALSE;
        }
    }

    if (neg) {
        dt->year = -dt->year;
    }

    if (tm == NULL) {
        return TRUE;
    }

    while (p < end && (isspace(*p) || *p == 'T')) {
        p++;
    }

    tm->millisec = 0;

    if (!parse_digits_sub(&tm->hour, &p, end, 2)) {
        tm->hour = 0;
        tm->minute = 0;
        tm->second = 0;
        return TRUE;
    }
    if (p < end && *p == ':') {
        p++;
    }
    if (!parse_digits_sub(&tm->minute, &p, end, 2)) {
        return FALSE;
    }
    if (p < end && *p == ':') {
        p++;
    }
    if (!parse_digits_sub(&tm->second, &p, end, 2)) {
        return FALSE;
    }

    return TRUE;
}
/**
 * RFC2822
 * [day-of-week, ","] date month year hour ":" minute [":" second [("," | ".") millisec]] [("+" | "-") zone]
 */
static int parse_rfc_sub(Date *dt, Time *tm, const char *src_p, int src_size)
{
    static TimeZoneAbbrName us_timezone[] = {
        {"est", -5},
        {"edt", -4},
        {"cst", -6},
        {"cdt", -5},
        {"mst", -7},
        {"mdt", -6},
        {"pst", -8},
        {"pdt", -7},
    };
    const char *p = src_p;
    const char *end = p + src_size;
    int pm = FALSE;
    int zone_offset = 0;
    int offset_done = FALSE;

    DateTime_init(dt, tm);

    while (p < end) {
        Str s;
        int alpha_only = TRUE;
        int digit_only = TRUE;
        
        while (p < end && (isspace_fox(*p) || *p == ',')) {
            p++;
        }

        s.p = p;
        while (p < end && !isspace_fox(*p) && *p != ',') {
            if (!isalphau_fox(*p)) {
                alpha_only = FALSE;
            }
            if (!isdigit_fox(*p)) {
                digit_only = FALSE;
            }
            p++;
        }
        s.size = p - s.p;

        if (s.size > 0) {
            if (alpha_only) {
                int i_month = -1;
                int i;

                for (i = 0; i < lengthof(time_month_str); i++) {
                    if (str_eqi(s.p, s.size, time_month_str[i], -1)) {
                        i_month = i;
                        break;
                    }
                }
                if (i_month != -1) {
                    dt->month = i_month + 1;
                } else if (str_eqi(s.p, s.size, "am", 2)) {
                    pm = FALSE;
                } else if (str_eqi(s.p, s.size, "pm", 2)) {
                    pm = TRUE;
                } else if (!offset_done) {
                    if (s.size == 3) {
                        for (i = 0; i < lengthof(us_timezone); i++) {
                            if (str_eqi(s.p, s.size, us_timezone[i].name, -1)) {
                                zone_offset = us_timezone[i].hours * 60;
                                break;
                            }
                        }
                    } else if (s.size == 1) {
                        switch (tolower_fox(s.p[i])) {
                        case 'a':
                            break;
                        case 'p':
                            break;
                        }
                    }
                }
            } else if (digit_only) {
                if (s.size >= 3) {
                    dt->year = parse_int(s.p, s.size, 99999999);
                } else {
                    dt->day_of_month = parse_int(s.p, s.size, 99);
                }
            } else if (s.p[0] == '+' || s.p[0] == '-') {
                int sign = (s.p[0] == '+' ? 1 : -1);
                int diff;
                const char *p2 = s.p + 1;
                const char *end2 = s.p + s.size;

                if (!parse_digits_sub(&diff, &p2, end2, 4)) {
                    continue;
                }
                zone_offset = sign * (diff / 100) * 60 + (diff % 100);
                offset_done = TRUE;
            } else {
                const char *p2 = s.p;
                const char *end2 = s.p + s.size;
                int hour = 0, minute = 0, second = 0;

                if (!parse_digits_sub(&hour, &p2, end2, 4)) {
                    continue;
                }
                if (p2 >= end2 || *p2 != ':') {
                    continue;
                }
                p2++;
                if (!parse_digits_sub(&minute, &p2, end2, 4)) {
                    continue;
                }
                if (p2 < end2 && *p2 == ':') {
                    p2++;
                    if (!parse_digits_sub(&second, &p2, end2, 4)) {
                        continue;
                    }
                }
                tm->hour = hour;
                tm->minute = minute;
                tm->second = second;
            }
        }
    }
    tm->minute -= zone_offset;
    if (pm) {
        tm->hour += 12;
    }

    return TRUE;
}

/**
 * yyyy-MM-dd hh:mm:ss
 */
static int timestamp_parse_iso(Value *vret, Value *v, RefNode *node)
{
    Date dt;
    Time tm;
    RefStr *src = Value_vp(v[1]);
    RefInt64 *rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
    *vret = vp_Value(rt);

    if (!parse_iso_sub(&dt, &tm, src->c, src->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid Time format %Q", src);
        return FALSE;
    }
    rt->u.i = DateTime_to_Timestamp(&dt, &tm);

    return TRUE;
}
static int timestamp_parse_rfc2822(Value *vret, Value *v, RefNode *node)
{
    Date dt;
    Time tm;
    RefStr *src = Value_vp(v[1]);
    RefInt64 *rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
    *vret = vp_Value(rt);

    if (!parse_rfc_sub(&dt, &tm, src->c, src->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid Time format %Q", src);
        return FALSE;
    }
    rt->u.i = DateTime_to_Timestamp(&dt, &tm);

    return TRUE;
}

static int word_parse_hms(Str src, int *hour, int *minute, int *second)
{
    const char *p = src.p;
    const char *end = p + src.size;
    int n;

    if (!parse_digits_sub(&n, &p, end, 2)) {
        return FALSE;
    }
    *hour = n;
    if (p >= end || *p != ':') {
        return FALSE;
    }
    p++;

    if (!parse_digits_sub(&n, &p, end, 2)) {
        return FALSE;
    }
    *minute = n;
    if (p >= end || *p != ':') {
        *second = 0;
        return TRUE;
    }
    p++;
    if (!parse_digits_sub(&n, &p, end, 2)) {
        return FALSE;
    }
    *second = n;

    return p == end;
}
static int word_parse_ymd(Str src, int *year, int *month, int *day)
{
    int y, m, d;
    const char *p = src.p;
    const char *end = p + src.size;

    if (!parse_digits_sub(&y, &p, end, 4)) {
        return FALSE;
    }
    if (*p != '-' && *p != '/') {
        return FALSE;
    }
    p++;

    if (!parse_digits_sub(&m, &p, end, 2)) {
        return FALSE;
    }
    if (*p != '-' && *p != '/') {
        *year = -10000;
        *month = y;
        *day = m;
        return TRUE;
    }
    p++;
    if (!parse_digits_sub(&d, &p, end, 2)) {
        return FALSE;
    }
    *year = y;
    *month = m;
    *day = d;

    return p == end;
}

static int adjust_datetime_sub(RefDateTime *dt, int *dst, int num, int *type, int parse_type, int adjust_month)
{
    switch (*type) {
    case PARSETYPE_NONE:
        return FALSE;
    case PARSETYPE_OFFSET:
        *dst += num;
        break;
    case PARSETYPE_NUM:
        *dst = num;
        break;
    }
    if (parse_type != PARSE_DELTA) {
        if (adjust_month) {
            Date_adjust_month(&dt->d);
        } else {
            DateTime_adjust(&dt->d, &dt->t);
        }
    }
    *type = PARSETYPE_NONE;
    return TRUE;
}
static int parse_month_name(Date *dt, Str src)
{
    char name[4];
    int i;

    if (src.size < 3) {
        return FALSE;
    }
    name[0] = toupper(src.p[0]);
    name[1] = tolower(src.p[1]);
    name[2] = tolower(src.p[2]);
    name[3] = '\0';

    for (i = 0; i < 12; i++) {
        if (strcmp(name, time_month_str[i]) == 0) {
            dt->month = i + 1;
            return TRUE;
        }
    }
    return FALSE;
}
static int Str_eqi_list(Str s, ...)
{
    va_list va;
    va_start(va, s);

    for (;;) {
        const char *p = va_arg(va, const char*);
        if (p == NULL) {
            break;
        }
        if (str_eqi(s.p, s.size, p, -1)) {
            va_end(va);
            return TRUE;
        }
    }
    va_end(va);
    return FALSE;
}
static int word_parse_datetime(RefDateTime *dt, const char *src_p, int src_size, int parse_type)
{
    int i;
    int type = PARSETYPE_NONE;
    int num = 0;
    int date_only = (dt->rh.type != fs->cls_datetime);

    for (i = 0; i < src_size; ) {
        int pos;
        Str s;
        while (i < src_size && isspace_fox(src_p[i])) {
            i++;
        }
        pos = i;
        while (i < src_size && !isspace_fox(src_p[i])) {
            i++;
        }
        s = Str_new(src_p + pos, i - pos);
        if (s.size == 0) {
            i++;
            continue;
        }
        if (Str_eqi_list(s, "y", "year", "years", NULL)) {
            if (parse_type == PARSE_DELTA) {
                // 1年のTimeDeltaは正確に決められない
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->d.year, num, &type, parse_type, TRUE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "a", "aj", "jy", "julian-year", "annum", NULL)) {
            if (parse_type != PARSE_DELTA) {
                // ユリウス年はTimeDeltaのみ使用可能
                return FALSE;
            }
            if (date_only) {
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->t.hour, num * 8766, &type, parse_type, FALSE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "mon", "month", "months", NULL)) {
            if (parse_type == PARSE_DELTA) {
                // 1ヶ月のTimeDeltaは正確に決められない
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->d.month, num, &type, parse_type, TRUE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "w", "week", "weeks", NULL)) {
            switch (type) {
            case PARSETYPE_OFFSET:
                dt->d.day_of_month += num * 7;
                if (parse_type != PARSE_DELTA) {
                    DateTime_adjust(&dt->d, &dt->t);
                }
                break;
            case PARSETYPE_NUM: {
                // isoweek
                dt->d.isoweek_year = dt->d.year;
                dt->d.isoweek = num;
                dt->d.year = INT32_MIN;
                DateTime_adjust(&dt->d, &dt->t);
                break;
            }
            default:
                return FALSE;
            }
            type = PARSETYPE_NONE;
        } else if (Str_eqi_list(s, "d", "day", "days", NULL)) {
            if (!adjust_datetime_sub(dt, &dt->d.day_of_month, num, &type, parse_type, FALSE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "h", "hour", "hours", NULL)) {
            if (date_only) {
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->t.hour, num, &type, parse_type, FALSE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "min", "minute", "minutes", NULL)) {
            if (date_only) {
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->t.minute, num, &type, parse_type, FALSE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "s", "sec", "second", "seconds", NULL)) {
            if (date_only) {
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->t.second, num, &type, parse_type, FALSE)) {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "ms", "msec", "millisecond", "milliseconds", NULL)) {
            if (date_only) {
                return FALSE;
            }
            if (!adjust_datetime_sub(dt, &dt->t.millisec, num, &type, parse_type, FALSE)) {
                return FALSE;
            }
        } else if (s.p[0] == '+' || s.p[0] == '-') {
            int n;
            if (type != PARSETYPE_NONE) {
                return FALSE;
            }
            if (date_only) {
                return FALSE;
            }

            n = parse_int(s.p + 1, s.size - 1, INT32_MAX);
            if (n < 0) {
                return FALSE;
            }
            type = PARSETYPE_OFFSET;
            if (s.p[0] == '-') {
                num = -n;
            } else {
                num = n;
            }
            if (parse_type == PARSE_NEGATIVE) {
                num = -num;
            }
        } else if (parse_type != PARSE_ABS) {
            if ((parse_type == PARSE_POSITIVE || parse_type == PARSE_NEGATIVE) && isdigit(s.p[0])) {
                type = PARSETYPE_OFFSET;
                num = parse_int(s.p, s.size, INT32_MAX);
                if (parse_type == PARSE_NEGATIVE) {
                    num = -num;
                }
            } else {
                return FALSE;
            }
        } else if (Str_eqi_list(s, "now", "today", "this-month", "this-year", NULL)) {
            if (type != PARSETYPE_NONE) {
                return FALSE;
            }

            dt->ts = get_now_time();
            dt->off = TimeZone_offset_local(dt->tz, dt->ts);
            Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts + dt->off->offset);
            if (!str_eqi(s.p, s.size, "now", 3)) {
                dt->t.hour = 0;
                dt->t.minute = 0;
                dt->t.second = 0;
                dt->t.millisec = 0;
                if (!str_eqi(s.p, s.size, "today", -1)) {
                    dt->d.day_of_month = 1;
                    if (!str_eqi(s.p, s.size, "this-month", -1)) {
                        dt->d.month = 1;
                    }
                }
            }
        } else {
            int n1, n2, n3;
            if (type != PARSETYPE_NONE) {
                return FALSE;
            }
            if (word_parse_ymd(s, &n1, &n2, &n3)) {
                if (n1 >= -9999) {
                    dt->d.year = n1;
                }
                dt->d.month = n2;
                dt->d.day_of_month = n3;
                type = PARSETYPE_NONE;
            } else if (word_parse_hms(s, &n1, &n2, &n3)) {
                if (date_only) {
                    return FALSE;
                }
                dt->t.hour = n1;
                dt->t.minute = n2;
                dt->t.second = n3;
                type = PARSETYPE_NONE;
            } else if (parse_month_name(&dt->d, s)) {
                type = PARSETYPE_NONE;
            } else if (isdigit(s.p[0])){
                type = PARSETYPE_NUM;
                num = parse_int(s.p, s.size, INT32_MAX);
            } else {
                return FALSE;
            }
        }
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * .NETのフォーマットに近い
 */
static void build_datetime_string(StrBuf *buf, const char *fmt_p, int fmt_size, const Date *dt, const Time *tm, const TimeOffset *off, const LocaleData *loc)
{
    int i = 0;
    int end;
    int n;
    char c_buf[16];

    if (fmt_p == NULL) {
        return;
    }
    if (fmt_size < 0) {
        fmt_size = strlen(fmt_p);
    }

    while (i < fmt_size) {
        switch (fmt_p[i]) {
        case 'y':   // yy yyyy
        case 'Y': { // YY YYYY (isoweek)
            int ch = fmt_p[i];
            int32_t year = (ch == 'y' ? dt->year : dt->isoweek_year);
            n = 0;
            end = i + 4;
            if (end > fmt_size) {
                end = fmt_size;
            }
            do {
                i++;
                n++;
            } while (i < end && fmt_p[i] == ch);

            switch (n) {
            case 1:
                sprintf(c_buf, "%d", year);
                break;
            case 2: {
                int32_t y = year % 100;
                if (y < 0) {
                    y += 100;
                }
                sprintf(c_buf, "%02d", y);
                break;
            }
            default:
                if (dt->year < 0) {
                    sprintf(c_buf, "-%04d", -year);
                } else {
                    sprintf(c_buf, "%04d", year);
                }
                break;
            }
            StrBuf_add(buf, c_buf, -1);
            break;
        }
        case 'M':  // M MM MMM MMMM (月、数字と名前)
            n = 0;
            end = i + 4;
            if (end > fmt_size) {
                end = fmt_size;
            }
            do {
                i++;
                n++;
            } while (i < end && fmt_p[i] == 'M');

            switch (n) {
            case 1:
                sprintf(c_buf, "%d", dt->month);
                StrBuf_add(buf, c_buf, -1);
                break;
            case 2:
                sprintf(c_buf, "%02d", dt->month);
                StrBuf_add(buf, c_buf, -1);
                break;
            case 3:
                {
                    const char *s = loc->month[dt->month - 1];
                    if (s != NULL) {
                        StrBuf_add(buf, s, -1);
                    } else {
                        sprintf(c_buf, "%02d", dt->month);
                        StrBuf_add(buf, c_buf, -1);
                    }
                }
                break;
            case 4:
                {
                    const char *s = loc->month_w[dt->month - 1];
                    if (s != NULL) {
                        StrBuf_add(buf, s, -1);
                    } else {
                        sprintf(c_buf, "%02d", dt->month);
                        StrBuf_add(buf, c_buf, -1);
                    }
                }
                break;
            }
            break;
        case 'w':  // ISO week (1 - 53)
            i++;
            if (i < fmt_size && fmt_p[i] == 'w') {
                i++;
                sprintf(c_buf, "%02d", dt->isoweek);
            } else {
                sprintf(c_buf, "%d", dt->isoweek);
            }
            StrBuf_add(buf, c_buf, -1);
            break;
        case 'W':  // ISO week number (1:Mon - 7:Sun)
            i++;
            StrBuf_add_c(buf, '0' + dt->day_of_week);
            break;
        case 'd':  // d dd ddd dddd (日、曜日)
            n = 0;
            end = i + 4;
            if (end > fmt_size) {
                end = fmt_size;
            }
            do {
                i++;
                n++;
            } while (i < end && fmt_p[i] == 'd');

            switch (n) {
            case 1:
                sprintf(c_buf, "%d", dt->day_of_month);
                StrBuf_add(buf, c_buf, -1);
                break;
            case 2:
                sprintf(c_buf, "%02d", dt->day_of_month);
                StrBuf_add(buf, c_buf, -1);
                break;
            case 3: {
                const char *s = loc->week[dt->day_of_week - 1];
                if (s != NULL) {
                    StrBuf_add(buf, s, -1);
                }
                break;
            }
            case 4: {
                const char *s = loc->week_w[dt->day_of_week - 1];
                if (s != NULL) {
                    StrBuf_add(buf, s, -1);
                }
                break;
            }
            }
            break;
        case 'H':  // H HH (24時間)
            if (tm != NULL) {
                i++;
                if (i < fmt_size && fmt_p[i] == 'H') {
                    i++;
                    sprintf(c_buf, "%02d", tm->hour);
                } else {
                    sprintf(c_buf, "%d", tm->hour);
                }
                StrBuf_add(buf, c_buf, -1);
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'h':   // h hh (12時間)
            if (tm != NULL) {
                int hour = tm->hour % 12;
                if (hour == 0) {
                    hour = 12;
                }
                i++;
                if (i < fmt_size && fmt_p[i] == 'h') {
                    i++;
                    sprintf(c_buf, "%02d", hour);
                } else {
                    sprintf(c_buf, "%d", hour);
                }
                StrBuf_add(buf, c_buf, -1);
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'a':   // a AM,PM
            if (tm != NULL) {
                const char *s = loc->am_pm[tm->hour < 12 ? 0 : 1];
                if (s != NULL) {
                    StrBuf_add(buf, s, -1);
                }
                i++;
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'm':  // m mm 分
            if (tm != NULL) {
                i++;
                if (i < fmt_size && fmt_p[i] == 'm') {
                    i++;
                    sprintf(c_buf, "%02d", tm->minute);
                } else {
                    sprintf(c_buf, "%d", tm->minute);
                }
                StrBuf_add(buf, c_buf, -1);
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 's':  // s ss 秒
            if (tm != NULL) {
                i++;
                if (i < fmt_size && fmt_p[i] == 's') {
                    i++;
                    sprintf(c_buf, "%02d", tm->second);
                } else {
                    sprintf(c_buf, "%d", tm->second);
                }
                StrBuf_add(buf, c_buf, -1);
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'S':  // ミリ秒
            if (tm != NULL) {
                n = 0;
                end = i + 3;
                if (end > fmt_size) {
                    end = fmt_size;
                }
                do {
                    i++;
                    n++;
                } while (i < end && fmt_p[i] == 'S');

                switch (n) {
                case 1:
                    sprintf(c_buf, "%d", tm->millisec / 100);
                    StrBuf_add(buf, c_buf, -1);
                    break;
                case 2:
                    sprintf(c_buf, "%02d", tm->millisec / 10);
                    StrBuf_add(buf, c_buf, -1);
                    break;
                case 3:
                    sprintf(c_buf, "%03d", tm->millisec);
                    StrBuf_add(buf, c_buf, -1);
                    break;
                }
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'X':  // 既定の日付
            n = 0;
            end = i + 4;
            if (end > fmt_size) {
                end = fmt_size;
            }
            do {
                i++;
                n++;
            } while (i < end && fmt_p[i] == 'X');

            build_datetime_string(buf, loc->date[n - 1], -1, dt, tm, off, loc);
            break;
        case 'x':  // 既定の時刻
            if (tm != NULL) {
                n = 0;
                end = i + 4;
                if (end > fmt_size) {
                    end = fmt_size;
                }
                do {
                    i++;
                    n++;
                } while (i < end && fmt_p[i] == 'x');

                build_datetime_string(buf, loc->time[n - 1], -1, dt, tm, off, loc);
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'z':   // 時差(+00:00)
            if (off != NULL) {
                int offset = off->offset;
                int sign = '+';
                if (offset < 0) {
                    offset = -offset;
                    sign = '-';
                }
                sprintf(c_buf, "%c%02d:%02d", sign, offset / 3600000, (offset / 60000) % 60);
                StrBuf_add(buf, c_buf, -1);
                i++;
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case 'Z':  // タイムゾーン
            if (off != NULL) {
                StrBuf_add(buf, off->abbr, -1);
                i++;
            } else {
                StrBuf_add_c(buf, '?');
                i++;
            }
            break;
        case '\'': // '任意の文字列'
            i++;
            if (i < fmt_size) {
                if (fmt_p[i] == '\'') {
                    StrBuf_add_c(buf, '\'');
                    i++;
                } else {
                    StrBuf_add_c(buf, fmt_p[i]);
                    i++;
                    while (i < fmt_size) {
                        char c = fmt_p[i];
                        if (c == '\'') {
                            i++;
                            if (i < fmt_size) {
                                if (fmt_p[i] == '\'') {
                                    StrBuf_add_c(buf, '\'');
                                } else {
                                    break;
                                }
                            } else {
                                break;
                            }
                        } else {
                            StrBuf_add_c(buf, c);
                        }
                        i++;
                    }
                }
            }
            break;
        default:
            if (!isalphau_fox(fmt_p[i])) {
                StrBuf_add_c(buf, fmt_p[i]);
            } else {
                StrBuf_add_c(buf, '?');
            }
            i++;
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int timestamp_add(Value *vret, Value *v, RefNode *node)
{
    int64_t tm1 = VALUE_INT64(*v);
    RefNode *v1_type = Value_type(v[1]);
    int neg = FUNC_INT(node);

    if (v1_type == cls_timedelta) {
        double ret;
        double tm2 = Value_float2(v[1]);
        RefInt64 *rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
        *vret = vp_Value(rt);
        if (neg) {
            ret = (double)tm1 - tm2;
        } else {
            ret = (double)tm1 + tm2;
        }
        if (ret < TIMESTAMP_MIN || ret > TIMESTAMP_MAX) {
            throw_errorf(fs->mod_lang, "ValueError", "Timestamp overfloat");
            return FALSE;
        }
        rt->u.i = (int64_t)ret;
    } else if (v1_type == fs->cls_str) {
        RefDateTime dt;
        RefInt64 *rt;
        RefStr *str = Value_vp(v[1]);

        DateTime_init(&dt.d, &dt.t);
        dt.off = 0;
        dt.tz = fs->tz_utc;
        Timestamp_to_DateTime(&dt.d, &dt.t, tm1);

        if (!word_parse_datetime(&dt, str->c, str->size, neg ? PARSE_NEGATIVE : PARSE_POSITIVE)) {
            throw_errorf(fs->mod_lang, "ParseError", "Invalid time format");
            return FALSE;
        }
        rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
        *vret = vp_Value(rt);
        adjust_timezone(&dt);
        rt->u.i = dt.ts;
    } else if (!neg) {
        throw_errorf(fs->mod_lang, "TypeError", "TimeDelta or Str required but %n", v1_type);
        return FALSE;
    } else if (v1_type == fs->cls_timestamp) {
        // TimeStamp - TimeStamp
        int64_t tm2 = VALUE_INT64(v[1]);
        *vret = float_Value(cls_timedelta, (double)tm1 - (double)tm2);
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "TimeDelta, TimeStamp or Str required but %n", v1_type);
        return FALSE;
    }
    return TRUE;
}
static int timestamp_iso8601(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *rt = Value_vp(*v);
    Date dt;
    Time tm;
    char c_buf[48];
    const char *format = FUNC_VP(node);

    Timestamp_to_DateTime(&dt, &tm, rt->u.i);
    sprintf(c_buf, format,
            dt.year, dt.month, dt.day_of_month,
            tm.hour, tm.minute, tm.second);

    *vret = cstr_Value(fs->cls_str, c_buf, -1);

    return TRUE;
}
static int timestamp_rfc2822(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *rt = Value_vp(*v);
    Date dt;
    Time tm;
    char c_buf[48];

    Timestamp_to_DateTime(&dt, &tm, rt->u.i);
    sprintf(c_buf, "%s, %02d %s %d %02d:%02d:%02d GMT",
            time_week_str[dt.day_of_week - 1], dt.day_of_month, time_month_str[dt.month - 1], dt.year,
            tm.hour, tm.minute, tm.second);

    *vret = cstr_Value(fs->cls_str, c_buf, -1);

    return TRUE;
}
static int parse_time_resolution(RefStr *rs)
{
    if (rs->size == 1) {
        switch (rs->c[0]) {
        case 'y':
            return GET_YEAR;
        case 'M':
            return GET_MONTH;
        case 'w':
            return GET_DAY_OF_WEEK;
        case 'd':
            return GET_DAY_OF_MONTH;
        case 'h':
            return GET_HOUR;
        case 'm':
            return GET_MINUTE;
        case 's':
            return GET_SECOND;
        case 'f':
            return GET_MILLISEC;
        }
    } if (str_eqi(rs->c, rs->size, "year", 4)) {
        return GET_YEAR;
    } else if (str_eqi(rs->c, rs->size, "month", 5)) {
        return GET_MONTH;
    } else if (str_eqi(rs->c, rs->size, "week", 4)) {
        return GET_DAY_OF_WEEK;
    } else if (str_eqi(rs->c, rs->size, "day", 3)) {
        return GET_DAY_OF_MONTH;
    } else if (str_eqi(rs->c, rs->size, "hour", 4)) {
        return GET_HOUR;
    } else if (str_eqi(rs->c, rs->size, "minute", 6)) {
        return GET_MINUTE;
    } else if (str_eqi(rs->c, rs->size, "second", 6)) {
        return GET_SECOND;
    } else if (str_eqi(rs->c, rs->size, "ms", 6)) {
        return GET_MILLISEC;
    }
    return GET_ERR;
}
static int timestamp_round(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *src = Value_vp(*v);
    RefInt64 *dst;
    int round_mode = parse_round_mode(Value_vp(v[1]));
    int64_t resol = 0;
    int64_t src_i = src->u.i;
    int sign = 1;

    if (src_i < 0) {
        src_i = -src_i;
        sign = -1;
    }

    if (round_mode == ROUND_ERR || round_mode == ROUND_FLOOR || round_mode == ROUND_CEILING || round_mode == ROUND_HALF_EVEN) {
        throw_errorf(fs->mod_lang, "ValueError", "Unknown round mode %v", v[1]);
        return FALSE;
    }
    switch (parse_time_resolution(Value_vp(v[2]))) {
    case GET_DAY_OF_MONTH:
        resol = MSECS_PER_DAY;
        break;
    case GET_HOUR:
        resol = MSECS_PER_HOUR;
        break;
    case GET_MINUTE:
        resol = MSECS_PER_MINUTE;
        break;
    case GET_SECOND:
        resol = MSECS_PER_SECOND;
        break;
    case GET_MILLISEC:
        resol = 1;
        break;
    default:
        throw_errorf(fs->mod_lang, "ValueError", "Unknown mode %v", v[2]);
        return FALSE;
    }
    if (sign < 0) {
        switch (round_mode) {
        case ROUND_DOWN:
            round_mode = ROUND_UP;
            break;
        case ROUND_UP:
            round_mode = ROUND_DOWN;
            break;
        case ROUND_HALF_DOWN:
            round_mode = ROUND_HALF_UP;
            break;
        case ROUND_HALF_UP:
            round_mode = ROUND_HALF_DOWN;
            break;
        }
    }
    if (resol > 1) {
        switch (round_mode) {
        case ROUND_DOWN:
            src_i = src_i / resol * resol;
            break;
        case ROUND_UP:
            src_i = (src_i + resol - 1) / resol * resol;
            break;
        case ROUND_HALF_DOWN:
            src_i = (src_i + resol / 2 - 1) / resol * resol;
            break;
        case ROUND_HALF_UP:
            src_i = (src_i + resol / 2) / resol * resol;
            break;
        }
    }
    dst = buf_new(fs->cls_timestamp, sizeof(RefInt64));
    dst->u.i = src_i * sign;
    *vret = vp_Value(dst);

    return TRUE;
}
static int timestamp_tostr(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        if (str_eqi(fmt->c, fmt->size, "iso8601", -1)) {
            return timestamp_iso8601(vret, v, node);
        } else if (str_eqi(fmt->c, fmt->size, "rfc2822", -1)) {
            return timestamp_rfc2822(vret, v, node);
        } else {
            throw_errorf(fs->mod_lang, "FormatError", "Unknown format %q", fmt->c);
            return FALSE;
        }
    } else {
        RefInt64 *rt = Value_vp(*v);
        Date dt;
        Time tm;
        char c_buf[48];

        Timestamp_to_DateTime(&dt, &tm, rt->u.i);
        sprintf(c_buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                dt.year, dt.month, dt.day_of_month,
                tm.hour, tm.minute, tm.second, tm.millisec);

        *vret = cstr_Value(fs->cls_str, c_buf, -1);
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int timedelta_new(Value *vret, Value *v, RefNode *node)
{
    double num[] = {
        0.0, 0.0, 0.0, 0.0, 0.0,
    };
    int i;
    RefFloat *rt = buf_new(cls_timedelta, sizeof(RefFloat));
    *vret = vp_Value(rt);

    for (i = 1; v + i < fg->stk_top; i++) {
        num[i - 1] = Value_float(v[i]);
    }
    rt->d = num[0] * MSECS_PER_DAY + num[1] * MSECS_PER_HOUR + num[2] * MSECS_PER_MINUTE + num[3] * MSECS_PER_SECOND + num[4];

    return TRUE;
}

int timedelta_parse_string(double *ret, const char *src_p, int src_size)
{
    RefDateTime dt;
    double ts;

    if (src_size < 0) {
        src_size = strlen(src_p);
    }
    dt.rh.type = cls_timedelta;

    DateTime_init(&dt.d, &dt.t);
    dt.d.day_of_month = 0;
    // TODO:timedelta専用にする
    if (!word_parse_datetime(&dt, src_p, src_size, PARSE_DELTA)) {
        return FALSE;
    }

    ts = (double)dt.d.day_of_month * MSECS_PER_DAY + (double)dt.t.hour * MSECS_PER_HOUR;
    ts += dt.t.minute * MSECS_PER_MINUTE + dt.t.second * MSECS_PER_SECOND + dt.t.millisec;
    *ret = ts;

    return TRUE;
}
static int timedelta_parse(Value *vret, Value *v, RefNode *node)
{
    RefStr *fmt = Value_vp(v[1]);
    RefFloat *rt = buf_new(cls_timedelta, sizeof(RefFloat));
    *vret = vp_Value(rt);

    if (!timedelta_parse_string(&rt->d, fmt->c, fmt->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid time format %Q", fmt);
        return FALSE;
    }

    return TRUE;
}
static int timedelta_empty(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rt = Value_vp(*v);
    *vret = bool_Value(rt->d == 0.0);
    return TRUE;
}
static int timedelta_negative(Value *vret, Value *v, RefNode *node)
{
    double src = Value_float2(*v);
    *vret = float_Value(cls_timedelta, -src);
    return TRUE;
}
static int timedelta_add(Value *vret, Value *v, RefNode *node)
{
    int sign = FUNC_INT(node);
    double r1 = Value_float2(*v);
    double r2 = Value_float2(v[1]);

    *vret = float_Value(cls_timedelta, r1 + r2 * sign);

    return TRUE;
}
static int timedelta_mul(Value *vret, Value *v, RefNode *node)
{
    double r1 = Value_float2(*v);
    double d2 = Value_float(v[1]);

    *vret = float_Value(cls_timedelta, r1 * d2);

    return TRUE;
}
static int timedelta_div(Value *vret, Value *v, RefNode *node)
{
    double r1 = Value_float2(*v);
    RefNode *v1_type = Value_type(v[1]);

    if (v1_type == fs->cls_float) {
        double d2 = Value_float2(v[1]);
        double d = r1 / d2;

        if (isinf(d)) {
            throw_errorf(fs->mod_lang, "ZeroDivisionError", "TimeDelta / 0.0");
            return FALSE;
        }
        *vret = float_Value(cls_timedelta, d);
    } else if (v1_type == cls_timedelta) {
        double r2 = Value_float2(v[1]);
        *vret = float_Value(fs->cls_float, r1 / r2);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_float, cls_timedelta, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}
static int timedelta_get(Value *vret, Value *v, RefNode *node)
{
    int64_t ret = (int64_t)Value_float2(*v);

    switch (FUNC_INT(node)) {
    case GET_DAY_OF_YEAR:
        ret /= MSECS_PER_DAY;
        break;
    case GET_HOUR:
        ret /= MSECS_PER_HOUR;
        break;
    case GET_MINUTE:
        ret /= MSECS_PER_MINUTE;
        break;
    case GET_SECOND:
        ret /= MSECS_PER_SECOND;
        break;
    case GET_MILLISEC:
        break;
    }
    *vret = int64_Value(ret);

    return TRUE;
}
static int timedelta_get_float(Value *vret, Value *v, RefNode *node)
{
    double ret = Value_float2(*v);

    switch (FUNC_INT(node)) {
    case GET_DAY_OF_YEAR:
        ret /= (double)MSECS_PER_DAY;
        break;
    case GET_SECOND:
        ret /= (double)MSECS_PER_SECOND;
        break;
    case GET_MILLISEC:
        break;
    }
    *vret = float_Value(fs->cls_float, ret);

    return TRUE;
}

static int format_get_leading_chars(const char *s_p, int s_size, int *pi, int ch, int max)
{
    int i;
    int init = *pi;
    int end = init + max;

    for (i = *pi; i < end && i < s_size; i++) {
        if (s_p[i] != ch) {
            break;
        }
    }
    *pi = i;
    return i - init;
}
static void print_leading_zero(StrBuf *sb, int width, int64_t val)
{
    char cbuf[24];
    char *dst;
    char *p;

    p = cbuf;
    while (val > 0) {
        *p++ = '0' + (val % 10);
        val /= 10;
    }
    *p = '\0';
    if (width > (int)(p - cbuf)) {
        int leading_zero = width - (p - cbuf);
        int i;

        StrBuf_alloc(sb, sb->size + width);
        dst = sb->p + sb->size - width;
        for (i = 0; i < leading_zero; i++) {
            *dst++ = '0';
        }
        dst = sb->p + sb->size - 1;
    } else {
        StrBuf_alloc(sb, sb->size + (p - cbuf));
        dst = sb->p + sb->size - 1;
    }

    p = cbuf;
    while (*p != '\0') {
        *dst-- = *p++;
    }
}
static void timedelta_tostr_sub(StrBuf *sbuf, const char *fmt_p, int fmt_size, int64_t diff_src)
{
    enum {
        MAX_DIGIT = 32,
    };

    int day, hour, minute, second, millisec;
    int sign = 1;
    int width;
    int64_t diff;
    int i = 0;

    if (fmt_size < 0) {
        fmt_size = strlen(fmt_p);
    }

    if (diff_src < 0) {
        diff_src = -diff_src;
        sign = -1;
    }
    diff = diff_src;

    day = diff / 86400000LL;
    diff %= 86400000LL;
    hour = diff / 3600000LL;
    diff %= 3600000LL;
    minute = diff / 60000LL;
    diff %= 60000LL;
    second = diff / 1000LL;
    millisec = diff % 1000LL;

    while (i < fmt_size) {
        switch (fmt_p[i]) {
        case '+':
            StrBuf_add_c(sbuf, sign < 0 ? '-' : '+');
            i++;
            break;
        case '-':
            StrBuf_add_c(sbuf, sign < 0 ? '-' : ' ');
            i++;
            break;
        case 'D':
        case 'd':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'd', MAX_DIGIT);
            print_leading_zero(sbuf, width, day);
            break;
        case 'h':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'h', 2);
            print_leading_zero(sbuf, width, hour);
            break;
        case 'm':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'm', 2);
            print_leading_zero(sbuf, width, minute);
            break;
        case 's':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 's', 2);
            print_leading_zero(sbuf, width, second);
            break;
        case 't':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 't', 3);
            switch (width) {
            case 1:
                print_leading_zero(sbuf, 1, millisec / 100);
                break;
            case 2:
                print_leading_zero(sbuf, 2, millisec / 10);
                break;
            case 3:
                print_leading_zero(sbuf, 3, millisec);
                break;
            }
            break;
        case 'H':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'H', MAX_DIGIT);
            print_leading_zero(sbuf, width, diff_src / 3600000LL);
            break;
        case 'M':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'M', MAX_DIGIT);
            print_leading_zero(sbuf, width, diff_src / 60000LL);
            break;
        case 'S':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'S', MAX_DIGIT);
            print_leading_zero(sbuf, width, diff_src / 1000LL);
            break;
        case 'T':
            width = format_get_leading_chars(fmt_p, fmt_size, &i, 'T', MAX_DIGIT);
            print_leading_zero(sbuf, width, diff_src);
            break;
        case '\'': // '任意の文字列'
            i++;
            if (i < fmt_size) {
                if (fmt_p[i] == '\'') {
                    StrBuf_add_c(sbuf, '\'');
                    i++;
                } else {
                    StrBuf_add_c(sbuf, fmt_p[i]);
                    i++;
                    while (i < fmt_size) {
                        char c = fmt_p[i];
                        if (c == '\'') {
                            i++;
                            if (i < fmt_size) {
                                if (fmt_p[i] == '\'') {
                                    StrBuf_add_c(sbuf, '\'');
                                } else {
                                    break;
                                }
                            } else {
                                break;
                            }
                        } else {
                            StrBuf_add_c(sbuf, c);
                        }
                        i++;
                    }
                }
            }
            break;
        default:
            StrBuf_add_c(sbuf, fmt_p[i]);
            i++;
            break;
        }
    }
}
static int timedelta_tostr(Value *vret, Value *v, RefNode *node)
{
    double diff = Value_float2(*v);
    StrBuf buf;

    StrBuf_init_refstr(&buf, 0);
    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        timedelta_tostr_sub(&buf, fmt->c, fmt->size, diff);
    } else {
        timedelta_tostr_sub(&buf, "'TimeDelta'(+d, hh:mm:ss.ttt)", -1, diff);
    }
    *vret = StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}
static int timedelta_round(Value *vret, Value *v, RefNode *node)
{
    double src = Value_float2(*v);
    int round_mode = parse_round_mode(Value_vp(v[1]));
    double resol = 0.0;
    double src_d = src;
    int sign = 1;

    if (src_d < 0.0) {
        src_d = -src_d;
        sign = -1;
    }

    if (round_mode == ROUND_ERR || round_mode == ROUND_FLOOR || round_mode == ROUND_CEILING) {
        throw_errorf(fs->mod_lang, "ValueError", "Unknown round mode %v", v[1]);
        return FALSE;
    }
    if (round_mode == ROUND_HALF_DOWN || round_mode == ROUND_HALF_EVEN) {
        round_mode = ROUND_HALF_UP;
    }
    switch (parse_time_resolution(Value_vp(v[2]))) {
    case GET_DAY_OF_MONTH:
        resol = MSECS_PER_DAY;
        break;
    case GET_HOUR:
        resol = MSECS_PER_HOUR;
        break;
    case GET_MINUTE:
        resol = MSECS_PER_MINUTE;
        break;
    case GET_SECOND:
        resol = MSECS_PER_SECOND;
        break;
    case GET_MILLISEC:
        resol = 1;
        break;
    default:
        throw_errorf(fs->mod_lang, "ValueError", "Unknown mode %v", v[2]);
        return FALSE;
    }
    if (sign < 0) {
        switch (round_mode) {
        case ROUND_DOWN:
            round_mode = ROUND_UP;
            break;
        case ROUND_UP:
            round_mode = ROUND_DOWN;
            break;
        case ROUND_HALF_DOWN:
            round_mode = ROUND_HALF_UP;
            break;
        case ROUND_HALF_UP:
            round_mode = ROUND_HALF_DOWN;
            break;
        }
    }
    if (resol > 0.0) {
        switch (round_mode) {
        case ROUND_DOWN:
            src_d = floor(src_d / resol) * resol;
            break;
        case ROUND_UP:
            src_d = ceil(src_d / resol) * resol;
            break;
        case ROUND_HALF_UP:
            src_d = floor((src_d + resol / 2) / resol) * resol;
            break;
        }
    }
    *vret = float_Value(cls_timedelta, src_d * sign);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int timezone_new(Value *vret, Value *v, RefNode *node)
{
    RefTimeZone *tz = Value_to_tz(v[1], 0);

    if (tz == NULL) {
        return FALSE;
    }
    *vret = Value_cp(vp_Value(tz));

    return TRUE;
}
static RefTimeZone *timezone_marshal_read_sub(Value r)
{
    RefTimeZone *tz;
    uint32_t size;
    int rd_size;
    char cbuf[64];

    if (!stream_read_uint32(r, &size)) {
        return NULL;
    }
    if (size > 63) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return NULL;
    }
    rd_size = size;
    if (!stream_read_data(r, NULL, cbuf, &rd_size, FALSE, TRUE)) {
        return NULL;
    }
    cbuf[rd_size] = '\0';

    tz = load_timezone(cbuf, -1);
    if (tz == NULL) {
        throw_errorf(fs->mod_lang, "ValueError", "RefTimeZone %q not found", cbuf);
        return NULL;
    }

    return tz;
}
static int timezone_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefTimeZone *tz = timezone_marshal_read_sub(r);

    if (tz == NULL) {
        return FALSE;
    }
    *vret = Value_cp(vp_Value(tz));

    return TRUE;
}
static int timezone_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefTimeZone *tz = Value_vp(*v);
    const char *name = tz->name;
    int name_size = strlen(name);

    if (!stream_write_uint32(w, name_size)) {
        return FALSE;
    }
    if (!stream_write_data(w, name, name_size)) {
        return FALSE;
    }

    return TRUE;
}
static int timezone_tostr(Value *vret, Value *v, RefNode *node)
{
    RefTimeZone *tz = Value_vp(*v);
    RefStr *fmt = fs->str_0;

    if (fg->stk_top > v + 1) {
        fmt = Value_vp(v[1]);
    }
    if (fmt->size == 0) {
        *vret = printf_Value("TimeZone(%s)", tz->name);
    } else if (fmt->c[0] == 'N' || fmt->c[0] == 'n') {
        *vret = Value_cp(vp_Value(tz));
    } else {
        throw_errorf(fs->mod_lang, "FormatError", "Unknown format string %q", fmt->c);
        return FALSE;
    }
    return TRUE;
}
static int timezone_local(Value *vret, Value *v, RefNode *node)
{
    *vret = Value_cp(vp_Value(get_local_tz()));
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// vが、-INT32_MAX〜INT32_MAXの範囲外ならエラーをthrowする
static int validate_int32_range(Value *v, int n, int argn)
{
    int i;
    for (i = 0; i < n; i++) {
        if (Value_isref(v[i])) {
            throw_errorf(fs->mod_lang, "ValueError", "Out of range (argument #%d)", argn + i);
            return FALSE;
        }
    }
    return TRUE;
}
static int datetime_new(Value *vret, Value *v, RefNode *node)
{
    int is_isoweek = FUNC_INT(node);
    Value *va;
    int argc = fg->stk_top - v - 1;
    RefDateTime *dt = buf_new(fs->cls_datetime, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    // TimeZoneがあれば、処理する
    if (argc > 0) {
        RefNode *v1_type = Value_type(v[1]);

        if (v1_type == fs->cls_timezone) {
            dt->tz = Value_vp(v[1]);
            va = v + 2;
            argc--;
        } else if (v1_type == fs->cls_str) {
            RefStr *name = Value_vp(v[1]);
            RefTimeZone *tz = load_timezone(name->c, name->size);
            if (tz == NULL) {
                throw_errorf(fs->mod_lang, "ValueError", "TimeZone %q not found", name->c);
                return FALSE;
            }
            dt->tz = tz;
            va = v + 2;
            argc--;
        } else {
            dt->tz = get_local_tz();
            va = v + 1;
        }
    } else {
        va = v + 1;
    }

    DateTime_init(&dt->d, &dt->t);

    if (argc > 0) {
        RefNode *v1_type = Value_type(va[0]);
        if (v1_type == fs->cls_timestamp) {
            int64_t ts;
            if (argc > 1) {
                throw_errorf(fs->mod_lang, "ArgumentError", "Too many arguments (%d given)", argc);
                return FALSE;
            }
            ts = VALUE_INT64(va[0]);
            dt->off = TimeZone_offset_utc(dt->tz, ts);
            dt->ts = ts;
            Timestamp_to_DateTime(&dt->d, &dt->t, ts + dt->off->offset);
        } else if (v1_type == fs->cls_int) {
            if (argc > 7) {
                throw_errorf(fs->mod_lang, "ArgumentError", "Too many arguments (%d given)", argc);
                return FALSE;
            }
            if (argc > 1) {
                RefNode *v2_type = Value_type(va[1]);
                if (v2_type != fs->cls_int) {
                    throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_timestamp, fs->cls_int, v2_type, va - v + 2);
                    return FALSE;
                }
            }
            if (!validate_int32_range(va, argc, va - v)) {
                return FALSE;
            }
            if (is_isoweek) {
                dt->d.year = INT32_MIN;
                if (argc > 0) {
                    dt->d.isoweek_year = Value_int32(va[0]);
                }
                if (argc > 1) {
                    dt->d.isoweek = Value_int32(va[1]);
                }
                if (argc > 2) {
                    dt->d.day_of_week = Value_int32(va[2]);
                }
            } else {
                if (argc > 0) {
                    dt->d.year = Value_int32(va[0]);
                }
                if (argc > 1) {
                    dt->d.month = Value_int32(va[1]);
                }
                if (argc > 2) {
                    dt->d.day_of_month = Value_int32(va[2]);
                }
            }
            if (argc > 3) {
                dt->t.hour = Value_int32(va[3]);
            }
            if (argc > 4) {
                dt->t.minute = Value_int32(va[4]);
            }
            if (argc > 5) {
                dt->t.second = Value_int32(va[5]);
            }
            if (argc > 6) {
                dt->t.millisec = Value_int32(va[6]);
            }
        } else {
            throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_timestamp, fs->cls_int, v1_type, va - v + 1);
            return FALSE;
        }
    }
    adjust_timezone(dt);

    return TRUE;
}
static int datetime_now(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = buf_new(fs->cls_datetime, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    if (fg->stk_top > v + 1) {
        dt->tz = Value_to_tz(v[1], 0);
        if (dt->tz == NULL) {
            return FALSE;
        }
    } else {
        dt->tz = get_local_tz();
    }
    dt->ts = get_now_time();
    dt->off = TimeZone_offset_utc(dt->tz, dt->ts);
    Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts + dt->off->offset);

    return TRUE;
}
// TODO
static int datetime_parse_format(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}
static int datetime_marshal_read(Value *vret, Value *v, RefNode *node)
{
    uint32_t uval;
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefNode *type = FUNC_VP(node);
    RefDateTime *dt = buf_new(type, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    if (!stream_read_uint32(r, &uval)) {
        return FALSE;
    }
    dt->ts = ((uint64_t)uval) << 32;
    if (!stream_read_uint32(r, &uval)) {
        return FALSE;
    }
    dt->ts |= uval;

    if (type == fs->cls_datetime) {
        dt->tz = timezone_marshal_read_sub(r);
        if (dt->tz == NULL) {
            return FALSE;
        }
        dt->off = TimeZone_offset_utc(dt->tz, dt->ts);
    }
    Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts + dt->off->offset);

    return TRUE;
}
static int datetime_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefDateTime *dt = Value_vp(*v);
    const char *name = dt->tz->name;
    int name_size = strlen(name);

    if (!stream_write_uint32(w, (uint32_t)(dt->ts >> 32))) {
        return FALSE;
    }
    if (!stream_write_uint32(w, (uint32_t)(dt->ts & 0xFFFFffff))) {
        return FALSE;
    }

    if (dt->rh.type == fs->cls_datetime) {
        if (!stream_write_uint32(w, name_size)) {
            return FALSE;
        }
        if (!stream_write_data(w, name, name_size)) {
            return FALSE;
        }
    }
    return TRUE;
}

static int datetime_parse(Value *vret, Value *v, RefNode *node)
{
    RefStr *src;
    int64_t ts;
    RefDateTime *dt = buf_new(fs->cls_datetime, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    if (fg->stk_top > v + 2) {
        dt->tz = Value_to_tz(v[1], 0);
        if (dt->tz == NULL) {
            return FALSE;
        }
        src = Value_vp(v[2]);
    } else {
        RefNode *v1_type = Value_type(v[1]);
        if (v1_type != fs->cls_str) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, v1_type, 1);
            return FALSE;
        }
        dt->tz = get_local_tz();
        src = Value_vp(v[1]);
    }

    DateTime_init(&dt->d, &dt->t);
    if (!word_parse_datetime(dt, src->c, src->size, PARSE_ABS)) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid time format %Q", src);
        return FALSE;
    }
    ts = DateTime_to_Timestamp(&dt->d, &dt->t);
    // TimeOffsetを推定
    dt->off = TimeZone_offset_local(dt->tz, ts);
    dt->ts = ts - dt->off->offset;
    // タイムスタンプを戻して、もう一度TimeOffsetを計算
    dt->off = TimeZone_offset_utc(dt->tz, dt->ts);
    // もう一度計算
    Timestamp_to_DateTime(&dt->d, &dt->t, dt->ts + dt->off->offset);

    return TRUE;
}
static int datetime_get(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    int ret = 0;

    switch (FUNC_INT(node)) {
    case GET_YEAR:
        ret = dt->d.year;
        break;
    case GET_MONTH:
        ret = dt->d.month;
        break;
    case GET_DAY_OF_YEAR:
        ret = dt->d.day_of_year;
        break;
    case GET_DAY_OF_MONTH:
        ret = dt->d.day_of_month;
        break;
    case GET_DAY_OF_WEEK:
        ret = dt->d.day_of_week;
        break;
    case GET_ISOWEEK:
        ret = dt->d.isoweek;
        break;
    case GET_ISOWEEK_YEAR:
        ret = dt->d.isoweek_year;
        break;
    case GET_HOUR:
        ret = dt->t.hour;
        break;
    case GET_MINUTE:
        ret = dt->t.minute;
        break;
    case GET_SECOND:
        ret = dt->t.second;
        break;
    case GET_MILLISEC:
        ret = dt->t.millisec;
        break;
    }
    *vret = int32_Value(ret);

    return TRUE;
}
static int datetime_timestamp(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    RefInt64 *rt = buf_new(fs->cls_timestamp, sizeof(RefInt64));
    *vret = vp_Value(rt);
    rt->u.i = dt->ts;
    return TRUE;
}
static int datetime_timezone(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    RefTimeZone *tz = dt->tz;
    *vret = Value_cp(vp_Value(tz));
    return TRUE;
}
static int datetime_zoneabbr(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    *vret = cstr_Value(fs->cls_str, dt->off->abbr, -1);
    return TRUE;
}
static int datetime_is_dst(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    *vret = bool_Value(dt->off->is_dst);
    return TRUE;
}
static int datetime_offset(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
        *vret = float_Value(cls_timedelta, dt->off->offset);
    return TRUE;
}
static int datetime_date(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *src = Value_vp(*v);
    RefDateTime *dst = buf_new(cls_date, sizeof(RefDateTime));
    *vret = vp_Value(dst);
    dst->d = src->d;
    dst->t = src->t;
    dst->ts = src->ts;
    return TRUE;
}
static int datetime_tostr(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    const LocaleData *loc = fv->loc_neutral;
    const char *fmt_p;
    int fmt_size;
    StrBuf buf;

    if (fg->stk_top > v + 1) {
        RefStr *rs = Value_vp(v[1]);
        fmt_p = rs->c;
        fmt_size = rs->size;
        if (fg->stk_top > v + 2) {
            loc = Value_locale_data(v[2]);
        }
    } else {
        fmt_p = "yyyy-MM-dd'T'HH:mm:ss.SSSz";
        fmt_size = -1;
    }
    StrBuf_init_refstr(&buf, 0);
    build_datetime_string(&buf, fmt_p, fmt_size, &dt->d, &dt->t, dt->off, loc);
    *vret = StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}
static int datetime_hash(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    int32_t hash = ((dt->ts >> 32) ^ (dt->ts & 0xFFFFffff) ^ (intptr_t)dt->tz) & INT32_MAX;
    *vret = int32_Value(hash);
    return TRUE;
}
static int datetime_eq(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt1 = Value_vp(v[0]);
    RefDateTime *dt2 = Value_vp(v[1]);
    int ret = (dt1->ts == dt2->ts && dt1->tz == dt2->tz);
    *vret = bool_Value(ret);
    return TRUE;
}
static int datetime_add(Value *vret, Value *v, RefNode *node)
{
    int neg = FUNC_INT(node);
    RefDateTime *dt1 = Value_vp(v[0]);
    RefNode *v1_type = Value_type(v[1]);
    RefDateTime *dt2 = buf_new(dt1->rh.type, sizeof(RefDateTime));
    *vret = vp_Value(dt2);

    if (v1_type == fs->cls_str) {
        RefStr *fmt = Value_vp(v[1]);
        dt2->d = dt1->d;
        dt2->t = dt1->t;
        if (!word_parse_datetime(dt2, fmt->c, fmt->size, neg ? PARSE_NEGATIVE : PARSE_POSITIVE)) {
            throw_errorf(fs->mod_lang, "ValueError", "Invalid time format %Q", fmt);
            return FALSE;
        }
        dt2->tz = dt1->tz;
        adjust_timezone(dt2);
    } else if (v1_type == cls_timedelta) {
        double delta = Value_float2(v[1]);
        double ts;
        if (neg) {
            ts = (double)dt1->ts - delta;
        } else {
            ts = (double)dt1->ts + delta;
        }
        if (ts < TIMESTAMP_MIN || ts > TIMESTAMP_MAX) {
            throw_errorf(fs->mod_lang, "ValueError", "Timestamp overfloat");
            return FALSE;
        }
        dt2->ts = (int64_t)ts;
        dt2->tz = dt1->tz;
        dt2->off = TimeZone_offset_local(dt2->tz, dt2->ts);
        Timestamp_to_DateTime(&dt2->d, &dt2->t, dt2->ts + dt2->off->offset);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, cls_timedelta, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int date_new(Value *vret, Value *v, RefNode *node)
{
    int is_isoweek = FUNC_INT(node);
    RefDateTime *dt = buf_new(cls_date, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    dt->d.year = 1;
    dt->d.month = 1;
    dt->d.day_of_month = 1;

    if (!validate_int32_range(v + 1, 3, 1)) {
        return FALSE;
    }

    if (is_isoweek) {
        dt->d.isoweek_year = Value_int32(v[1]);
        dt->d.isoweek = Value_int32(v[2]);
        dt->d.day_of_week = Value_int32(v[3]);
    } else {
        dt->d.year = Value_int32(v[1]);
        dt->d.month = Value_int32(v[2]);
        dt->d.day_of_month = Value_int32(v[3]);
    }
    dt->ts = DateTime_to_Timestamp(&dt->d, NULL);
    Timestamp_to_DateTime(&dt->d, NULL, dt->ts);

    return TRUE;
}
static int date_today(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = buf_new(cls_date, sizeof(RefDateTime));
    RefTimeZone *tz;
    int64_t tm;
    TimeOffset *off;
    *vret = vp_Value(dt);

    if (fg->stk_top > v + 1) {
        tz = Value_to_tz(v[1], 0);
        if (tz == NULL) {
            return FALSE;
        }
    } else {
        tz = get_local_tz();
    }
    tm = get_now_time();
    off = TimeZone_offset_utc(tz, tm);
    dt->ts = (tm + off->offset) / MSECS_PER_DAY * MSECS_PER_DAY;
    Timestamp_to_DateTime(&dt->d, NULL, dt->ts);

    return TRUE;
}
static int date_parse(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(v[1]);
    RefDateTime *dt = buf_new(cls_date, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    DateTime_init(&dt->d, &dt->t);
    if (!word_parse_datetime(dt, src->c, src->size, PARSE_ABS)) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid time format %Q", src);
        return FALSE;
    }
    dt->ts = DateTime_to_Timestamp(&dt->d, NULL);
    Timestamp_to_DateTime(&dt->d, NULL, dt->ts);

    return TRUE;
}
static int date_tostr(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt = Value_vp(*v);
    const LocaleData *loc = fv->loc_neutral;
    const char *fmt_p;
    int fmt_size;
    StrBuf buf;

    if (fg->stk_top > v + 1) {
        RefStr *rs = Value_vp(v[1]);
        fmt_p = rs->c;
        fmt_size = rs->size;
        if (fg->stk_top > v + 2) {
            loc = Value_locale_data(v[2]);
        }
    } else {
        fmt_p = "yyyy-MM-dd";
        fmt_size = -1;
    }
    StrBuf_init_refstr(&buf, 0);
    build_datetime_string(&buf, fmt_p, fmt_size, &dt->d, NULL, NULL, loc);
    *vret = StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}
static int date_sub(Value *vret, Value *v, RefNode *node)
{
    RefNode *v1_type = Value_type(v[1]);

    if (v1_type == cls_date) {
        // Date - Date
        RefDateTime *dt1 = Value_vp(v[0]);
        RefDateTime *dt2 = Value_vp(v[1]);
        *vret = float_Value(cls_timedelta, (double)dt1->ts - (double)dt2->ts);
        return TRUE;
    } else if (v1_type == fs->cls_str || v1_type == cls_timedelta) {
        return datetime_add(vret, v, node);
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "TimeDelta, Date or Str required but %n", v1_type);
        return FALSE;
    }
}
static int date_eq(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt1 = Value_vp(v[0]);
    RefDateTime *dt2 = Value_vp(v[1]);

    *vret = bool_Value(dt1->ts == dt2->ts);
    return TRUE;
}
static int date_cmp(Value *vret, Value *v, RefNode *node)
{
    RefDateTime *dt1 = Value_vp(v[0]);
    RefDateTime *dt2 = Value_vp(v[1]);

    if (dt1->ts < dt2->ts) {
        *vret = int32_Value(-1);
    } else if (dt1->ts > dt2->ts) {
        *vret = int32_Value(1);
    } else {
        *vret = int32_Value(0);
    }
    return TRUE;
}
static int date_incdec(Value *vret, Value *v, RefNode *node)
{
    int inc = FUNC_INT(node);
    RefDateTime *d1 = Value_vp(*v);
    RefDateTime *dt = buf_new(cls_date, sizeof(RefDateTime));
    *vret = vp_Value(dt);

    dt->ts = d1->ts + inc * MSECS_PER_DAY;
    Timestamp_to_DateTime(&dt->d, NULL, dt->ts);
    
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int time_sleep(Value *vret, Value *v, RefNode *node)
{
    int64_t ms;
    RefNode *v1_type = Value_type(v[1]);

    if (v1_type == fs->cls_int) {
        ms = Value_int64(v[1], NULL);
    } else if (v1_type == cls_timedelta) {
        ms = VALUE_INT64(v[1]);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_int, cls_timedelta, v1_type, 1);
        return FALSE;
    }
    if (ms < 0LL) {
        throw_errorf(fs->mod_lang, "ValueError", "Passed negative value");
        return FALSE;
    } else if (ms > INT32_MAX) {
        ms = INT32_MAX;
    }
    native_sleep(ms);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_time_func(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "sleep", NODE_FUNC_N, 0);
    define_native_func_a(n, time_sleep, 1, 1, NULL, NULL);
}

static void define_time_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    // TimeStamp
    cls = fs->cls_timestamp;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, timestamp_new, 0, 7, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "now", NODE_NEW_N, 0);
    define_native_func_a(n, timestamp_now, 0, 0, NULL);
    n = define_identifier(m, cls, "parse_iso", NODE_NEW_N, 0);
    define_native_func_a(n, timestamp_parse_iso, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "parse_rfc", NODE_NEW_N, 0);
    define_native_func_a(n, timestamp_parse_rfc2822, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, int64_marshal_read, 1, 1, fs->cls_timestamp, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, timestamp_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, int64_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, int64_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, int64_eq, 1, 1, NULL, fs->cls_timestamp);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, int64_cmp, 1, 1, NULL, fs->cls_timestamp);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, timestamp_add, 1, 1, (void*)FALSE, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, timestamp_add, 1, 1, (void*)TRUE, NULL);
    n = define_identifier(m, cls, "iso8601", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timestamp_iso8601, 0, 0, (void*)"%04d%02d%02dT%02d%02d%02dZ");
    n = define_identifier(m, cls, "isodate", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timestamp_iso8601, 0, 0, (void*)"%04d-%02d-%02dT%02d:%02d:%02dZ");
    n = define_identifier(m, cls, "httpdate", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timestamp_rfc2822, 0, 0, NULL);
    n = define_identifier(m, cls, "round", NODE_FUNC_N, 0);
    define_native_func_a(n, timestamp_round, 2, 2, NULL, fs->cls_str, fs->cls_str);
    extends_method(cls, fs->cls_obj);

    // TimeDelta
    cls = cls_timedelta;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, timedelta_new, 0, 5, NULL, fs->cls_number, fs->cls_number, fs->cls_number, fs->cls_number, fs->cls_number);
    n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    define_native_func_a(n, timedelta_parse, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, float_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, float_marshal_read, 1, 1, cls_timedelta, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, float_eq, 1, 1, NULL, cls_timedelta);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, float_cmp, 1, 1, NULL, cls_timedelta);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_add, 1, 1, (void*) 1, cls_timedelta);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_add, 1, 1, (void*) -1, cls_timedelta);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_mul, 1, 1, NULL, fs->cls_float);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_div, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_negative, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, int64_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_empty, 0, 0, NULL);
    n = define_identifier(m, cls, "days", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get, 0, 0, (void*) GET_DAY_OF_YEAR);
    n = define_identifier(m, cls, "hours", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get, 0, 0, (void*) GET_HOUR);
    n = define_identifier(m, cls, "minutes", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get, 0, 0, (void*) GET_MINUTE);
    n = define_identifier(m, cls, "seconds", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get, 0, 0, (void*) GET_SECOND);
    n = define_identifier(m, cls, "millisecs", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get, 0, 0, (void*) GET_MILLISEC);
    n = define_identifier(m, cls, "days_float", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get_float, 0, 0, (void*) GET_DAY_OF_YEAR);
    n = define_identifier(m, cls, "seconds_float", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get_float, 0, 0, (void*) GET_SECOND);
    n = define_identifier(m, cls, "milliseconds_float", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, timedelta_get_float, 0, 0, (void*) GET_MILLISEC);
    n = define_identifier(m, cls, "round", NODE_FUNC_N, 0);
    define_native_func_a(n, timedelta_round, 2, 2, NULL, fs->cls_str, fs->cls_str);
    extends_method(cls, fs->cls_obj);

    // TimeZone
    cls = fs->cls_timezone;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, timezone_new, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, timezone_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, timezone_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, object_eq, 1, 1, NULL, fs->cls_timezone);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, timezone_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier(m, cls, "UTC", NODE_CONST, 0);
    n->u.k.val = Value_cp(vp_Value(fs->tz_utc));
    n = define_identifier(m, cls, "LOCAL", NODE_CONST_U_N, 0);
    define_native_func_a(n, timezone_local, 0, 0, NULL);

    extends_method(cls, fs->cls_obj);

    // DateTime
    // TimeStampとTimeZoneを保持
    cls = fs->cls_datetime;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, datetime_new, 0, 8, (void*)FALSE, NULL, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "from_isoweek", NODE_NEW_N, 0);
    define_native_func_a(n, datetime_new, 0, 8, (void*)TRUE, NULL, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "now", NODE_NEW_N, 0);
    define_native_func_a(n, datetime_now, 0, 1, NULL, NULL);
    n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    define_native_func_a(n, datetime_parse, 1, 2, NULL, NULL, fs->cls_str);
    n = define_identifier(m, cls, "parse_format", NODE_NEW_N, 0);
    define_native_func_a(n, datetime_parse_format, 2, 3, NULL, fs->cls_str, fs->cls_str, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, datetime_marshal_read, 1, 1, fs->cls_datetime, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_add, 1, 1, (void*) FALSE, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_add, 1, 1, (void*) TRUE, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_eq, 1, 1, NULL, fs->cls_datetime);
    n = define_identifier(m, cls, "year", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_YEAR);
    n = define_identifier(m, cls, "month", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_MONTH);
    n = define_identifier(m, cls, "day_of_year", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_DAY_OF_YEAR);
    n = define_identifier(m, cls, "day_of_month", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_DAY_OF_MONTH);
    n = define_identifier(m, cls, "day_of_week", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_DAY_OF_WEEK);
    n = define_identifier(m, cls, "isoweek", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_ISOWEEK);
    n = define_identifier(m, cls, "isoweek_year", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_ISOWEEK_YEAR);
    n = define_identifier(m, cls, "hour", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_HOUR);
    n = define_identifier(m, cls, "minute", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_MINUTE);
    n = define_identifier(m, cls, "second", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_SECOND);
    n = define_identifier(m, cls, "millisecond", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_MILLISEC);

    n = define_identifier(m, cls, "timestamp", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_timestamp, 0, 0, NULL);
    n = define_identifier(m, cls, "timezone", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_timezone, 0, 0, NULL);
    n = define_identifier(m, cls, "zone_abbr", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_zoneabbr, 0, 0, NULL);
    n = define_identifier(m, cls, "is_dst", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_is_dst, 0, 0, NULL);
    n = define_identifier(m, cls, "offset", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_offset, 0, 0, NULL);
    n = define_identifier(m, cls, "date", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_date, 0, 0, NULL);
    extends_method(cls, fs->cls_obj);


    // Date
    // 日付のみを保持
    cls = cls_date;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, date_new, 3, 3, (void*)FALSE, fs->cls_int, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "from_isoweek", NODE_NEW_N, 0);
    define_native_func_a(n, date_new, 3, 3, (void*)TRUE, fs->cls_int, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "today", NODE_NEW_N, 0);
    define_native_func_a(n, date_today, 0, 1, NULL, NULL);
    n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    define_native_func_a(n, date_parse, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, datetime_marshal_read, 1, 1, cls_date, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, date_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, datetime_add, 1, 1, (void*) FALSE, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, date_sub, 1, 1, (void*) TRUE, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, date_eq, 1, 1, NULL, cls_date);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, date_cmp, 1, 1, NULL, cls_date);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_INC], NODE_FUNC_N, 0);
    define_native_func_a(n, date_incdec, 0, 0, (void*)1);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DEC], NODE_FUNC_N, 0);
    define_native_func_a(n, date_incdec, 0, 0, (void*)-1);
    n = define_identifier(m, cls, "year", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_YEAR);
    n = define_identifier(m, cls, "month", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_MONTH);
    n = define_identifier(m, cls, "day_of_year", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_DAY_OF_YEAR);
    n = define_identifier(m, cls, "day_of_month", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_DAY_OF_MONTH);
    n = define_identifier(m, cls, "day_of_week", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_DAY_OF_WEEK);
    n = define_identifier(m, cls, "isoweek", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_ISOWEEK);
    n = define_identifier(m, cls, "isoweek_year", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, datetime_get, 0, 0, (void*) GET_ISOWEEK_YEAR);
    extends_method(cls, fs->cls_obj);


    cls = define_identifier(m, m, "TimeRangeError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
}

RefNode *init_time_module_stubs()
{
    RefNode *m = Module_new_sys("time");

    fs->cls_timestamp = define_identifier(m, m, "TimeStamp", NODE_CLASS, 0);
    fs->cls_datetime = define_identifier(m, m, "DateTime", NODE_CLASS, 0);
    fs->cls_timezone = define_identifier(m, m, "TimeZone", NODE_CLASS, 0);
    cls_timedelta = define_identifier(m, m, "TimeDelta", NODE_CLASS, 0);
    cls_date = define_identifier(m, m, "Date", NODE_CLASS, 0);

    fs->tz_utc = load_timezone("Etc/UTC", -1);

    return m;
}
void init_time_module_1(RefNode *m)
{
    define_time_func(m);
    define_time_class(m);

    m->u.m.loaded = TRUE;
}
