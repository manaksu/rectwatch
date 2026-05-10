/*
 * RectWatch -- PebbleKit JS
 * appKeys alphabetical -> index:
 *   A_STEPS -> key 0   0=hide  1=show
 *   B_THEME -> key 1   0=light 1=dark
 */

function loadCfg() {
  return {
    st: +(localStorage.getItem('st') || '0'),
    th: +(localStorage.getItem('th') || '0')
  };
}
function saveCfg(c) {
  localStorage.setItem('st', c.st);
  localStorage.setItem('th', c.th);
}
function sendMsg(c) {
  var keys = [[0, c.st], [1, c.th]];
  function sendNext(i) {
    if (i >= keys.length) return;
    var msg = {}; msg[keys[i][0]] = keys[i][1];
    Pebble.sendAppMessage(msg,
      function() { sendNext(i+1); },
      function(e) { console.log('fail key '+keys[i][0]); sendNext(i+1); }
    );
  }
  sendNext(0);
}
function radio(name, opts, sel) {
  return opts.map(function(l, i) {
    var checked = (i === sel) ? ' checked' : '';
    return '<label class="opt"><input type="radio" name="' + name + '" value="' + i + '"' + checked + '><span>' + l + '</span></label>';
  }).join('');
}
function buildConfig(c) {
  var style = 'body{margin:0;font:15px/1.6 -apple-system,sans-serif;background:#1a1a1a;color:#ccc;padding:20px}'
    + 'h3{font-size:11px;text-transform:uppercase;letter-spacing:.08em;color:#888;margin:22px 0 8px}'
    + 'h3:first-child{margin-top:0}'
    + '.opt{display:flex;align-items:center;gap:12px;background:#252525;border-radius:8px;padding:13px;margin:5px 0}'
    + '.opt input{accent-color:#aaa;width:18px;height:18px;flex-shrink:0;margin:0}'
    + '.opt span{font-size:14px}'
    + '#s{display:block;width:100%;padding:14px;background:#333;color:#fff;border:1px solid #444;border-radius:8px;font-size:15px;margin-top:24px;cursor:pointer;box-sizing:border-box}';
  var script = "document.getElementById('s').onclick=function(){"
    + "function g(n){var e=document.querySelector('input[name='+n+']:checked');return e?+e.value:0;}"
    + "location.href='pebblejs://close#'+encodeURIComponent(JSON.stringify({st:g('st'),th:g('th')}));"
    + "};";
  var html = '<!-- v1 --><!DOCTYPE html><html><head>'
    + '<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
    + '<style>' + style + '</style></head><body>'
    + '<h3>Face Theme</h3>' + radio('th', ['Light', 'Dark'], c.th)
    + '<h3>Step Scale</h3>' + radio('st', ['Hide', 'Show'], c.st)
    + '<button id="s">Save</button>'
    + '<script>' + script + '<\/script></body></html>';
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}
Pebble.addEventListener('ready', function() {
  setTimeout(function() { sendMsg(loadCfg()); }, 500);
});
Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(buildConfig(loadCfg()));
});
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response || e.response === '' || e.response === 'CANCELLED') return;
  var raw = e.response;
  if (raw.indexOf('#') !== -1) raw = raw.substring(raw.lastIndexOf('#') + 1);
  var c; try { c = JSON.parse(decodeURIComponent(raw)); } catch(err) { return; }
  saveCfg(c); sendMsg(c);
});
