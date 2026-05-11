/* RectWatch JS — A_HEALTH=0, B_STEPS=1, C_THEME=2, D_BOLD=3 */
function loadCfg(){return{he:+(localStorage.getItem('he')||'1'),st:+(localStorage.getItem('st')||'1'),th:+(localStorage.getItem('th')||'0'),bo:+(localStorage.getItem('bo')||'0'),ep:+(localStorage.getItem('ep')||'1'),co:+(localStorage.getItem('co')||'0')};}
function saveCfg(c){localStorage.setItem('he',c.he);localStorage.setItem('st',c.st);localStorage.setItem('th',c.th);localStorage.setItem('bo',c.bo);localStorage.setItem('ep',c.ep);localStorage.setItem('co',c.co);}
function sendMsg(c){
  var keys=[[0,c.he],[1,c.st],[2,c.th],[3,c.bo],[4,c.ep],[5,c.co]];
  function next(i){if(i>=keys.length)return;var msg={};msg[keys[i][0]]=keys[i][1];Pebble.sendAppMessage(msg,function(){next(i+1);},function(){next(i+1);});}
  next(0);
}
function radio(name,opts,sel){return opts.map(function(l,i){var ch=(i===sel)?' checked':'';return '<label class="opt"><input type="radio" name="'+name+'" value="'+i+'"'+ch+'><span>'+l+'</span></label>';}).join('');}
function buildConfig(c){
  var style='body{margin:0;font:15px/1.6 -apple-system,sans-serif;background:#1a1a1a;color:#ccc;padding:20px}h3{font-size:11px;text-transform:uppercase;letter-spacing:.08em;color:#888;margin:22px 0 8px}h3:first-child{margin-top:0}.opt{display:flex;align-items:center;gap:12px;background:#252525;border-radius:8px;padding:13px;margin:5px 0}.opt input{accent-color:#aaa;width:18px;height:18px;flex-shrink:0;margin:0}.opt span{font-size:14px}#s{display:block;width:100%;padding:14px;background:#333;color:#fff;border:1px solid #444;border-radius:8px;font-size:15px;margin-top:24px;cursor:pointer;box-sizing:border-box}';
  var script="document.getElementById('s').onclick=function(){function g(n){var e=document.querySelector('input[name='+n+']:checked');return e?+e.value:0;}location.href='pebblejs://close#'+encodeURIComponent(JSON.stringify({he:g('he'),st:g('st'),th:g('th'),bo:g('bo'),ep:g('ep'),co:g('co')}));};";
  var html='<!-- v8 --><!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>'+style+'</style></head><body>'
    +'<h3>Face Theme</h3>'+radio('th',['Light','Dark'],c.th)
    +'<h3>Health Stats</h3>'+radio('he',['Hide','Show'],c.he)
    +'<h3>Step Scale</h3>'+radio('st',['Hide','Show'],c.st)
    +'<h3>Bold Text</h3>'+radio('bo',['Normal','Bold'],c.bo)
    +'<h3>E-Paper Texture</h3>'+radio('ep',['Off','On'],c.ep)
    +'<h3>Text Colour</h3>'+radio('co',['Black','Dark Grey','Dark Red'],c.co)
    +'<button id="s">Save</button><script>'+script+'<\/script></body></html>';
  return 'data:text/html;charset=utf-8,'+encodeURIComponent(html);
}
Pebble.addEventListener('ready',function(){setTimeout(function(){sendMsg(loadCfg());},500);});
Pebble.addEventListener('showConfiguration',function(){Pebble.openURL(buildConfig(loadCfg()));});
Pebble.addEventListener('webviewclosed',function(e){
  if(!e||!e.response||e.response===''||e.response==='CANCELLED')return;
  var raw=e.response;if(raw.indexOf('#')!==-1)raw=raw.substring(raw.lastIndexOf('#')+1);
  var c;try{c=JSON.parse(decodeURIComponent(raw));}catch(err){return;}
  saveCfg(c);sendMsg(c);
});
