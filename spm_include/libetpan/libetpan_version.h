#ifndef LIBETPAN_VERSION_H

#define LIBETPAN_VERSION_H

#ifndef LIBETPAN_VERSION_MAJOR
#define LIBETPAN_VERSION_MAJOR 1
#endif

#ifndef LIBETPAN_VERSION_MINOR
#define LIBETPAN_VERSION_MINOR 6
#endif

#ifndef LIBETPAN_REENTRANT
#define LIBETPAN_REENTRANT 1
#endif

int libetpan_get_version_major(void);
int libetpan_get_version_minor(void);

#endif
