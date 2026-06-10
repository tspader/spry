// Generated from src/abi/*.jtd.json by tools/gen-types.ts. Do not edit.

export type Direction = "row" | "column";
export type Align = "start" | "center" | "end" | "stretch";
export type Justify = "start" | "center" | "end" | "between";
export type Event = "click" | "submit";
export type OnResponse = "ignore" | "patch";
export type Code = "invalid" | "unauthenticated" | "denied" | "missing" | "conflict" | "failed" | "unavailable" | "timeout" | "cancelled";
export type Type = "string" | "boolean" | "int8" | "uint8" | "int16" | "uint16" | "int32" | "uint32" | "float32" | "float64";

export type BoxProps = {
  direction?: Direction;
  gap?: number;
  padding?: number;
  align?: Align;
  justify?: Justify;
};

export type TextProps = {
  text: string;
};

export type LinkProps = {
  text: string;
  href: string;
};

export type InputProps = {
  name: string;
  value?: string;
  placeholder?: string;
};

export type ButtonProps = {
  text: string;
};

export type FaultIssue = {
  path: string;
  code: string;
};

export type Fault = {
  code: Code;
  message?: string;
  issues?: FaultIssue[];
};

export type EndpointArg = {
  type: Type;
};

export type EndpointArgs = {
  properties?: Record<string, EndpointArg>;
  optionalProperties?: Record<string, EndpointArg>;
};

export type Endpoint = {
  args?: EndpointArgs;
};

export type Endpoints = Record<string, Endpoint>;

export type Interaction =
  | { action: "invoke"; event: Event; handler: string; onResponse: OnResponse; target?: string; body?: Record<string, string> };

export type Node =
  | { kind: "box"; id?: string; props?: BoxProps; on?: Interaction; children?: Node[] }
  | { kind: "text"; props: TextProps; id?: string }
  | { kind: "link"; props: LinkProps; id?: string }
  | { kind: "input"; props: InputProps; id?: string }
  | { kind: "button"; props: ButtonProps; id?: string; on?: Interaction };
