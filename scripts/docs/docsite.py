"""Offline Markdown reader; prose stays in Markdown, API docs remain Doxygen output."""
from pathlib import Path
from urllib.parse import unquote, quote
import html
import json
import os
import re
import shutil
import sys

from docmodel import read, write_changed, encode, digest, markdown_files
from doccheck import slug, without_code


STYLE = """
:root{font:16px/1.7 system-ui,'Microsoft YaHei',sans-serif;color:#203041;background:#f4f6f8}
body{margin:0}nav{position:fixed;inset:0 auto 0 0;width:260px;background:#142839;color:#fff;padding:24px;overflow:auto;box-sizing:border-box}
nav a{color:#cfe8f5;display:block;text-decoration:none;margin:8px 0}nav input{box-sizing:border-box;width:100%;padding:10px}
main{margin-left:260px;padding:36px 4vw;max-width:1100px}article{background:white;border-radius:12px;padding:32px;box-shadow:0 3px 18px #18304412}
a{color:#086d94;overflow-wrap:anywhere}h1{font-size:1.9rem}h2{margin-top:2em;border-bottom:1px solid #dde5eb;padding-bottom:8px}
table{border-collapse:collapse;width:100%;font-size:.93em;display:block;overflow:auto}th,td{border:1px solid #d5dfe5;padding:9px;vertical-align:top}th{background:#edf4f8}
pre{padding:18px;background:#edf2f6;overflow:auto;white-space:pre-wrap}code{font-family:Consolas,monospace;font-size:.9em}img{max-width:100%;height:auto}
.notice{font-size:.85rem;color:#536676;margin-bottom:20px}.mermaid svg{max-width:100%;height:auto}footer{padding-top:20px;color:#536676;font-size:.85rem}
@media(max-width:900px){nav{position:static;width:auto;max-height:220px}main{margin:0;padding:16px}article{padding:18px}}
"""


def page_path(source, root):
    relative = source.relative_to(root / "docs")
    return relative.with_name("index.html") if relative.name == "README.md" else relative.with_suffix(".html")


def site_inputs(root, files):
    inputs = {p.relative_to(root).as_posix(): digest(p) for p in files}
    inputs["renderer"] = digest(Path(__file__))
    inputs["package-lock"] = digest(root / "scripts/docs/package-lock.json")
    # Assets linked from any current MD are included so changed screenshots refresh the site.
    for p in (root / "docs/logic/screenshots").rglob("*"):
        if p.is_file(): inputs[p.relative_to(root).as_posix()] = digest(p)
    # Source previews and downloads must refresh even when the prose is unchanged.
    for source in files:
        for target in re.findall(r'!?\[[^\]]*\]\((<[^>]+>|[^\n)]+)\)', without_code(read(source))):
            target = unquote(target.strip().strip('<>')).split('#', 1)[0]
            if not target or re.match(r'^[A-Za-z]+://|^mailto:', target):
                continue
            target = re.sub(r':\d+$', '', target)
            path = (source.parent / target).resolve()
            if path.is_relative_to(root) and path.is_file():
                inputs[path.relative_to(root).as_posix()] = digest(path)
    return inputs


def build(root):
    sys.path.insert(0, str(root / "out/docs-tools/python"))
    try:
        import markdown
    except ImportError as exc:
        raise RuntimeError("先运行 scripts/docs/setup.ps1 安装本项目文档依赖") from exc
    out = root / "out/docs/site"
    files = markdown_files(root)
    inputs = site_inputs(root, files)
    stamp = out / ".inputs.json"
    if stamp.is_file():
        previous = json.loads(read(stamp))
        if previous.get('inputs') == inputs and previous.get('outputs') and all(
                (out / name).is_file() and digest(out / name) == sha
                for name, sha in previous['outputs'].items()):
            return {"updated": False, "pages": len(files), "entry": str(out / "index.html")}
    out.mkdir(parents=True, exist_ok=True)
    mermaid = root / "out/docs-tools/node/node_modules/mermaid/dist"
    if not mermaid.is_dir():
        raise RuntimeError("缺少本地Mermaid依赖，请运行 scripts/docs/setup.ps1")
    destination = out / "assets/mermaid"
    shutil.copytree(mermaid, destination, dirs_exist_ok=True)
    entries = []
    for source in files:
        title = re.search(r"^# (.+)$", read(source), re.M)
        entries.append(dict(title=title[1] if title else source.stem,
                            path=page_path(source, root).as_posix(),
                            text=re.sub(r"\s+", " ", read(source))[:30000]))
    for source in files:
        output = out / page_path(source, root)
        content = markdown.markdown(read(source), extensions=["tables", "fenced_code", "toc", "sane_lists"],
            extension_configs={"toc": {"slugify": lambda text, separator: slug(text)}})
        content = re.sub(r'<pre><code class="language-mermaid">(.*?)</code></pre>',
                         r'<pre class="mermaid">\1</pre>', content, flags=re.S)
        # Keep the reader offline; optional external badges stay text links.
        content = re.sub(r'<img\b[^>]*\bsrc="https?://[^>]*>',
                         '<span class="notice">（外部图片请访问原始链接）</span>', content)
        def relative(path):
            return quote(os.path.relpath(path, output.parent).replace("\\", "/"), safe="/#:")
        def link(match):
            attribute, original = match.groups()
            target = html.unescape(original)
            if re.match(r"^(?:https?|mailto|data):", target):
                return match[0]
            raw, separator, anchor = unquote(target).partition("#")
            if not raw:
                return match[0]
            line = re.search(r":(\d+)$", raw)
            if line: raw = raw[:line.start()]
            path = Path(raw) if re.match(r"^[A-Za-z]:[/\\]", raw) else (source.parent / raw).resolve()
            if not path.is_relative_to(root) or not path.is_file():
                return match[0]
            if path in files:
                dest = out / page_path(path, root)
            elif path.suffix in {".cpp", ".h", ".py", ".ps1", ".dox"} or path.name == "AGENTS.md":
                dest = out / "_source" / path.relative_to(root).with_suffix(path.suffix + ".html")
                body = "\n".join(f'<span id="L{n}">{n:4}  {html.escape(t)}</span>' for n, t in enumerate(read(path).splitlines(), 1))
                write_changed(dest, '<!doctype html><meta charset="utf-8"><title>' + html.escape(path.name) + '</title><pre>' + body + '</pre>')
                if line: separator, anchor = "#", "L" + line[1]
            else:
                dest = out / "_files" / path.relative_to(root)
                write_changed(dest, path.read_bytes())
            return attribute + '="' + html.escape(relative(dest) + (separator + anchor if separator else ""), quote=True) + '"'
        content = re.sub(r'(href|src)="([^"]+)"', link, content)
        current = next(e for e in entries if e["path"] == page_path(source, root).as_posix())
        nav = '<h2>FuseVision</h2><input id="search" placeholder="搜索全部文档"><div id="results"></div>'
        nav += ''.join('<a href="' + relative(out / e["path"]) + '">' + html.escape(e["title"]) + '</a>' for e in entries)
        # No remote asset requests are necessary: Mermaid and source previews are local.
        data = json.dumps(entries, ensure_ascii=False).replace("<", "\\u003c")
        script = f'''<script type="module">
import mermaid from {json.dumps('./' + relative(out / "assets/mermaid/mermaid.esm.min.mjs"))};
mermaid.initialize({{startOnLoad:false,securityLevel:'strict',theme:'neutral'}});
try {{await mermaid.run();}} catch(e) {{console.warn('Mermaid render',e);}}
</script><script>
const entries={data}, base={json.dumps(relative(out) + '/')};
document.getElementById('search').addEventListener('input',e=>{{
 const q=e.target.value.toLowerCase().trim(), container=document.getElementById('results');container.replaceChildren();
 if(!q)return;for(const x of entries.filter(x=>(x.title+' '+x.text).toLowerCase().includes(q)).slice(0,15)){{
 const a=document.createElement('a');a.href=base+x.path;a.textContent=x.title;container.appendChild(a);}}
}});</script>'''
        document = '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
        document += '<title>' + html.escape(current["title"]) + '</title><style>' + STYLE + '</style><body><nav>' + nav
        document += '</nav><main><div class="notice">由当前Markdown生成；验证结论以文内所标日期和范围为准。</div><article>' + content
        document += '</article><footer>维护源：' + html.escape(source.relative_to(root).as_posix()) + '</footer></main>' + script + '</body></html>'
        write_changed(output, document)
    outputs = {p.relative_to(out).as_posix(): digest(p) for p in out.rglob('*')
               if p.is_file() and p != stamp}
    write_changed(stamp, encode(dict(inputs=inputs, outputs=outputs)))
    return {"updated": True, "pages": len(files), "entry": str(out / "index.html")}
