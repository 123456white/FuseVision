// Project-local exporter using the same official xmind-generator as ui-logic-xmind.
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
import {readFile, writeFile} from 'node:fs/promises';
import path from 'node:path';

const [root, action, input, output] = process.argv.slice(2);
const local = createRequire(path.join(root, 'out/docs-tools/node/package.json'));
if (action === 'mermaid') {
  // Mermaid sanitizes labels while parsing; Node needs a real DOMPurify window.
  const {JSDOM} = local('jsdom');
  const dom = new JSDOM('');
  globalThis.window = dom.window;
  globalThis.document = dom.window.document;
  const {default: mermaid} = await import(pathToFileURL(local.resolve('mermaid')).href);
  mermaid.initialize({startOnLoad: false, securityLevel: 'strict'});
  const diagrams = JSON.parse(await readFile(input, 'utf8'));
  for (const diagram of diagrams) {
    try { await mermaid.parse(diagram.code); }
    catch (error) { throw new Error(`${diagram.file}: ${error.message}`); }
  }
  dom.window.close();
  console.log(`Mermaid: ${diagrams.length} diagram(s) parsed`);
} else if (action === 'xmind') {
  const {Topic, RootTopic, Relationship, Workbook} = await import(pathToFileURL(local.resolve('xmind-generator')).href);
  const spec = JSON.parse(await readFile(input, 'utf8'));
  async function node(n, isRoot = false) {
    const b = (isRoot ? RootTopic : Topic)(n.title);
    b.ref(n.id);
    const notes = [n.note, n.sources?.join('\n')].filter(Boolean);
    if (notes.length) b.note(notes.join('\n\n'));
    const labels = [...new Set([...(n.labels ?? []), n.status].filter(Boolean))];
    if (labels.length) b.labels(labels);
    if (n.image) {
      const data = await readFile(path.resolve(path.dirname(input), n.image));
      b.image({name: 'screenshot.png', data});
    }
    b.children(await Promise.all((n.children ?? []).map(c => node(c))));
    return b;
  }
  const sheets = [];
  for (const sheet of spec.sheets) {
    const b = await node(sheet.root, true);
    b.sheetTitle(sheet.title);
    b.relationships((sheet.relationships ?? []).map(r => Relationship(r.title ?? '', {from: r.from, to: r.to})));
    sheets.push(b);
  }
  const archive = Buffer.from(await Workbook(sheets).archive());
  await writeFile(output, archive, {flag: 'wx'});
  console.log(`XMind: ${sheets.length} sheets, ${archive.length} bytes`);
} else {
  throw new Error(`Unknown action: ${action}`);
}
