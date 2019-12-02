#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

struct options {
	char *name;
	char *directory;
};

struct xvfs_state {
	char **children;
	unsigned long child_count;
	unsigned long child_len;
	int bucket_count;
	int max_index;
};

enum xvfs_minirivet_mode {
	XVFS_MINIRIVET_MODE_COPY,
	XVFS_MINIRIVET_MODE_TCL,
	XVFS_MINIRIVET_MODE_TCL_PRINT
};

/*
 * adler32() function from zlib 1.1.4 and under the same license
 */
static unsigned long adler32(unsigned long adler, const unsigned char *buf, unsigned int len) {
	const int len_max = 5552;
	const unsigned long base = 65521;
	unsigned long s1 = adler & 0xffff;
	unsigned long s2 = (adler >> 16) & 0xffff;
	int k, i;

	if (buf == NULL) {
		return(1UL);
	}

	while (len > 0) {
		k = len < len_max ? len : len_max;
		len -= k;

		while (k >= 16) {
			for (i = 0; i < 16; i++) {
				s2 += buf[i];
			}
			s1 = buf[15];

			buf += 16;
			k   -= 16;
		}

		if (k != 0) {
			do {
				s1 += *buf++;
				s2 += s1;
			} while (--k);
		}

		s1 %= base;
		s2 %= base;
	}

	return((s2 << 16) | s1);
}

/*
 * Handle XVFS Rivet template file substitution
 */
static void parse_xvfs_minirivet_file(FILE *outfp, const char * const external_file_name, const char * const internal_file_name) {
	FILE *fp;
	unsigned long file_size;
	unsigned char buf[10];
	size_t item_count;
	int idx;

	fp = fopen(external_file_name, "rb");
	if (!fp) {
		return;
	}

	fprintf(outfp, "\t{\n");
	fprintf(outfp, "\t\t.name = \"%s\",\n", internal_file_name);
	fprintf(outfp, "\t\t.type = XVFS_FILE_TYPE_REG,\n");
	fprintf(outfp, "\t\t.data.fileContents = (const unsigned char *) \"");

	file_size = 0;
	while (1) {
		item_count = fread(&buf, 1, sizeof(buf), fp);
		if (item_count <= 0) {
			break;
		}

		if (file_size != 0) {
			fprintf(outfp, "\n\t\t\t\"");
		}

		for (idx = 0; idx < item_count; idx++) {
			fprintf(outfp, "\\x%02x", (int) buf[idx]);
		}
		fprintf(outfp, "\"");

		file_size += item_count;
	}

	fclose(fp);

	fprintf(outfp, ",\n");
	fprintf(outfp, "\t\t.size = %lu\n", file_size);
	fprintf(outfp, "\t},\n");
}

static void parse_xvfs_minirivet_directory(FILE *outfp, struct xvfs_state *xvfs_state, const char * const directory, const char * const prefix) {
	const unsigned int max_path_len = 8192, max_children = 65536;
	unsigned long child_idx, child_count;
	DIR *dp;
	struct dirent *file_info;
	struct stat file_stat;
	char *full_path_buf;
	char *rel_path_buf;
	char **children;
	int stat_ret;
	int snprintf_ret;

	dp = opendir(directory);
	if (!dp) {
		return;
	}

	full_path_buf = malloc(max_path_len);
	rel_path_buf = malloc(max_path_len);
	children = malloc(sizeof(*children) * max_children);

	child_idx = 0;
	while (1) {
		file_info = readdir(dp);
		if (!file_info) {
			break;
		}

		if (strcmp(file_info->d_name, ".") == 0) {
			continue;
		}

		if (strcmp(file_info->d_name, "..") == 0) {
			continue;
		}

		snprintf_ret = snprintf(full_path_buf, max_path_len, "%s/%s", directory, file_info->d_name);
		if (snprintf_ret >= max_path_len) {
			continue;
		}

		snprintf_ret = snprintf(rel_path_buf, max_path_len, "%s%s%s",
			prefix,
			strcmp(prefix, "") == 0 ? "" : "/",
			file_info->d_name
		);
		if (snprintf_ret >= max_path_len) {
			continue;
		}

		stat_ret = stat(full_path_buf, &file_stat);
		if (stat_ret != 0) {
			continue;
		}

		children[child_idx] = strdup(file_info->d_name);
		child_idx++;

		if (S_ISDIR(file_stat.st_mode)) {
			parse_xvfs_minirivet_directory(outfp, xvfs_state, full_path_buf, rel_path_buf);
		} else {
			parse_xvfs_minirivet_file(outfp, full_path_buf, rel_path_buf);

			xvfs_state->children[xvfs_state->child_count] = strdup(rel_path_buf);
			xvfs_state->child_count++;
		}
	}
	free(full_path_buf);
	free(rel_path_buf);

	child_count = child_idx;

	fprintf(outfp, "\t{\n");
	fprintf(outfp, "\t\t.name = \"%s\",\n", prefix);
	fprintf(outfp, "\t\t.type = XVFS_FILE_TYPE_DIR,\n");
	fprintf(outfp, "\t\t.size = %lu,\n", child_count);
	fprintf(outfp, "\t\t.data.dirChildren  = (const char *[]) {");
	for (child_idx = 0; child_idx < child_count; child_idx++) {
		if (child_idx != 0) {
			fprintf(outfp, ", ");
		}

		fprintf(outfp, "\"%s\"", children[child_idx]);

		free(children[child_idx]);
	}
	fprintf(outfp, "}\n");

	free(children);

	fprintf(outfp, "\t},\n");

	xvfs_state->children[xvfs_state->child_count] = strdup(prefix);
	xvfs_state->child_count++;

	closedir(dp);

	return;
}

static void parse_xvfs_minirivet_hashtable_header(FILE *outfp, struct xvfs_state *xvfs_state) {
	const int max_bucket_count = 30;
	int bucket_count;
	int idx1, idx2;
	int check_hash;
	int first_entry;

	if (xvfs_state->child_count > max_bucket_count) {
		bucket_count = max_bucket_count;
	} else {
		bucket_count = xvfs_state->child_count;
	}
	xvfs_state->bucket_count = bucket_count;
	xvfs_state->max_index = xvfs_state->child_count;

	fprintf(outfp, "\tlong pathIndex_idx;\n");
	fprintf(outfp, "\tint pathIndex_hash;\n");

	/*
	 * XXX:TODO: Make this not O(n^2)
	 */
	for (idx1 = 0; idx1 < bucket_count; idx1++) {
		fprintf(outfp, "\tstatic const long pathIndex_hashTable_%i[] = {\n", idx1);
		fprintf(outfp, "\t\t");
		first_entry = 1;

		for (idx2 = 0; idx2 < xvfs_state->child_count; idx2++) {
			check_hash = adler32(0, (unsigned char *) xvfs_state->children[idx2], strlen(xvfs_state->children[idx2])) % bucket_count;
			if (check_hash != idx1) {
				continue;
			}

			if (!first_entry) {
				fprintf(outfp, ", ");
			}
			first_entry = 0;

			if (check_hash == idx1) {
				fprintf(outfp, "%i", idx2);
			}
		}

		if (!first_entry) {
			fprintf(outfp, ", ");
		}
		fprintf(outfp, "XVFS_NAME_LOOKUP_ERROR");

		fprintf(outfp, "\n");

		fprintf(outfp, "\t};\n");
	}

	for (idx2 = 0; idx2 < xvfs_state->child_count; idx2++) {
		free(xvfs_state->children[idx2]);
	}
	free(xvfs_state->children);

	fprintf(outfp, "\tstatic const long * const pathIndex_hashTable[%i] = {\n", bucket_count);
	for (idx1 = 0; idx1 < bucket_count; idx1++) {
		fprintf(outfp, "\t\tpathIndex_hashTable_%i,\n", idx1);
	}
	fprintf(outfp, "\t};\n");
	return;
}

static void parse_xvfs_minirivet_hashtable_body(FILE *outfp, struct xvfs_state *xvfs_state) {
	fprintf(outfp, "\tpathIndex_hash = Tcl_ZlibAdler32(0, (unsigned char *) path, pathLen) %% %i;\n", xvfs_state->bucket_count);
	fprintf(outfp, "\tfor (pathIndex_idx = 0; pathIndex_idx < %i; pathIndex_idx++) {\n", xvfs_state->max_index);
	fprintf(outfp, "\t\tpathIndex = pathIndex_hashTable[pathIndex_hash][pathIndex_idx];\n");
	fprintf(outfp, "\t\tif (pathIndex == XVFS_NAME_LOOKUP_ERROR) {\n");
	fprintf(outfp, "\t\t\tbreak;\n");
	fprintf(outfp, "\t\t}\n");
	fprintf(outfp, "\n");
	fprintf(outfp, "\t\tif (strcmp(path, xvfs_example_data[pathIndex].name) == 0) {\n");
	fprintf(outfp, "\t\t\treturn(pathIndex);\n");
	fprintf(outfp, "\t\t}\n");
	fprintf(outfp, "\t}\n");
	return;
}

static void parse_xvfs_minirivet_handle_tcl_print(FILE *outfp, const struct options * const options, struct xvfs_state *xvfs_state, char *command) {
	char *buffer_p, *buffer_e;

	buffer_p = command;
	while (*buffer_p && isspace(*buffer_p)) {
		buffer_p++;
	}

	buffer_e = buffer_p + strlen(buffer_p) - 1;
	while (buffer_e >= buffer_p && isspace(*buffer_e)) {
		*buffer_e = '\0';
		buffer_e--;
	}

	if (strcmp(buffer_p, "$::xvfs::fsName") == 0) {
		fprintf(outfp, "%s", options->name);
	} else if (strcmp(buffer_p, "$::xvfs::fileInfoStruct") == 0) {
		fprintf(outfp, "static const struct xvfs_file_data xvfs_");
		fprintf(outfp, "%s", options->name);
		fprintf(outfp, "_data[] = {\n");
		parse_xvfs_minirivet_directory(outfp, xvfs_state, options->directory, "");
		fprintf(outfp, "};\n");
	} else if (strcmp(buffer_p, "[zlib adler32 $::xvfs::fsName 0]") == 0) {
		fprintf(outfp, "%lu", adler32(0, (unsigned char *) options->name, strlen(options->name)));
	} else if (strcmp(buffer_p, "$hashTableHeader") == 0) {
		parse_xvfs_minirivet_hashtable_header(outfp, xvfs_state);
	} else if (strcmp(buffer_p, "[dict get $hashTable body]") == 0) {
		parse_xvfs_minirivet_hashtable_body(outfp, xvfs_state);
	} else {
		fprintf(outfp, "@INVALID@%s@INVALID@", buffer_p);
	}

	return;
}

static int parse_xvfs_minirivet(FILE *outfp, const char * const file, const struct options * const options) {
	struct xvfs_state xvfs_state;
	FILE *fp;
	int ch, ch_buf;
	char tcl_buffer[8192], *tcl_buffer_p;
	enum xvfs_minirivet_mode mode;

	fp = fopen(file, "r");
	if (!fp) {
		return(0);
	}

	xvfs_state.child_count = 0;
	xvfs_state.child_len   = 65536;
	xvfs_state.children    = malloc(sizeof(*xvfs_state.children) * xvfs_state.child_len);

#define parse_xvfs_minirivet_getbyte(var) var = fgetc(fp); if (var == EOF) { break; }

	mode = XVFS_MINIRIVET_MODE_COPY;
	tcl_buffer_p = NULL;
	while (1) {
		parse_xvfs_minirivet_getbyte(ch);

		switch (mode) {
			case XVFS_MINIRIVET_MODE_COPY:
				if (ch == '<') {
					parse_xvfs_minirivet_getbyte(ch_buf);
					if (ch_buf != '?') {
						fputc('<', outfp);
						ch = ch_buf;

						break;
					}

					tcl_buffer_p = tcl_buffer;
					parse_xvfs_minirivet_getbyte(ch_buf);
					if (ch_buf == '=') {
						mode = XVFS_MINIRIVET_MODE_TCL_PRINT;
					} else {
						mode = XVFS_MINIRIVET_MODE_TCL;
						*tcl_buffer_p = ch_buf;
						tcl_buffer_p++;
					}
					*tcl_buffer_p = '\0';
					continue;
				}
				break;
			case XVFS_MINIRIVET_MODE_TCL:
			case XVFS_MINIRIVET_MODE_TCL_PRINT:
				if (ch == '?') {
					parse_xvfs_minirivet_getbyte(ch_buf);
					if (ch_buf != '>') {
						*tcl_buffer_p = ch;
						tcl_buffer_p++;

						ch = ch_buf;
						
						break;
					}

					*tcl_buffer_p = '\0';

					if (mode == XVFS_MINIRIVET_MODE_TCL_PRINT) {
						parse_xvfs_minirivet_handle_tcl_print(outfp, options, &xvfs_state, tcl_buffer);
					}

					mode = XVFS_MINIRIVET_MODE_COPY;
					continue;
				}
				break;
		}

		switch (mode) {
			case XVFS_MINIRIVET_MODE_COPY:
				fputc(ch, outfp);
				break;
			case XVFS_MINIRIVET_MODE_TCL:
			case XVFS_MINIRIVET_MODE_TCL_PRINT:
				*tcl_buffer_p = ch;
				tcl_buffer_p++;
				break;
		}
	}

#undef parse_xvfs_minirivet_getbyte

	fclose(fp);

	return(1);
}

static int xvfs_create(FILE *outfp, const struct options * const options) {
	return(parse_xvfs_minirivet(outfp, "lib/xvfs/xvfs.c.rvt", options));
}

/*
 * Parse command line options
 */
static int parse_options(int argc, char **argv, struct options *options) {
	char *arg;
	char **option;
	int idx;
	int retval;

	for (idx = 0; idx < argc; idx++) {
		arg = argv[idx];

		if (strcmp(arg, "--directory") == 0) {
			option = &options->directory;
		} else if (strcmp(arg, "--name") == 0) {
			option = &options->name;
		} else {
			fprintf(stderr, "Invalid argument %s\n", arg);

			return(0);
		}

		idx++;
		arg = argv[idx];
		*option = arg;
	}

	retval = 1;
	if (!options->directory) {		
		fprintf(stderr, "error: --directory must be specified\n");
		retval = 0;
	}

	if (!options->name) {
		fprintf(stderr, "error: --name must be specified\n");
		retval = 0;
	}

	return(retval);
}

int main(int argc, char **argv) {
	struct options options = {0};
	int parse_options_ret, xvfs_create_ret;

	argc--;
	argv++;

	parse_options_ret = parse_options(argc, argv, &options);
	if (!parse_options_ret) {
		return(1);
	}

	xvfs_create_ret = xvfs_create(stdout, &options);
	if (!xvfs_create_ret) {
		return(1);
	}

	return(0);
}
