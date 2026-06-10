import { Database } from "bun:sqlite";
import { mkdirSync } from "node:fs";
import { dirname } from "node:path";
import { spryApp, Fault } from "spry/server";
import { button, column, input, invoke, row, text } from "spry/ui";
import type { Node } from "spry/schema";
import endpoints from "../ui/endpoints.json";
import tree from "../ui/tree.json";

const root = new URL("../../", import.meta.url);
const dbPath = new URL("example/db/demo.db", root);

const ROW_LIMIT = 20;

mkdirSync(dirname(dbPath.pathname), { recursive: true });
const db = new Database(dbPath.pathname, { create: true });
seed(db);

function seed(db: Database) {
  db.run(`create table if not exists artists (id integer primary key, name text not null)`);
  db.run(`create table if not exists albums (id integer primary key, title text not null, artist_id integer references artists(id), year integer)`);
  const count = db.query(`select count(*) as n from artists`).get() as { n: number };
  if (count.n > 0) return;
  const artists = ["Can", "Neu!", "Faust", "Cluster"];
  const insertArtist = db.prepare(`insert into artists (name) values (?)`);
  for (const name of artists) insertArtist.run(name);
  const albums: Array<[string, number, number]> = [
    ["Tago Mago", 1, 1971],
    ["Ege Bamyasi", 1, 1972],
    ["Future Days", 1, 1973],
    ["Neu!", 2, 1972],
    ["Neu! 75", 2, 1975],
    ["Faust IV", 3, 1973],
    ["Zuckerzeit", 4, 1974],
  ];
  const insertAlbum = db.prepare(`insert into albums (title, artist_id, year) values (?, ?, ?)`);
  for (const [title, artist, year] of albums) insertAlbum.run(title, artist, year);
}

function quoteIdent(name: string): string {
  return `"${name.replaceAll(`"`, `""`)}"`;
}

function tableNames(): string[] {
  const rows = db
    .query(`select name from sqlite_master where type = 'table' and name not like 'sqlite_%' order by name`)
    .all() as Array<{ name: string }>;
  return rows.map((r) => r.name);
}

function tableColumns(table: string): string[] {
  const rows = db.query(`pragma table_info(${quoteIdent(table)})`).all() as Array<{ name: string }>;
  return rows.map((r) => r.name);
}

function requireTable(table: string): void {
  if (!tableNames().includes(table)) throw new Fault("missing", `no such table '${table}'`);
}

function cellText(value: unknown): string {
  if (value === null || value === undefined) return "∅";
  return String(value);
}

function resultGrid(columns: string[], rows: Array<Record<string, unknown>>): Node {
  if (rows.length === 0) return column({ gap: 4 }, text("(no rows)"));
  return column(
    { gap: 4 },
    row({ gap: 16 }, ...columns.map((c) => text(c))),
    ...rows.map((r) => row({ gap: 16 }, ...columns.map((c) => text(cellText(r[c]))))),
  );
}

function tableGrid(table: string): Node {
  const columns = tableColumns(table);
  const rows = db
    .query(`select rowid as _spry_rowid, * from ${quoteIdent(table)} limit ${ROW_LIMIT}`)
    .all() as Array<Record<string, unknown>>;

  return column(
    { gap: 4 },
    text(`${table} (first ${ROW_LIMIT} rows; click a cell to edit)`),
    row({ gap: 16 }, ...columns.map((c) => text(c))),
    ...rows.map((r) =>
      row(
        { gap: 16 },
        ...columns.map((c) =>
          button({
            text: cellText(r[c]),
            on: invoke({
              handler: "edit_cell",
              onResponse: "patch",
              target: "editor",
              body: { table, rowid: String(r["_spry_rowid"]), column: c },
            }),
          }),
        ),
      ),
    ),
  );
}

const app = await spryApp({
  endpoints,
  tree,
  handlers: {
    tables() {
      return column(
        { gap: 6 },
        text("tables"),
        ...tableNames().map((name) =>
          button({
            text: name,
            on: invoke({ handler: "open_table", onResponse: "patch", target: "grid", body: { table: name } }),
          }),
        ),
      );
    },

    open_table(args) {
      const table = args.table as string;
      requireTable(table);
      return tableGrid(table);
    },

    edit_cell(args) {
      const table = args.table as string;
      const rowid = args.rowid as number;
      const col = args.column as string;
      requireTable(table);
      if (!tableColumns(table).includes(col)) throw new Fault("missing", `no such column '${col}'`);

      const current = db
        .query(`select ${quoteIdent(col)} as value from ${quoteIdent(table)} where rowid = ?`)
        .get(rowid) as { value: unknown } | null;
      if (!current) throw new Fault("missing", `no row ${rowid} in '${table}'`);

      return column(
        { gap: 8 },
        text(`editing ${table}[${rowid}].${col}`),
        row(
          { gap: 8, align: "center" },
          input({ name: "value", value: cellText(current.value) }),
          button({
            text: "Save",
            on: invoke({
              handler: "save_cell",
              onResponse: "patch",
              target: "grid",
              body: { table, rowid: String(rowid), column: col },
            }),
          }),
        ),
      );
    },

    save_cell(args) {
      const table = args.table as string;
      const rowid = args.rowid as number;
      const col = args.column as string;
      const value = args.value as string;
      requireTable(table);
      if (!tableColumns(table).includes(col)) throw new Fault("missing", `no such column '${col}'`);

      db.run(`update ${quoteIdent(table)} set ${quoteIdent(col)} = ? where rowid = ?`, [value, rowid]);
      return tableGrid(table);
    },

    exec(args) {
      const sql = args.sql as string;
      try {
        const rows = db.query(sql).all() as Array<Record<string, unknown>>;
        const columns = rows.length ? Object.keys(rows[0]!) : [];
        return resultGrid(columns, rows.slice(0, ROW_LIMIT));
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        return column({ gap: 4 }, text(`SQL error: ${message}`));
      }
    },
  },
});

export default { port: 3000, fetch: app.fetch };
