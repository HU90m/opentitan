class LowriscBlock extends HTMLElement {
  static observedAttributes = ["pos", "center"];

  #observer = null;

  constructor() {
    super();
    this.#observer = new ResizeObserver(() => this.update());
  }

  attributeChangedCallback() {
    this.update();
  }

  connectedCallback() {
    this.update();
    this.#observer.observe(this);
  }

  disconnectedCallback() {
    this.#observer.disconnect();
  }

  setCssVariable(name, value) {
    this.style[value ? "setProperty" : "removeProperty"](name, value);
  }

  update() {
    this.classList.add("lowrisc-block");

    const [x, y, w, h] = (this.getAttribute("pos") || "0").split(/\s+/);

    this.setCssVariable("--block-top", y);
    this.setCssVariable("--block-left", x);
    this.setCssVariable("--block-width", w);
    this.setCssVariable("--block-height", h);

    const center = this.getAttribute("center") || "";
    this.setCssVariable("--offset-top", center.includes("y") && h ? - h / 2 : 0);
    this.setCssVariable("--offset-left", center.includes("x") && w ? - w / 2 : 0);
  }
}

customElements.define('lowrisc-block', LowriscBlock);

class LowriscCrossbar extends LowriscBlock {
  static observedAttributes = [...super.observedAttributes, "length"];

  #container = null;

  connectedCallback() {
    this.#container = document.createElement("div");
    this.#container.style.position = "absolute";
    this.#container.style.top = "0";
    this.#container.style.left = "0";
    this.#container.style.bottom = "0";
    this.#container.style.right = "0";
    this.appendChild(this.#container);

    super.connectedCallback();
  }

  update() {
    super.update();
    this.classList.add("lowrisc-crossbar");

    if (!this.#container) return;

    const [s, e] = (this.getAttribute("length") || "").split(/\s+/);
    const width = this.offsetWidth;
    const height = this.offsetHeight;

    const start = s ? parseFloat(s) : 0.05;
    const end = e ? parseFloat(e) : 0.35;

    this.#container.innerHTML = `
      <svg xmlns="http://www.w3.org/2000/svg"
        overflow="visible"
        vector-effect="non-scaling-stroke"
        shape-rendering="geometricPrecision"
        style="width: 100%; height: 100%;"
        viewBox="0 0 ${width} ${height}"
      >
        <path d="
          M${width * start} ${height * start}
          L${width * end} ${height * end}
        " />
        <path d="
          M${width * (1 - start)} ${height * start}
          L${width * (1 - end)} ${height * end}
        " />
        <path d="
          M${width * (1 - start)} ${height * (1 - start)}
          L${width * (1 - end)} ${height * (1 - end)}
        " />
        <path d="
          M${width * start} ${height * (1 - start)}
          L${width * end} ${height * (1 - end)}
        " />
      </svg>
    `;
  };
}

customElements.define('lowrisc-crossbar', LowriscCrossbar);

class LowriscArrow extends LowriscBlock {
  static observedAttributes = [...super.observedAttributes, "horizontal", "head"];

  update() {
    super.update();
    this.classList.add("lowrisc-arrow");

    const horizontal = this.hasAttribute("horizontal");
    const [overhang_ratio = 1, height_ratio = 0.8] = (this.getAttribute("head") || "1 0.6").split(/\s+/);

    const width = horizontal ? this.offsetHeight : this.offsetWidth;
    const length = horizontal ? this.offsetWidth : this.offsetHeight;
    const overhang = width * overhang_ratio / 2;
    const height = width * height_ratio;
    this.innerHTML = `
      <svg xmlns="http://www.w3.org/2000/svg"
        overflow="visible"
        vector-effect="non-scaling-stroke"
        shape-rendering="geometricPrecision"
        style="
          width: ${width}px;
          height: ${length}px;
          transform: rotate(${horizontal ? "-90deg" : "0deg"});
          transform-origin: ${width / 2}px ${width / 2}px;
        "
        viewBox="0 0 ${width} ${length}"
      >
        <path
          d="
            M0 ${height}
            h-${overhang}
            l${overhang + width / 2} -${height}
            l${overhang + width / 2} ${height}
            h-${overhang}
            v${length - 2 * height}
            h${overhang}
            l-${overhang + width / 2} ${height}
            l-${overhang + width / 2} -${height}
            h${overhang}
            Z
          "
        />
      </svg>
    `;
  };
}

customElements.define('lowrisc-arrow', LowriscArrow);
