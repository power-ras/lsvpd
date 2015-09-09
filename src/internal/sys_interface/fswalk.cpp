/***************************************************************************
 *   Copyright (C) 2006, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Eric Munson and Brad Peters                                           *
 *   munsone@us.ibm.com, bpeters@us.ibm.com                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <libvpd-2/lsvpd.hpp>
#include <libvpd-2/helper_functions.hpp>
#include <libvpd-2/logger.hpp>
#include <libvpd-2/lsvpd_error_codes.hpp>

#ifdef TRACE_ON
	#include <libvpd-2/debug.hpp>
#endif

#include <fswalk.hpp>

#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>
#include <dirent.h>

#define STRIT(x) #x
#define TOSTRING(x) STRIT(x)

inline static string _errmsg(string line, string file, string str)
{
	return ( file + " [" + line + "]: " + str);
}

#define errmsg(str) _errmsg(TOSTRING(__LINE__), __FILE__, str)

using namespace std;
using namespace lsvpd;

FSWalk::~FSWalk()
{
}

/* Generic class for working a FS tree
 * @arg dir All operations which request a specific path fall back on
 *	    this dir if not specified
 */
FSWalk::FSWalk(string dir)
{
	HelperFunctions::fs_fixPath(dir);
	rootDir = dir;
}

/*
 * @brief Returns 0 if file found and opened, -1 if failed
 * @arg file: The full path and filename to the file
 * @arg lines: Number of lines found in fine
 * @arg maxLen: The length of the longest line found
 */
int FSWalk::fileScout(char *file, int *lines, int *maxLen)
{
	FILE *fi;
	int	ch;
	int len = 0;
	int ct = 0;

	*lines = 0;
	*maxLen = 0;
	fi = fopen(file, "r");
	if(fi == NULL)
		return -FILE_NOT_FOUND;

	do {
		ch = fgetc(fi);
		len++;
		if(ch == '\n') {
			if (len > *maxLen)
				*maxLen = len;
			len = 0;
			ct ++;
		}
	} while(ch != EOF);

	fclose(fi);
	*lines = ct;
	return 0;
}

int FSWalk::fs_isDir(char *path)
{
	struct stat astats;

	if ((lstat(path, &astats)) != 0) {
		return false;
	}
	if (S_ISDIR(astats.st_mode))
		return true;
	return false;

}

int FSWalk::fs_isFile(char *path)
{
	struct stat astats;

	if ((lstat(path, &astats)) != 0) {
		return false;
	}
	if (S_ISREG(astats.st_mode))
		return true;
	return false;
}

int FSWalk::fs_isLink(char *path)
{
	struct stat astats;

	if ((lstat(path, &astats)) != 0) {
		return false;
	}

	if (S_ISLNK((astats.st_mode))) {
		return true;
	}

	return false;
}

/* String versions */

int FSWalk::fs_isDir(string path)
{
	int ret;
	struct stat astats;

	if ((lstat(path.c_str(), &astats)) != 0)
		ret = false;
	if (S_ISDIR(astats.st_mode))
		ret = true;

	return ret;

}

int FSWalk::fs_isFile(string path)
{
	int ret;
	struct stat astats;

	if ((lstat(path.c_str(), &astats)) != 0)
		ret = false;
	if (S_ISREG(astats.st_mode))
		ret = true;

	return ret;
}

int FSWalk::fs_isLink(string path)
{
	int ret;
	struct stat astats;

	if ((lstat(path.c_str(), &astats)) != 0)
		ret = false;
	if (S_ISLNK(astats.st_mode))
		ret = true;

	return ret;
}

/* getDirContents(char * path, char type, char **dirList)
 * @brief   : Generates a list of entries, either files or directories,
 *	      within a FS directory
 * @arg path: absolute directory path
 * @arg type: file: 'f' directory: 'd' or link: 'l'
 * @arg dirList:  A pointer to unallocated memory, which will be filled
 *	          with 1 string (directory entry) per array element
 * @return: Number of directory entries found
 */
int FSWalk::fs_getDirContents(string path_t, char type,
			      vector<string>& list)
{
	DIR *dir = NULL;
	Logger l;
	string msg, msg2;
	struct dirent *dirent = NULL;
	const char *path;
	char file_path[SYSFS_PATH_MAX];
	string absTargetPath;

	if (!fs_isDir(path_t))
		return -DIRECTORY_NOT_FOUND;

	path = path_t.c_str();
	if (NULL == path && rootDir.length() > 0)
		path = rootDir.c_str();
	else if(NULL == path) {
		return -DIRECTORY_NOT_FOUND;
	}

	dir = opendir(path);
	if (dir == NULL) {
		msg = string("Error opening directory: ") + string(path) + "\n";
		l.log( errmsg(msg), LOG_ERR );
		return -1;
	}

	while((dirent = readdir(dir)) != NULL) {
		if (0 == strcmp(dirent->d_name, "."))
			continue;
		if (0 == strcmp(dirent->d_name, ".."))
			continue;

		memset(file_path, 0, SYSFS_PATH_MAX);
		strncpy(file_path, path, SYSFS_PATH_MAX - 1);

		if (strlen(file_path) + 1 < SYSFS_PATH_MAX)
			strncat(file_path, "/", 1);
		if (strlen(file_path) + strlen(dirent->d_name) < SYSFS_PATH_MAX)
			strncat(file_path, dirent->d_name, strlen(dirent->d_name));

		file_path[SYSFS_PATH_MAX - 1] = '\0';

		if (type == 'f' && fs_isFile(file_path)) {
			list.push_back(string(dirent->d_name));
		}
		else if (type == 'd' && fs_isDir(file_path)) {
			list.push_back(string(dirent->d_name));
		}
		else if (type == 'l' && fs_isLink(file_path)) {
			list.push_back(string(dirent->d_name));
		}
		else if (type == '*') {
			list.push_back(string(dirent->d_name));
		}
	}

	closedir(dir);
	return list.size( );
}
