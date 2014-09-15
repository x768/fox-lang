#define DEFINE_GLOBALS
#include "image_raw.h"
#include <string.h>
#include <stdio.h>



static int get_map_sub(int *has_value, Value *value, RefMap *v_map, const char *key, RefNode *expected)
{
	Value vkey;
	HashValueEntry *ret;

	vkey = fs->cstr_Value(fs->cls_str, key, -1);
	if (!fs->refmap_get(&ret, v_map, vkey)) {
		return FALSE;
	}
	if (ret != NULL) {
		RefNode *type = fs->Value_type(ret->val);
		if (type != expected) {
			fs->throw_errorf(fs->mod_lang, "TypeError", "%n required but %n", expected, type);
			return FALSE;
		}
		*value = ret->val;
		*has_value = TRUE;
	} else {
		*has_value = FALSE;
	}
	return TRUE;
}
static int get_map_str(RefStr **value, RefMap *v_map, const char *key)
{
	Value vret;
	int has_value;

	if (!get_map_sub(&has_value, &vret, v_map, key, fs->cls_str)) {
		return FALSE;
	}
	if (has_value) {
		*value = Value_vp(vret);
	} else {
		*value = NULL;
	}
	return TRUE;
}
static int get_map_int(int *has_value, int *value, RefMap *v_map, const char *key)
{
	Value vret;
	if (!get_map_sub(has_value, &vret, v_map, key, fs->cls_int)) {
		return FALSE;
	}
	if (*has_value) {
		*value = fs->Value_int(vret, NULL);
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int load_image_bmp(Value *vret, Value *v, RefNode *node)
{
	RefImage *image = Value_vp(v[1]);
	int info_only = Value_bool(v[3]);

	if (!load_image_bmp_sub(image, v[2], info_only)) {
		return FALSE;
	}
	return TRUE;
}
static int load_image_pnm(Value *vret, Value *v, RefNode *node)
{
	RefImage *image = Value_vp(v[1]);
	int info_only = Value_bool(v[3]);

	if (!load_image_pnm_sub(image, v[2], info_only)) {
		return FALSE;
	}
	return TRUE;
}
static int save_image_bmp(Value *vret, Value *v, RefNode *node)
{
	RefImage *image = Value_vp(v[1]);

	if (!save_image_bmp_sub(image, v[2])) {
		return FALSE;
	}
	return TRUE;
}
static int save_image_pnm(Value *vret, Value *v, RefNode *node)
{
	RefImage *image = Value_vp(v[1]);
	RefMap *rm_param = Value_vp(v[3]);
	int type = FUNC_INT(node);
	RefStr *mode = NULL;
	int threshold = 128;
	int has_value;

	if (type == TYPE_NONE) {
		switch (image->bands) {
		case BAND_L:
		case BAND_LA:
			type = TYPE_PGM_BIN;
			break;
		default:
			type = TYPE_PPM_BIN;
			break;
		}
	}

	if (!get_map_str(&mode, rm_param, "mode")) {
		return FALSE;
	}
	if (mode != NULL) {
		if (str_eqi(mode->c, mode->size, "a", -1) || str_eqi(mode->c, mode->size, "ascii", -1)) {
			type -= 3;
		} else if (str_eqi(mode->c, mode->size, "t", -1) || str_eqi(mode->c, mode->size, "text", -1)) {
			type -= 3;
		} else if (str_eqi(mode->c, mode->size, "b", -1) || str_eqi(mode->c, mode->size, "bin", -1) || str_eqi(mode->c, mode->size, "binary", -1)) {
		} else {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown mode (ascii (text) or binary only)");
			return FALSE;
		}
	}
	if (!get_map_int(&has_value, &threshold, rm_param, "threshold")) {
		return FALSE;
	}
	if (has_value) {
		if (threshold < 0) {
			threshold = 0;
		} else if (threshold > 255) {
			threshold = 255;
		}
	}

	if (!save_image_pnm_sub(image, v[2], type, threshold)) {
		return FALSE;
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier(m, m, "load_image_bmp", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, load_image_bmp, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "load_image_x_portable_anymap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, load_image_pnm, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "load_image_x_portable_bitmap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, load_image_pnm, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "load_image_x_portable_graymap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, load_image_pnm, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "load_image_x_portable_pixmap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, load_image_pnm, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);


	n = fs->define_identifier(m, m, "save_image_bmp", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, save_image_bmp, 3, 3, (void*)TYPE_NONE, NULL, fs->cls_streamio, fs->cls_map);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "save_image_x_portable_anymap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, save_image_pnm, 3, 3, (void*)TYPE_NONE, NULL, fs->cls_streamio, fs->cls_map);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "save_image_x_portable_bitmap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, save_image_pnm, 3, 3, (void*)TYPE_PBM_BIN, NULL, fs->cls_streamio, fs->cls_map);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "save_image_x_portable_graymap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, save_image_pnm, 3, 3, (void*)TYPE_PGM_BIN, NULL, fs->cls_streamio, fs->cls_map);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);

	n = fs->define_identifier(m, m, "save_image_x_portable_pixmap", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, save_image_pnm, 3, 3, (void*)TYPE_PPM_BIN, NULL, fs->cls_streamio, fs->cls_map);
	fs->add_unresolved_args(m, mod_image, "Image", n, 0);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_raw = m;

	mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
	define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
