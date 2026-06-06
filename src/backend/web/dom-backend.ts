import type { Backend } from "../backend.ts";
import * as abi from "../../abi/abi.gen.ts";

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

export function domBackend(root: HTMLElement): Backend<HTMLElement> {
  return {
    capabilities: () => abi.HOST_CAP_ELEMENT | abi.HOST_CAP_TEXT | abi.HOST_CAP_FLEX | abi.HOST_CAP_LINK,

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
      }
    },

    appendChild(parent, child) {
      parent.appendChild(child);
    },

    setRoot(node) {
      root.replaceChildren(node);
    },

    fatal(message) {
      throw new Error(`runtime fatal: ${message}`);
    },
  };
}
