/*
 * m_xml.h
 *
 *  Created on: 2012/03/18
 *      Author: frog
 */

#ifndef M_XML_H_
#define M_XML_H_

#include "fox_io.h"
#include "m_unicode.h"


enum {
	INDEX_NAME = 0,
	INDEX_ATTR = 1,
	INDEX_CHILDREN = 2,
};

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

extern UnicodeStatic *uni;

#ifdef DEFINE_GLOBALS
#undef extern
#endif

int select_css(Value *vret, Value *v, int num, Str sel);
int delete_css(Value *vret, Value *v, int num, Str sel);
int delete_nodelist(Value *vret, Value *v, int num, RefArray *ra);


#endif /* M_XML_H_ */
