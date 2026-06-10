export interface HostIface {
  dispatch(token: number): void;
  deliver(token: number, outcome: number, body: string): void;
}

export interface Backend<Node> {
  capabilities(): number;
  createElement(kind: number): Node;
  setDirection(node: Node, value: number): void;
  setGap(node: Node, value: number): void;
  setPadding(node: Node, value: number): void;
  setAlign(node: Node, value: number): void;
  setJustify(node: Node, value: number): void;
  setText(node: Node, value: string): void;
  setHref(node: Node, value: string): void;
  setValue(node: Node, value: string): void;
  setName(node: Node, value: string): void;
  setPlaceholder(node: Node, value: string): void;
  appendChild(parent: Node, child: Node): void;
  setRoot(node: Node): void;
  onEvent(node: Node, event: number, token: number): void;
  invoke(token: number, handler: string, body: string): void;
  report(token: number, fault: string): void;
  clearChildren(node: Node): void;
  getValue(node: Node): string;
  fatal(message: string): void;
}
