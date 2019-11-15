const inipp = $[`${path.dirname(path.dirname(path.dirname(__dirname)))}/x64/Release/inipp`];

let errors = false;

async function test(f){
  let c = `${path.dirname(f)}/${path.basename(f, '.ini')}__result.ini`;
  let d = `${path.dirname(f)}/${path.basename(f, '.ini')}__failed.ini`;
  try {
    let r = (await inipp('-o', f, { output: true })).replace(/\r/g, '').trim();
    if (!fs.existsSync(c)){
      fs.writeFileSync(c, r);
    } else if ($.readText(c).trim() != r){
      $.echo(β.red(`Test failed: ${f}`));
      fs.writeFileSync(d, r);
    }
  } catch (e){
    $.echo(β.red(e));
    $.echo(β.red(`Test crashed: ${f}`));
    errors = true;
  }
}

for (let f of $.glob('*/*.ini')){
  if (/__(result|failed)\.ini$/i.test(f)) continue;
  await test(f);
}
  
if (errors){
  if (process.argv.indexOf('--silent') === -1){
    await $.read('<Press any key to continue>');
  } else {
    $.fail('Tests failed');
  }
}