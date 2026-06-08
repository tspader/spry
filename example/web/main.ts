import { mount } from "../../src/host/web/host.ts";
import { domBackend } from "../../src/backend/web/dom-backend.ts";

const root = document.getElementById("app");
if (!root) throw new Error("missing #app mount point");

const instance = await mount("/runtime.wasm", (iface) => domBackend(root, iface));

const tree = await fetch("/tree").then((r) => r.text());
const rc = instance.render(tree);
if (rc !== 0) throw new Error(`render failed with code ${rc}`);
