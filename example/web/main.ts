import { mount } from "spry/host/web";
import { domBackend } from "spry/backend/web";

const root = document.getElementById("app");
if (!root) throw new Error("missing #app mount point");

const instance = await mount("/runtime.wasm", (iface) => domBackend(root, iface));

const tree = await fetch("/tree").then((r) => r.text());
const rc = instance.render(tree);
if (rc !== 0) throw new Error(`render failed with code ${rc}`);
