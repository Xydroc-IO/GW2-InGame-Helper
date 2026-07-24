#pragma once

/* Injected on every main-frame load.
   Modern sites (Snowcrows / MetaBattle / gw2efficiency / Google / Gemini / DDG):
   downlevel CSS for CEF 103 (oklch, color-mix, @property, display-p3, dvh).
   Non-search pages: strip common ad/consent overlays. */
static const char kSnowcrowBootJs[] = R"JS(
(function(){
if (window.__scBoot) return;
window.__scBoot = 1;

var host = (location.hostname || '').toLowerCase();
var isGoogleHost = /(^|\.)google\.com$/.test(host);
var isDdgHost = /(^|\.)duckduckgo\.com$/.test(host);
var isSearchHost = isGoogleHost || isDdgHost;
var needsCssFix = isSearchHost ||
  /(^|\.)snowcrows\.com$|(^|\.)metabattle\.com$|(^|\.)gw2efficiency\.com$|(^|\.)hardstuck\.gg$/.test(host);
/* Forced wide viewport helps Snowcrows-style layouts; breaks Google/Gemini/DDG readability. */
var clampViewport = !isSearchHost &&
  /(^|\.)snowcrows\.com$|(^|\.)metabattle\.com$|(^|\.)gw2efficiency\.com$|(^|\.)hardstuck\.gg$/.test(host);

function clamp01(x){ return x<0?0:x>1?1:x; }
function oklchToRgb(L,C,h,a){
  if (L>1) L*=0.01;
  var hr=h*Math.PI/180, ca=C*Math.cos(hr), cb=C*Math.sin(hr);
  var l_=L+0.3963377774*ca+0.2158037573*cb;
  var m_=L-0.1055613458*ca-0.0638541728*cb;
  var s_=L-0.0894841775*ca-1.2914855480*cb;
  var l=l_*l_*l_, m=m_*m_*m_, s=s_*s_*s_;
  var r=+4.0767416621*l-3.3077115913*m+0.2309699292*s;
  var g=-1.2684380046*l+2.6097574011*m-0.3413193965*s;
  var b=-0.0041960863*l-0.7034186147*m+1.7076147010*s;
  function f(x){ x=clamp01(x); return x<=0.0031308?12.92*x:1.055*Math.pow(x,1/2.4)-0.055; }
  var R=Math.round(f(r)*255), G=Math.round(f(g)*255), B=Math.round(f(b)*255);
  if (a==null || a>=0.999) return 'rgb('+R+','+G+','+B+')';
  return 'rgba('+R+','+G+','+B+','+a+')';
}
function parseNum(t){
  t=String(t).trim();
  var pct=/%$/.test(t);
  var v=parseFloat(t);
  return {v:v, pct:pct};
}
function replOklch(css){
  return css.replace(/oklch\(([^)]*)\)/g, function(_, body){
    var parts=body.split('/');
    var nums=parts[0].trim().split(/[\s,]+/).filter(Boolean);
    if (nums.length<3) return _;
    var L=parseNum(nums[0]), C=parseNum(nums[1]), h=parseNum(nums[2]);
    var a=1;
    if (parts[1]){ var A=parseNum(parts[1]); a=A.pct?A.v/100:A.v; }
    return oklchToRgb(L.v,C.v,h.v,a);
  });
}
function stripIn(css){
  /* Gradients only — never strip "in srgb" from color-mix() (that broke Gemini). */
  return css.replace(/(linear|radial|conic)-gradient\(\s*in\s+(?:oklab|oklch|srgb|hsl|lab|xyz)\s*,?/g,'$1-gradient(');
}
function stripProperty(css){
  return css.replace(/@property\s+[^{]+\{[^}]*\}/g,'');
}
function parseColor(str, vars){
  str=String(str).trim();
  var m;
  if ((m=/^var\(\s*(--[^,\s)]+)/.exec(str)) && vars[m[1]]) return vars[m[1]];
  if ((m=/^#([0-9a-fA-F]{3,8})$/.exec(str))){
    var h=m[1];
    if (h.length===3) h=h[0]+h[0]+h[1]+h[1]+h[2]+h[2];
    var a=1;
    if (h.length===8){ a=parseInt(h.slice(6,8),16)/255; h=h.slice(0,6); }
    return {r:parseInt(h.slice(0,2),16),g:parseInt(h.slice(2,4),16),b:parseInt(h.slice(4,6),16),a:a};
  }
  if ((m=/^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\)$/.exec(str))){
    return {r:+m[1],g:+m[2],b:+m[3],a:m[4]==null?1:+m[4]};
  }
  return null;
}
function collectVars(css){
  var vars={}, re=/(--[A-Za-z0-9_-]+)\s*:\s*(rgba?\([^)]+\)|#[0-9a-fA-F]{3,8})/g, m;
  while ((m=re.exec(css))){
    var c=parseColor(m[2], {});
    if (c) vars[m[1]]=c;
  }
  return vars;
}
function rewriteMix(css, vars){
  return css.replace(/color-mix\((?:[^()]|\([^)]*\))*\)/g, function(full){
    /* Gemini: color-mix(in srgb,#1f1f1f 10%,transparent) — "in SPACE," optional if already stripped */
    var m=/color-mix\(\s*(?:in\s+[\w-]+\s*,\s*)?(.+?)\s+(\d+(?:\.\d+)?)%\s*,\s*transparent\s*\)/.exec(full);
    if (!m) return full;
    var col=m[1].trim(), pct=+m[2];
    if (col.indexOf('color-mix(')>=0) return full;
    var c=parseColor(col, vars);
    if (!c) return full;
    var a=c.a*(pct/100);
    return 'rgba('+c.r+','+c.g+','+c.b+','+a+')';
  });
}
function rewriteDisplayP3(css){
  return css.replace(/color\(display-p3\s+([^)]*)\)/g, function(full, body){
    body=String(body).trim();
    var rgbPart=body, alphaPart='';
    var slash=body.lastIndexOf('/');
    if (slash>=0){ rgbPart=body.slice(0,slash).trim(); alphaPart=body.slice(slash+1).trim(); }
    var nums=rgbPart.split(/[\s,]+/).filter(Boolean);
    if (nums.length<3) return full;
    function ch(x){ var v=parseFloat(x); return isNaN(v)?0:Math.max(0,Math.min(1,v)); }
    var r=Math.round(ch(nums[0])*255), g=Math.round(ch(nums[1])*255), b=Math.round(ch(nums[2])*255);
    var a=1;
    if (alphaPart){
      var A=parseNum(alphaPart);
      a=A.pct?A.v/100:A.v;
      if (isNaN(a)) a=1;
    }
    return 'rgba('+r+','+g+','+b+','+a+')';
  });
}
function flattenViewportUnits(css){
  return css.replace(/\b(\d+(?:\.\d+)?)dvh\b/g,'$1vh').replace(/\b(\d+(?:\.\d+)?)dvw\b/g,'$1vw');
}
function flattenNestingMarkers(css){
  /* Gemini emits some nesting "&" selectors at top level — invalid on CEF 103. */
  return css.replace(/ &/g,' ').replace(/&>/g,'>').replace(/&\./g,'.').replace(/&:/g,':').replace(/&\[/g,'[');
}
function downlevel(css){
  /* Rewrite color-mix BEFORE stripping gradient color spaces — a broad stripIn
     used to destroy "in srgb" inside color-mix and leave Gemini Material vars invalid. */
  css=replOklch(css);
  css=rewriteDisplayP3(css);
  var vars=collectVars(css);
  css=rewriteMix(css, vars);
  css=stripIn(css);
  css=flattenViewportUnits(css);
  css=flattenNestingMarkers(css);
  css=css.split('@supports (color:color-mix(in lab,red,red))').join('@supports (color:red)');
  css=stripProperty(css);
  return css;
}
function injectGeminiReadability(){
  try{
    if (!/(^|\.)gemini\.google\.com$/.test(host)) return;
    if (document.getElementById('gw2-gemini-readability')) return;
    var st=document.createElement('style');
    st.id='gw2-gemini-readability';
    st.setAttribute('data-sc-fix','1');
    st.textContent=[
      'html,body{min-height:100%;}',
      '.theme-host{color:var(--gem-sys-color--on-surface,#1f1f1f);',
      'background-color:var(--gem-sys-color--surface,#f0f4f8);}',
      '.theme-host.dark-theme{color:var(--gem-sys-color--on-surface,#e3e3e3);',
      'background-color:var(--gem-sys-color--surface,#1f1f1f);}',
      'input,textarea,button,[contenteditable]{color:inherit;}'
    ].join('');
    (document.head||document.documentElement).appendChild(st);
  }catch(e){}
}
function needsDownlevel(text){
  return text.indexOf('oklch(')>=0 || text.indexOf('color-mix(')>=0 || text.indexOf('@property')>=0 ||
    text.indexOf('dvh')>=0 || text.indexOf('dvw')>=0 || text.indexOf('color(display')>=0 ||
    text.indexOf(' &')>=0;
}
function killAds(){
  /* Broad ad selectors break Google Search / Gemini / DDG SPA chrome. */
  if (isSearchHost) return;
  var sel=[
    '#CookieConsent','#coiOverlay','.coi-banner',
    '[id*="nitro"]','[class*="nitro"]','[class*="nitropay"]',
    'iframe[src*="doubleclick"]','iframe[src*="googlesyndication"]',
    'iframe[src*="nitropay"]','ins.adsbygoogle','[id^="google_ads"]',
    '[class*="adsbygoogle"]','[data-ad]','[id*="ad-slot"]','[class*="ad-slot"]'
  ].join(',');
  try{ document.querySelectorAll(sel).forEach(function(n){ try{n.remove();}catch(e){} }); }catch(e){}
}
function fixInlineStyles(){
  var styles=[].slice.call(document.querySelectorAll('style:not([data-sc-fix])'));
  for (var i=0;i<styles.length;i++){
    var el=styles[i];
    var text=el.textContent||'';
    if (!text || !needsDownlevel(text)) continue;
    try{
      el.textContent=downlevel(text);
      el.setAttribute('data-sc-fix','1');
    }catch(e){}
  }
}
async function fixSheets(){
  fixInlineStyles();
  var links=[].slice.call(document.querySelectorAll('link[rel="stylesheet"]:not([data-sc-done])'));
  if (!links.length) return;
  var done=0;
  for (var i=0;i<links.length;i++){
    var link=links[i];
    var href=link.href||'';
    try{ link.setAttribute('data-sc-done','1'); }catch(e){}
    if (!href || href.indexOf('data:')===0) continue;
    try{
      /* omit credentials so cross-origin gstatic CSS can still be fetched under CORS */
      var res=await fetch(href, {credentials:'omit', cache:'force-cache', mode:'cors'});
      if (!res.ok) continue;
      var text=await res.text();
      if (!text || text.length<20) continue;
      if (!needsDownlevel(text)) continue;
      var fixed=downlevel(text);
      var style=document.createElement('style');
      style.setAttribute('data-sc-fix','1');
      style.textContent=fixed;
      if (link.parentNode) link.parentNode.insertBefore(style, link.nextSibling);
      else document.head.appendChild(style);
      try{ link.disabled=true; }catch(e){}
      try{ link.remove(); }catch(e){}
      done++;
    }catch(e){}
  }
  if (done>0) document.documentElement.setAttribute('data-sc-css','1');
}
function watchCssMutations(){
  try{
    var mo=new MutationObserver(function(muts){
      var needSheets=false;
      for (var i=0;i<muts.length;i++){
        var nodes=muts[i].addedNodes;
        for (var j=0;j<nodes.length;j++){
          var n=nodes[j];
          if (!n || n.nodeType!==1) continue;
          if (n.tagName==='LINK' && String(n.rel||'').indexOf('stylesheet')>=0)
            needSheets=true;
          else if (n.tagName==='STYLE' && !n.getAttribute('data-sc-fix')){
            var t=n.textContent||'';
            if (needsDownlevel(t)){
              try{ n.textContent=downlevel(t); n.setAttribute('data-sc-fix','1'); }catch(e){}
            }
          }
        }
      }
      if (needSheets) fixSheets();
    });
    mo.observe(document.documentElement,{childList:true,subtree:true});
  }catch(e){}
}
function boot(){
  if (needsCssFix){
    if (clampViewport){
      try{
        var m=document.querySelector('meta[name=viewport]');
        if(!m){ m=document.createElement('meta'); m.name='viewport'; document.head.appendChild(m); }
        m.setAttribute('content','width=1280');
      }catch(e){}
    }
    fixSheets();
    watchCssMutations();
    try{
      document.addEventListener('livewire:navigated', function(){
        document.documentElement.removeAttribute('data-sc-css');
        fixSheets(); killAds();
      });
    }catch(e){}
  }
  if (!isSearchHost){
    killAds();
    try{
      var mo=new MutationObserver(function(){ killAds(); });
      mo.observe(document.documentElement,{childList:true,subtree:true});
    }catch(e){}
  }
  tipGoogleLogin();
  injectGeminiReadability();
  wireCheatSheetChecks();
}
/* Google Account sign-in is blocked in embedded CEF. Point users at Open Ext. */
function tipGoogleLogin(){
  try{
    var h=(location.hostname||'').toLowerCase();
    var path=(location.pathname||'').toLowerCase();
    var isAccounts=/(^|\.)accounts\.google\.com$/.test(h);
    var isSignin=isGoogleHost && /signin|ServiceLogin|oauth/i.test(path+location.search);
    if (!isAccounts && !isSignin) return;
    if (document.getElementById('gw2-google-login-tip')) return;
    var tip=document.createElement('div');
    tip.id='gw2-google-login-tip';
    tip.setAttribute('role','status');
    tip.style.cssText=[
      'position:fixed','z-index:2147483646','left:12px','right:12px','top:12px',
      'padding:10px 36px 10px 12px','border-radius:8px','font:13px/1.35 system-ui,sans-serif',
      'color:#1a1a1a','background:#fff8e6','border:1px solid #e0c36a','box-shadow:0 2px 10px rgba(0,0,0,.18)'
    ].join(';');
    tip.innerHTML='Google often blocks sign-in in this in-game browser. Use <b>Open Ext</b> in the toolbar for Gemini Pro / Google login in your system browser (sessions are separate).';
    var x=document.createElement('button');
    x.type='button';
    x.textContent='\u00d7';
    x.setAttribute('aria-label','Dismiss');
    x.style.cssText='position:absolute;right:8px;top:6px;border:0;background:transparent;font:18px/1 sans-serif;cursor:pointer;color:#553';
    x.onclick=function(){ tip.remove(); };
    tip.appendChild(x);
    (document.body||document.documentElement).appendChild(tip);
  }catch(e){}
}
/* Offline cheat sheets: ensure checklist rows toggle even if cached HTML is old. */
function wireCheatSheetChecks(){
  try{
    if (!/^file:/i.test(String(location.protocol||''))) return;
    var boxes=document.querySelectorAll('ul.checks input[type=checkbox]');
    if (boxes.length){
      var key='gw2helper.checks.'+(document.title||location.pathname||'sheet');
      var saved={};
      try{saved=JSON.parse(localStorage.getItem(key)||'{}')||{};}catch(e){}
      function save(){
        var state={};
        for (var i=0;i<boxes.length;i++) if (boxes[i].checked) state[i]=1;
        try{localStorage.setItem(key,JSON.stringify(state));}catch(e){}
      }
      for (var i=0;i<boxes.length;i++){
        (function(box,idx){
          if (!box.getAttribute('data-gw2-wired')){
            box.setAttribute('data-gw2-wired','1');
            if (saved[idx]) box.checked=true;
            box.addEventListener('change', save);
          }
        })(boxes[i], i);
      }
      return;
    }
    /* Legacy decorative rows (no <input>) — click to toggle .done */
    var items=document.querySelectorAll('ul.checks > li');
    if (!items.length) return;
    if (!document.getElementById('gw2-checks-css')){
      var st=document.createElement('style');
      st.id='gw2-checks-css';
      st.textContent=[
        'ul.checks>li{cursor:pointer;user-select:none}',
        'ul.checks>li.done{opacity:.85}',
        'ul.checks>li.done .box{background:rgba(106,170,106,.22);border-color:rgba(106,170,106,.65);position:relative}',
        "ul.checks>li.done .box::after{content:'';position:absolute;left:3px;top:0;width:4px;height:8px;border:solid #a8d0a8;border-width:0 2px 2px 0;transform:rotate(45deg)}",
        'ul.checks>li.done>span:not(.box){text-decoration:line-through}'
      ].join('');
      (document.head||document.documentElement).appendChild(st);
    }
    var key2='gw2helper.checks.'+(document.title||location.pathname||'sheet');
    var saved2={};
    try{saved2=JSON.parse(localStorage.getItem(key2)||'{}')||{};}catch(e){}
    function save2(){
      var state={};
      for (var j=0;j<items.length;j++) if (items[j].classList.contains('done')) state[j]=1;
      try{localStorage.setItem(key2,JSON.stringify(state));}catch(e){}
    }
    for (var j=0;j<items.length;j++){
      (function(li,idx){
        if (li.getAttribute('data-gw2-wired')) return;
        li.setAttribute('data-gw2-wired','1');
        li.style.cursor='pointer';
        if (saved2[idx]) li.classList.add('done');
        li.addEventListener('mousedown', function(e){
          if (e.button!==0) return;
          e.preventDefault();
          if (li.classList.contains('done')) li.classList.remove('done');
          else li.classList.add('done');
          save2();
        });
      })(items[j], j);
    }
  }catch(e){}
}
if (document.readyState==='loading') document.addEventListener('DOMContentLoaded', boot);
else boot();
})();
)JS";
