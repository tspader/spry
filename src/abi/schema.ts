import { z } from "zod";

export const Direction = z.enum(["row", "column"]);
export const Align = z.enum(["start", "center", "end", "stretch"]);
export const Justify = z.enum(["start", "center", "end", "between"]);

export const BoxProps = z.object({
  direction: Direction.optional(),
  gap: z.number().int().optional(),
  padding: z.number().int().optional(),
  align: Align.optional(),
  justify: Justify.optional(),
});

export const TextProps = z.object({
  text: z.string(),
});

export const LinkProps = z.object({
  text: z.string(),
  href: z.string(),
});

export const InputProps = z.object({
  name: z.string(),
  value: z.string().optional(),
  placeholder: z.string().optional(),
});

export const ButtonProps = z.object({
  text: z.string(),
});

export const Interaction = z.object({
  event: z.enum(["click", "submit"]),
  action: z.string(),
  target: z.string(),
  swap: z.literal("inner").optional(),
});

export type Node =
  | { kind: "box"; id?: string; props?: z.infer<typeof BoxProps>; on?: z.infer<typeof Interaction>; children?: Node[] }
  | { kind: "text"; id?: string; props: z.infer<typeof TextProps> }
  | { kind: "link"; id?: string; props: z.infer<typeof LinkProps> }
  | { kind: "input"; id?: string; props: z.infer<typeof InputProps> }
  | { kind: "button"; id?: string; props: z.infer<typeof ButtonProps>; on?: z.infer<typeof Interaction> };

export const Node: z.ZodType<Node> = z.lazy(() =>
  z.discriminatedUnion("kind", [
    z.object({ kind: z.literal("box"), id: z.string().optional(), props: BoxProps.optional(), on: Interaction.optional(), children: z.array(Node).optional() }),
    z.object({ kind: z.literal("text"), id: z.string().optional(), props: TextProps }),
    z.object({ kind: z.literal("link"), id: z.string().optional(), props: LinkProps }),
    z.object({ kind: z.literal("input"), id: z.string().optional(), props: InputProps }),
    z.object({ kind: z.literal("button"), id: z.string().optional(), props: ButtonProps, on: Interaction.optional() }),
  ]),
);
