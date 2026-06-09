// Generated from src/abi/ui.jtd.json by tools/gen-zod.ts. Do not edit.
import { z } from "zod";

export const Direction = z.enum(["row", "column"]);
export const Align = z.enum(["start", "center", "end", "stretch"]);
export const Justify = z.enum(["start", "center", "end", "between"]);
export const Event = z.enum(["click", "submit"]);
export const Swap = z.enum(["inner"]);

export const BoxProps = z.strictObject({
  direction: Direction.optional(),
  gap: z.number().int().optional(),
  padding: z.number().int().optional(),
  align: Align.optional(),
  justify: Justify.optional(),
});

export const Interaction = z.strictObject({
  event: Event,
  action: z.string(),
  target: z.string(),
  swap: Swap.optional(),
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

export type Node =
  | { kind: "box"; id?: string; props?: z.infer<typeof BoxProps>; on?: z.infer<typeof Interaction>; children?: Node[] }
  | { kind: "text"; props: z.infer<typeof TextProps>; id?: string }
  | { kind: "link"; props: z.infer<typeof LinkProps>; id?: string }
  | { kind: "input"; props: z.infer<typeof InputProps>; id?: string }
  | { kind: "button"; props: z.infer<typeof ButtonProps>; id?: string; on?: z.infer<typeof Interaction> };

export const Node: z.ZodType<Node> = z.lazy(() =>
  z.discriminatedUnion("kind", [
    z.strictObject({ kind: z.literal("box"), id: z.string().optional(), props: BoxProps.optional(), on: Interaction.optional(), children: z.array(Node).optional() }),
    z.strictObject({ kind: z.literal("text"), props: TextProps, id: z.string().optional() }),
    z.strictObject({ kind: z.literal("link"), props: LinkProps, id: z.string().optional() }),
    z.strictObject({ kind: z.literal("input"), props: InputProps, id: z.string().optional() }),
    z.strictObject({ kind: z.literal("button"), props: ButtonProps, id: z.string().optional(), on: Interaction.optional() }),
  ]),
);
