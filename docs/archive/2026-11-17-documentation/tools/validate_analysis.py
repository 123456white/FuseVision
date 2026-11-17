"""Validate module/workflow evidence without rewriting historical reports."""
from pathlib import Path
import hashlib
import json
import re
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[3]
LOGIC = ROOT / 'docs/logic'
EVIDENCE = LOGIC / 'evidence'
checks = []


def check(name, passed, detail):
    checks.append(dict(name=name, passed=bool(passed), detail=detail))


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def walk(node):
    yield node
    children = node.get('children', [])
    if isinstance(children, dict):
        children = [child for group in children.values() for child in group]
    for child in children:
        yield from walk(child)


def cpp_tokens(text):
    """Comparison aid only: omit includes/comments, normalize the moved UI guard."""
    text = re.sub(r'^\s*#include.*$', '', text, flags=re.M)
    text = text.replace('AccessControl::requireWrite', 'WriteAccess::require')
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|\w+|[^\s]')
    return [match[0] for match in tokens.finditer(text) if not match[0].startswith(('//', '/*'))]


baseline = read(EVIDENCE / 'native-annotation-start-baseline.json')['files']
retired = {
    'src/deeplearning/integrations/LabelMePaths.h',
    'src/deeplearning/integrations/labelme/labelme_embed.py',
    'src/deeplearning/integrations/labelme/run_labelme.py',
    'src/deeplearning/integrations/labelme/labelme_paths.py',
    'tests/test_labelme_paths.py',
}
missing = [name for name in baseline if not (ROOT / name).is_file() and name not in retired]
check('native-baseline-files-retained-or-explicitly-retired', not missing,
      dict(initialFiles=len(baseline), retired=sorted(retired), missing=missing))
mutable_evidence = {'source-references.json', 'refactor-source-references.json'}
protected = [name for name in baseline
             if (name.startswith('docs/logic/evidence/') and Path(name).name not in mutable_evidence)
             or name.startswith('docs/requirements/')
             or name in {'CMakePresets.json', 'CmakeBuild.ps1', '.gitignore'}]
altered = [name for name in protected if not (ROOT / name).is_file() or digest(ROOT / name) != baseline[name]]
check('historical-evidence-and-protected-files-unchanged', not altered,
      dict(checked=len(protected), altered=altered))
allowed_code = {
    'src/deeplearning/CMakeLists.txt',
    *('src/deeplearning/widgets/' + f + ext for f in ['AnnotationCanvas','AnnotationManagement','DeepLearningWidget'] for ext in ['.cpp','.h']),
    *('src/deeplearning/tools/' + f + ext for f in ['AnnotationTool','RectTool','PolygonTool','SelectTool'] for ext in ['.cpp','.h']),
    'src/deeplearning/model/AnnotationModel.cpp', 'src/deeplearning/model/AnnotationModel.h',
}
unexpected = [name for name, sha in baseline.items() if (ROOT / name).is_file()
              and digest(ROOT / name) != sha and name not in allowed_code
              and not name.startswith(('tests/', 'cmake/', 'docs/')) and name != 'CMakeLists.txt']
check('native-changes-within-authorized-scope', not unexpected, dict(unexpected=unexpected))
snapshot_path = ROOT / 'out/native-annotation-baseline.zip'
if snapshot_path.is_file():
    with zipfile.ZipFile(snapshot_path) as snapshot:
        bad = [name for name, sha in baseline.items()
               if name not in snapshot.namelist() or hashlib.sha256(snapshot.read(name)).hexdigest() != sha]
        check('native-original-worktree-snapshot', not bad, dict(files=len(baseline), invalid=bad))
else:
    check('native-original-worktree-snapshot', False, 'Local baseline zip missing')
native_sources = [ROOT / 'src/deeplearning/widgets' / f for f in ['AnnotationManagement.cpp', 'DeepLearningWidget.cpp', 'DeepLearningWidget.h']]
launch_tokens = ['onLaunchLabelMe', 'enterLabelMeMode', 'labelMeRequested', 'QProcess', 'SetParent']
active = [(str(path.relative_to(ROOT)), token) for path in native_sources for token in launch_tokens if token in path.read_text(encoding='utf8')]
deploy = (ROOT / 'cmake/DeployRuntime.cmake').read_text(encoding='utf8')
check('labelme-runtime-retired', not active and 'labelme' not in deploy.lower()
      and all(not (ROOT / name).exists() for name in retired), dict(active=active, retired=sorted(retired)))

features = read(LOGIC / 'features.json')['features']
issues = read(ROOT / 'docs/refactor/issues.json')
check('feature-ids-retained', len(features) == 38 and len({f['id'] for f in features}) == 38, dict(count=len(features)))
check('single-account-decision-retained', any(i['id'] == 'ISS-007' and '单账户' in i['status'] and '关闭' in i['status'] for i in issues),
      'Q01 decided 2026-11-17; Q02–Q04 remain product decisions')
refs = []
for name in ['source-references.json', 'refactor-source-references.json']:
    refs.extend(read(EVIDENCE / name))
for item in features + issues:
    refs.extend(item.get('sources', []))
invalid = []
for ref in refs:
    path = ROOT / ref['path']
    lines = path.read_text(encoding='utf-8-sig').splitlines() if path.is_file() else []
    if not 0 < ref['line'] <= len(lines) or ref['symbol'] not in lines[ref['line'] - 1]:
        invalid.append(ref)
check('current-symbol-line-references', not invalid, dict(count=len(refs), invalid=invalid))

mindmap = read(LOGIC / 'FuseVision-功能与代码.json')
bad, topic_count, source_count = [], 0, 0
for sheet in mindmap['sheets']:
    nodes = list(walk(sheet['root']))
    ids = [node.get('id') for node in nodes]
    topic_count += len(nodes)
    if len(ids) != len(set(ids)):
        bad.append('duplicate ids: ' + sheet['title'])
    for relation in sheet.get('relationships', []):
        if relation['from'] not in ids or relation['to'] not in ids:
            bad.append(relation)
    for node in nodes:
        if node.get('image') and not (LOGIC / node['image']).is_file():
            bad.append(node['image'])
        for source in node.get('sources', []):
            match = re.fullmatch(r'(src/[^:]+):(\d+) (.+)', source)
            if not match:
                continue
            source_count += 1
            path = ROOT / match[1]
            lines = path.read_text(encoding='utf-8-sig').splitlines() if path.is_file() else []
            line = int(match[2])
            if not 0 < line <= len(lines) or match[3] not in lines[line - 1]:
                bad.append(source)
check('xmind-json-graph-and-current-sources', not bad,
      dict(sheets=len(mindmap['sheets']), topics=topic_count, sources=source_count, invalid=bad))
screenshots = ['P04-workflow-gallery-20261115.png', 'P05-workflow-reopened-20261115.png',
               'P00-single-account-20261117.png', 'P01-guest-readonly-20261117.png', 'P12-single-account-20261117.png']
with zipfile.ZipFile(LOGIC / 'FuseVision-功能与代码.xmind') as archive:
    content = json.loads(archive.read('content.json'))
    bad, image_count, all_text = [], 0, ''
    for sheet in content:
        nodes = list(walk(sheet['rootTopic']))
        ids = {node['id'] for node in nodes}
        for relation in sheet.get('relationships', []):
            if relation.get('end1Id') not in ids or relation.get('end2Id') not in ids:
                bad.append(relation)
        for node in nodes:
            all_text += node.get('title', '') + node.get('notes', {}).get('plain', {}).get('content', '')
            if node.get('image'):
                image_count += 1
                if node['image']['src'].replace('xap:', '') not in archive.namelist():
                    bad.append(node['image'])
    check('xmind-archive-resources', not bad and archive.testzip() is None and len(content) == 9,
          dict(sheets=len(content), images=image_count, invalid=bad))
    check('xmind-feature-and-module-coverage', all(f['id'] in all_text and f['title'] in all_text for f in features)
          and all(name in all_text for name in ['FuseVisionDeepLearningData', 'traditional', 'Doxygen 1.18.0']),
          'F01–F38 and current module architecture')
    embedded = {hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()
                if name.lower().endswith(('.png', '.jpg', '.jpeg'))}
    check('xmind-existing-screenshots-retained', all(digest(LOGIC / 'screenshots' / name) in embedded for name in screenshots), screenshots)

broken, links = [], 0
for document in (ROOT / 'docs').rglob('*.md'):
    for target in re.findall(r'\]\(([^)]+)\)', document.read_text(encoding='utf8')):
        target = target.strip('<>')
        if re.match(r'^(https?|app|codex|mailto):', target) or target.startswith('#'):
            continue
        target = re.sub(r':\d+$', '', target.split('#')[0])
        path = Path(target)
        if not path.is_absolute():
            path = document.parent / path
        links += 1
        if not path.exists() and path.resolve() != (EVIDENCE / 'native-annotation-validation.json').resolve():
            broken.append(dict(document=str(document.relative_to(ROOT)), target=target))
check('markdown-local-links', not broken, dict(count=links, broken=broken))

log = (EVIDENCE / 'native-annotation-ctest-release.log').read_text(encoding='utf8', errors='replace')
workflow = re.search(r'Start (\d+): workflow-regression', log)
test_id = workflow[1] if workflow else 'missing'
cases = re.findall(r'^' + test_id + r': PASS (.+)', log, re.M)
failures = re.findall(r'^\d+: FAIL (.+)', log, re.M)
check('workflow-regression', len(cases) == 15 and not failures, dict(cases=cases, failures=failures))
check('single-account-regressions-executed', all(name in cases for name in [
    'guest denied handlers and login state', 'guest pages single account login logout and password']),
    'Guest denial, all pages readable, fixed-account login/logout/password')
passed_tests = re.findall(r'Test\s+#\d+: ([\w-]+)\s+\.+\s+Passed', log)
expected = {'data-contract-regression', 'module-boundaries', 'native-annotation-regression', 'workflow-regression', 'startup-smoke'}
check('ctest-release', set(passed_tests) == expected and '100% tests passed, 0 tests failed out of 5' in log,
      dict(passed=passed_tests, configuration='Release; Widgets offscreen, data test QCoreApplication'))
check('data-only-behavior-executed', all(name in log for name in [
    'PASS data-only project and model configuration', 'PASS data-only undo storage round trip and failure',
    'PASS data-only paired export independently reread']),
    'Independent configuration, model, geometry, storage, rollback and image pairing')
native = re.search(r'Start (\d+): native-annotation-regression', log)
native_id = native[1] if native else 'missing'
native_cases = re.findall(r'^' + native_id + r': PASS (.+)', log, re.M)
check('native-annotation-behavior-executed', len(native_cases) >= 25 and not failures,
      dict(cases=native_cases, failures=failures))

output = ROOT / 'out/docs/doxygen'
index = output / 'xml/index.xml'
html = output / 'html/index.html'
warning_file = output / 'warnings.log'
if index.is_file() and html.is_file() and warning_file.is_file():
    xml = ET.parse(index).getroot()
    compounds = xml.findall('compound')
    groups = {item.findtext('name') for item in compounds if item.get('kind') == 'group'}
    types = {item.findtext('name') for item in compounds if item.get('kind') in {'class', 'struct'}}
    expected_groups = {'core', 'ui_common', 'deeplearning', 'dl_domain', 'dl_state', 'dl_storage',
                       'dl_tools', 'dl_widgets', 'traditional', 'app'}
    warnings = warning_file.read_text(encoding='utf8').strip()
    check('doxygen-html-xml-and-groups', xml.get('version') == '1.18.0' and groups == expected_groups
          and {'AnnotationShape', 'AnnotationModel', 'AnnotationStore', 'DatasetFileService', 'FvdlInfo', 'FvprojInfo'} <= types
          and 'Doxygen 1.18.0' in html.read_text(encoding='utf8') and not warnings,
          dict(version=xml.get('version'), groups=sorted(groups), types=len(types), warnings=warnings,
               files=sum(1 for path in output.rglob('*') if path.is_file()),
               coverage='EXTRACT_ALL; key interfaces documented, historical member completeness not enforced'))
else:
    check('doxygen-html-xml-and-groups', False, 'Generate local Doxygen artifacts before validation')

report = dict(date='2026-11-17', configuration='windows-x64-release',
              verification='Rebuilt Release; native Qt input and save/recovery regression, original 15 workflow groups, data and boundaries; Doxygen 1.18.0',
              passed=all(item['passed'] for item in checks), checks=checks,
              notExecuted=['Visible Windows desktop and native dialogs', 'Camera hardware; LabelMe runtime retired and no longer part of acceptance',
                           'Confirmed settings reset and full process restart', 'Debug build, real dataset/performance benchmark and human interaction fluency',
                           'XMind desktop client', 'clang-format full-source run'],
              decisions=['Q01 single-account requirement implemented previously', 'Q02 anomaly classification versus masks',
                         'Q03 rectangle labels and task mapping', 'Q04 historical role headcount, audit and backup requirements'])
(EVIDENCE / 'native-annotation-validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
for item in checks:
    print(('PASS ' if item['passed'] else 'FAIL ') + item['name'])
raise SystemExit(0 if report['passed'] else 1)
