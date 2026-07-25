// Minimal DOM helpers for the crew consoles.
//
// The one rule these exist to enforce: **a control's DOM node is created once and
// never replaced.** The previous console rebuilt `panel.innerHTML` ten times a second
// (the markup embeds live speed/charge/range values, so it always differed), which
// destroyed and recreated every button. Browsers cancel a click when the mousedown
// target is removed before mouseup, so most real presses were silently dropped —
// worst on a phone, where a tap lasts 80-200ms. Synthetic test clicks never reproduced
// it because they press and release in under a millisecond.
//
// So: build the tree once, then only ever write text, attributes and classes.

export interface ElOpts {
  class?: string;
  text?: string;
  /** Applied as data-* attributes. */
  data?: Record<string, string>;
  attrs?: Record<string, string>;
  children?: (Node | null)[];
}

export const el = <K extends keyof HTMLElementTagNameMap>(
  tag: K,
  opts: ElOpts = {},
): HTMLElementTagNameMap[K] => {
  const node = document.createElement(tag);
  if (opts.class) node.className = opts.class;
  if (opts.text !== undefined) node.textContent = opts.text;
  for (const [k, v] of Object.entries(opts.data ?? {})) node.dataset[k] = v;
  for (const [k, v] of Object.entries(opts.attrs ?? {})) node.setAttribute(k, v);
  for (const child of opts.children ?? []) if (child) node.appendChild(child);
  return node;
};

/** Writes only when the value actually differs, so the DOM isn't dirtied 60x/second. */
export const setText = (node: Element, text: string): void => {
  if (node.textContent !== text) node.textContent = text;
};

export const setDisabled = (node: HTMLButtonElement | HTMLInputElement, disabled: boolean): void => {
  if (node.disabled !== disabled) node.disabled = disabled;
};

export const setFlag = (node: Element, name: string, on: boolean): void => {
  node.classList.toggle(name, on);
};

export const setWidth = (node: HTMLElement, fraction: number): void => {
  const pct = `${Math.max(0, Math.min(1, fraction)) * 100}%`;
  if (node.style.width !== pct) node.style.width = pct;
};

export const setHidden = (node: HTMLElement, hidden: boolean): void => {
  if (node.hidden !== hidden) node.hidden = hidden;
};

/**
 * Reconciles a keyed list against a container, reusing rows by key.
 *
 * Rows are only created for genuinely new keys and only removed for genuinely gone
 * ones, so a contact you are about to tap does not get rebuilt underneath your finger
 * just because its range readout ticked over. Order is only touched when the key order
 * actually changed — contact ids are stable and monotonic, so in a fight this is
 * effectively never.
 */
export const syncList = <T>(
  container: HTMLElement,
  items: readonly T[],
  keyOf: (item: T) => string,
  create: (item: T) => HTMLElement,
  update: (node: HTMLElement, item: T) => void,
  cache: Map<string, HTMLElement>,
): void => {
  const live = new Set<string>();

  items.forEach((item, i) => {
    const key = keyOf(item);
    live.add(key);

    let node = cache.get(key);
    if (!node) {
      node = create(item);
      node.dataset['key'] = key;
      cache.set(key, node);
    }
    update(node, item);

    // Only move a node if it isn't already in the right slot.
    if (container.children[i] !== node) {
      container.insertBefore(node, container.children[i] ?? null);
    }
  });

  for (const [key, node] of cache) {
    if (live.has(key)) continue;
    node.remove();
    cache.delete(key);
  }
};
