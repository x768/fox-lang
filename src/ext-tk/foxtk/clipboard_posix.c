#include "gui_compat.h"
#include "gui.h"
#include <string.h>
#include <stdlib.h>


int conv_graphics_to_image(Value *vret, GraphicsHandle img)
{
	int width, height;
	int pitch, has_alpha = FALSE;  // pitch:1行あたりのバイト数
	RefImage *fi = fs->new_buf(cls_image, sizeof(RefImage));
	*vret = vp_Value(fi);

	has_alpha = gdk_pixbuf_get_has_alpha(img);
	pitch = gdk_pixbuf_get_rowstride(img);

	if (has_alpha) {
		if (gdk_pixbuf_get_n_channels(img) != 4) {
			goto ERROR_END;
		}
	} else {
		if (gdk_pixbuf_get_n_channels(img) != 3) {
			goto ERROR_END;
		}
	}
	if (gdk_pixbuf_get_colorspace(img) != GDK_COLORSPACE_RGB) {
		goto ERROR_END;
	}
	if (gdk_pixbuf_get_bits_per_sample(img) != 8) {
		goto ERROR_END;
	}

	width = gdk_pixbuf_get_width(img);
	height = gdk_pixbuf_get_height(img);
	if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
		fs->throw_errorf(mod_gui, "GuiError", "Graphics size too large (max:%d)", MAX_IMAGE_SIZE);
		return FALSE;
	}

	{
		int byte_size = gdk_pixbuf_get_byte_length(img);
		uint8_t *dst = malloc(byte_size);
		guchar *src = gdk_pixbuf_get_pixels(img);
		// ImageとGdkPixmapの色の並びは同じ
		memcpy(dst, src, byte_size);
		fi->data = dst;
	}

	if (has_alpha) {
		fi->bands = BAND_RGBA;
	} else {
		fi->bands = BAND_RGB;
	}
	fi->width = width;
	fi->height = height;
	fi->pitch = pitch;

	return TRUE;

ERROR_END:
	fs->throw_errorf(mod_gui, "GuiError", "Graphics format not supported");
	return FALSE;
}

GraphicsHandle conv_image_to_graphics(RefImage *img)
{
	GraphicsHandle handle = NULL;
	int width = img->width;
	int height = img->height;
	int src_pitch = img->pitch;
	const uint8_t *src_buf = img->data;
	uint8_t *palette = NULL;
	int x, y;

	guchar *dst_buf;
	int dst_pitch;

	handle = gdk_pixbuf_new(GDK_COLORSPACE_RGB, img->bands == BAND_RGBA, 8, width, height);
	dst_buf = gdk_pixbuf_get_pixels(handle);
	dst_pitch = gdk_pixbuf_get_rowstride(handle);

	if (img->bands == BAND_P) {
		palette = malloc(3 * PALETTE_NUM);
		for (x = 0; x < PALETTE_NUM; x++) {
			uint32_t c = img->palette[x];
			palette[x * 3 + 0] = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
			palette[x * 3 + 1] = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
			palette[x * 3 + 2] = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
		}
	}

	for (y = 0; y < height; y++) {
		const uint8_t *src_line = src_buf + src_pitch * y;
		uint8_t *dst_line = dst_buf + dst_pitch * y;

		switch (img->bands) {
		case BAND_RGBA:
			memcpy(dst_buf + dst_pitch * y, src_buf + src_pitch * y, width * 4);
			break;
		case BAND_RGB:
			memcpy(dst_buf + dst_pitch * y, src_buf + src_pitch * y, width * 3);
			break;
		case BAND_P:
			for (x = 0; x < width; x++) {
				int c = src_line[x] * 3;
				dst_line[x * 3 + 0] = palette[c + 0];
				dst_line[x * 3 + 1] = palette[c + 1];
				dst_line[x * 3 + 2] = palette[c + 2];
			}
			break;
		case BAND_LA:
			for (x = 0; x < width; x++) {
				uint8_t c = src_line[x * 2];
				dst_line[x * 3 + 0] = c;
				dst_line[x * 3 + 1] = c;
				dst_line[x * 3 + 2] = c;
			}
			break;
		case BAND_L:
			for (x = 0; x < width; x++) {
				uint8_t c = src_line[x];
				dst_line[x * 3 + 0] = c;
				dst_line[x * 3 + 1] = c;
				dst_line[x * 3 + 2] = c;
			}
			break;
		}
	}

	if (palette != NULL) {
		free(palette);
	}

	return handle;
}

////////////////////////////////////////////////////////////////////////////

int clipboard_clear()
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		// TODO クリップボードをクリアする方法が不明
		gtk_clipboard_clear(p_clipboard);
		gtk_clipboard_set_can_store(p_clipboard, NULL, 0);
		gtk_clipboard_store(p_clipboard);
	}
	return TRUE;
}

int clipboard_is_available(const RefNode *klass)
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if (p_clipboard == NULL) {
		return FALSE;
	}

	if (klass == fs->cls_str) {
		return gtk_clipboard_wait_is_text_available(p_clipboard);
	} else if (klass == fs->cls_file) {
		return gtk_clipboard_wait_is_uris_available(p_clipboard);
	} else if (klass == cls_image) {
		return gtk_clipboard_wait_is_image_available(p_clipboard);
	} else {
		return FALSE;
	}
}

Value clipboard_get_text()
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		gchar *str = gtk_clipboard_wait_for_text(p_clipboard);
		if (str != NULL) {
			return fs->cstr_Value_conv(str, -1, NULL);
		}
	}
	return VALUE_NULL;
}
void clipboard_set_text(const char *src_p, int src_size)
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		if (src_size < 0) {
			src_size = strlen(src_p);
		}
		gtk_clipboard_set_text(p_clipboard, src_p, src_size);
		gtk_clipboard_set_can_store(p_clipboard, NULL, 0);
		gtk_clipboard_store(p_clipboard);
	}
}

int clipboard_get_files(Value *v, int uri)
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		gchar **files = gtk_clipboard_wait_for_uris(p_clipboard);

		if (files != NULL) {
			int i;
			RefArray *afile = fs->refarray_new(0);
			*v = vp_Value(afile);

			for (i = 0; files[i] != NULL && *files[i] != '\0'; i++) {
				if (uri) {
					Value *vf = fs->refarray_push(afile);
					*vf = fs->cstr_Value(fs->cls_uri, files[i], -1);
				} else {
					char *filename = g_filename_from_uri(files[i], NULL, NULL);
					if (filename != NULL) {
						Value *vf = fs->refarray_push(afile);
						*vf = fs->cstr_Value(fs->cls_file, filename, -1);
						g_free(filename);
					}
				}
			}
			g_strfreev(files);
		}
	}
	return TRUE;
}
int clipboard_set_files(Value *v, int num)
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		// TODO
	}
	return TRUE;
}

int clipboard_get_image(Value *v)
{
	int ret = TRUE;
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		GdkPixbuf *pbuf = gtk_clipboard_wait_for_image(p_clipboard);
		if (pbuf != NULL) {
			if (!conv_graphics_to_image(v, pbuf)) {
				ret = FALSE;
			}
		}
	}
	return ret;
}

int clipboard_set_image(RefImage *img)
{
	GtkClipboard *p_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (p_clipboard != NULL) {
		GraphicsHandle gh = conv_image_to_graphics(img);

		if (gh == NULL) {
			return FALSE;
		}
		gtk_clipboard_set_image(p_clipboard, gh);
		gtk_clipboard_set_can_store(p_clipboard, NULL, 0);
		gtk_clipboard_store(p_clipboard);
		g_object_unref(gh);
	}
	return TRUE;
}
