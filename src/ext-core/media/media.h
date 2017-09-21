#ifndef MEDIA_H_INCLUDED
#define MEDIA_H_INCLUDED

#include "m_number.h"
#include "m_media.h"


#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_media;
extern RefNode *cls_audio;
extern RefNode *cls_mediareader;
extern RefNode *cls_mediawriter;

#ifdef DEFINE_GLOBALS
#undef extern
#endif

// m_media.c
int Audio_set_size(RefAudio *snd, int size);

// codec.c
int audio_load_wav(RefAudio *snd, Value r, int info_only);
int audio_save_wav(RefAudio *snd, Value w);

#endif /* MEDIA_H_INCLUDED */
