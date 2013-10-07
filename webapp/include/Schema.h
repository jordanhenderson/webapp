#ifndef SCHEMA_H
#define SCHEMA_H
//APP specific definitions
#define TEMPLATE_VIDEO "video.html"
#define TEMPLATE_IMAGE "image.html"
#define TEMPLATE_FLASH "flash.html"
#define TEMPLATE_TEXT "text.html"
#define DEFAULT_THUMB "default.png"
#define ALBUM_SET 0
#define ALBUM_RANDOM 1
#define THUMB_TYPE_FILE 0
#define THUMB_TYPE_ALBUM 1

//QUERY DEFINTIONS
#define SELECT_RANDOM_ALBUMIDS "SELECT id FROM albums WHERE type = " XSTR(ALBUM_RANDOM) ";"
#define SELECT_SET_ALBUMIDS "SELECT id FROM albums WHERE type = " XSTR(ALBUM_SET) ";"
#define SELECT_ENABLED_FILES "SELECT id FROM files WHERE albumid = ? AND enabled = 1;"
#define SELECT_DUPLICATE_ALBUM_COUNT "SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;"
#define SELECT_FILENAME "SELECT filename FROM files WHERE id = ?;"
#define SELECT_ALBUM_FILE "SELECT id, fileid FROM albumfiles WHERE albumid = ?;"
#define SELECT_FILE_THUMBID "SELECT thumbid FROM files WHERE id = ?;"
#define SELECT_ALBUM_PATH_THUMB "SELECT path, thumbid FROM albums WHERE id = ?;"
#define SELECT_ALBUM_PATH "SELECT path, recursive FROM albums WHERE id = ?;"
#define SELECT_ALBUM_COUNT "SELECT COUNT(*) FROM albums;"
#define SELECT_SYSTEM(var) "SELECT value FROM system WHERE name=\"" var "\""
#define SELECT_PATHS_FROM_ALBUM "SELECT path FROM files JOIN albumfiles ON albumfiles.fileid = files.id WHERE albumid = ?;"
#define SELECT_FILE_DETAILS "SELECT f.id AS id, al.id AS aid, f.name AS name, f.rating AS rating, f.views AS views, \
" XSTR(THUMB_TYPE_FILE) " AS thumb_type, \
(SELECT (" SELECT_SYSTEM("store_path") ") || '/' || al.path || '/' || f.path) AS path, \
(SELECT (" SELECT_SYSTEM("thumbs_path") ") || '/' || al.path || '/' || f.path) AS thumb \
FROM files f JOIN albumfiles alf ON f.id=alf.fileID JOIN albums al ON al.id=alf.albumid WHERE 1 "
#define _SELECT_BOTH_DETAILS(COND, COND2) "SELECT f.id AS id, al.id AS aid, CASE al.type WHEN " XSTR(ALBUM_RANDOM) " THEN f.name WHEN " XSTR(ALBUM_SET) " THEN al.name END AS name, \
CASE al.type WHEN " XSTR(ALBUM_RANDOM) " THEN f.rating WHEN " XSTR(ALBUM_SET) " THEN al.rating END AS rating, \
CASE al.type WHEN " XSTR(ALBUM_RANDOM) " THEN f.views WHEN " XSTR(ALBUM_SET) " THEN al.views END AS views, \
CASE al.type WHEN " XSTR(ALBUM_RANDOM) " THEN " XSTR(THUMB_TYPE_FILE) " WHEN " XSTR(ALBUM_SET) " THEN " XSTR(THUMB_TYPE_ALBUM) " END AS thumb_type, \
(SELECT (" SELECT_SYSTEM("store_path") ") || '/' || al.path || '/' || f.path) AS path, \
(SELECT (" SELECT_SYSTEM("thumbs_path") ") || '/' || al.path || '/' || f.path) AS thumb \
FROM files f JOIN albumfiles alf ON f.id=alf.fileID JOIN albums al ON al.id=alf.albumid WHERE ((al.type = " XSTR(ALBUM_RANDOM) COND ") OR (al.type = \
" XSTR(ALBUM_SET) " AND f.id IN (SELECT fileid FROM albumfiles WHERE albumid=al.id ORDER BY id ASC LIMIT 1) " COND2 ")) "
#define SELECT_BOTH_DETAILS _SELECT_BOTH_DETAILS("", "")
#define SELECT_ALBUM_DETAILS "SELECT al.id AS id, name, added, lastedited, type, rating, recursive, views, al.path AS path, \
" XSTR(THUMB_TYPE_ALBUM) " AS thumb_type, \
(SELECT (" SELECT_SYSTEM("thumbs_path") ") || '/' || al.path || '/' || f.path FROM files f JOIN albumfiles alf ON alf.fileid=f.id AND alf.id IN \
(SELECT id FROM albumfiles WHERE albumid=al.id ORDER BY id ASC LIMIT 1) JOIN albums ON albums.id = al.id) AS thumb FROM albums al WHERE 1 "
#define SELECT_SEARCH _SELECT_BOTH_DETAILS(" AND f.name LIKE ?", "AND al.name LIKE ?") 
#define SELECT_ALBUM_ID_WITH_FILE "SELECT al.id FROM albums al JOIN albumfiles alf ON alf.albumid = al.id JOIN files f ON alf.fileid = f.id WHERE f.id = ?"

#define INSERT_ALBUM "INSERT INTO albums (name, added, lastedited, path, type, recursive) VALUES (?,?,?,?,?,?);"
#define INSERT_FILE "INSERT INTO files (name, path, added, thumbid) VALUES (?,?,?,?);"
#define INSERT_FILE_NO_THUMB "INSERT INTO files (name, path, added) VALUES (?,?,?);"
#define INSERT_ALBUM_FILE "INSERT INTO albumfiles (albumid, fileid) VALUES (?,?);"
#define INSERT_THUMB "INSERT INTO thumbs(path) VALUES (?);"

#define UPDATE_THUMB "UPDATE %s SET thumbid = ? WHERE id = ?;"
#define UPDATE_ALBUM "UPDATE albums SET %s = ? WHERE id = ?"  
#define UPDATE_FILE "UPDATE files SET %s = ? WHERE id = ?"

#define DELETE_ALBUM "DELETE FROM albums WHERE id = ?;"
#define DELETE_MISSING_FILES "DELETE FROM files WHERE id IN (SELECT fileid FROM albumfiles alf WHERE alf.albumid = ?) AND path NOT IN ("

#define CONDITION_FILE_ENABLED " AND f.enabled = 1 "
#define CONDITION_FILEID " AND f.id = ? "
#define CONDITION_ALBUMID " AND al.id = ? "
#define CONDITION_ALBUM_NAME " AND al.name LIKE ? "
#define CONDITION_N_FILE(op, kwd) " AND f.id = (SELECT " kwd "(f.id) FROM albumfiles alf JOIN files f ON f.id = fileid WHERE fileid " op " ? AND f.enabled = 1 AND alf.albumid = ? LIMIT 1)"

#define TOGGLE_FILES "UPDATE files SET enabled = 1 - enabled WHERE id IN (SELECT f.id FROM files f JOIN albumfiles alf ON alf.fileid = f.id JOIN albums al ON alf.albumid = al.id WHERE 1 "
#define ORDER_DEFAULT " ORDER BY "

#define INC_FILE_VIEWS "UPDATE files SET views = views + 1 WHERE id = ?"
#define INC_ALBUM_VIEWS "UPDATE albums SET views = views + 1 WHERE id = ?"

#define DEFAULT_PAGE_LIMIT 32
#define MAX_PAGE_LIMIT 100

#define PRAGMA_FOREIGN "PRAGMA foreign_keys = ON;"

#define CREATE_DATABASE "BEGIN; \
CREATE TABLE IF NOT EXISTS 'albums' ('id' INTEGER PRIMARY KEY  NOT NULL ,'name' TEXT,'added' DATETIME,'lastedited' DATETIME,'path' TEXT NOT NULL ,'type' INTEGER NOT NULL ,'thumbid' INTEGER REFERENCES thumbs(id) ON DELETE SET NULL,'rating' DOUBLE DEFAULT (0) ,'recursive' INTEGER NOT NULL , 'views' INTEGER DEFAULT 0); \
CREATE TABLE IF NOT EXISTS 'files' ('id' INTEGER PRIMARY KEY  NOT NULL,'name' TEXT,'path' TEXT,'added' TEXT,'enabled' INTEGER NOT NULL  DEFAULT (1) ,'thumbid' INTEGER REFERENCES thumbs(id) ON DELETE SET NULL,'rating' DOUBLE DEFAULT(0), 'views' INTEGER DEFAULT 0, 'lastedited' TEXT); \
CREATE TABLE IF NOT EXISTS 'albumfiles' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , 'albumid' INTEGER NOT NULL REFERENCES albums(id) ON DELETE CASCADE, 'fileid' INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE ); \
CREATE TABLE IF NOT EXISTS 'system' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , 'name' TEXT NOT NULL , 'value' TEXT, UNIQUE(name) ON CONFLICT IGNORE); \
CREATE TRIGGER IF NOT EXISTS 'file_cleanup' AFTER DELETE ON 'albums' BEGIN DELETE FROM files WHERE id NOT IN (SELECT fileid FROM albumfiles); END; \
CREATE TRIGGER IF NOT EXISTS 'thumbs_trigger_files' AFTER DELETE ON 'files' BEGIN DELETE FROM thumbs WHERE id NOT IN (SELECT thumbid FROM files f WHERE thumbid NOT NULL UNION SELECT thumbid FROM albums WHERE thumbid NOT NULL); END; \
CREATE TRIGGER IF NOT EXISTS 'thumbs_trigger_albums' AFTER DELETE ON 'albums' BEGIN DELETE FROM thumbs WHERE id NOT IN (SELECT thumbid FROM files f WHERE thumbid NOT NULL UNION SELECT thumbid FROM albums WHERE thumbid NOT NULL); END; \
CREATE UNIQUE INDEX IF NOT EXISTS 'alf_index' ON 'albumfiles' ('albumid' ASC, 'fileid' ASC); \
UPDATE sqlite_sequence SET seq = (SELECT MAX(id) FROM system) WHERE name='system'; \
INSERT INTO 'system' ('name', 'value') VALUES ('default_thumb','default.png'); \
INSERT INTO 'system' ('name', 'value') VALUES ('thumbs_path','thumbs'); \
INSERT INTO 'system' ('name', 'value') VALUES ('store_path','store'); \
COMMIT;"

//END QUERY DEFINITIONS

#define SYSTEM_SCRIPT_COUNT 3
#define SYSTEM_SCRIPT_TEMPLATE 0
#define SYSTEM_SCRIPT_THUMBS_DEFAULT 1
#define SYSTEM_SCRIPT_PROCESS 2
#define SYSTEM_SCRIPT_FILENAMES {"core/template.lua", "thumbs/default.lua", "core/process.lua"}

//Public API map
#define APIMAP(m) 	m["addAlbum"] = &Webapp::addAlbum; \
	m["getAlbums"] = &Webapp::getAlbums; \
	m["delAlbums"] = &Webapp::delAlbums; \
	m["addBulkAlbums"] = &Webapp::addBulkAlbums; \
	m["login"] = &Webapp::login; \
	m["setThumb"] = &Webapp::setThumb; \
	m["getFiles"] = &Webapp::getFiles; \
	m["search"] = &Webapp::search; \
	m["refreshAlbums"] = &Webapp::refreshAlbums; \
	m["disableFiles"] = &Webapp::disableFiles; \
	m["clearCache"] = &Webapp::clearCache; \
	m["getBoth"] = &Webapp::getBoth; \
	m["updateAlbum"] = &Webapp::updateAlbum; \
	m["updateFile"] = &Webapp::updateFile; \
	m["logout"] = &Webapp::logout;


//PROTOCOL SCHEMA DEFINITIONS
#define STRING_VARS 6
#define PROTOCOL_LENGTH_SIZEINFO sizeof(int) * STRING_VARS

#define STATE_READ_URI 1
#define STATE_READ_COOKIES 2
#define STATE_FINAL 3
#endif
