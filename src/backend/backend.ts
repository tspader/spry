export interface HostIface {
  dispatch(token: number): void;
  deliver(token: number, json: string): void;
}

export interface Backend<Node> {
  capabilities(): number;
  createElement(kind: number): Node;
  setAttr(node: Node, attr: number, value: number): void;
  setAttrStr(node: Node, attr: number, value: string): void;
  appendChild(parent: Node, child: Node): void;
  setRoot(node: Node): void;
  onEvent(node: Node, event: number, token: number): void;
  submit(token: number, action: string, body: string): void;
  clearChildren(node: Node): void;
  getValue(node: Node): string;
  fatal(message: string): void;
}
