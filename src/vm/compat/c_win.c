#include "fox.h"
#include "config.h"
#include <wincrypt.h>


static int last_error;
FileHandle STDIN_FILENO;
FileHandle STDOUT_FILENO;
FileHandle STDERR_FILENO;


void *dlopen_fox(const char *fname, int dummy)
{
	wchar_t *wtmp = filename_to_utf16(fname, NULL);
	void *p = (void*)LoadLibraryW(wtmp);
	free(wtmp);

	last_error = (p == NULL);
	return p;
}
void *dlsym_fox(void *handle, const char *name)
{
	void *p = GetProcAddress((HANDLE) handle, name);
	last_error = (p == NULL);
	return p;
}
const char *dlerror_fox(void)
{
	enum {
		ERRMSG_SIZE = 512,
	};
	static char *err_buf;

	if (last_error) {
		wchar_t *wbuf = malloc(ERRMSG_SIZE * sizeof(wchar_t));
		FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			GetLastError(),
			0,
			wbuf,
			ERRMSG_SIZE,
			NULL);
		
		if (err_buf == NULL) {
			err_buf = Mem_get(&fg->st_mem, ERRMSG_SIZE);
		}
		WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, err_buf, ERRMSG_SIZE, NULL, NULL);
		free(wbuf);
		return err_buf;
	} else {
		return NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

int is_root_dir(Str path)
{
	if (path.size == 2) {
		return TRUE;
	} else {
		return FALSE;
	}
}
int is_absolute_path(Str path)
{
	if (path.size >= 2 && isalpha(path.p[0]) && path.p[1] == ':') {
		return TRUE;
	} else if (path.size >= 3 && (path.p[0] == '\\' || path.p[0] == '/') && (path.p[1] == '\\' || path.p[1] == '/')) {
		return TRUE;
	} else {
		return FALSE;
	}
}

// 通常、ディレクトリ名は最後の/または\を省略して表現するが、
// rootディレクトリは/またはC:\で表すため、表示するときだけ変換する
Str get_root_name(Str path)
{
	if (path.size == 2 && isalpha(path.p[0]) && path.p[1] == ':') {
		char buf[4];
		const RefStr *p;

		sprintf(buf, "%c:\\", toupper(path.p[0]));
		p = intern(buf, 3);
		return Str_new(p->c, p->size);
	} else {
		return path;
	}
}

int64_t get_now_time()
{
	FILETIME ftm;
	int64_t i64;

	GetSystemTimeAsFileTime(&ftm);
	i64 = (uint64_t)ftm.dwLowDateTime | ((uint64_t)ftm.dwHighDateTime << 32);
	// 1601年1月1日から 100ナノ秒間隔でカウント
	// 1601-01-01から1969-01-01までの秒数を引く
	return i64 / 10000 - 11644473600000LL;
}

//////////////////////////////////////////////////////////////////////////////////////////

DIR *opendir_fox(const char *dname)
{
	DIR *d = malloc(sizeof(DIR) + sizeof(WIN32_FIND_DATAW));
	wchar_t *wtmp = filename_to_utf16(dname, L"\\*");
	d->hDir = FindFirstFileW(wtmp, (WIN32_FIND_DATAW*)d->wfd);
	free(wtmp);

	if (d->hDir != INVALID_HANDLE_VALUE) {
		WIN32_FIND_DATAW *p = (WIN32_FIND_DATAW*)d->wfd;
		d->first = TRUE;
		WideCharToMultiByte(CP_UTF8, 0, p->cFileName, -1, d->ent.d_name, sizeof(d->ent.d_name), NULL, NULL);
		if ((p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			d->ent.d_type = DT_DIR;
		} else {
			d->ent.d_type = DT_REG;
		}
		return d;
	} else {
		free(d);
		return NULL;
	}
}
struct dirent *readdir_fox(DIR *d)
{
	if (d->first) {
		d->first = FALSE;
		return &d->ent;
	} else if (FindNextFileW(d->hDir, (WIN32_FIND_DATAW*)d->wfd)) {
		WIN32_FIND_DATAW *p = (WIN32_FIND_DATAW*) &d->wfd;
		WideCharToMultiByte(CP_UTF8, 0, p->cFileName, -1, d->ent.d_name, sizeof(d->ent.d_name), NULL, NULL);
		if ((p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			d->ent.d_type = DT_DIR;
		} else {
			d->ent.d_type = DT_REG;
		}
		return &d->ent;
	} else {
		return NULL;
	}
}
void closedir_fox(DIR *d)
{
	FindClose((HANDLE)d->hDir);
	free(d);
}

//////////////////////////////////////////////////////////////////////////////////////////

int mkdir_fox(const char *dname, int dummy)
{
	wchar_t *wtmp = filename_to_utf16(dname, NULL);
	int ret = CreateDirectoryW(wtmp, NULL);
	free(wtmp);

	return ret != 0 ? 0 : -1;
}
int remove_fox(const char *fname)
{
	wchar_t *wtmp = filename_to_utf16(fname, NULL);
	int ret = DeleteFileW(wtmp);
	free(wtmp);

	return ret != 0 ? 0 : -1;
}
int rename_fox(const char *fname, const char *tname)
{
	wchar_t *wfname = filename_to_utf16(fname, NULL);
	wchar_t *wtname = filename_to_utf16(tname, NULL);
	int ret = MoveFileW(wfname, wtname);
	free(wfname);
	free(wtname);

	return ret != 0 ? 0 : -1;
}
char *getcwd_fox(char *p, int n)
{
	wchar_t wbuf[MAX_PATH];
	char *buf = malloc(MAX_PATH + 1);

	GetCurrentDirectoryW(MAX_PATH, wbuf);
	WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, MAX_PATH, NULL, NULL);

	return buf;
}

int64_t get_file_size(FileHandle fh)
{
	if (fh != -1) {
		HANDLE hFile = (HANDLE)fh;
		DWORD size_h;
		DWORD size = GetFileSize(hFile, &size_h);
		return ((uint64_t)size) | ((uint64_t)size_h << 32);
	} else {
		return -1;
	}
}

int get_file_mtime(int64_t *tm, const char *fname)
{
	wchar_t *wtmp = filename_to_utf16(fname, NULL);
	HANDLE hFile = CreateFileW(wtmp, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	free(wtmp);

	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	} else {
		FILETIME ftm;
		int64_t i64;
		GetFileTime(hFile, NULL, NULL, &ftm);
		CloseHandle(hFile);

		i64 = (uint64_t)ftm.dwLowDateTime | ((uint64_t)ftm.dwHighDateTime << 32);
		// 1601年1月1日から 100ナノ秒間隔でカウント
		// 1601-01-01から1969-01-01までの秒数を引く
		*tm = i64 / 10000 - 11644473600000LL;

		return TRUE;
	}

	return TRUE;
}

int exists_file(const char *file)
{
	WIN32_FIND_DATAW fd;
	wchar_t *wtmp = filename_to_utf16(file, NULL);
	HANDLE hFind = FindFirstFileW(wtmp, &fd);
	free(wtmp);

	if (hFind == INVALID_HANDLE_VALUE) {
		return EXISTS_NONE;
	}
	FindClose(hFind);

	return (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? EXISTS_DIR : EXISTS_FILE;
}

void get_random(void *buf, int len)
{
	HCRYPTPROV hProv;
	CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT);
	if (CryptGenRandom(hProv, len, (BYTE*)buf) == 0) {
		// an error occured
	}
	CryptReleaseContext(hProv, 0);
}

void native_sleep(int ms)
{
	Sleep(ms);
}

Str get_fox_home()
{
#ifdef FOX_HOME
	return Str_new(FOX_HOME, -1);
#else
	wchar_t path[MAX_PATH];
	char *cbuf;
	wchar_t *p;
	int maxlen;
	int n = 2;

	// exeのpathから、'\\'を2つ除去する
	GetModuleFileNameW(NULL, path, MAX_PATH);
	p = path + wcslen(path);

	while (p > path) {
		p--;
		if (*p == L'\\') {
			*p = L'\0';
			n--;
			if (n <= 0) {
				break;
			}
		}
	}

	maxlen = wcslen(path) * 4 + 1;
	cbuf = Mem_get(&fg->st_mem, maxlen);
	WideCharToMultiByte(CP_UTF8, 0, path, -1, cbuf, maxlen, NULL, NULL);

	return Str_new(cbuf, -1);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int parse_envs(Str *key, const char **val, const char *src)
{
	const char *p = src;
	while (isalnum(*p) || *p == '_') {
		p++;
	}
	if (*p != '=') {
		return FALSE;
	}
	*key = Str_new(src, p - src);
	*val = p + 1;

	return TRUE;
}

static void init_env(void)
{
	wchar_t *wenv_top = GetEnvironmentStringsW();
	wchar_t *wenvp = wenv_top;

	Hash_init(&fs->envs, &fg->st_mem, 64);
	while (*wenvp != L'\0') {
		Str key;
		const char *val;
		char *cbuf = utf16to8(wenvp);
		if (parse_envs(&key, &val, cbuf)) {
			Hash_add_p(&fs->envs, &fg->st_mem, intern(key.p, key.size), str_dup_p(val, -1, &fg->st_mem));
		}
		wenvp += wcslen(wenvp) + 1;
		free(cbuf);
	}

	FreeEnvironmentStringsW(wenv_top);
}

