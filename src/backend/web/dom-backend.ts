import type { Backend, HostIface } from "spry/backend";
import * as abi from "spry/abi";
import * as ui from "spry/ui-enums";

const ALIGN: Record<number, string> = {
  [ui.SPRY_ALIGN_START]: "flex-start",
  [ui.SPRY_ALIGN_CENTER]: "center",
  [ui.SPRY_ALIGN_END]: "flex-end",
  [ui.SPRY_ALIGN_STRETCH]: "stretch",
};

const JUSTIFY: Record<number, string> = {
  [ui.SPRY_JUSTIFY_START]: "flex-start",
  [ui.SPRY_JUSTIFY_CENTER]: "center",
  [ui.SPRY_JUSTIFY_END]: "flex-end",
  [ui.SPRY_JUSTIFY_BETWEEN]: "space-between",
};

const EVENT_NAME: Record<number, string> = {
  [ui.SPRY_EVENT_CLICK]: "click",
  [ui.SPRY_EVENT_SUBMIT]: "submit",
};

export function domBackend(root: HTMLElement, iface: HostIface): Backend<HTMLElement> {
  return {
    capabilities: () =>
      abi.HOST_CAP_ELEMENT | abi.HOST_CAP_TEXT | abi.HOST_CAP_FLEX | abi.HOST_CAP_LINK |
      abi.HOST_CAP_INPUT | abi.HOST_CAP_EVENTS | abi.HOST_CAP_SUBMIT,

    createElement(kind) {
      switch (kind) {
        case ui.SPRY_NODE_KIND_BOX: {
          const el = document.createElement("div");
          el.style.display = "flex";
          return el;
        }
        case ui.SPRY_NODE_KIND_TEXT:
          return document.createElement("span");
        case ui.SPRY_NODE_KIND_LINK:
          return document.createElement("a");
        case ui.SPRY_NODE_KIND_INPUT:
          return document.createElement("input");
        case ui.SPRY_NODE_KIND_BUTTON:
          return document.createElement("button");
        default:
          throw new Error(`dom-backend: unknown element kind ${kind}`);
      }
    },

    setDirection(node, value) {
      node.style.flexDirection = value === ui.SPRY_DIRECTION_COLUMN ? "column" : "row";
    },

    setGap(node, value) {
      node.style.gap = `${value}px`;
    },

    setPadding(node, value) {
      node.style.padding = `${value}px`;
    },

    setAlign(node, value) {
      node.style.alignItems = ALIGN[value] ?? "flex-start";
    },

    setJustify(node, value) {
      node.style.justifyContent = JUSTIFY[value] ?? "flex-start";
    },

    setText(node, value) {
      node.textContent = value;
    },

    setHref(node, value) {
      (node as HTMLAnchorElement).href = value;
    },

    setValue(node, value) {
      (node as HTMLInputElement).value = value;
    },

    setName(node, value) {
      (node as HTMLInputElement).name = value;
    },

    setPlaceholder(node, value) {
      (node as HTMLInputElement).placeholder = value;
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
