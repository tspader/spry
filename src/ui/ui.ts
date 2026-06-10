import type { Interaction, Node } from "spry/schema";

export type BoxOpts = {
  id?: string;
  on?: Interaction;
  direction?: "row" | "column";
  gap?: number;
  padding?: number;
  align?: "start" | "center" | "end" | "stretch";
  justify?: "start" | "center" | "end" | "between";
};

export function box(opts: BoxOpts, ...children: Node[]): Node {
  const { id, on, ...props } = opts;
  return {
    kind: "box",
    ...(id !== undefined && { id }),
    ...(on !== undefined && { on }),
    props,
    children,
  };
}

export function row(opts: Omit<BoxOpts, "direction">, ...children: Node[]): Node {
  return box({ ...opts, direction: "row" }, ...children);
}

export function column(opts: Omit<BoxOpts, "direction">, ...children: Node[]): Node {
  return box({ ...opts, direction: "column" }, ...children);
}

export function text(value: string, id?: string): Node {
  return { kind: "text", ...(id !== undefined && { id }), props: { text: value } };
}

export function link(value: string, href: string, id?: string): Node {
  return { kind: "link", ...(id !== undefined && { id }), props: { text: value, href } };
}

export type InputOpts = {
  id?: string;
  value?: string;
  placeholder?: string;
};

export function input(name: string, opts: InputOpts = {}): Node {
  const { id, ...rest } = opts;
  return { kind: "input", ...(id !== undefined && { id }), props: { name, ...rest } };
}

export function button(value: string, on?: Interaction, id?: string): Node {
  return {
    kind: "button",
    ...(id !== undefined && { id }),
    ...(on !== undefined && { on }),
    props: { text: value },
  };
}

export type InvokeOpts = {
  event?: "click" | "submit";
  handler: string;
  onResponse?: "patch" | "ignore";
  target?: string;
  body?: Record<string, string>;
};

export function invoke(opts: InvokeOpts): Interaction {
  const { event = "click", handler, onResponse = "ignore", target, body } = opts;
  return {
    action: "invoke",
    event,
    handler,
    onResponse,
    ...(target !== undefined && { target }),
    ...(body !== undefined && { body }),
  };
}
