#ifndef FOX_XML_H_
#define FOX_XML_H_

#include "fox_io.h"
#include "m_xml.h"
#include "m_unicode.h"

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_xml;

extern RefNode *cls_nodelist;
extern RefNode *cls_node;
extern RefNode *cls_elem;
extern RefNode *cls_text;

#ifdef DEFINE_GLOBALS
#undef extern
#endif

// m_xml.c
int ch_get_category(int ch);

// xml_select.c
int select_css(Value *vret, Value *v, int num, Str sel);
int delete_css(Value *vret, Value *v, int num, Str sel);
int delete_nodelist(Value *vret, Value *v, int num, RefArray *ra);


#endif /* FOX_XML_H_ */
