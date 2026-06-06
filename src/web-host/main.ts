import { mount } from "./host.ts";
import { domBackend } from "./dom-backend.ts";

const root = document.getElementById("app");
if (!root) throw new Error("missing #app mount point");

const backend = domBackend(root);
const instance = await mount("/runtime.wasm", backend);

const tree = await fetch("/tree").then((r) => r.text());
const rc = instance.render(tree);
if (rc !== 0) throw new Error(`render failed with code ${rc}`);
