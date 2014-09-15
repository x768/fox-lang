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
	GET_HOUR,
	GET_MINUTE,
	GET_SECOND,
	GET_MILLISEC,
	GET_INDEX_MAX,
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
} RefTimeZoneAbbrName;

static RefNode *cls_timedelta;

#define VALUE_INT64(v) (((RefInt64*)(intptr_t)(v))->u.i)

////////////////////////////////////////////////////////////////////////////////////////////////////

void time_to_RFC2822_UTC(int64_t tm, char *dst)
{
	Calendar cal;
	Time_to_Calendar(&cal, tm);

	sprintf(dst, "%s, %02d %s %d %02d:%02d:%02d +0000",
			time_week_str[cal.day_of_week - 1], cal.day_of_month, time_month_str[cal.month - 1], cal.year,
			cal.hour, cal.minute, cal.second);
}
void time_to_cookie_date(int64_t tm, char *dst)
{
	Calendar cal;
	Time_to_Calendar(&cal, tm);

	sprintf(dst, "%s, %02d-%s-%d %02d:%02d:%02d GMT",
			time_week_str[cal.day_of_week - 1], cal.day_of_month, time_month_str[cal.month - 1], cal.year,
			cal.hour, cal.minute, cal.second);
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
void adjust_timezone(RefTime *dt)
{
	int64_t i_tm;

	Calendar_to_Time(&i_tm, &dt->cal);
	// TimeOffsetを推定
	dt->off = TimeZone_offset_local(dt->tz, i_tm);
	dt->tm = i_tm - dt->off->offset;
	// タイムスタンプを戻して、もう一度TimeOffsetを計算
	dt->off = TimeZone_offset_utc(dt->tz, dt->tm);
	// もう一度計算
	Time_to_Calendar(&dt->cal, dt->tm + dt->off->offset);
}
/**
 * 指定したタイムスタンプとRefTimeZoneから、Calendarを決定
 */
void adjust_date(RefTime *dt)
{
	dt->off = TimeZone_offset_local(dt->tz, dt->tm);
	Time_to_Calendar(&dt->cal, dt->tm + dt->off->offset);
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
				throw_errorf(fs->mod_lang, "ValueError", "RefTimeZone %Q not found", name);
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

	if (type == fs->cls_time) {
		RefTime *dt = Value_vp(v);
		return dt->tm + dt->off->offset;
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
 * return: FOX_TZで設定したRefTimeZoneを持つDate
 */
Value time_Value(int64_t i_tm, RefTimeZone *tz)
{
	RefTime *dt = new_buf(fs->cls_time, sizeof(RefTime));

	dt->tz = (tz != NULL ? tz : get_local_tz());
	dt->tm = i_tm;
	dt->off = TimeZone_offset_local(dt->tz, i_tm);
	Time_to_Calendar(&dt->cal, i_tm + dt->off->offset);

	return vp_Value(dt);
}

static void Calendar_init(Calendar *c)
{
	c->year = 1;
	c->month = 1;
	c->day_of_month = 1;
	c->hour = 0;
	c->minute = 0;
	c->second = 0;
	c->millisec = 0;
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

	Value r = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	RefNode *cls = FUNC_VP(node);
	RefInt64 *rt = new_buf(cls, sizeof(RefInt64));
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
	Value w = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
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
	Calendar cal;
	RefInt64 *rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
	*vret = vp_Value(rt);

	for (i = 1; v + i < fg->stk_top; i++) {
		num[i - 1] = Value_int(v[i], NULL);
	}
	cal.year = num[0];
	cal.month = num[1];
	cal.day_of_month = num[2];
	cal.hour = num[3];
	cal.minute = num[4];
	cal.second = num[5];
	cal.millisec = num[6];
	Calendar_to_Time(&rt->u.i, &cal);

	return TRUE;
}
static int timestamp_now(Value *vret, Value *v, RefNode *node)
{
	RefInt64 *rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
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
	*ret = parse_int(Str_new(top, p - top), 99999999);

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
static int parse_iso_sub(Calendar *cal, Str src)
{
	const char *p = src.p;
	const char *end = p + src.size;
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

	if (!parse_digits_sub(&cal->year, &p, end, year_n)) {
		return FALSE;
	}
	if (p < end && (*p == 'w' || *p == 'W')) {
		p++;
		// TODO ISO週番号
	} else {
		if (p < end && *p == '-') {
			p++;
		}
		if (!parse_digits_sub(&cal->month, &p, end, 2)) {
			return FALSE;
		}
		if (p < end && *p == '-') {
			p++;
		}
		if (!parse_digits_sub(&cal->day_of_month, &p, end, 2)) {
			return FALSE;
		}
	}

	if (neg) {
		cal->year = -cal->year;
	}

	while (p < end && (isspace(*p) || *p == 'T')) {
		p++;
	}

	cal->millisec = 0;

	if (!parse_digits_sub(&cal->hour, &p, end, 2)) {
		cal->hour = 0;
		cal->minute = 0;
		cal->second = 0;
		return TRUE;
	}
	if (p < end && *p == ':') {
		p++;
	}
	if (!parse_digits_sub(&cal->minute, &p, end, 2)) {
		return FALSE;
	}
	if (p < end && *p == ':') {
		p++;
	}
	if (!parse_digits_sub(&cal->second, &p, end, 2)) {
		return FALSE;
	}

	return TRUE;
}
/**
 * RFC2822
 * [day-of-week, ","] date month year hour ":" minute [":" second [("," | ".") millisec]] [("+" | "-") zone]
 */
static int parse_rfc_sub(Calendar *cal, Str src)
{
	static RefTimeZoneAbbrName us_timezone[] = {
		{"est", -5},
		{"edt", -4},
		{"cst", -6},
		{"cdt", -5},
		{"mst", -7},
		{"mdt", -6},
		{"pst", -8},
		{"pdt", -7},
	};
	const char *p = src.p;
	const char *end = p + src.size;
	int pm = FALSE;
	int zone_offset = 0;
	int offset_done = FALSE;

	cal->year = 1;
	cal->month = 1;
	cal->day_of_month = 1;
	cal->hour = 0;
	cal->minute = 0;
	cal->second = 0;
	cal->millisec = 0;

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
					cal->month = i_month + 1;
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
					cal->year = parse_int(s, 99999999);
				} else {
					cal->day_of_month = parse_int(s, 99);
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
				cal->hour = hour;
				cal->minute = minute;
				cal->second = second;
			}
		}
	}
	cal->minute -= zone_offset;
	if (pm) {
		cal->hour += 12;
	}

	return TRUE;
}

/**
 * yyyy-MM-dd hh:mm:ss
 */
static int timestamp_parse_iso(Value *vret, Value *v, RefNode *node)
{
	Calendar cal;
	Str src = Value_str(v[1]);
	RefInt64 *rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
	*vret = vp_Value(rt);

	if (!parse_iso_sub(&cal, src)) {
		throw_errorf(fs->mod_lang, "ParseError", "Invalid Time format %Q", src);
		return FALSE;
	}
	Calendar_to_Time(&rt->u.i, &cal);

	return TRUE;
}
static int timestamp_parse_rfc2822(Value *vret, Value *v, RefNode *node)
{
	Calendar cal;
	Str src = Value_str(v[1]);
	RefInt64 *rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
	*vret = vp_Value(rt);

	if (!parse_rfc_sub(&cal, src)) {
		throw_errorf(fs->mod_lang, "ParseError", "Invalid Time format %Q", src);
		return FALSE;
	}
	Calendar_to_Time(&rt->u.i, &cal);

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

static int adjust_time_sub(RefTime *dt, int *dst, int num, int *type, int parse_type, int adjust_month)
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
			Calendar_adjust_month(&dt->cal);
		} else {
			Calendar_adjust(&dt->cal);
		}
	}
	*type = PARSETYPE_NONE;
	return TRUE;
}
static int parse_month_name(Calendar *cal, Str src)
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
			cal->month = i + 1;
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
static int word_parse_time(RefTime *dt, Str src, int parse_type)
{
	int i;
	int type = PARSETYPE_NONE;
	int num = 0;

	for (i = 0; i < src.size; ) {
		int pos;
		Str s;
		while (i < src.size && isspace_fox(src.p[i])) {
			i++;
		}
		pos = i;
		while (i < src.size && !isspace_fox(src.p[i])) {
			i++;
		}
		s = Str_new(src.p + pos, i - pos);
		if (s.size == 0) {
			i++;
			continue;
		}
		if (Str_eqi_list(s, "y", "year", "years", NULL)) {
			if (parse_type == PARSE_DELTA) {
				// 1年のTimeDeltaは正確に決められない
				return FALSE;
			}
			if (!adjust_time_sub(dt, &dt->cal.year, num, &type, parse_type, TRUE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "a", "aj", "jy", "julian-year", "annum", NULL)) {
			if (parse_type != PARSE_DELTA) {
				// ユリウス年はTimeDeltaのみ使用可能
				return FALSE;
			}
			if (!adjust_time_sub(dt, &dt->cal.hour, num * 8766, &type, parse_type, FALSE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "mon", "month", "months", NULL)) {
			if (parse_type == PARSE_DELTA) {
				// 1ヶ月のTimeDeltaは正確に決められない
				return FALSE;
			}
			if (!adjust_time_sub(dt, &dt->cal.month, num, &type, parse_type, TRUE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "w", "week", "weeks", NULL)) {
			switch (type) {
			case PARSETYPE_OFFSET:
				dt->cal.day_of_month += num * 7;
				if (parse_type != PARSE_DELTA) {
					Calendar_adjust(&dt->cal);
				}
				break;
			case PARSETYPE_NUM:
				Calendar_set_isoweek(&dt->cal, dt->cal.year, num);
				break;
			default:
				return FALSE;
			}
			type = PARSETYPE_NONE;
		} else if (Str_eqi_list(s, "d", "day", "days", NULL)) {
			if (!adjust_time_sub(dt, &dt->cal.day_of_month, num, &type, parse_type, FALSE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "h", "hour", "hours", NULL)) {
			if (!adjust_time_sub(dt, &dt->cal.hour, num, &type, parse_type, FALSE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "min", "minute", "minutes", NULL)) {
			if (!adjust_time_sub(dt, &dt->cal.minute, num, &type, parse_type, FALSE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "s", "sec", "second", "seconds", NULL)) {
			if (!adjust_time_sub(dt, &dt->cal.second, num, &type, parse_type, FALSE)) {
				return FALSE;
			}
		} else if (Str_eqi_list(s, "ms", "msec", "millisecond", "milliseconds", NULL)) {
			if (!adjust_time_sub(dt, &dt->cal.millisec, num, &type, parse_type, FALSE)) {
				return FALSE;
			}
		} else if (s.p[0] == '+' || s.p[0] == '-') {
			int n;
			if (type != PARSETYPE_NONE) {
				return FALSE;
			}

			n = parse_int(Str_new(s.p + 1, s.size - 1), INT_MAX);
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
				num = parse_int(s, INT_MAX);
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

			dt->tm = get_now_time();
			dt->off = TimeZone_offset_local(dt->tz, dt->tm);
			Time_to_Calendar(&dt->cal, dt->tm + dt->off->offset);
			if (!str_eqi(s.p, s.size, "now", 3)) {
				dt->cal.hour = 0;
				dt->cal.minute = 0;
				dt->cal.second = 0;
				dt->cal.millisec = 0;
				if (!str_eqi(s.p, s.size, "today", -1)) {
					dt->cal.day_of_month = 1;
					if (!str_eqi(s.p, s.size, "this-month", -1)) {
						dt->cal.month = 1;
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
					dt->cal.year = n1;
				}
				dt->cal.month = n2;
				dt->cal.day_of_month = n3;
				type = PARSETYPE_NONE;
			} else if (word_parse_hms(s, &n1, &n2, &n3)) {
				dt->cal.hour = n1;
				dt->cal.minute = n2;
				dt->cal.second = n3;
				type = PARSETYPE_NONE;
			} else if (parse_month_name(&dt->cal, s)) {
				type = PARSETYPE_NONE;
			} else if (isdigit(s.p[0])){
				type = PARSETYPE_NUM;
				num = parse_int(s, INT_MAX);
			} else {
				return FALSE;
			}
		}
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int timestamp_iso8601(Value *vret, Value *v, RefNode *node);

/**
 * .NETのフォーマットに近い
 */
static void parse_time_format(StrBuf *buf, const char *fmt_p, int fmt_size, const Calendar *cal, const TimeOffset *off, const RefLocale *loc)
{
	int i = 0;
	int end;
	int n;
	char c_buf[16];

	if (fmt_size < 0) {
		fmt_size = strlen(fmt_p);
	}

	while (i < fmt_size) {
		switch (fmt_p[i]) {
		case 'y':  // yy yyyy
			n = 0;
			end = i + 4;
			if (end > fmt_size) {
				end = fmt_size;
			}
			do {
				i++;
				n++;
			} while (i < end && fmt_p[i] == 'y');

			switch (n) {
			case 1:
				sprintf(c_buf, "%d", cal->year);
				break;
			case 2: {
				int y = cal->year;
				if (y < 0) {
					y = y % 100;
					if (y < 0) {
						y += 100;
					}
					sprintf(c_buf, "%02d", y);
				} else {
					sprintf(c_buf, "%02d", y % 100);
				}
				break;
			}
			default:
				if (cal->year < 0) {
					sprintf(c_buf, "-%04d", -cal->year);
				} else {
					sprintf(c_buf, "%04d", cal->year);
				}
				break;
			}
			StrBuf_add(buf, c_buf, -1);
			break;
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
				sprintf(c_buf, "%d", cal->month);
				StrBuf_add(buf, c_buf, -1);
				break;
			case 2:
				sprintf(c_buf, "%02d", cal->month);
				StrBuf_add(buf, c_buf, -1);
				break;
			case 3:
				{
					const char *s = loc->month[cal->month - 1];
					if (s != NULL) {
						StrBuf_add(buf, s, -1);
					} else {
						sprintf(c_buf, "%02d", cal->month);
						StrBuf_add(buf, c_buf, -1);
					}
				}
				break;
			case 4:
				{
					const char *s = loc->month_w[cal->month - 1];
					if (s != NULL) {
						StrBuf_add(buf, s, -1);
					} else {
						sprintf(c_buf, "%02d", cal->month);
						StrBuf_add(buf, c_buf, -1);
					}
				}
				break;
			}
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
				sprintf(c_buf, "%d", cal->day_of_month);
				StrBuf_add(buf, c_buf, -1);
				break;
			case 2:
				sprintf(c_buf, "%02d", cal->day_of_month);
				StrBuf_add(buf, c_buf, -1);
				break;
			case 3: {
				const char *s = loc->week[cal->day_of_week - 1];
				if (s != NULL) {
					StrBuf_add(buf, s, -1);
				}
				break;
			}
			case 4: {
				const char *s = loc->week_w[cal->day_of_week - 1];
				if (s != NULL) {
					StrBuf_add(buf, s, -1);
				}
				break;
			}
			}
			break;
		case 'H':  // H HH (24時間)
			i++;
			if (i < fmt_size && fmt_p[i] == 'H') {
				i++;
				sprintf(c_buf, "%02d", cal->hour);
			} else {
				sprintf(c_buf, "%d", cal->hour);
			}
			StrBuf_add(buf, c_buf, -1);
			break;
		case 'h': { // h hh (12時間)
			int hour = cal->hour % 12;
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
			break;
		}
		case 'a': { // a AM,PM
			const char *s = loc->am_pm[cal->hour < 12 ? 0 : 1];
			if (s != NULL) {
				StrBuf_add(buf, s, -1);
			}
			i++;
			break;
		}
		case 'm':  // m mm 分
			i++;
			if (i < fmt_size && fmt_p[i] == 'm') {
				i++;
				sprintf(c_buf, "%02d", cal->minute);
			} else {
				sprintf(c_buf, "%d", cal->minute);
			}
			StrBuf_add(buf, c_buf, -1);
			break;
		case 's':  // s ss 秒
			i++;
			if (i < fmt_size && fmt_p[i] == 's') {
				i++;
				sprintf(c_buf, "%02d", cal->second);
			} else {
				sprintf(c_buf, "%d", cal->second);
			}
			StrBuf_add(buf, c_buf, -1);
			break;
		case 'S':  // ミリ秒
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
				sprintf(c_buf, "%d", cal->millisec / 100);
				StrBuf_add(buf, c_buf, -1);
				break;
			case 2:
				sprintf(c_buf, "%02d", cal->millisec / 10);
				StrBuf_add(buf, c_buf, -1);
				break;
			case 3:
				sprintf(c_buf, "%03d", cal->millisec);
				StrBuf_add(buf, c_buf, -1);
				break;
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

			parse_time_format(buf, loc->date[n - 1], -1, cal, off, loc);
			break;
		case 'x':  // 既定の時刻
			n = 0;
			end = i + 4;
			if (end > fmt_size) {
				end = fmt_size;
			}
			do {
				i++;
				n++;
			} while (i < end && fmt_p[i] == 'x');

			parse_time_format(buf, loc->time[n - 1], -1, cal, off, loc);
			break;
		case 'z': { // 時差(+00:00)
			int offset = off->offset;
			int sign = '+';
			if (offset < 0) {
				offset = -offset;
				sign = '-';
			}
			sprintf(c_buf, "%c%02d:%02d", sign, offset / 3600000, (offset / 60000) % 60);
			StrBuf_add(buf, c_buf, -1);
			i++;
			break;
		}
		case 'Z':  // タイムゾーン
			StrBuf_add_r(buf, off->name);
			i++;
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
			StrBuf_add_c(buf, fmt_p[i]);
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
		int64_t tm2 = VALUE_INT64(v[1]);
		RefInt64 *rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
		*vret = vp_Value(rt);
		if (neg) {
			rt->u.i = tm1 - tm2;
		} else {
			rt->u.i = tm1 + tm2;
		}
	} else if (v1_type == fs->cls_str) {
		RefTime dt;
		RefInt64 *rt;
		Str str = Value_str(v[1]);

		Calendar_init(&dt.cal);
		dt.off = 0;
		dt.tz = fs->tz_utc;
		Time_to_Calendar(&dt.cal, tm1);

		if (!word_parse_time(&dt, str, neg ? PARSE_NEGATIVE : PARSE_POSITIVE)) {
			throw_errorf(fs->mod_lang, "ParseError", "Invalid time format");
			return FALSE;
		}
		rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
		*vret = vp_Value(rt);
		adjust_timezone(&dt);
		rt->u.i = dt.tm;
	} else if (!neg) {
		throw_errorf(fs->mod_lang, "TypeError", "TimeDelta or Str required but %n", v1_type);
		return FALSE;
	} else if (v1_type == fs->cls_timestamp) {
		int64_t tm2 = VALUE_INT64(v[1]);
		RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
		*vret = vp_Value(rt);
		rt->u.i = tm1 - tm2;
	} else {
		throw_errorf(fs->mod_lang, "TypeError", "TimeDelta, TimeStamp or Str required but %n", v1_type);
		return FALSE;
	}
	return TRUE;
}
static int timestamp_iso8601(Value *vret, Value *v, RefNode *node)
{
	RefInt64 *rt = Value_vp(*v);
	Calendar cal;
	char c_buf[48];

	Time_to_Calendar(&cal, rt->u.i);
	sprintf(c_buf, "%04d-%02d-%02dT%02d:%02d:%02dZ",
			cal.year, cal.month, cal.day_of_month,
			cal.hour, cal.minute, cal.second);

	*vret = cstr_Value(fs->cls_str, c_buf, -1);

	return TRUE;
}
static int timestamp_rfc2822(Value *vret, Value *v, RefNode *node)
{
	RefInt64 *rt = Value_vp(*v);
	Calendar cal;
	char c_buf[48];

	Time_to_Calendar(&cal, rt->u.i);
	sprintf(c_buf, "%s, %02d %s %d %02d:%02d:%02d GMT",
			time_week_str[cal.day_of_week - 1], cal.day_of_month, time_month_str[cal.month - 1], cal.year,
			cal.hour, cal.minute, cal.second);

	*vret = cstr_Value(fs->cls_str, c_buf, -1);

	return TRUE;
}
static int timestamp_tostr(Value *vret, Value *v, RefNode *node)
{
	if (fg->stk_top > v + 1) {
		Str fmt = Value_str(v[1]);
		if (str_eqi(fmt.p, fmt.size, "iso8601", -1)) {
			return timestamp_iso8601(vret, v, node);
		} else if (str_eqi(fmt.p, fmt.size, "rfc2822", -1)) {
			return timestamp_rfc2822(vret, v, node);
		} else {
			throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fmt);
			return FALSE;
		}
	} else {
		RefInt64 *rt = Value_vp(*v);
		Calendar cal;
		char c_buf[48];

		Time_to_Calendar(&cal, rt->u.i);
		sprintf(c_buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
				cal.year, cal.month, cal.day_of_month,
				cal.hour, cal.minute, cal.second, cal.millisec);

		*vret = cstr_Value(fs->cls_str, c_buf, -1);
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int timedelta_new(Value *vret, Value *v, RefNode *node)
{
	int64_t num[] = {
		0, 0, 0, 0, 0,
	};
	int i;
	RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
	*vret = vp_Value(rt);

	for (i = 1; v + i < fg->stk_top; i++) {
		num[i - 1] = Value_int(v[i], NULL);
	}
	rt->u.i = num[0] * MSECS_PER_DAY + num[1] * MSECS_PER_HOUR + num[2] * MSECS_PER_MINUTE + num[3] * MSECS_PER_SECOND + num[4];

	return TRUE;
}

static int word_parse_time(RefTime *dt, Str src, int parse_type);

int timedelta_parse_string(int64_t *ret, Str src)
{
	RefTime dt;
	int64_t tm;

	Calendar_init(&dt.cal);
	dt.cal.day_of_month = 0;
	if (!word_parse_time(&dt, src, PARSE_DELTA)) {
		return FALSE;
	}

	tm = (int64_t)dt.cal.day_of_month * MSECS_PER_DAY + (int64_t)dt.cal.hour * MSECS_PER_HOUR;
	tm += dt.cal.minute * MSECS_PER_MINUTE + dt.cal.second * MSECS_PER_SECOND + dt.cal.millisec;
	*ret = tm;

	return TRUE;
}
static int timedelta_parse(Value *vret, Value *v, RefNode *node)
{
	Str fmt = Value_str(v[1]);
	RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
	*vret = vp_Value(rt);

	if (!timedelta_parse_string(&rt->u.i, fmt)) {
		throw_errorf(fs->mod_lang, "ParseError", "Invalid time format %Q", fmt);
		return FALSE;
	}

	return TRUE;
}
static int timedelta_empty(Value *vret, Value *v, RefNode *node)
{
	RefInt64 *rt = Value_vp(*v);
	*vret = bool_Value(rt->u.i == 0LL);
	return TRUE;
}
static int timedelta_negative(Value *vret, Value *v, RefNode *node)
{
	RefInt64 *src = Value_vp(*v);
	RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
	*vret = vp_Value(rt);
	rt->u.i = -src->u.i;
	return TRUE;
}
static int timedelta_add(Value *vret, Value *v, RefNode *node)
{
	int sign = FUNC_INT(node);
	int64_t r1 = VALUE_INT64(*v);
	int64_t r2 = VALUE_INT64(v[1]);

	RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
	*vret = vp_Value(rt);
	rt->u.i = r1 + r2 * sign;

	return TRUE;
}
static int timedelta_mul(Value *vret, Value *v, RefNode *node)
{
	int64_t r1 = VALUE_INT64(*v);
	double d2 = Value_float(v[1]);
	RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
	*vret = vp_Value(rt);
	rt->u.i = (int64_t)((double)r1 * d2);

	return TRUE;
}
static int timedelta_div(Value *vret, Value *v, RefNode *node)
{
	int64_t r1 = VALUE_INT64(*v);
	RefNode *v1_type = Value_type(v[1]);

	if (v1_type == fs->cls_float) {
		RefInt64 *rt;
		double d2 = Value_float(v[1]);
		double d = r1 / d2;

		if (isinf(d)) {
			throw_errorf(fs->mod_lang, "ZeroDivisionError", "TimeDelta / 0.0");
			return FALSE;
		}
		rt = new_buf(cls_timedelta, sizeof(RefInt64));
		*vret = vp_Value(rt);
		rt->u.i = (int64_t)d;
	} else if (v1_type == cls_timedelta) {
		RefFloat *rd;
		int64_t r2 = VALUE_INT64(v[1]);
		rd = new_buf(fs->cls_float, sizeof(RefFloat));
		*vret = vp_Value(rd);
		rd->d = (double)r1 / (double)r2;
	} else {
		throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_float, cls_timedelta, v1_type, 1);
		return FALSE;
	}

	return TRUE;
}
static int timedelta_get(Value *vret, Value *v, RefNode *node)
{
	int64_t ret = VALUE_INT64(*v);

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
	int64_t r1 = VALUE_INT64(*v);
	double ret = (double)r1;
	RefFloat *rd = new_buf(fs->cls_float, sizeof(RefFloat));
	*vret = vp_Value(rd);

	switch (FUNC_INT(node)) {
	case GET_DAY_OF_YEAR:
		ret /= (double)MSECS_PER_DAY;
		break;
	case GET_SECOND:
		ret /= (double)MSECS_PER_SECOND;
		break;
	}
	rd->d = ret;

	return TRUE;
}

static int format_get_leading_chars(Str s, int *pi, int ch, int max)
{
	int i;
	int init = *pi;
	int end = init + max;

	for (i = *pi; i < end && i < s.size; i++) {
		if (s.p[i] != ch) {
			break;
		}
	}
	*pi = i;
	return i - init;
}
static void timedelta_tostr_sub(StrBuf *sbuf, Str fmt, int64_t diff_src)
{
	enum {
		MAX_DIGIT = 32,
	};

	int day, hour, minute, second, millisec;
	int sign = 1;
	int width;
	int64_t diff;
	char cbuf[48];
	int i = 0;

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

	while (i < fmt.size) {
		switch (fmt.p[i]) {
		case '+':
			StrBuf_add_c(sbuf, sign < 0 ? '-' : '+');
			i++;
			break;
		case '-':
			StrBuf_add_c(sbuf, sign < 0 ? '-' : ' ');
			i++;
			break;
		case 'd':
			width = format_get_leading_chars(fmt, &i, 'd', MAX_DIGIT);
			sprintf(cbuf, "%0*d", width, day);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 'h':
			width = format_get_leading_chars(fmt, &i, 'h', 2);
			sprintf(cbuf, "%0*d", width, hour);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 'm':
			width = format_get_leading_chars(fmt, &i, 'm', 2);
			sprintf(cbuf, "%0*d", width, minute);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 's':
			width = format_get_leading_chars(fmt, &i, 's', 2);
			sprintf(cbuf, "%0*d", width, second);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 't':
			width = format_get_leading_chars(fmt, &i, 't', 3);
			switch (width) {
			case 1:
				sprintf(cbuf, "%d", millisec / 100);
				break;
			case 2:
				sprintf(cbuf, "%02d", millisec / 10);
				break;
			case 3:
				sprintf(cbuf, "%03d", millisec);
				break;
			}
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 'H':
			width = format_get_leading_chars(fmt, &i, 'H', MAX_DIGIT);
			sprintf(cbuf, "%0*" FMT_INT64_PREFIX "d", width, diff_src / 3600000LL);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 'M':
			width = format_get_leading_chars(fmt, &i, 'M', MAX_DIGIT);
			sprintf(cbuf, "%0*" FMT_INT64_PREFIX "d", width, diff_src / 60000LL);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 'S':
			width = format_get_leading_chars(fmt, &i, 'S', MAX_DIGIT);
			sprintf(cbuf, "%0*" FMT_INT64_PREFIX "d", width, diff_src / 1000LL);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case 'T':
			width = format_get_leading_chars(fmt, &i, 'T', MAX_DIGIT);
			sprintf(cbuf, "%0*" FMT_INT64_PREFIX "d", width, (long long int)diff_src);
			StrBuf_add(sbuf, cbuf, -1);
			break;
		case '\'': // '任意の文字列'
			i++;
			if (i < fmt.size) {
				if (fmt.p[i] == '\'') {
					StrBuf_add_c(sbuf, '\'');
					i++;
				} else {
					StrBuf_add_c(sbuf, fmt.p[i]);
					i++;
					while (i < fmt.size) {
						char c = fmt.p[i];
						if (c == '\'') {
							i++;
							if (i < fmt.size) {
								if (fmt.p[i] == '\'') {
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
			StrBuf_add_c(sbuf, fmt.p[i]);
			i++;
			break;
		}
	}
}
static int timedelta_tostr(Value *vret, Value *v, RefNode *node)
{
	int64_t diff = VALUE_INT64(*v);
	StrBuf buf;
	Str fmt;

	if (fg->stk_top > v + 1) {
		fmt = Value_str(v[1]);
	} else {
		fmt = Str_new("'TimeDelta'(+d, hh:mm:ss.ttt)", -1);
	}

	StrBuf_init_refstr(&buf, 0);
	timedelta_tostr_sub(&buf, fmt, diff);
	*vret = StrBuf_str_Value(&buf, fs->cls_str);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int timezone_new(Value *vret, Value *v, RefNode *node)
{
	RefTimeZone *tz = Value_to_tz(v[1], 0);

	if (tz == NULL) {
		return FALSE;
	}
	*vret = ref_cp_Value(&tz->rh);

	return TRUE;
}
static RefTimeZone *timezone_marshal_read_sub(Value r)
{
	RefTimeZone *tz;
	uint32_t size;
	int rd_size;
	char cbuf[64];

	if (!read_int32(&size, r)) {
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
	Value r = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	RefTimeZone *tz = timezone_marshal_read_sub(r);

	if (tz == NULL) {
		return FALSE;
	}
	*vret = ref_cp_Value(&tz->rh);

	return TRUE;
}
static int timezone_marshal_write(Value *vret, Value *v, RefNode *node)
{
	Value w = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	RefTimeZone *tz = Value_vp(*v);
	RefStr *name = tz->name;

	if (!write_int32(name->size, w)) {
		return FALSE;
	}
	if (!stream_write_data(w, name->c, name->size)) {
		return FALSE;
	}

	return TRUE;
}
static int timezone_tostr(Value *vret, Value *v, RefNode *node)
{
	RefTimeZone *tz = Value_vp(*v);
	Str fmt = fs->Str_EMPTY;

	if (fg->stk_top > v + 1) {
		fmt = Value_str(v[1]);
	}
	if (fmt.size == 0) {
		*vret = printf_Value("TimeZone(%r)", tz->name);
	} else if (fmt.p[0] == 'N' || fmt.p[0] == 'n') {
		*vret = ref_cp_Value(&tz->name->rh);
	} else {
		throw_errorf(fs->mod_lang, "FormatError", "Unknown format string %Q", fmt);
		return FALSE;
	}
	return TRUE;
}
static int timezone_local(Value *vret, Value *v, RefNode *node)
{
	*vret = ref_cp_Value(&get_local_tz()->rh);
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int date_new(Value *vret, Value *v, RefNode *node)
{
	Value *va;
	Calendar cal;
	int argc = fg->stk_top - v - 1;
	RefTime *dt = new_buf(fs->cls_time, sizeof(RefTime));
	*vret = vp_Value(dt);

	// RefTimeZoneがあれば、処理する
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
				throw_errorf(fs->mod_lang, "ValueError", "RefTimeZone %q not found", name->c);
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

	Calendar_init(&cal);

	if (argc > 0) {
		RefNode *v1_type = Value_type(va[0]);
		if (v1_type == fs->cls_timestamp) {
			int64_t i_tm;
			if (argc > 1) {
				throw_errorf(fs->mod_lang, "ArgumentError", "Too many arguments (%d given)", argc);
				return FALSE;
			}
			i_tm = VALUE_INT64(va[0]);
			dt->off = TimeZone_offset_utc(dt->tz, i_tm);
			dt->tm = i_tm;
			Time_to_Calendar(&dt->cal, i_tm + dt->off->offset);
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
			if (argc > 0) {
				cal.year = Value_integral(va[0]);
			}
			if (argc > 1) {
				cal.month = Value_integral(va[1]);
			}
			if (argc > 2) {
				cal.day_of_month = Value_integral(va[2]);
			}
			if (argc > 3) {
				cal.hour = Value_integral(va[3]);
			}
			if (argc > 4) {
				cal.minute = Value_integral(va[4]);
			}
			if (argc > 5) {
				cal.second = Value_integral(va[5]);
			}
			if (argc > 6) {
				cal.millisec = Value_integral(va[6]);
			}

			dt->cal = cal;
			adjust_timezone(dt);
		} else {
			throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_timestamp, fs->cls_int, v1_type, va - v + 1);
			return FALSE;
		}
	} else {
		dt->cal = cal;
		adjust_timezone(dt);
	}

	return TRUE;
}
static int date_now(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = new_buf(fs->cls_time, sizeof(RefTime));
	*vret = vp_Value(dt);

	if (fg->stk_top > v + 1) {
		dt->tz = Value_to_tz(v[1], 0);
		if (dt->tz == NULL) {
			return FALSE;
		}
	} else {
		dt->tz = get_local_tz();
	}
	dt->tm = get_now_time();
	dt->off = TimeZone_offset_local(dt->tz, dt->tm);
	Time_to_Calendar(&dt->cal, dt->tm + dt->off->offset);

	return TRUE;
}
// TODO
static int date_parse_format(Value *vret, Value *v, RefNode *node)
{
	/*
	Value *v = e->stk_base;
	Date *dt = Value_new_ptr(v, cls_date, sizeof(Date), NULL);
	Calendar *cal = &dt->cal;

	Str src = Value_str(v[1]);
	Str fmt = Value_str(v[2]);
	int i;

	if (fg->stk_top > v + 3) {
		if (!Value_to_tz(&dt->tz, &v[3], vm, 2)) {
			add_trace(NULL, node, 0);
			return FALSE;
		}
	} else {
		dt->tz = get_local_tz();
	}

	for (i = 0; i < fmt.size; i++) {
	}
	*/

	return TRUE;
}
static int date_marshal_read(Value *vret, Value *v, RefNode *node)
{
	uint32_t uval;
	Value r = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	RefTime *dt = new_buf(fs->cls_time, sizeof(RefTime));
	*vret = vp_Value(dt);

	if (!read_int32(&uval, r)) {
		return FALSE;
	}
	dt->tm = ((uint64_t)uval) << 32;
	if (!read_int32(&uval, r)) {
		return FALSE;
	}
	dt->tm |= uval;
	dt->tz = timezone_marshal_read_sub(r);
	if (dt->tz == NULL) {
		return FALSE;
	}

	dt->off = TimeZone_offset_local(dt->tz, dt->tm);
	Time_to_Calendar(&dt->cal, dt->tm + dt->off->offset);

	return TRUE;
}
static int date_marshal_write(Value *vret, Value *v, RefNode *node)
{
	Value w = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	RefTime *dt = Value_vp(*v);
	RefStr *name = dt->tz->name;

	if (!write_int32((uint32_t)(dt->tm >> 32), w)) {
		return FALSE;
	}
	if (!write_int32((uint32_t)(dt->tm & 0xFFFFffff), w)) {
		return FALSE;
	}

	if (!write_int32(name->size, w)) {
		return FALSE;
	}
	if (!stream_write_data(w, name->c, name->size)) {
		return FALSE;
	}
	return TRUE;
}

static int date_parse(Value *vret, Value *v, RefNode *node)
{
	Str src;
	int64_t i_tm;
	RefTime *dt = new_buf(fs->cls_time, sizeof(RefTime));
	*vret = vp_Value(dt);

	if (fg->stk_top > v + 2) {
		dt->tz = Value_to_tz(v[1], 0);
		if (dt->tz == NULL) {
			return FALSE;
		}
		src = Value_str(v[2]);
	} else {
		RefNode *v1_type = Value_type(v[1]);
		if (v1_type != fs->cls_str) {
			throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, v1_type, 1);
			return FALSE;
		}
		dt->tz = get_local_tz();
		src = Value_str(v[1]);
	}

	Calendar_init(&dt->cal);
	if (!word_parse_time(dt, src, PARSE_ABS)) {
		throw_errorf(fs->mod_lang, "ValueError", "Invalid time format %Q", src);
		return FALSE;
	}
	Calendar_to_Time(&i_tm, &dt->cal);
	// TimeOffsetを推定
	dt->off = TimeZone_offset_local(dt->tz, i_tm);
	dt->tm = i_tm - dt->off->offset;
	// タイムスタンプを戻して、もう一度TimeOffsetを計算
	dt->off = TimeZone_offset_utc(dt->tz, dt->tm);
	// もう一度計算
	Time_to_Calendar(&dt->cal, dt->tm + dt->off->offset);

	return TRUE;
}
static int date_get(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	int ret = 0;

	switch (FUNC_INT(node)) {
	case GET_YEAR:
		ret = dt->cal.year;
		break;
	case GET_MONTH:
		ret = dt->cal.month;
		break;
	case GET_DAY_OF_YEAR:
		ret = dt->cal.day_of_year;
		break;
	case GET_DAY_OF_MONTH:
		ret = dt->cal.day_of_month;
		break;
	case GET_DAY_OF_WEEK:
		ret = dt->cal.day_of_week;
		break;
	case GET_HOUR:
		ret = dt->cal.hour;
		break;
	case GET_MINUTE:
		ret = dt->cal.minute;
		break;
	case GET_SECOND:
		ret = dt->cal.second;
		break;
	case GET_MILLISEC:
		ret = dt->cal.millisec;
		break;
	}
	*vret = int32_Value(ret);

	return TRUE;
}
static int date_timestamp(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	RefInt64 *rt = new_buf(fs->cls_timestamp, sizeof(RefInt64));
	*vret = vp_Value(rt);
	rt->u.i = dt->tm;
	return TRUE;
}
static int date_timezone(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	RefTimeZone *tz = dt->tz;
	*vret = ref_cp_Value(&tz->rh);
	return TRUE;
}
static int date_zoneabbr(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	RefStr *abbr = dt->off->name;
	*vret = ref_cp_Value(&abbr->rh);
	return TRUE;
}
static int date_is_dst(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	*vret = bool_Value(dt->off->is_dst);
	return TRUE;
}
static int date_offset(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	RefInt64 *rt = new_buf(cls_timedelta, sizeof(RefInt64));
	*vret = vp_Value(rt);
	rt->u.i = dt->off->offset;
	return TRUE;
}
static int date_tostr(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	const RefLocale *loc = fs->loc_neutral;
	const char *fmt_p;
	int fmt_size;
	StrBuf buf;

	if (fg->stk_top > v + 1) {
		RefStr *rs = Value_vp(v[1]);
		fmt_p = rs->c;
		fmt_size = rs->size;
		if (fg->stk_top > v + 2) {
			loc = Value_vp(v[2]);
		}
	} else {
		fmt_p = "yyyy-MM-ddTHH:mm:ss.SSSz";
		fmt_size = -1;
	}
	StrBuf_init_refstr(&buf, 0);
	parse_time_format(&buf, fmt_p, fmt_size, &dt->cal, dt->off, loc);
	*vret = StrBuf_str_Value(&buf, fs->cls_str);

	return TRUE;
}
static int date_hash(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt = Value_vp(*v);
	int32_t hash = ((dt->tm >> 32) ^ (dt->tm & 0xFFFFffff) ^ (intptr_t)dt->tz) & INT32_MAX;
	*vret = int32_Value(hash);
	return TRUE;
}
static int date_eq(Value *vret, Value *v, RefNode *node)
{
	RefTime *dt1 = Value_vp(v[0]);
	RefTime *dt2 = Value_vp(v[1]);
	int ret = (dt1->tm == dt2->tm && dt1->tz == dt2->tz);
	*vret = bool_Value(ret);
	return TRUE;
}
static int date_add(Value *vret, Value *v, RefNode *node)
{
	int neg = FUNC_INT(node);
	RefTime *dt1 = Value_vp(v[0]);
	RefNode *v1_type = Value_type(v[1]);
	RefTime *dt2 = new_buf(fs->cls_time, sizeof(RefTime));
	*vret = vp_Value(dt2);

	if (v1_type == fs->cls_str) {
		Str fmt = Value_str(v[1]);
		dt2->cal = dt1->cal;
		if (!word_parse_time(dt2, fmt, neg ? PARSE_NEGATIVE : PARSE_POSITIVE)) {
			throw_errorf(fs->mod_lang, "ValueError", "Invalid time format %Q", fmt);
			return FALSE;
		}
		dt2->tz = dt1->tz;
		adjust_timezone(dt2);
	} else if (v1_type == cls_timedelta) {
		int64_t delta = VALUE_INT64(v[1]);
		if (neg) {
			dt2->tm = dt1->tm - delta;
		} else {
			dt2->tm = dt1->tm + delta;
		}
		dt2->tz = dt1->tz;
		dt2->off = TimeZone_offset_local(dt2->tz, dt2->tm);
		Time_to_Calendar(&dt2->cal, dt2->tm + dt2->off->offset);
	} else {
		throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, cls_timedelta, v1_type, 1);
		return FALSE;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int time_sleep(Value *vret, Value *v, RefNode *node)
{
	int64_t ms;
	RefNode *v1_type = Value_type(v[1]);

	if (v1_type == fs->cls_int) {
		ms = Value_int(v[1], NULL);
	} else if (v1_type == cls_timedelta) {
		ms = VALUE_INT64(v[1]);
	} else {
		throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_int, cls_timedelta, v1_type, 1);
		return FALSE;
	}
	if (ms < 0) {
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
	n = define_identifier(m, cls, "isodate", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, timestamp_iso8601, 0, 0, NULL);
	n = define_identifier(m, cls, "httpdate", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, timestamp_rfc2822, 0, 0, NULL);
	extends_method(cls, fs->cls_obj);

	// TimeDelta
	cls = cls_timedelta;
	n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	define_native_func_a(n, timedelta_new, 0, 5, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
	n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
	define_native_func_a(n, timedelta_parse, 1, 1, NULL, fs->cls_str);
	n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	define_native_func_a(n, timedelta_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, int64_hash, 0, 0, NULL);
	n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
	define_native_func_a(n, int64_marshal_read, 1, 1, cls_timedelta, fs->cls_marshaldumper);

	n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	define_native_func_a(n, int64_eq, 1, 1, NULL, cls_timedelta);
	n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
	define_native_func_a(n, int64_cmp, 1, 1, NULL, cls_timedelta);
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
	n->u.k.val = ref_cp_Value(&fs->tz_utc->rh);
	n = define_identifier(m, cls, "LOCAL", NODE_CONST_U_N, 0);
	define_native_func_a(n, timezone_local, 0, 0, NULL);

	extends_method(cls, fs->cls_obj);

	// Time
	// TimeStampとRefTimeZoneを保持
	cls = fs->cls_time;
	n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	define_native_func_a(n, date_new, 0, 8, NULL, NULL, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
	n = define_identifier(m, cls, "now", NODE_NEW_N, 0);
	define_native_func_a(n, date_now, 0, 1, NULL, NULL);
	n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
	define_native_func_a(n, date_parse, 1, 2, NULL, NULL, fs->cls_str);
	n = define_identifier(m, cls, "parse_format", NODE_NEW_N, 0);
	define_native_func_a(n, date_parse_format, 2, 3, NULL, fs->cls_str, fs->cls_str, NULL);
	n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
	define_native_func_a(n, date_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

	n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	define_native_func_a(n, date_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_hash, 0, 0, NULL);
	n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
	define_native_func_a(n, date_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
	n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
	define_native_func_a(n, date_add, 1, 1, (void*) FALSE, NULL);
	n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
	define_native_func_a(n, date_add, 1, 1, (void*) TRUE, NULL);

	n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	define_native_func_a(n, date_eq, 1, 1, NULL, fs->cls_timestamp);
	n = define_identifier(m, cls, "year", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_YEAR);
	n = define_identifier(m, cls, "month", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_MONTH);
	n = define_identifier(m, cls, "day_of_year", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_DAY_OF_YEAR);
	n = define_identifier(m, cls, "day_of_month", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_DAY_OF_MONTH);
	n = define_identifier(m, cls, "day_of_week", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_DAY_OF_WEEK);
	n = define_identifier(m, cls, "hour", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_HOUR);
	n = define_identifier(m, cls, "minute", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_MINUTE);
	n = define_identifier(m, cls, "second", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_SECOND);
	n = define_identifier(m, cls, "msec", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_get, 0, 0, (void*) GET_MILLISEC);

	n = define_identifier(m, cls, "timestamp", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_timestamp, 0, 0, NULL);
	n = define_identifier(m, cls, "timezone", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_timezone, 0, 0, NULL);
	n = define_identifier(m, cls, "zoneabbr", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_zoneabbr, 0, 0, NULL);
	n = define_identifier(m, cls, "is_dst", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_is_dst, 0, 0, NULL);
	n = define_identifier(m, cls, "offset", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, date_offset, 0, 0, NULL);
	extends_method(cls, fs->cls_obj);

	cls = define_identifier(m, m, "TimeRangeError", NODE_CLASS, 0);
	define_error_class(cls, fs->cls_error, m);

}

RefNode *init_time_module_stubs()
{
	RefNode *m = new_sys_Module("time");

	fs->cls_timestamp = define_identifier(m, m, "TimeStamp", NODE_CLASS, 0);
	fs->cls_time = define_identifier(m, m, "Time", NODE_CLASS, 0);
	fs->cls_timezone = define_identifier(m, m, "TimeZone", NODE_CLASS, 0);
	cls_timedelta = define_identifier(m, m, "TimeDelta", NODE_CLASS, 0);

	fs->tz_utc = load_timezone("Etc/UTC", -1);

	return m;
}
void init_time_module_1(RefNode *m)
{
	define_time_func(m);
	define_time_class(m);

	m->u.m.loaded = TRUE;
}



