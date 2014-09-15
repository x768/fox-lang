#ifndef M_PROCESS_H_
#define M_PROCESS_H_

#include "fox_io.h"

enum {
	INDEX_P_HANDLE = INDEX_STREAM_NUM,
	INDEX_P_NUM,
};

typedef struct {
	RefHeader rh;

	int valid;
	int exit_status;
	FileHandle fd_in;
	FileHandle fd_out;

#ifdef WIN32
	FileHandle fd_err;
	FileHandle p_cin;
	FileHandle p_cout;
	FileHandle p_cerr;
	void *process_info;  // PROCESS_INFORMATION
#else
	int pid;
#endif
} RefProcessHandle;

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern RefNode *mod_process;
extern const FoxStatic *fs;
extern FoxGlobal *fg;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


int process_new_sub(RefProcessHandle *ph, int create_pipe, const char *path, char **argv, int flags);
int process_wait_sub(RefProcessHandle *ph);
int is_process_terminated(RefProcessHandle *ph);
void process_close_sub(RefProcessHandle *ph);
void pipeio_close_sub(RefProcessHandle *ph);
int read_pipe(RefProcessHandle *ph, char *data, int data_size);
void write_pipe(RefProcessHandle *ph, const char *data, int data_size);

RefCharset *get_local_codepage(void);

#endif /* M_PROCESS_H_ */
