#ifndef DEMO_SQL_H
#define DEMO_SQL_H

#define DEMO_SQL_TABLES \
  "select name from sqlite_master where type = 'table' and name not like 'sqlite_%' order by name"

#define DEMO_SQL_TABLE_EXISTS \
  "select 1 from sqlite_master where type = 'table' and name = ?"

#define DEMO_SQL_GRID \
  "select rowid, * from {} limit {}"

#define DEMO_SQL_CELL \
  "select {} from {} where rowid = ?"

#define DEMO_SQL_UPDATE_CELL \
  "update {} set {} = ? where rowid = ?"

#define DEMO_SQL_SEED \
  "create table if not exists artists (id integer primary key, name text not null);" \
  "create table if not exists albums (id integer primary key, title text not null, artist_id integer references artists(id), year integer);" \
  "insert into artists (name) select * from (values ('Can'), ('Neu!'), ('Faust'), ('Cluster')) where (select count(*) from artists) = 0;" \
  "insert into albums (title, artist_id, year) select * from (values ('Tago Mago', 1, 1971), ('Ege Bamyasi', 1, 1972), ('Future Days', 1, 1973), ('Neu!', 2, 1972), ('Neu! 75', 2, 1975), ('Faust IV', 3, 1973), ('Zuckerzeit', 4, 1974)) where (select count(*) from albums) = 0;"

#endif
