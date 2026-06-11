import { invoke, patch } from "spry/ui";

const tree = (
  <column gap={4} id="root">
    <text>spry</text>
    <link text="docs" href="https://example.com" />
    <row gap={16} align="center">
      <input name="q" placeholder="search" />
      <button
        text="Go"
        on={invoke({
          event: "submit",
          handler: "search",
          onResponse: "patch",
          target: "results",
          body: { q: "x", page: "1" },
        })}
      />
    </row>
    <button text="Refresh" on={patch("refresh", "root", { count: 3 })} />
    <box />
    <text>ready {2} go</text>
    <text id="status">status text</text>
  </column>
);

const out = process.argv[2];
if (!out) throw new Error("usage: bun run conformance.tsx <out.json>");
await Bun.write(out, JSON.stringify(tree));
