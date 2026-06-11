// Generated from src/abi/ui.jtd.json by tools/gen-types.ts. Do not edit.
import type { Align, Direction, Event, Interaction, Justify, Node, OnResponse } from "./schema";

export type BoxOpts = {
  id?: string;
  on?: Interaction;
  direction?: Direction;
  gap?: number;
  padding?: number;
  align?: Align;
  justify?: Justify;
};

export type TextOpts = {
  id?: string;
  text: string;
};

export type LinkOpts = {
  id?: string;
  text: string;
  href: string;
};

export type InputOpts = {
  id?: string;
  name: string;
  value?: string;
  placeholder?: string;
};

export type ButtonOpts = {
  id?: string;
  on?: Interaction;
  text: string;
};

export type InvokeOpts = {
  event?: Event;
  handler: string;
  onResponse?: OnResponse;
  target?: string;
  body?: Record<string, string | number | boolean>;
};

export function box(opts: BoxOpts = {}, ...children: Node[]): Node {
  const o = opts;
  const { id, on, ...props } = o;
  return {
    kind: "box",
    ...(id !== undefined && { id }),
    ...(on !== undefined && { on }),
    props,
    ...(children.length > 0 && { children }),
  };
}

export function text(opts: string | TextOpts): Node {
  const o = typeof opts === "string" ? { text: opts } : opts;
  const { id, ...props } = o;
  return {
    kind: "text",
    ...(id !== undefined && { id }),
    props,
  };
}

export function link(opts: LinkOpts): Node {
  const o = opts;
  const { id, ...props } = o;
  return {
    kind: "link",
    ...(id !== undefined && { id }),
    props,
  };
}

export function input(opts: InputOpts): Node {
  const o = opts;
  const { id, ...props } = o;
  return {
    kind: "input",
    ...(id !== undefined && { id }),
    props,
  };
}

export function button(opts: string | ButtonOpts): Node {
  const o = typeof opts === "string" ? { text: opts } : opts;
  const { id, on, ...props } = o;
  return {
    kind: "button",
    ...(id !== undefined && { id }),
    ...(on !== undefined && { on }),
    props,
  };
}

export function row(opts: Omit<BoxOpts, "direction"> = {}, ...children: Node[]): Node {
  return box({ ...opts, direction: "row" }, ...children);
}

export function column(opts: Omit<BoxOpts, "direction"> = {}, ...children: Node[]): Node {
  return box({ ...opts, direction: "column" }, ...children);
}

export function invoke(opts: InvokeOpts): Interaction {
  return {
    action: "invoke",
    event: opts.event ?? "click",
    handler: opts.handler,
    onResponse: opts.onResponse ?? "ignore",
    ...(opts.target !== undefined && { target: opts.target }),
    ...(opts.body !== undefined && { body: Object.fromEntries(Object.entries(opts.body).map(([k, v]) => [k, String(v)])) }),
  };
}

export function patch(handler: string, target: string, body?: Record<string, string | number | boolean>): Interaction {
  return invoke({ handler, target, ...(body !== undefined && { body }), onResponse: "patch" });
}
