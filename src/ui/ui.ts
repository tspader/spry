import type { Node } from "spry/schema";
import { box } from "../abi/ui.gen";
import type { BoxOpts } from "../abi/ui.gen";

export * from "../abi/ui.gen";

export function row(opts: Omit<BoxOpts, "direction"> = {}, ...children: Node[]): Node {
  return box({ ...opts, direction: "row" }, ...children);
}

export function column(opts: Omit<BoxOpts, "direction"> = {}, ...children: Node[]): Node {
  return box({ ...opts, direction: "column" }, ...children);
}
