import type { Node } from "../abi/schema";
import { INTRINSICS, textNode } from "../abi/jsx.gen";
import type { IntrinsicElements as SpryIntrinsicElements } from "../abi/jsx.gen";

type Props = Record<string, unknown>;

function appendChild(child: unknown, out: Node[]): void {
  if (child === null || child === undefined || typeof child === "boolean") return;
  if (Array.isArray(child)) {
    for (const c of child) appendChild(c, out);
    return;
  }
  if (typeof child === "string" || typeof child === "number") {
    out.push(textNode(String(child)));
    return;
  }
  out.push(child as Node);
}

function appendText(child: unknown, out: string[], tag: string): void {
  if (child === null || child === undefined || typeof child === "boolean") return;
  if (Array.isArray(child)) {
    for (const c of child) appendText(c, out, tag);
    return;
  }
  if (typeof child === "string" || typeof child === "number") {
    out.push(String(child));
    return;
  }
  throw new Error(`spry: <${tag}> children must be text`);
}

export function Fragment(props: { children?: unknown }): Node[] {
  const out: Node[] = [];
  appendChild(props.children, out);
  return out;
}

export function jsx(tag: unknown, props: Props | null, _key?: unknown): Node {
  const all = props ?? {};
  if (typeof tag === "function") return (tag as (p: Props) => Node)(all);

  const def = INTRINSICS[String(tag)];
  if (!def) throw new Error(`spry: unknown element <${String(tag)}>`);

  const { children, id, on, ...rest } = all;
  const out: Props = { ...rest, ...def.preset };

  const kids: Node[] = [];
  if (def.sugar !== undefined && children !== undefined) {
    if (out[def.sugar] !== undefined) throw new Error(`spry: <${String(tag)}> has both '${def.sugar}' and children`);
    const parts: string[] = [];
    appendText(children, parts, String(tag));
    out[def.sugar] = parts.join("");
  } else {
    appendChild(children, kids);
  }

  const node: Props = { kind: def.kind };
  if (id !== undefined) node.id = id;
  if (on !== undefined) node.on = on;
  node.props = out;
  if (kids.length) {
    if (!def.container) throw new Error(`spry: <${String(tag)}> cannot have children`);
    node.children = kids;
  }
  return node as unknown as Node;
}

export const jsxs = jsx;

export namespace JSX {
  export type Element = Node;
  export interface IntrinsicElements extends SpryIntrinsicElements {}
  export interface ElementChildrenAttribute {
    children: unknown;
  }
}
