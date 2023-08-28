document$.subscribe(({ body }) => {
  renderMathInElement(body, {
    delimiters: [
      { left: "$$",  right: "$$",  display: true },
      { left: "$",   right: "$",   display: false },
    ],
  })

  replace_wavejson_codeblocks()

  WaveDrom.ProcessAll()
})

/// Replaces <pre class="wavejson"><code>JSON</code></pre>
/// with <script type="WaveDrom">JSON</script>
function replace_wavejson_codeblocks() {
  const pres = document.querySelectorAll(".wavejson")

  pres.forEach(pre => {
    const replacement = document.createElement("script")
    replacement.setAttribute("type", "WaveDrom");

    replacement.innerHTML = pre.firstElementChild.textContent

    pre.replaceWith(replacement)
  })
}
