// Compiles sample Blockly workspaces with the real compiler (headless Blockly),
// checks the image against tools/elxasm.py and runs it on the real VM (elx_sim).
const fs = require('fs'), path = require('path'), cp = require('child_process');
const ROOT = path.resolve(__dirname, '../..');
global.window = global;
global.Blockly = require(process.env.BLOCKLY || 'blockly');
for (const f of ['spec.gen.js', 'assembler.js', 'blocks.js', 'compiler.js']) eval(fs.readFileSync(`${ROOT}/apps/studio/js/${f}`, 'utf8'));
ElxBlocks.register(Blockly);

const num = (v) => ({ shadow: { type: 'math_number', fields: { NUM: v } } });
const col = (v) => ({ shadow: { type: 'elx_colour', fields: { COLOUR: String(v) } } });
const blk = (type, extra = {}) => ({ type, ...extra });
const chain = (...bs) => { for (let i = bs.length - 1; i > 0; i--) bs[i - 1].next = { block: bs[i] }; return bs[0]; };

const cases = {
  square: { blocks: { blocks: [ { ...chain(
    blk('elx_when_start', { x: 0, y: 0 }),
    blk('elx_repeat', { inputs: { TIMES: num(4), DO: { block: chain(
      blk('elx_drive_timed', { fields: { DIR: '1' }, inputs: { SPEED: num(50), SECS: num(1) } }),
      blk('elx_turn', { fields: { DIR: '2' }, inputs: { DEG: num(90) } })) } } }),
    blk('elx_react', { fields: { EMOTION: '1' } })) } ] } },
  vars_and_if: { variables: [{ name: 'score', id: 'vs' }], blocks: { blocks: [ { ...chain(
    blk('elx_when_start', { x: 0, y: 0 }),
    blk('variables_set', { fields: { VAR: { id: 'vs' } }, inputs: { VALUE: num(3) } }),
    blk('math_change', { fields: { VAR: { id: 'vs' } }, inputs: { DELTA: num(2) } }),
    blk('elx_if_else', { inputs: {
      COND: { block: blk('elx_gt', { inputs: { A: { block: blk('variables_get', { fields: { VAR: { id: 'vs' } } }) }, B: num(4) } }) },
      DO: { block: blk('elx_show_text', { fields: { TEXT: 'Big "5"' } }) },
      ELSE: { block: blk('elx_face_show', { fields: { FACE: '2' } }) } } }),
    blk('elx_show_number', { inputs: { N: { block: blk('elx_add', { inputs: { A: { block: blk('variables_get', { fields: { VAR: { id: 'vs' } } }) }, B: num(0.5) } }) } } })) } ] } },
  parallel_events: { blocks: { blocks: [
    { ...chain(blk('elx_when_start', { x: 0, y: 0 }), blk('elx_forever', { inputs: { DO: { block: chain(
      blk('elx_lights_all', { inputs: { COLOR: col(0xff0000) } }), blk('elx_wait', { inputs: { SECS: num(0.5) } }),
      blk('elx_lights_all', { inputs: { COLOR: { block: blk('elx_rgb', { inputs: { R: num(0), G: num(64), B: num(255) } }) } } }), blk('elx_wait', { inputs: { SECS: num(0.5) } })) } } })) },
    { ...chain(blk('elx_when_touched', { x: 0, y: 300 }), blk('elx_react', { fields: { EMOTION: '1' } })) },
    { ...chain(blk('elx_when_obstacle', { x: 0, y: 400, fields: { CM: 12 } }), blk('elx_stop_motors'), blk('elx_play_sound', { fields: { SOUND: '4' } })) },
    blk('elx_show_number', { x: 400, y: 0, inputs: { N: num(1) } }),
  ] } },
  wait_until_touch: { blocks: { blocks: [ { ...chain(
    blk('elx_when_start', { x: 0, y: 0 }),
    blk('elx_wait_until', { inputs: { COND: { block: blk('elx_touched') } } }),
    blk('elx_play_note', { fields: { NOTE: '60' }, inputs: { BEATS: num(1) } }),
    blk('elx_stop', { fields: { WHAT: 'all' } })) } ] } },
  lists_messages: { variables: [{ name: 'path', id: 'lp', type: 'List' }, { name: 'i', id: 'vi' }], blocks: { blocks: [
    { ...chain(blk('elx_when_start', { x: 0, y: 0 }),
      blk('elx_list_clear', { fields: { LIST: { id: 'lp' } } }),
      blk('elx_list_add', { fields: { LIST: { id: 'lp' } }, inputs: { ITEM: num(5) } }),
      blk('elx_list_add', { fields: { LIST: { id: 'lp' } }, inputs: { ITEM: num(7) } }),
      blk('elx_list_insert', { fields: { LIST: { id: 'lp' } }, inputs: { ITEM: num(3), INDEX: num(1) } }),
      blk('elx_list_replace', { fields: { LIST: { id: 'lp' } }, inputs: { INDEX: num(2), ITEM: num(9) } }),
      blk('elx_show_number', { inputs: { N: { block: blk('elx_list_item', { fields: { LIST: { id: 'lp' } }, inputs: { INDEX: num(2) } }) } } }),
      blk('elx_if', { inputs: { COND: { block: blk('elx_list_contains', { fields: { LIST: { id: 'lp' } }, inputs: { ITEM: num(7) } }) },
        DO: { block: blk('elx_broadcast', { fields: { MSG: 'Party' } }) } } }),
      blk('elx_show_number', { inputs: { N: { block: blk('elx_list_length', { fields: { LIST: { id: 'lp' } } }) } } })) },
    { ...chain(blk('elx_when_receive', { x: 0, y: 500, fields: { MSG: 'party' } }), blk('elx_light_effect', { fields: { EFFECT: '5' }, inputs: { COLOR: { block: blk('elx_random_color') } } })) },
  ] } },
  my_blocks_melody: { variables: [{ name: 'size', id: 'ps' }], blocks: { blocks: [
    { type: 'procedures_defnoreturn', x: 400, y: 0, extraState: { params: [{ name: 'size', id: 'ps' }] }, fields: { NAME: 'square' },
      inputs: { STACK: { block: blk('elx_repeat', { inputs: { TIMES: num(4), DO: { block: chain(
        blk('elx_drive_timed', { fields: { DIR: '1' }, inputs: { SPEED: num(50), SECS: { block: blk('variables_get', { fields: { VAR: { id: 'ps' } } }) } } }),
        blk('elx_turn', { fields: { DIR: '2' }, inputs: { DEG: num(90) } })) } } }) } } },
    { ...chain(blk('elx_when_start', { x: 0, y: 0 }),
      { type: 'procedures_callnoreturn', extraState: { name: 'square', params: ['size'] }, inputs: { ARG0: num(0.5) } },
      blk('elx_play_my_melody', { fields: { NOTES: 'C D E - G5 F#' } }),
      blk('elx_if', { inputs: { COND: { block: blk('elx_obstacle_closer', { inputs: { CM: num(20) } }) }, DO: { block: blk('elx_follow_junction', { inputs: { SPEED: num(40) } }) } } }),
      blk('elx_draw_rect', { fields: { FILL: '1' }, inputs: { X: num(1), Y: num(2), W: num(3), H: num(4) } })) },
  ] } },
};
const stims = { parallel_events: ['--stim', 'touched.0@300:400', '--stim', 'obstacle.12@1500:1600'], wait_until_touch: ['--stim', 'touched@500:600'] };

let fail = 0;
fs.mkdirSync(ROOT + '/tests/build', { recursive: true });
for (const [name, json] of Object.entries(cases)) {
  const ws = new Blockly.Workspace();
  Blockly.serialization.workspaces.load(json, ws);
  const r = ElxCompiler.compile(ws, name);
  if (!r.ok) { console.log('FAIL ' + name, r.errors); fail++; continue; }
  const asmFile = `${ROOT}/tests/build/studio_${name}.elxa`;
  fs.writeFileSync(asmFile, r.asm);
  const py = cp.execFileSync('python3', [ROOT + '/tools/elxasm.py', asmFile, '--hex', '-o', '/dev/null']).toString().split('\n')[0].trim();
  const same = py === Buffer.from(r.image).toString('hex');
  fs.writeFileSync(asmFile.replace('.elxa', '.elxb'), r.image);
  const trace = cp.execFileSync(ROOT + '/tests/build/elx_sim', [asmFile.replace('.elxa', '.elxb'), '--ms', '2500', ...(stims[name] || [])]).toString();
  if (!same) fail++;
  console.log(`${same ? 'PASS' : 'FAIL'} ${name}: ${r.stats.bytes} B, ${r.stats.scripts} scripts, warnings=${r.warnings.length}, srcmarks=${r.sourceMap.length}`);
  console.log(trace.split('\n').slice(0, 14).map((l) => '    ' + l).join('\n'));
}
// error reporting: no hat block
{ const ws = new Blockly.Workspace(); Blockly.serialization.workspaces.load({ blocks: { blocks: [blk('elx_face_blink')] } }, ws);
  const r = ElxCompiler.compile(ws, 'x'); console.log(r.ok ? 'FAIL no-hat accepted' : 'PASS no-hat -> ' + r.errors[0].msg); if (r.ok) fail++; }
process.exit(fail ? 1 : 0);
