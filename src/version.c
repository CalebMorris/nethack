/* See LICENSE in the root of this project for change info */
#include "hack.h"
#include "date.h"
#include "patchlevel.h"
#include "extern.h"
#include "winprocs.h"

/* fill buffer with short version (so caller can avoid including date.h) */
char * version_string (char *buf) {
        return strcpy(buf, VERSION_STRING);
}

/* fill and return the given buffer with the long nethack version string */
char * getversionstring (char *buf) {
        strcpy(buf, VERSION_ID);
        return buf;
}

int doversion(void) {
        char buf[BUFSZ];

        pline("%s", getversionstring(buf));
        return 0;
}

int doextversion(void) {
        display_file(OPTIONS_USED, true);
        return 0;
}

bool check_version(struct version_info *version_data, const char *filename, bool complain) {
    if (version_data->incarnation != VERSION_NUMBER) {
        if (complain)
            pline("Version mismatch for file \"%s\".", filename);
        return false;
    } else if (version_data->feature_set != VERSION_FEATURES || //
            version_data->entity_count != VERSION_SANITY1 || //
            version_data->struct_sizes != VERSION_SANITY2) {
        if (complain)
            pline("Configuration incompatibility for file \"%s\".", filename);
        return false;
    }
    return true;
}

/* this used to be based on file date and somewhat OS-dependant,
   but now examines the initial part of the file's contents */
bool uptodate(int fd, const char *name) {
    int rlen;
    struct version_info vers_info;
    bool verbose = name ? true : false;

    rlen = read(fd, (void *) &vers_info, sizeof vers_info);
    minit();            /* ZEROCOMP */
    if (rlen == 0) {
        if (verbose) {
            pline("File \"%s\" is empty?", name);
            wait_synch();
        }
        return false;
    }
    if (!check_version(&vers_info, name, verbose)) {
        if (verbose) wait_synch();
        return false;
    }
    return true;
}

void store_version (int fd) {
    const static struct version_info version_data = {
        VERSION_NUMBER, VERSION_FEATURES,
        VERSION_SANITY1, VERSION_SANITY2
    };

    bufoff(fd);
    /* bwrite() before bufon() uses plain write() */
    bwrite(fd,(void *)&version_data,(unsigned)(sizeof version_data));
    bufon(fd);
    return;
}

unsigned long get_feature_notice_ver (char *str) {
    char buf[BUFSZ];
    int ver_maj, ver_min, patch;
    char *istr[3];
    int j = 0;

    if (!str) return 0L;
    str = strcpy(buf, str);
    istr[j] = str;
    while (*str) {
        if (*str == '.') {
            *str++ = '\0';
            j++;
            istr[j] = str;
            if (j == 2) break;
        } else if (index("0123456789", *str) != 0) {
            str++;
        } else
            return 0L;
    }
    if (j != 2) return 0L;
    ver_maj = atoi(istr[0]);
    ver_min = atoi(istr[1]);
    patch = atoi(istr[2]);
    return FEATURE_NOTICE_VER(ver_maj,ver_min,patch);
    /* macro from hack.h */
}

unsigned long get_current_feature_ver (void) {
    return FEATURE_NOTICE_VER(VERSION_MAJOR,VERSION_MINOR,PATCHLEVEL);
}
