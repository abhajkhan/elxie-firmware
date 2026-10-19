// JS assembler must produce byte-identical images to tools/elxasm.py.
const fs = require('fs'), path = require('path'), cp = require('child_process');
const ROOT = path.resolve(__dirname, '../..');
global.window = global;
eval(fs.readFileSync(ROOT + '/apps/studio/js/spec.gen.js', 'utf8'));
eval(fs.readFileSync(ROOT + '/apps/studio/js/assembler.js', 'utf8'));
let fail = 0;
const files = fs.readdirSync(ROOT + '/spec/test-vectors').filter(f => f.endsWith('.elxa')).map(f => ROOT + '/spec/test-vectors/' + f)
  .concat(fs.readdirSync(ROOT + '/tools/test-console/demos').map(f => ROOT + '/tools/test-console/demos/' + f));
for (const f of files) {
  const js = Buffer.from(ElxAsm.assemble(fs.readFileSync(f, 'utf8')).image).toString('hex');
  const py = cp.execFileSync('python3', [ROOT + '/tools/elxasm.py', f, '--hex', '-o', '/dev/null']).toString().split('\n')[0].trim();
  const ok = js === py;
  if (!ok) fail++;
  console.log((ok ? 'PASS ' : 'FAIL ') + path.basename(f));
}
process.exit(fail ? 1 : 0);
