import metadata from "./metadata.js";
import metrics from "./metrics.js";

let tabIndex = 1;

for (const [id, data] of Object.entries(metadata)) {
    const element = document.getElementById(id);

    if (data.id) {
        // Avoid writing a double slash, as `hugo` will strip it out!
        const slash = "/";
        data.href = data.href || `https:${slash}${slash}docs.opentitan.org/hw/${data.scope}/${data.id}/doc/`;

        if (metrics[data.id]) {
            data.metrics = metrics[data.id];
        } else {
            console.warn(`Missing metrics for ${data.id}`);
        }
    }

    const focusable = document.createElement("div");
    element.appendChild(focusable);
    focusable.classList.add("block-focus");
    focusable.id = `focusable-${id}`;

    if (data.metrics) {
        const revision = data.metrics.revisions.slice(-1)[0];
        const tooltip = document.createElement("div");
        tooltip.classList.add("diagram-tooltip");
        tooltip.innerHTML = `
            <div class="tooltip-box">
                <span class="tooltip-title">
                    ${data.metrics.name}
                </span>
                <span class="tooltip-status">
                    ver ${revision.version} (${revision.design_stage} / ${revision.verification_stage})
                </span>
            </div>
        `;

        element.appendChild(tooltip);
        focusable.tabIndex = tabIndex++;
    }

    if (data.href) {
        const link = document.createElement("a");
        link.href = data.href;

        link.style.display = "contents";
        element.parentElement.appendChild(link);
        link.appendChild(element);

        element.classList.add("clickable");
        focusable.tabIndex = tabIndex++;
    }
}
