#include "fox_xml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


enum {
	STK_EOS,
	STK_ERROR,
	STK_SPACE,
	STK_IDENT,   // ident
	STK_STR,     // "abc" 'abc'
	STK_DOT,     // .
	STK_COMMA,   // ,
	STK_HASH,    // #
	STK_COLON,   // :
	STK_LP,      // (
	STK_RP,      // )
	STK_LB,      // [
	STK_RB,      // ]
	STK_EQ,      // =
	STK_STAR,    // *
	STK_SLASH,   // /
	STK_PLUS,    // +
	STK_LT,      // <
	STK_GT,      // >
	STK_TILDE_EQ,// ~=
	STK_BAR_EQ,  // |=
	STK_HAT_EQ,  // ^=
	STK_DOLL_EQ, // $=
	STK_STAR_EQ, // *=
};

enum {
	XT_ANY,
	XT_NAME,
	XT_ATTR_EQ,      // key=val
	XT_ATTR_MATCH,   // key~=val class="hoge piyo foo"
	XT_ATTR_FIRST,   // key|=val class="en-us"
	XT_ATTR_HEAD,    // key^=val
	XT_ATTR_TAIL,    // key$=val
	XT_ATTR_INC,     // key*=val
	XT_ATTR_EXIST,   // 属性keyが存在していれば、値は何でもいい
	XT_EXPR,
	XT_NEXT_SIBLING, // A + B
};

typedef struct {
	int type;
	Str val;
	char *p;
	const char *end;
} SelectorTok;


typedef struct XMLAttrPattern {
	struct XMLAttrPattern *next;
	int type;
	Str key;
	Str val;
} XMLAttrPattern;

typedef struct XMLPattern {
	struct XMLPattern *or;
	struct XMLPattern *next;
	int index;
	int subnode;           // TRUE:子孫, FALSE:子のみ
	XMLAttrPattern *attr;  // NULLの場合、子孫セレクタ
} XMLPattern;

typedef struct {
	XMLPattern *root;
	XMLPattern **p;
	int num;
} XMLPatternList;

static void SelectorTok_init(SelectorTok *tk, const char *src_p, int src_size, Mem *mem)
{
	tk->p = fs->str_dup_p(src_p, src_size, mem);
	tk->end = tk->p + src_size;
	tk->type = STK_EOS;
}
static void SelectorTok_next(SelectorTok *tk)
{
	int ch;

	if (tk->p < tk->end && isspace_fox(*tk->p)) {
		tk->p++;
		while (tk->p < tk->end && isspace_fox(*tk->p)) {
			tk->p++;
		}
		tk->type = STK_SPACE;
		return;
	}
	if (tk->p >= tk->end) {
		tk->type = STK_EOS;
		return;
	}

	ch = *tk->p++;
	switch (ch) {
	case '/':
		tk->type = STK_SLASH;
		break;
	case '=':
		tk->type = STK_EQ;
		break;
	case '[':
		tk->type = STK_LB;
		break;
	case ']':
		tk->type = STK_RB;
		break;
	case '(':
		tk->type = STK_LP;
		break;
	case ')':
		tk->type = STK_RP;
		break;
	case '.':
		tk->type = STK_DOT;
		break;
	case '#':
		tk->type = STK_HASH;
		break;
	case ':':
		tk->type = STK_COLON;
		break;
	case ',':
		tk->type = STK_COMMA;
		break;
	case '"':
		// TODO "str" の解析
		tk->type = STK_ERROR;
		break;
	case '\'':
		// TODO 'str' の解析
		tk->type = STK_ERROR;
		break;
	case '+':
		tk->type = STK_PLUS;
		break;
	case '<':
		tk->type = STK_LT;
		break;
	case '>':
		tk->type = STK_GT;
		break;
	case '~':
		if (*tk->p == '=') {
			tk->p++;
			tk->type = STK_TILDE_EQ;
		} else {
			tk->type = STK_ERROR;
			break;
		}
		break;
	case '|':
		if (*tk->p == '=') {
			tk->p++;
			tk->type = STK_BAR_EQ;
		} else {
			tk->type = STK_ERROR;
			break;
		}
		break;
	case '^':
		if (*tk->p == '=') {
			tk->p++;
			tk->type = STK_HAT_EQ;
		} else {
			tk->type = STK_ERROR;
			break;
		}
		break;
	case '$':
		if (*tk->p == '=') {
			tk->p++;
			tk->type = STK_DOLL_EQ;
		} else {
			tk->type = STK_ERROR;
			break;
		}
		break;
	case '*':
		if (*tk->p == '=') {
			tk->p++;
			tk->type = STK_STAR_EQ;
		} else {
			tk->type = STK_STAR;
			break;
		}
		break;
	default:
		if (isalnumu_fox(ch) || (ch & 0x80) != 0) {
			// 識別子
			tk->p--;
			tk->val.p = tk->p;
			for (;;) {
				int ch2 = fs->utf8_codepoint_at(tk->p);
				int type = ch_get_category(ch2);

				if (type < CATE_Ll || type > CATE_No) {
					if (ch2 != '-' && ch2 != '_') {
						break;
					}
				}
				fs->utf8_next((const char**)&tk->p, tk->end);
			}
			tk->val.size = tk->p - tk->val.p;
			if (tk->val.size > 0) {
				tk->type = STK_IDENT;
			} else {
				tk->type = STK_ERROR;
			}
		} else {
			tk->type = STK_ERROR;
		}
		break;
	}
}

static void XMLPatternList_set_p_sub(XMLPattern ***ppp, XMLPattern *node)
{
	for (; node != NULL; node = node->next) {
		XMLPattern **pp = *ppp;
		*pp++ = node;
		*ppp = pp;
		if (node->or != NULL) {
			XMLPatternList_set_p_sub(ppp, node->or);
		}
	}
}
static void XMLPatternList_set_p(XMLPatternList *pl, Mem *mem)
{
	XMLPattern **pp;

	if (pl->num == 0) {
		pl->p = NULL;
		return;
	}
	pl->p = malloc(sizeof(XMLPattern*) * pl->num);
	pp = pl->p;
	XMLPatternList_set_p_sub(&pp, pl->root);
}
static void XMLPatternList_mark(XMLPatternList *pl, uint8_t *mask, int index)
{
	XMLPattern *pat = pl->p[index];

	mask[index] = TRUE;
	for (pat = pat->or; pat != NULL; pat = pat->or) {
		mask[pat->index] = TRUE;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////

/*

* 	全称セレクタ 	あらゆる要素にマッチする。
E 	タイプセレクタ 	Eという名称の要素にマッチする。
E F 	子孫セレクタ 	E要素の子孫であるF要素にマッチする。
E > F 	子供セレクタ 	E要素の子供であるF要素にマッチする。
E + F 	隣接兄弟セレクタ 	兄であるE要素の直後にある弟のF要素にマッチする。
E[foo] 	属性セレクタ 	foo属性が設定されたE要素にマッチする（値が何であれ）。
E[foo="warning"] 	属性セレクタ 	foo属性値が warning であるE要素にマッチする。
E[foo~="warning"] 	属性セレクタ 	foo属性値が空白類文字で区切られた語のリストであり，そのうちひとつが warning という語であるE要素にマッチする。
E[lang|="en"] 	属性セレクタ 	lang属性値がハイフンで区切られた語のリストであり，そのリストが en という語で始っているE要素にマッチする。
DIV.warning 	クラスセレクタ 	DIV[class~="warning"]
E#myid 	IDセレクタ 	E[id="myid"]

 */
static int parse_css_selector_sub(XMLPattern **pp, int *pnum, SelectorTok *tk, Mem *mem)
{
	XMLPattern *pat;
	XMLPattern **ppat;
	XMLAttrPattern *attr;
	XMLAttrPattern **pattr;
	int subnode = TRUE;

	// 空白を除外
	if (tk->type == STK_SPACE) {
		SelectorTok_next(tk);
	}
	ppat = pp;

	for (;;) {
		if (tk->type != STK_GT) {
			pat = fs->Mem_get(mem, sizeof(*pat));
			memset(pat, 0, sizeof(*pat));
			*ppat = pat;
			ppat = &pat->next;
			pattr = &pat->attr;
			pat->index = *pnum;
			(*pnum)++;

			if (*pp == NULL) {
				*pp = pat;
			}
			pat->subnode = subnode;
			subnode = TRUE;
		}

		switch (tk->type) {
		case STK_STAR:
			SelectorTok_next(tk);
			break;
		case STK_IDENT:
			attr = fs->Mem_get(mem, sizeof(*attr));
			memset(attr, 0, sizeof(*attr));
			*pattr = attr;
			pattr = &attr->next;

			attr->type = XT_NAME;
			attr->val = tk->val;
			SelectorTok_next(tk);
			break;
		default:
			break;
		}
		for (;;) {
			int type = tk->type;
			switch (type) {
			case STK_DOT:
			case STK_HASH:
			case STK_COLON:
				SelectorTok_next(tk);
				if (tk->type != STK_IDENT) {
					goto ERROR_END;
				}
				attr = fs->Mem_get(mem, sizeof(*attr));
				memset(attr, 0, sizeof(*attr));
				*pattr = attr;
				pattr = &attr->next;

				switch (type) {
				case STK_DOT:
					attr->type = XT_ATTR_MATCH;
					attr->key = Str_new("class", 5);
					attr->val = tk->val;
					break;
				case STK_HASH:
					attr->type = XT_ATTR_EQ;
					attr->key = Str_new("id", 2);
					attr->val = tk->val;
					break;
				case STK_COLON:
					if (Str_eq_p(tk->val, "checked")) {
						attr->type = XT_ATTR_EXIST;
						attr->key = Str_new("checked", 7);
					} else if (Str_eq_p(tk->val, "selected")) {
						attr->type = XT_ATTR_EXIST;
						attr->key = Str_new("selected", 8);
					} else {
						goto ERROR_END;
					}
					break;
				}
				SelectorTok_next(tk);
				break;
			case STK_LB:
				SelectorTok_next(tk);
				if (tk->type != STK_IDENT) {
					goto ERROR_END;
				}

				attr = fs->Mem_get(mem, sizeof(*attr));
				memset(attr, 0, sizeof(*attr));
				*pattr = attr;
				pattr = &attr->next;

				attr->type = XT_ATTR_EXIST;
				attr->key = tk->val;
				SelectorTok_next(tk);
				if (tk->type == STK_EQ) {
					attr->type = XT_ATTR_EQ;
				} else if (tk->type == STK_TILDE_EQ) {
					attr->type = XT_ATTR_MATCH;
				} else if (tk->type == STK_BAR_EQ) {
					attr->type = XT_ATTR_FIRST;
				}

				if (attr->type != XT_ATTR_EXIST) {
					SelectorTok_next(tk);
					if (tk->type != STK_IDENT && tk->type != STK_STR) {
						goto ERROR_END;
					}
					attr->val = tk->val;
					SelectorTok_next(tk);
				}

				if (tk->type != STK_RB) {
					goto ERROR_END;
				}
				SelectorTok_next(tk);
				break;
			case STK_GT:
				subnode = FALSE;
				SelectorTok_next(tk);
				break;
			default:
				goto BREAK;
			}
		}
	BREAK:
		*pattr = NULL;

		switch (tk->type) {
		case STK_EOS:
		case STK_COMMA:
			*ppat = NULL;
			return TRUE;
		case STK_SPACE:
			break;
		default:
			goto ERROR_END;
		}
		SelectorTok_next(tk);
	}

ERROR_END:
	if (tk->type == STK_ERROR) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown token");
	} else {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Unexpected token");
	}
	return FALSE;
}
static int parse_css_selector(XMLPatternList *pl, Str sel, Mem *mem)
{
	SelectorTok tk;
	XMLPattern **pp = &pl->root;

	pl->root = NULL;
	pl->p = NULL;
	pl->num = 0;

	SelectorTok_init(&tk, sel.p, sel.size, mem);
	SelectorTok_next(&tk);

	for (;;) {
		*pp = NULL;
		if (!parse_css_selector_sub(pp, &pl->num, &tk, mem)) {
			return FALSE;
		}
		pp = &(*pp)->or;
		if (tk.type == STK_COMMA) {
			SelectorTok_next(&tk);
			if (tk.type == STK_SPACE) {
				SelectorTok_next(&tk);
			}
		} else if (tk.type == STK_EOS) {
			break;
		} else {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Unexpected token");
			return FALSE;
		}
	}
	*pp = NULL;
	XMLPatternList_set_p(pl, mem);

	return TRUE;
}

/**
 * XMLPatternListから、XMLNodeを選択
 */
static int select_xml_match_node(Ref *r, XMLPattern *pat)
{
	XMLAttrPattern *attr;

	for (attr = pat->attr; attr != NULL; attr = attr->next) {
		switch (attr->type) {
		case XT_ANY:
			break;
		case XT_NAME: {
			RefStr *name = Value_vp(r->v[INDEX_ELEM_NAME]);
			if (!str_eq(name->c, name->size, attr->val.p, attr->val.size)) {
				return FALSE;
			}
			break;
		}
		case XT_ATTR_EQ:
		case XT_ATTR_MATCH:
		case XT_ATTR_FIRST: {
			HashValueEntry *he;
			Value key;
			RefStr *val;

			key = fs->cstr_Value(fs->cls_str, attr->key.p, attr->key.size);
			// Str#_op_eqでは、例外が発生しないのでチェックを省略
			fs->refmap_get(&he, Value_vp(r->v[INDEX_ELEM_ATTR]), key);
			if (he == NULL) {
				return FALSE;
			}
			val = Value_vp(he->val);
			switch (attr->type) {
			case XT_ATTR_EXIST:
				break;
			case XT_ATTR_EQ:
				if (!str_eq(val->c, val->size, attr->val.p, attr->val.size)) {
					return FALSE;
				}
				break;
			case XT_ATTR_MATCH: {
				int i = 0;
				int asize = attr->val.size;
				int found = FALSE;
				for (;;) {
					while (i < val->size - asize && isspace_fox(val->c[i])) {
						i++;
					}
					if (memcmp(val->c, attr->val.p + i, asize) == 0) {
						if (i + asize == val->size || isspace_fox(val->c[i + asize])) {
							found = TRUE;
							break;
						}
					}
					while (i < val->size - asize && !isspace_fox(val->c[i])) {
						i++;
					}
					if (i >= val->size - asize) {
						break;
					}
				}
				if (!found) {
					return FALSE;
				}
				break;
			}
			case XT_ATTR_FIRST: {
				int asize = attr->val.size;
				int found = FALSE;
				if (val->size >= asize && memcmp(val->c, attr->val.p, asize) == 0) {
					if (val->size == asize || !isalnumu_fox(val->c[asize])) {
						found = TRUE;
					}
				}
				if (!found) {
					return FALSE;
				}
				break;
			}
			}
			break;
		}
		default:
			return FALSE;
		}
	}
	return TRUE;
}
/**
 * v : 探索対象のXML
 * ra : 結果のXMLElem配列
 */
static void select_xml_nodes_sub(RefArray *ra, XMLPatternList *pl, uint8_t *mask, Value v)
{
	int i;
	int num = pl->num;
	Ref *r;
	uint8_t *mask2 = mask;
	if (fs->Value_type(v) != cls_elem) {
		return;
	}

	r = Value_vp(v);
	for (i = 0; i < num; i++) {
		if (mask[i]) {
			XMLPattern *pat = pl->p[i];
			if (select_xml_match_node(r, pat)) {
				if (pat->next == NULL) {
					Value *ve = fs->refarray_push(ra);
					*ve = fs->Value_cp(v);
				}
				if (pat->next != NULL) {
					if (mask2 == mask) {
						int j;
						mask2 = malloc(num);
						for (j = 0; j < num; j++) {
							if (mask[j] && pl->p[j]->subnode) {
								mask2[j] = TRUE;
							} else {
								mask2[j] = FALSE;
							}
						}
					}
					XMLPatternList_mark(pl, mask, pat->next->index);
				}
			}
		}
	}

	{
		RefArray *ra2 = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
		for (i = 0; i < ra2->size; i++) {
			select_xml_nodes_sub(ra, pl, mask2, ra2->p[i]);
		}
		if (mask != mask2) {
			free(mask2);
		}
	}
}
static void select_xml_nodes(RefArray *ra, XMLPatternList *pl, Value *v, Mem *mem)
{
	uint8_t *mask = NULL;

	if (pl->root == NULL || pl->num == 0) {
		return;
	}
	mask = malloc(pl->num);
	memset(mask, 0, pl->num);
	XMLPatternList_mark(pl, mask, 0);

	select_xml_nodes_sub(ra, pl, mask, *v);
	free(mask);
}

int select_css(Value *vret, Value *v, int num, Str sel)
{
	Mem mem;
	int i;
	XMLPatternList pl;
	RefArray *ra = fs->refarray_new(0);

	*vret = vp_Value(ra);
	ra->rh.type = cls_nodelist;
	fs->Mem_init(&mem, 512);

	if (!parse_css_selector(&pl, sel, &mem)) {
		fs->Mem_close(&mem);
		return FALSE;
	}
	for (i = 0; i < num; i++) {
		select_xml_nodes(ra, &pl, &v[i], &mem);
	}
	fs->Mem_close(&mem);
	return TRUE;
}
int delete_css(Value *vret, Value *v, int num, Str sel)
{
	Mem mem;
	XMLPatternList pl;
	int i;
	RefArray *ra = fs->refarray_new(0);
	*vret = vp_Value(ra);
	ra->rh.type = cls_nodelist;
	fs->Mem_init(&mem, 512);

	if (!parse_css_selector(&pl, sel, &mem)) {
		fs->Mem_close(&mem);
		return FALSE;
	}
	for (i = 0; i < ra->size; i++) {
	}

	fs->Mem_close(&mem);
	return TRUE;
}
int delete_nodelist(Value *vret, Value *v, int num, RefArray *ra)
{
	return TRUE;
}
