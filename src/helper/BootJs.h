#pragma once

/* Injected on every main-frame load.
   Modern sites (Snowcrows / MetaBattle / gw2efficiency): downlevel CSS for CEF 103.
   Everyone: strip common ad/consent overlays. */
static const char kSnowcrowBootJs[] = R"JS(
(function(){
if (window.__scBoot) return;
window.__scBoot = 1;

var host = (location.hostname || '').toLowerCase();
var needsCssFix = /(^|\.)snowcrows\.com$|(^|\.)metabattle\.com$|(^|\.)gw2efficiency\.com$/.test(host);

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
  return css.replace(/\s+in (?:oklab|oklch|srgb|hsl|lab|xyz)\b/g,'');
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
    var m=/color-mix\(\s*in\s+[\w-]+\s*,\s*(.+)\s+(\d+(?:\.\d+)?)%\s*,\s*transparent\s*\)/.exec(full);
    if (!m) return full;
    var col=m[1].trim(), pct=+m[2];
    if (col.indexOf('color-mix(')>=0) return full;
    var c=parseColor(col, vars);
    if (!c) return full;
    var a=c.a*(pct/100);
    return 'rgba('+c.r+','+c.g+','+c.b+','+a+')';
  });
}
function downlevel(css){
  css=replOklch(css);
  css=stripIn(css);
  css=css.split('@supports (color:color-mix(in lab,red,red))').join('@supports (color:red)');
  css=stripProperty(css);
  var vars=collectVars(css);
  css=rewriteMix(css, vars);
  return css;
}
function killAds(){
  var sel=[
    '#CookieConsent','#coiOverlay','.coi-banner',
    '[id*="nitro"]','[class*="nitro"]','[class*="nitropay"]',
    'iframe[src*="doubleclick"]','iframe[src*="googlesyndication"]',
    'iframe[src*="nitropay"]','ins.adsbygoogle','[id^="google_ads"]',
    '[class*="adsbygoogle"]','[data-ad]','[id*="ad-slot"]','[class*="ad-slot"]'
  ].join(',');
  try{ document.querySelectorAll(sel).forEach(function(n){ try{n.remove();}catch(e){} }); }catch(e){}
}
async function fixSheets(){
  if (document.documentElement.getAttribute('data-sc-css')==='1') return;
  var links=[].slice.call(document.querySelectorAll('link[rel="stylesheet"]'));
  if (!links.length) return;
  var done=0;
  for (var i=0;i<links.length;i++){
    var link=links[i];
    var href=link.href||'';
    if (!href || href.indexOf('data:')===0) continue;
    try{
      var res=await fetch(href, {credentials:'same-origin', cache:'force-cache'});
      if (!res.ok) continue;
      var text=await res.text();
      if (!text || text.length<20) continue;
      if (text.indexOf('oklch(')<0 && text.indexOf('color-mix(')<0 && text.indexOf('@property')<0)
        continue;
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
function boot(){
  if (needsCssFix){
    try{
      var m=document.querySelector('meta[name=viewport]');
      if(!m){ m=document.createElement('meta'); m.name='viewport'; document.head.appendChild(m); }
      m.setAttribute('content','width=1280');
    }catch(e){}
    fixSheets();
    try{
      document.addEventListener('livewire:navigated', function(){
        document.documentElement.removeAttribute('data-sc-css');
        fixSheets(); killAds();
      });
    }catch(e){}
  }
  killAds();
  try{
    var mo=new MutationObserver(function(){ killAds(); });
    mo.observe(document.documentElement,{childList:true,subtree:true});
  }catch(e){}
}
if (document.readyState==='loading') document.addEventListener('DOMContentLoaded', boot);
else boot();
})();
)JS";
