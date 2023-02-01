import metadata from "./metadata.js";

let tabIndex = 1;

const hostname = "https://docs.opentitan.org";

for (const [key, data] of Object.entries(metadata)) {
    const element = document.getElementById(key);

    const focusable = document.createElement("div");
    element.appendChild(focusable);
    focusable.classList.add("block-focus");

    if (data.status) {
        const tooltip = document.createElement("div");
        tooltip.classList.add("diagram-tooltip");
        tooltip.innerHTML = `
            <div class="tooltip-box">
                <span class="tooltip-title">
                    ${data.id}
                </span>
                <span class="tooltip-status">
                    ${data.status}
                </span>
            </div>
        `;

        element.appendChild(tooltip);
        focusable.tabIndex = tabIndex++;
    }

    if (data.href || data.scope) {
        const href = data.href || `/hw/${data.scope}/${data.id}/doc/`;
        const link = document.createElement("a");
        link.href = hostname + href;

        link.style.display = "contents";
        element.parentElement.appendChild(link);
        link.appendChild(element);

        element.classList.add("clickable");
        focusable.tabIndex = tabIndex++;
    }
}
