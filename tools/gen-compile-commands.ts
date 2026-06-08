// Writes compile_commands.json from "file::flags" specs passed on argv. The
// Makefile owns the flag sets and hands them here, so this script only formats
// JSON. arguments[0] is "clang" (not the zig path) so clangd applies the flags
// with its own embedded clang instead of trying to interrogate `zig cc`.
const root = process.cwd();

const db = process.argv.slice(2).map((spec) => {
  const sep = spec.indexOf("::");
  if (sep < 0) throw new Error(`gen-compile-commands: bad spec '${spec}'`);
  const file = spec.slice(0, sep);
  const flags = spec.slice(sep + 2).trim().split(/\s+/).filter(Boolean);
  return {
    directory: root,
    file: `${root}/${file}`,
    arguments: ["clang", ...flags, "-c", `${root}/${file}`],
  };
});

await Bun.write("compile_commands.json", JSON.stringify(db, null, 2) + "\n");
console.log(`gen-compile-commands: wrote ${db.length} entries`);
