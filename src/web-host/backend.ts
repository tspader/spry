export interface Backend<Node> {
  capabilities(): number;
  createElement(kind: number): Node;
  setAttr(node: Node, attr: number, value: number): void;
  setAttrStr(node: Node, attr: number, value: string): void;
  appendChild(parent: Node, child: Node): void;
  setRoot(node: Node): void;
  fatal(message: string): void;
}
