// Generated from src/abi/ui.jtd.json by tools/gen-zod.ts. Do not edit.
import { z } from "zod";

export const Direction = z.enum(["row", "column"]);
export const Align = z.enum(["start", "center", "end", "stretch"]);
export const Justify = z.enum(["start", "center", "end", "between"]);
export const Event = z.enum(["click", "submit"]);
export const OnResponse = z.enum(["patch", "ignore"]);
export const Code = z.enum(["invalid", "unauthenticated", "denied", "missing", "conflict", "failed", "unavailable", "timeout", "cancelled"]);
export const Type = z.enum(["string", "boolean", "int8", "uint8", "int16", "uint16", "int32", "uint32", "float32", "float64"]);

export const BoxProps = z.strictObject({
  direction: Direction.optional(),
  gap: z.number().int().optional(),
  padding: z.number().int().optional(),
  align: Align.optional(),
  justify: Justify.optional(),
});

export const TextProps = z.strictObject({
  text: z.string(),
});

export const LinkProps = z.strictObject({
  text: z.string(),
  href: z.string(),
});

export const InputProps = z.strictObject({
  name: z.string(),
  value: z.string().optional(),
  placeholder: z.string().optional(),
});

export const ButtonProps = z.strictObject({
  text: z.string(),
});

export const FaultIssue = z.strictObject({
  path: z.string(),
  code: z.string(),
});

export const Fault = z.strictObject({
  code: Code,
  message: z.string().optional(),
  issues: z.array(FaultIssue).optional(),
});

export const EndpointArg = z.strictObject({
  type: Type,
});

export const EndpointArgs = z.strictObject({
  properties: z.record(z.string(), EndpointArg).optional(),
  optionalProperties: z.record(z.string(), EndpointArg).optional(),
});

export const Endpoint = z.strictObject({
  args: EndpointArgs.optional(),
});

export const Endpoints = z.record(z.string(), Endpoint);

export type Interaction =
  | { action: "invoke"; event: z.infer<typeof Event>; handler: string; onResponse: z.infer<typeof OnResponse>; target?: string; body?: Record<string, string> };

export type Node =
  | { kind: "box"; id?: string; props?: z.infer<typeof BoxProps>; on?: Interaction; children?: Node[] }
  | { kind: "text"; props: z.infer<typeof TextProps>; id?: string }
  | { kind: "link"; props: z.infer<typeof LinkProps>; id?: string }
  | { kind: "input"; props: z.infer<typeof InputProps>; id?: string }
  | { kind: "button"; props: z.infer<typeof ButtonProps>; id?: string; on?: Interaction };

export const Interaction: z.ZodType<Interaction> = z.lazy(() =>
  z.discriminatedUnion("action", [
    z.strictObject({ action: z.literal("invoke"), event: Event, handler: z.string(), onResponse: OnResponse, target: z.string().optional(), body: z.record(z.string(), z.string()).optional() }),
  ]),
);

export const Node: z.ZodType<Node> = z.lazy(() =>
  z.discriminatedUnion("kind", [
    z.strictObject({ kind: z.literal("box"), id: z.string().optional(), props: BoxProps.optional(), on: Interaction.optional(), children: z.array(Node).optional() }),
    z.strictObject({ kind: z.literal("text"), props: TextProps, id: z.string().optional() }),
    z.strictObject({ kind: z.literal("link"), props: LinkProps, id: z.string().optional() }),
    z.strictObject({ kind: z.literal("input"), props: InputProps, id: z.string().optional() }),
    z.strictObject({ kind: z.literal("button"), props: ButtonProps, id: z.string().optional(), on: Interaction.optional() }),
  ]),
);
