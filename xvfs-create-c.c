#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct options {
	char *name;
	char *directory;
};

enum xvfs_minirivet_mode {
	XVFS_MINIRIVET_MODE_COPY,
	XVFS_MINIRIVET_MODE_TCL,
	XVFS_MINIRIVET_MODE_TCL_PRINT
};

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

static void parse_xvfs_minirivet_directory(FILE *outfp, const char * const directory) {
	fprintf(outfp, "\t...\n");
}

static int parse_xvfs_minirivet(FILE *outfp, const char * const file, const char * const name, const char * const directory) {
	FILE *fp;
	int ch, ch_buf[3];
	char tcl_buffer[8192], *tcl_buffer_p, *tcl_buffer_e;
	enum xvfs_minirivet_mode mode;

	fp = fopen(file, "r");
	if (!fp) {
		return(0);
	}

#define parse_xvfs_minirivet_getbyte(var) var = fgetc(fp); if (var == EOF) { break; }

	mode = XVFS_MINIRIVET_MODE_COPY;
	while (1) {
		parse_xvfs_minirivet_getbyte(ch);

		switch (mode) {
			case XVFS_MINIRIVET_MODE_COPY:
				if (ch == '<') {
					parse_xvfs_minirivet_getbyte(ch_buf[0]);
					if (ch_buf[0] != '?') {
						fputc('<', outfp);
						ch = ch_buf[0];

						break;
					}

					tcl_buffer_p = tcl_buffer;
					parse_xvfs_minirivet_getbyte(ch_buf[0]);
					if (ch_buf[0] == '=') {
						mode = XVFS_MINIRIVET_MODE_TCL_PRINT;
					} else {
						mode = XVFS_MINIRIVET_MODE_TCL;
						*tcl_buffer_p = ch_buf[0];
						tcl_buffer_p++;
					}
					*tcl_buffer_p = '\0';
					continue;
				}
				break;
			case XVFS_MINIRIVET_MODE_TCL:
			case XVFS_MINIRIVET_MODE_TCL_PRINT:
				if (ch == '?') {
					parse_xvfs_minirivet_getbyte(ch_buf[0]);
					if (ch_buf[0] != '>') {
						*tcl_buffer_p = ch;
						tcl_buffer_p++;

						ch = ch_buf[0];
						
						break;
					}

					*tcl_buffer_p = '\0';

					if (mode == XVFS_MINIRIVET_MODE_TCL_PRINT) {
						tcl_buffer_p = tcl_buffer;
						while (*tcl_buffer_p && isspace(*tcl_buffer_p)) {
							tcl_buffer_p++;
						}
						tcl_buffer_e = tcl_buffer_p + strlen(tcl_buffer_p) - 1;
						while (tcl_buffer_e >= tcl_buffer_p && isspace(*tcl_buffer_e)) {
							*tcl_buffer_e = '\0';
							tcl_buffer_e--;
						}

						if (strcmp(tcl_buffer_p, "$::xvfs::fsName") == 0) {
							fprintf(outfp, name);
						} else if (strcmp(tcl_buffer_p, "$::xvfs::fileInfoStruct") == 0) {
							fprintf(outfp, "static const struct xvfs_file_data xvfs_");
							fprintf(outfp, name);
							fprintf(outfp, "_data[] = {\n");
							parse_xvfs_minirivet_directory(outfp, directory);
							fprintf(outfp, "};\n");
						} else if (strcmp(tcl_buffer_p, "[zlib adler32 $::xvfs::fsName]") == 0) {
							fprintf(outfp, "0");
						} else {
							fprintf(outfp, "@INVALID@%s@INVALID@", tcl_buffer_p);
						}
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

	return(1);
}

static int xvfs_create(FILE *outfp, const char * const name, const char * const directory) {
	return(parse_xvfs_minirivet(outfp, "lib/xvfs/xvfs.c.rvt", name, directory));
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

	xvfs_create_ret = xvfs_create(stdout, options.name, options.directory);
	if (!xvfs_create_ret) {
		return(1);
	}

	return(0);
}
