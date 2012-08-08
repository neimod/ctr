#include <stdio.h>
#include <string.h>
#include "settings.h"

static unsigned char nullkey[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void settings_init(settings* usersettings)
{
	memset(usersettings, 0, sizeof(settings));
}

filepath* settings_get_lzss_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->lzsspath;
	else
		return 0;
}

filepath* settings_get_exefs_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->exefspath;
	else
		return 0;
}

filepath* settings_get_romfs_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->romfspath;
	else
		return 0;
}

filepath* settings_get_exheader_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->exheaderpath;
	else
		return 0;
}

filepath* settings_get_exefs_dir_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->exefsdirpath;
	else
		return 0;
}

filepath* settings_get_firm_dir_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->firmdirpath;
	else
		return 0;
}


filepath* settings_get_certs_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->certspath;
	else
		return 0;
}

filepath* settings_get_tik_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->tikpath;
	else
		return 0;
}

filepath* settings_get_tmd_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->tmdpath;
	else
		return 0;
}

filepath* settings_get_meta_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->metapath;
	else
		return 0;
}

filepath* settings_get_content_path(settings* usersettings)
{
	if (usersettings)
		return &usersettings->contentpath;
	else
		return 0;
}

unsigned int settings_get_mediaunit_size(settings* usersettings)
{
	if (usersettings)
		return usersettings->mediaunitsize;
	else
		return 0;
}

unsigned char* settings_get_ncch_key(settings* usersettings)
{
	if (usersettings)
		return usersettings->keys.ncchctrkey.data;
	else
		return nullkey;
}

unsigned char* settings_get_common_key(settings* usersettings)
{
	if (usersettings)
		return usersettings->keys.commonkey.data;
	else
		return nullkey;
}

int settings_is_common_key_valid(settings* usersettings)
{
	if (usersettings)
		return usersettings->keys.commonkey.valid;
	else
		return 0;
}


int settings_get_ignore_programid(settings* usersettings)
{
	if (usersettings)
		return usersettings->ignoreprogramid;
	else
		return 0;
}

void settings_set_lzss_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->lzsspath, path);
}

void settings_set_exefs_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->exefspath, path);
}

void settings_set_romfs_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->romfspath, path);
}

void settings_set_firm_dir_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->firmdirpath, path);
}


void settings_set_exheader_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->exheaderpath, path);
}

void settings_set_certs_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->certspath, path);
}

void settings_set_tik_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->tikpath, path);
}

void settings_set_tmd_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->tmdpath, path);
}

void settings_set_meta_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->metapath, path);
}

void settings_set_content_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->contentpath, path);
}

void settings_set_exefs_dir_path(settings* usersettings, const char* path)
{
	filepath_set(&usersettings->exefsdirpath, path);
}

void settings_set_mediaunit_size(settings* usersettings, unsigned int size)
{
	usersettings->mediaunitsize = size;
}

void settings_set_ignore_programid(settings* usersettings, int enable)
{
	usersettings->ignoreprogramid = enable;
}
