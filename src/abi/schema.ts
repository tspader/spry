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

export type Node =
  | { kind: "box"; props?: z.infer<typeof BoxProps>; children?: Node[] }
  | { kind: "text"; props: z.infer<typeof TextProps> }
  | { kind: "link"; props: z.infer<typeof LinkProps> };

export const Node: z.ZodType<Node> = z.lazy(() =>
  z.discriminatedUnion("kind", [
    z.object({ kind: z.literal("box"), props: BoxProps.optional(), children: z.array(Node).optional() }),
    z.object({ kind: z.literal("text"), props: TextProps }),
    z.object({ kind: z.literal("link"), props: LinkProps }),
  ]),
);
