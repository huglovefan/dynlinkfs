#define _XOPEN_SOURCE 600
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <fuse.h>

static int fs_getattr(const char *path,
                      struct stat *stbuf,
                      struct fuse_file_info *fi) {
	(void)fi;

	if (0 == memcmp(path, "/", 2)) {
		stbuf->st_mode = S_IFDIR|0755;
		return 0;
	}

	stbuf->st_mode = S_IFLNK|0644;
	return 0;
}

struct buffer {
	char *data;
	size_t capacity;
	size_t used;
};

static void append_c(struct buffer *buf, char c) {
	if (buf->used < buf->capacity) {
		buf->data[buf->used++] = c;
	}
}
static void append_s(struct buffer *buf, const char *s) {
	size_t len = strlen(s);
	size_t rem = buf->capacity-buf->used;
	if (len > rem) len = rem;
	memcpy(&buf->data[buf->used], s, len);
	buf->used += len;
}

static int pgetenv(pid_t pid,
                   const char *varname,
                   struct buffer *out) {
	char filename[32];
	int written;
	FILE *env;

	written = snprintf(filename, sizeof(filename), "/proc/%d/environ", pid);
	if (!(written >= 0 && (size_t)written < sizeof(filename))) {
		abort();
	}

	env = fopen(filename, "r");
	if (env == NULL) {
		fprintf(stderr, "pgetenv: %s: %s\n", filename, strerror(errno));
		return 0;
	}

	for (;;) {
		int c;
		int i;
		for (i = 0; (char)(c = fgetc(env)) == varname[i]; i++) {
			continue;
		}
		if (c != '=') {
			while ((c = fgetc(env)) != '\0') {
				if (c == EOF) goto out_err;
			}
			continue;
		}
		if (varname[i] != '\0') {
			while ((c = fgetc(env)) != '=' && c != '\0') {
				if (c == EOF) goto out_err;
			}
			continue;
		}
		while ((c = fgetc(env)) != '\0') {
			if (c == EOF) goto out_err;
			append_c(out, (char)c);
		}
		goto out_ok;
	}
out_err:
	fclose(env);
	return 0;
out_ok:
	fclose(env);
	return 1;
}

static int fs_readlink(const char *path, char *buf, size_t size) {
	struct buffer dt;
	dt.data = buf;
	dt.capacity = size-1;
	dt.used = 0;
	fprintf(stderr, "readlink %s size=%zu\n", path, size);
	assert(*path == '/'); path++;
	while (*path) {
		if (*path != '%') {
			append_c(&dt, *path++);
			continue;
		}
		path++;
		switch (*path) {
		case '{': {
			char *varname = (char *)path+1;
			char *closer = strchr(varname, '}');
			struct fuse_context *ctx = fuse_get_context();
			if (closer == NULL) return -EINVAL;
			*closer = '\0';
			if (!pgetenv(ctx->pid, varname, &dt)) {
				return -ENOENT;
			}
			*closer = '}';
			path += (size_t)(closer-varname)+2;
			break;
		}
		case 'p': {
			struct fuse_context *ctx = fuse_get_context();
			char str[8];
			int written = snprintf(str, sizeof(str), "%d", ctx->pid);
			if (!(written >= 0 && (size_t)written < sizeof(str))) {
				abort();
			}
			append_s(&dt, str);
			path++;
			break;
		}
		case 'S':
			append_c(&dt, '/');
			path++;
			break;
		case '\0':
			return -EINVAL;
		default:
			append_c(&dt, *path);
			path++;
			break;
		}
	}
	buf[dt.used] = '\0';
	return 0;
}

static struct fuse_operations fs_ops;

int main(int argc, char **argv) {
	fs_ops.getattr = fs_getattr;
	fs_ops.readlink = fs_readlink;
	return fuse_main(argc, argv, &fs_ops, NULL);
}
