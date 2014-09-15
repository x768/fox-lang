#ifndef _M_UNICODE_H_
#define _M_UNICODE_H_

#include "fox.h"

enum {
	// その他
	CATE_Cn,  // 未割り当て
	CATE_Cc,
	CATE_Cf,
	CATE_Co,
	CATE_Cs,

	// 文字
	CATE_Ll,
	CATE_Lm,
	CATE_Lo,
	CATE_Lt,
	CATE_Lu,

	// 結合文字
	CATE_Mc,
	CATE_Me,
	CATE_Mn,

	// 数字
	CATE_Nd,
	CATE_Nl,
	CATE_No,

	// 句読点
	CATE_Pc,
	CATE_Pd,
	CATE_Pe,
	CATE_Pf,
	CATE_Pi,
	CATE_Po,
	CATE_Ps,

	// 記号
	CATE_Sc,
	CATE_Sk,
	CATE_Sm,
	CATE_So,

	// 分離子
	CATE_Zl,
	CATE_Zp,
	CATE_Zs,
};

typedef struct {
	int (*ch_get_category)(int code);
} UnicodeStatic;

#endif /* _M_UNICODE_H_ */
