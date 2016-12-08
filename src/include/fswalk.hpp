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

#ifndef FS_WALK_H_
#define FS_WALK_H_
#define SYSFS_PATH_MAX 512

#include <vector>
#include <string>

using namespace std;

namespace lsvpd {

	class FSWalk {
		private:
			string rootDir;
		public:
			FSWalk() {}
			FSWalk(const string dir);
			~FSWalk();

			static int fileScout(char *file, int *lines, int *maxLen);

			inline const string& getRootDir( ) const { return rootDir; }

			int fileSearch(string rootpath_t,
				string pattern_t,
				const vector<string>& list);
			int fs_fileCountLines(char *file);
			static int fs_isDir(char *path);
			static int fs_isFile(char *path);
			static int fs_isLink(char *path);
			static int fs_isDir(string path);
			static int fs_isFile(string path);
			static int fs_isLink(string path);
			int fs_getDirContents(string path_t,
					char type,
					vector<string>& list);
			static string get_cmd_path(const char *);

	};

}

#endif /*FS_WALK_H_*/

