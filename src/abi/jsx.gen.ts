// Generated from src/abi/ui.jtd.json by tools/gen-types.ts. Do not edit.
import type { Node } from "./schema";
import type { BoxOpts, ButtonOpts, InputOpts, LinkOpts, TextOpts } from "./ui.gen";

export type JsxChild = Node | string | number | boolean | null | undefined | JsxChild[];
export type JsxText = string | number | boolean | null | undefined | JsxText[];

export interface IntrinsicElements {
  box: BoxOpts & { children?: JsxChild };
  text: (TextOpts & { children?: never }) | (Omit<TextOpts, "text"> & { children: JsxText });
  link: LinkOpts & { children?: never };
  input: InputOpts & { children?: never };
  button: (ButtonOpts & { children?: never }) | (Omit<ButtonOpts, "text"> & { children: JsxText });
  row: Omit<BoxOpts, "direction"> & { children?: JsxChild };
  column: Omit<BoxOpts, "direction"> & { children?: JsxChild };
}

export type Intrinsic = {
  kind: string;
  container: boolean;
  preset?: Record<string, unknown>;
  sugar?: string;
};

export const INTRINSICS: Record<string, Intrinsic> = {
  box: { kind: "box", container: true },
  text: { kind: "text", container: false, sugar: "text" },
  link: { kind: "link", container: false },
  input: { kind: "input", container: false },
  button: { kind: "button", container: false, sugar: "text" },
  row: { kind: "box", container: true, preset: {"direction":"row"} },
  column: { kind: "box", container: true, preset: {"direction":"column"} },
};

export function textNode(text: string): Node {
  return { kind: "text", props: { text: text } };
}
