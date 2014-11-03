#include "media.h"
#include <stdio.h>
#include <string.h>


/*
typedef struct {
  ID             chunkID;          // 12
  long           chunkSize;        // 16
  short          wFormatTag;       // 20
  unsigned short wChannels;        // 22
  unsigned long  dwSamplesPerSec;  // 24
  unsigned long  dwAvgBytesPerSec; // 28
  unsigned short wBlockAlign;      // 32
  unsigned short wBitsPerSample;   // 34
} FormatChunk;
*/
/*
typedef struct {
  ID             chunkID;          // +0
  long           chunkSize;        // +4
  unsigned char  waveformData[];   // +8
} DataChunk;
*/

static uint16_t ptr_read_uint16_le(const char *p)
{
    return ((uint8_t)p[0]) | ((uint8_t)p[1] << 8);
}
static uint32_t ptr_read_uint32_le(const char *p)
{
    return ((uint8_t)p[0]) | ((uint8_t)p[1] << 8) | ((uint8_t)p[2] << 16) | ((uint8_t)p[3] << 24);
}

static void ptr_write_uint16_le(char *p, uint16_t val)
{
    p[0] = val & 0xFF;
    p[1] = val >> 8;
}
static void ptr_write_uint32_le(char *p, uint32_t val)
{
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}

#ifdef FOX_BIG_ENDIAN
static void reverse_endian(int16_t *p, int length)
{
    int i;

    for (i = 0; i < length; i++) {
        int n = p[i];
        p[i] = (n & 0xFF) | ((n >> 8) & 0xFF);
    }
}
#endif

int audio_load_wav(RefAudio *snd, Value r, int info_only)
{
    char buf[40];
    int read_size;
    int chunk_size;

    read_size = 36;
    if (!fs->stream_read_data(r, NULL, buf, &read_size, FALSE, TRUE)) {
        return FALSE;
    }
    if (memcmp(buf + 0, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVEfmt ", 8) != 0) {
        goto ERROR_INVALID_FORMAT;
    }
    chunk_size = ptr_read_uint32_le(buf + 16);
    if (chunk_size < 16) {
        goto ERROR_INVALID_FORMAT;
    }
    if (ptr_read_uint16_le(buf + 20) != 1) {
        fs->throw_errorf(mod_media, "AudioError", "Illigal WAV parameter (only Linear PCM but not)");
        return FALSE;
    }
    snd->channels = ptr_read_uint16_le(buf + 22);
    if (snd->channels != 1 && snd->channels != 2) {
        fs->throw_errorf(mod_media, "AudioError", "Illigal WAV parameter (channels=%d)", snd->channels);
        return FALSE;
    }
    snd->samples = ptr_read_uint32_le(buf + 24);
    if (snd->samples < 1 && snd->samples > 1000000) {
        fs->throw_errorf(mod_media, "AudioError", "Illigal WAV parameter (samples=%d)", snd->samples);
        return FALSE;
    }
    snd->width = ptr_read_uint16_le(buf + 34);
    if (snd->width != 8 && snd->width != 16) {
        fs->throw_errorf(mod_media, "AudioError", "Illigal WAV parameter (bitpersample=%d)", snd->width);
        return FALSE;
    }
    snd->width /= 8;

    if (chunk_size > 16) {
        read_size = chunk_size - 16;
        if (!fs->stream_read_data(r, NULL, NULL, &read_size, FALSE, TRUE)) {
            return FALSE;
        }
    }

    for (;;) {
        read_size = 8;
        if (!fs->stream_read_data(r, NULL, buf, &read_size, FALSE, TRUE)) {
            return FALSE;
        }
        chunk_size = ptr_read_uint32_le(buf + 4);
        if (memcmp(buf, "data", 4) == 0) {
            break;
        }
        if (chunk_size > 0) {
            read_size = chunk_size;
            if (!fs->stream_read_data(r, NULL, NULL, &read_size, FALSE, TRUE)) {
                return FALSE;
            }
        }
    }
    if (info_only) {
        snd->length = chunk_size / (snd->width * snd->channels);
        return TRUE;
    }

    if (!Audio_set_size(snd, chunk_size / (snd->width * snd->channels))) {
        return FALSE;
    }

    // dataチャンクを読み込む
    read_size = snd->length * snd->width * snd->channels;
    if (!fs->stream_read_data(r, NULL, (char*)snd->u.u8, &read_size, FALSE, TRUE)) {
        return FALSE;
    }
#ifdef FOX_BIG_ENDIAN
    if (snd->width == 2) {
        reverse_endian(snd->u.i16, snd->length * snd->channels);
    }
#endif

    return TRUE;

ERROR_INVALID_FORMAT:
    fs->throw_errorf(mod_media, "AudioError", "Invalid WAV format");
    return FALSE;
}

int audio_save_wav(RefAudio *snd, Value w)
{
    enum {
        WAV_HEADER_SIZE = 12 + 16 + 8,
    };
    char buf[44];
    int data_size = snd->length * snd->width * snd->channels;

    memcpy(buf + 0, "RIFF", 4);
    ptr_write_uint32_le(buf + 4, data_size + WAV_HEADER_SIZE);
    memcpy(buf + 8, "WAVEfmt ", 8);

    ptr_write_uint32_le(buf + 16, 16);
    ptr_write_uint16_le(buf + 20, 1);
    ptr_write_uint16_le(buf + 22, snd->channels);
    ptr_write_uint32_le(buf + 24, snd->samples);
    ptr_write_uint32_le(buf + 28, snd->samples * snd->channels * snd->width);
    ptr_write_uint16_le(buf + 32, snd->channels * snd->width);
    ptr_write_uint16_le(buf + 34, snd->width * 8);

    memcpy(buf + 36, "data", 4);
    ptr_write_uint32_le(buf + 40, data_size);

    if (!fs->stream_write_data(w, buf, 44)) {
        return FALSE;
    }

#ifdef FOX_BIG_ENDIAN
    if (snd->width == 2) {
        reverse_endian(snd->u.i16, snd->length * snd->channels);
    }
#endif

    if (!fs->stream_write_data(w, (const char*)snd->u.u8, data_size)) {
        return FALSE;
    }

#ifdef FOX_BIG_ENDIAN
    if (snd->width == 2) {
        reverse_endian(snd->u.i16, snd->length * snd->channels);
    }
#endif

    return TRUE;
}

