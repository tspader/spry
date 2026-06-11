export const NODE = "node";
export const INTERACTION = "interaction";

export const SUGAR: Record<string, { of: string; preset: Record<string, string> }> = {
  row: { of: "box", preset: { direction: "row" } },
  column: { of: "box", preset: { direction: "column" } },
};

export const CONTENT: Record<string, string> = {
  text: "text",
  button: "text",
  link: "text",
  input: "name",
};

export const AUTO_TEXT = { kind: "text", prop: "text" };

export const INVOKE_SUGAR: Record<string, { of: string; preset: Record<string, string>; args: string[] }> = {
  patch: { of: "invoke", preset: { onResponse: "patch" }, args: ["handler", "target", "body"] },
};
