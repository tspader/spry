import type { Backend, HostIface } from "spry/backend";
import * as abi from "spry/abi";

const ALIGN: Record<number, string> = {
  [abi.ALIGN_START]: "flex-start",
  [abi.ALIGN_CENTER]: "center",
  [abi.ALIGN_END]: "flex-end",
  [abi.ALIGN_STRETCH]: "stretch",
};

const JUSTIFY: Record<number, string> = {
  [abi.JUSTIFY_START]: "flex-start",
  [abi.JUSTIFY_CENTER]: "center",
  [abi.JUSTIFY_END]: "flex-end",
  [abi.JUSTIFY_BETWEEN]: "space-between",
};

const EVENT_NAME: Record<number, string> = {
  [abi.EVENT_CLICK]: "click",
  [abi.EVENT_SUBMIT]: "submit",
};

export function domBackend(root: HTMLElement, iface: HostIface): Backend<HTMLElement> {
  return {
    capabilities: () =>
      abi.HOST_CAP_ELEMENT | abi.HOST_CAP_TEXT | abi.HOST_CAP_FLEX | abi.HOST_CAP_LINK |
      abi.HOST_CAP_INPUT | abi.HOST_CAP_EVENTS | abi.HOST_CAP_SUBMIT,

    createElement(kind) {
      switch (kind) {
        case abi.EL_BOX: {
          const el = document.createElement("div");
          el.style.display = "flex";
          return el;
        }
        case abi.EL_TEXT:
          return document.createElement("span");
        case abi.EL_LINK:
          return document.createElement("a");
        case abi.EL_INPUT:
          return document.createElement("input");
        case abi.EL_BUTTON:
          return document.createElement("button");
        default:
          throw new Error(`dom-backend: unknown element kind ${kind}`);
      }
    },

    setAttr(node, attr, value) {
      switch (attr) {
        case abi.ATTR_DIRECTION:
          node.style.flexDirection = value === abi.DIR_COLUMN ? "column" : "row";
          break;
        case abi.ATTR_GAP:
          node.style.gap = `${value}px`;
          break;
        case abi.ATTR_PADDING:
          node.style.padding = `${value}px`;
          break;
        case abi.ATTR_ALIGN:
          node.style.alignItems = ALIGN[value] ?? "flex-start";
          break;
        case abi.ATTR_JUSTIFY:
          node.style.justifyContent = JUSTIFY[value] ?? "flex-start";
          break;
      }
    },

    setAttrStr(node, attr, value) {
      switch (attr) {
        case abi.SATTR_TEXT:
          node.textContent = value;
          break;
        case abi.SATTR_HREF:
          (node as HTMLAnchorElement).href = value;
          break;
        case abi.SATTR_VALUE:
          (node as HTMLInputElement).value = value;
          break;
        case abi.SATTR_NAME:
          (node as HTMLInputElement).name = value;
          break;
        case abi.SATTR_PLACEHOLDER:
          (node as HTMLInputElement).placeholder = value;
          break;
      }
    },

    appendChild(parent, child) {
      parent.appendChild(child);
    },

    setRoot(node) {
      root.replaceChildren(node);
    },

    onEvent(node, event, token) {
      const name = EVENT_NAME[event] ?? "click";
      node.addEventListener(name, (e) => {
        e.preventDefault();
        iface.dispatch(token);
      });
    },

    submit(token, action, body) {
      fetch(action, {
        method: "POST",
        headers: { "content-type": "application/x-www-form-urlencoded" },
        body,
      })
        .then((r) => r.text())
        .then((json) => iface.deliver(token, json))
        .catch((err) => console.error("submit failed", err));
    },

    clearChildren(node) {
      node.replaceChildren();
    },

    getValue(node) {
      return (node as HTMLInputElement).value ?? "";
    },

    fatal(message) {
      throw new Error(`runtime fatal: ${message}`);
    },
  };
}
