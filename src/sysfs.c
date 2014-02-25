#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/**
 * sysfs_path_is_file: Check if the path supplied points to a file
 * @path: path to validate
 * Returns 0 if path points to file, 1 otherwise
 * Copied from sysfsutils-2.1.0 (which is LGPL2.1 or later), relicensed GPLv2 for use here.
 */
int sysfs_path_is_file(const char * path)
{
        struct stat astats;

        if (!path) {
                errno = EINVAL;
                return 1;
        }
        if ((lstat(path, &astats)) != 0) {
                return 1;
        }
        if (S_ISREG(astats.st_mode))
                return 0;

        return 1;
}

int sysfs_read_file(const char * path, char **output)
{
	int ret;
	char *result = NULL, *n;
	int fd;
	unsigned long resultsize = 0;
	ssize_t length = 0;

	resultsize = getpagesize();
	result = malloc(resultsize);
	if (!result)
		return -ENOMEM;
	memset(result, 0, resultsize);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ret = fd;
		goto free_out;
	}

	length = read(fd, result, resultsize-1);
	close(fd);

	if (length < 0) {
		ret = -1;
		goto free_out;
	}
	result[length] = '\0';
	if ((n = strchr(result, '\n')) != NULL)
		*n = '\0';
	*output = result;
	ret = 0;
	goto out;
free_out:
	free(result);
out:
	return ret;
}
