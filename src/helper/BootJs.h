#pragma once

/* Injected on every main-frame load.
   CEF Stable 150: skip oklch/color-mix CSS downlevel (native). Keep YouTube,
   login hints, viewport, and GW2 API batching fixes. */
static const char kSnowcrowBootJs[] = R"JS(
(function(){
if (window.__scBoot) return;
window.__scBoot = 1;

var host = (location.hostname || '').toLowerCase();
var isGoogleHost = /(^|\.)google\.com$/.test(host);
var isDdgHost = /(^|\.)duckduckgo\.com$/.test(host);
var isGw2App = /(^|\.)gw2\.app$/.test(host);
var isSearchHost = isGoogleHost || isDdgHost;
/* CSS rewrite disabled on CEF 150 — keep false. */
var needsCssFix = false;
var isSnowcrows = /(^|\.)snowcrows\.com$/.test(host);
/* Forced wide viewport helps MediaWiki-style layouts. Skip Snow Crows — Livewire
   + width=1280 causes nested scroll / overflow jank, and that host already gets a
   desktop-sized CEF view (HostWantsDesktopAdViewport) for ad rails. */
var clampViewport = !isSearchHost && !isSnowcrows && (isGw2App ||
  /(^|\.)metabattle\.com$|(^|\.)gw2efficiency\.com$|(^|\.)hardstuck\.gg$|(^|\.)guildjen\.com$/.test(host));
/* Response filter already downlevels same-origin CSS for these hosts. */
var cssFilterHost = needsCssFix;

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
  /* Preserve initial-value as ordinary custom props (Tailwind v4). */
  var fallbacks='';
  css=css.replace(/@property\s+(--[A-Za-z0-9_-]+)\s*\{([^}]*)\}/g, function(_, name, body){
    var m=/initial-value\s*:\s*([^;]+)/.exec(body||'');
    if (m){
      var v=String(m[1]).trim();
      if (v && v!=='initial') fallbacks+=name+':'+v+';';
    }
    return '';
  });
  if (fallbacks)
    css='*,:before,:after,::backdrop{'+fallbacks+'}'+css;
  return css;
}
function rewriteContainers(css){
  /* CEF 103: no @container — use @media so layout utilities still apply. */
  css=css.replace(/@container\\\/[a-zA-Z0-9_-]+\{[^}]*\}/g,'');
  css=css.replace(/@container\s+[a-zA-Z0-9_-]+\s*(\([^)]*\))\s*\{/g,'@media $1{');
  return css;
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
  css=rewriteContainers(css);
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
    text.indexOf('@container')>=0 ||
    text.indexOf('dvh')>=0 || text.indexOf('dvw')>=0 || text.indexOf('color(display')>=0 ||
    text.indexOf(' &')>=0;
}
function killAds(){
  /* Ads + consent + analytics allowed. Never strip NitroPay / AdSense / slots. */
}
function isInsideAdOrConsent(el){
  try{
    if (!el || !el.closest) return false;
    /* Do NOT match id*="nitro" — Snow Crows uses nitro-article-* on real content. */
    return !!el.closest(
      '.nitropay, [data-nitro], [data-nitropay], [id*="nitropay" i], [class*="nitropay" i],'+
      '[id^="nitro-ad"], [class*="nitro-ad"], .adsbygoogle, [data-ad], [data-ad-slot],'+
      '.ad-slot, [id*="google_ads" i], [id*="div-gpt-ad"],'+
      '#CookieConsent, [id*="cookieinformation" i], .fc-consent-root,'+
      '#onetrust-banner-sdk, .ot-sdk-container, [class*="cookie-consent" i]'
    );
  }catch(e){ return false; }
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
      if (needSheets) scheduleWork(function(){ fixSheets(); });
    });
    mo.observe(document.documentElement,{childList:true,subtree:true});
  }catch(e){}
}
var __scWorkTimer=0;
var __scWorkQueue=[];
function scheduleWork(fn){
  try{
    if (typeof fn==='function') __scWorkQueue.push(fn);
    if (__scWorkTimer) return;
    __scWorkTimer=setTimeout(function(){
      __scWorkTimer=0;
      var q=__scWorkQueue.slice();
      __scWorkQueue=[];
      for (var i=0;i<q.length;i++){
        try{ q[i](); }catch(e){}
      }
    }, 100);
  }catch(e){}
}
function boot(){
  /* Snow Crows: minimal CEF-OSR helpers only (ads under header + <select> polyfill). */
  if (isSnowcrows){
    injectSnowcrowsCompat();
    polyfillNativeSelects();
    try{
      document.addEventListener('livewire:navigated', function(){
        injectSnowcrowsCompat();
        polyfillNativeSelects();
      });
    }catch(e){}
    return;
  }
  if (needsCssFix){
    if (clampViewport){
      try{
        var m=document.querySelector('meta[name=viewport]');
        if(!m){ m=document.createElement('meta'); m.name='viewport'; document.head.appendChild(m); }
        m.setAttribute('content','width=1280');
      }catch(e){}
    }
    fixInlineStyles();
    if (!cssFilterHost || isSnowcrows) fixSheets();
    watchCssMutations();
    try{
      document.addEventListener('livewire:navigated', function(){
        document.documentElement.removeAttribute('data-sc-css');
        scheduleWork(function(){ fixInlineStyles(); fixSheets(); });
      });
    }catch(e){}
  }
  /* All hosts: replace native <select> — PET_POPUP ghost clicks refresh pages. */
  polyfillNativeSelects();
  unlockGuildjenMedia();
  tipGoogleLogin();
  tipGw2AppLogin();
  tipCloudflareChallenge();
  try{
    setTimeout(tipCloudflareChallenge, 1500);
    setTimeout(tipCloudflareChallenge, 4000);
  }catch(e){}
  replaceYoutubeEmbeds();
  replaceTwitchEmbeds();
  injectGeminiReadability();
  wireCheatSheetChecks();
}
/* CEF 103 OSR cannot keep YouTube embeds without tearing down the guide.
   Replace players with a card that opens the system browser instead. */
function youtubeWatchUrl(src){
  try{
    if (!src) return '';
    var m=src.match(/\/embed\/([A-Za-z0-9_-]{6,})/)||
      src.match(/youtu\.be\/([A-Za-z0-9_-]{6,})/)||
      src.match(/[?&]v=([A-Za-z0-9_-]{6,})/);
    if (m && m[1]) return 'https://www.youtube.com/watch?v='+m[1];
    if (/youtube\.com|youtu\.be|youtube-nocookie\.com/.test(src)) return src;
    return '';
  }catch(e){ return ''; }
}
function makeWatchCard(flag,title,note,linkText,url){
  var box=document.createElement('div');
  box.setAttribute(flag,'1');
  box.style.cssText='margin:12px 0;padding:14px 16px;border:1px solid #c9a227;'+
    'border-radius:6px;background:#1a1c24;color:#e8e6e3;font:14px/1.45 Segoe UI,sans-serif;';
  box.innerHTML=
    '<div style="font-weight:600;margin-bottom:6px;color:#e0c35a;">'+title+'</div>'+
    '<div style="opacity:.85;margin-bottom:10px;">'+note+'</div>'+
    '<a href="'+String(url).replace(/"/g,'&quot;')+'" style="color:#7eb6ff;font-weight:600;">'+
    linkText+'</a>';
  return box;
}
function makeYoutubeCard(watch){
  return makeWatchCard('data-gw2-yt','YouTube video',
    'In-game playback refreshes this page. Open it in your system browser instead.',
    'Watch on YouTube', watch);
}
function replaceYoutubeEmbeds(){
  try{
    /* Skip the MutationObserver/interval on hosts that never embed YT (e.g. SC
       login) — keeps Livewire pages lighter. Always run on Guildjen. */
    var likelyYt=/(^|\.)guildjen\.com$/.test(host) ||
      (document.documentElement && /youtube|youtu\.be/i.test(document.documentElement.innerHTML.slice(0,120000)));
    function sweep(){
      var nodes=document.querySelectorAll(
        'iframe.cmplz-video,iframe[data-service="youtube"],iframe[data-src-cmplz*="youtube"],'+
        'iframe[data-src-cmplz*="youtu.be"],iframe[src*="youtube.com"],iframe[src*="youtu.be"],'+
        'iframe[src*="youtube-nocookie.com"],iframe[data-src*="youtube"],iframe[data-src*="youtu.be"]');
      for (var i=0;i<nodes.length;i++){
        var el=nodes[i];
        if (!el || !el.parentNode) continue;
        if (el.getAttribute('data-gw2-yt')==='1') continue;
        /* Never replace YouTube iframes inside ad / consent containers. */
        if (isInsideAdOrConsent(el)) continue;
        var raw=el.getAttribute('data-src-cmplz')||el.getAttribute('data-src')||
          el.getAttribute('src')||'';
        var svc=el.getAttribute('data-service')||'';
        var watch=youtubeWatchUrl(raw);
        if (!watch && svc==='youtube') watch='https://www.youtube.com/';
        if (!watch) continue;
        try{
          el.parentNode.replaceChild(makeYoutubeCard(watch), el);
        }catch(e){}
      }
      var figs=document.querySelectorAll('figure.wp-block-embed-youtube');
      for (var f=0;f<figs.length;f++){
        var fig=figs[f];
        if (!fig || fig.getAttribute('data-gw2-yt')==='1') continue;
        if (fig.querySelector('[data-gw2-yt="1"]')) {
          fig.setAttribute('data-gw2-yt','1');
          continue;
        }
        var a=fig.querySelector('a[href*="youtube"],a[href*="youtu.be"]');
        var href=a?a.getAttribute('href'):'';
        var w=youtubeWatchUrl(href||'');
        if (!w) continue;
        try{
          fig.setAttribute('data-gw2-yt','1');
          fig.innerHTML='';
          fig.appendChild(makeYoutubeCard(w));
        }catch(e){}
      }
    }
    sweep();
    if (!likelyYt) return;
    var ticks=0;
    var iv=setInterval(function(){
      sweep();
      if (++ticks>=20) clearInterval(iv);
    }, 250);
    try{
      var mo=new MutationObserver(function(){ scheduleWork(sweep); });
      mo.observe(document.documentElement,{childList:true,subtree:true});
    }catch(e){}
  }catch(e){}
}
/* Twitch needs H.264 / AAC, which official CEF builds omit — its player stops
   at "Error #4000". Swap embeds for a card that opens the system browser. */
function twitchWatchUrl(src){
  try{
    if (!src || !/twitch\.tv/.test(src)) return '';
    var m=src.match(/[?&]channel=([A-Za-z0-9_]{2,})/);
    if (m) return 'https://www.twitch.tv/'+m[1];
    m=src.match(/[?&]video=v?(\d{4,})/);
    if (m) return 'https://www.twitch.tv/videos/'+m[1];
    m=src.match(/[?&]clip=([A-Za-z0-9_-]{4,})/);
    if (m) return 'https://clips.twitch.tv/'+m[1];
    return src;
  }catch(e){ return ''; }
}
function makeTwitchCard(watch){
  return makeWatchCard('data-gw2-ttv','Twitch stream',
    'The in-game browser ships without the codecs Twitch needs (Error #4000). '+
    'Open it in your system browser instead.',
    'Watch on Twitch', watch);
}
function replaceTwitchEmbeds(){
  try{
    var likelyTtv=document.documentElement &&
      /twitch\.tv/i.test(document.documentElement.innerHTML.slice(0,120000));
    if (!likelyTtv) return;
    function sweep(){
      var nodes=document.querySelectorAll(
        'iframe[src*="player.twitch.tv"],iframe[src*="clips.twitch.tv"],'+
        'iframe[data-src*="player.twitch.tv"],iframe[data-src*="clips.twitch.tv"],'+
        'iframe[data-src-cmplz*="twitch.tv"],iframe[data-service="twitch"]');
      for (var i=0;i<nodes.length;i++){
        var el=nodes[i];
        if (!el || !el.parentNode) continue;
        if (el.getAttribute('data-gw2-ttv')==='1') continue;
        if (isInsideAdOrConsent(el)) continue;
        var raw=el.getAttribute('data-src-cmplz')||el.getAttribute('data-src')||
          el.getAttribute('src')||'';
        var watch=twitchWatchUrl(raw);
        if (!watch && el.getAttribute('data-service')==='twitch')
          watch='https://www.twitch.tv/';
        if (!watch) continue;
        try{
          el.parentNode.replaceChild(makeTwitchCard(watch), el);
        }catch(e){}
      }
    }
    sweep();
    var ticks=0;
    var iv=setInterval(function(){
      sweep();
      if (++ticks>=20) clearInterval(iv);
    }, 250);
    try{
      var mo=new MutationObserver(function(){ scheduleWork(sweep); });
      mo.observe(document.documentElement,{childList:true,subtree:true});
    }catch(e){}
  }catch(e){}
}
/* Snow Crows CEF-OSR:
   - Elevate ONLY <header> above NitroPay so Profile/Inbox stay clickable.
     Do NOT elevate every .sticky.top-0 — that also hits Traits section sticky
     bars and buries gw2armory trait/skill hover cards (inline zIndex ~999).
   - Keep NitroPay / ad iframes at a low z-index under the header, but NEVER
     pointer-events:none — that made every ad click a no-op (publisher CPC).
   - Force Tippy + armory fixed tooltips above the header.
   - Un-clip armory embeds (site uses overflow-clip which hides hover cards).
   - Polyfill native <select> (PET_POPUP unreliable under OSR). */
function injectSnowcrowsCompat(){
  try{
    if (!isSnowcrows) return;
    var st=document.getElementById('gw2-sc-compat');
    if (!st){
      st=document.createElement('style');
      st.id='gw2-sc-compat';
      (document.head||document.documentElement).appendChild(st);
    }
    st.textContent=[
      'header, header.sticky{z-index:2147483000!important;position:relative;}',
      'header .nav-containment, header nav, header [x-data]{z-index:2147483000!important;}',
      /* Tippy (site chrome) */
      '[data-tippy-root],.tippy-box,.tippy-content{z-index:2147483640!important;}',
      /* gw2armory embeds — hashed gw2a--* classes, fixed tooltip shells */
      '[class^="gw2a--"][style*="position: fixed"],',
      '[class*=" gw2a--"][style*="position: fixed"],',
      '[class^="gw2a--"][style*="position:fixed"],',
      '[class*=" gw2a--"][style*="position:fixed"]{z-index:2147483640!important;}',
      '[data-armory-embed],.overflow-clip[data-armory-embed]{overflow:visible!important;}',
      '#nitro-sidebar-1,#nitro-sidebar-2,#nitro-sidebar-3,#nitro-sidebar-4,',
      '#nitro-footer-ad1,[id^="nitro-"],[id*="nitro-sidebar"],[id*="nitro-footer"],',
      'iframe[src*="nitropay"],iframe[src*="doubleclick"],iframe[src*="googlesyndication"],',
      'iframe[id*="google_ads"],iframe[src*="amazon-adsystem"],',
      '[id^="sc-np-"],[id*="sc-np-"]{z-index:1!important;}'
    ].join('');
    elevateArmoryTooltips();
    if (document.documentElement.getAttribute('data-gw2-armory-z')!=='1'){
      document.documentElement.setAttribute('data-gw2-armory-z','1');
      try{
        var mo=new MutationObserver(function(){ scheduleWork(elevateArmoryTooltips); });
        mo.observe(document.documentElement,{childList:true,subtree:true,attributes:true,attributeFilter:['style','class']});
      }catch(e2){}
    }
  }catch(e){}
}
function elevateArmoryTooltips(){
  try{
    var nodes=document.querySelectorAll('[class*="gw2a--"]');
    for (var i=0;i<nodes.length;i++){
      var el=nodes[i];
      if (!el || !el.style) continue;
      if (el.style.position==='fixed' || (el.getAttribute('style')||'').indexOf('fixed')>=0){
        el.style.setProperty('z-index','2147483640','important');
      }
    }
  }catch(e){}
}
function closeGw2SelectMenu(){
  try{
    var m=document.getElementById('gw2-select-menu');
    if (m) m.remove();
  }catch(e){}
}
/* CEF OSR cannot host native <select> widgets reliably (PET_POPUP). After a
   native option pick, a ghost mouse-up often lands on the page under the list
   and triggers a form submit / link navigation that looks like a "page refresh".
   Same helper binary on Windows and Wine/Linux — polyfill everywhere. */
function openGw2SelectMenu(sel){
  try{
    closeGw2SelectMenu();
    if (!sel || !sel.options || !sel.options.length) return;
    var rect=sel.getBoundingClientRect();
    var menu=document.createElement('div');
    menu.id='gw2-select-menu';
    menu.setAttribute('role','listbox');
    menu.setAttribute('data-gw2-for-select','1');
    menu.style.cssText=[
      'position:fixed','z-index:2147483646',
      'left:'+Math.max(8,rect.left)+'px',
      'top:'+(rect.bottom+4)+'px',
      'min-width:'+Math.max(rect.width,160)+'px',
      'max-height:min(320px,calc(100vh - '+(rect.bottom+16)+'px))',
      'overflow:auto','background:#1b1d24','color:#e8e6e3',
      'border:1px solid #3a3f4b','border-radius:8px',
      'box-shadow:0 8px 24px rgba(0,0,0,.45)','padding:4px 0',
      'font:14px/1.35 system-ui,sans-serif'
    ].join(';');
    for (var i=0;i<sel.options.length;i++){
      (function(opt, idx){
        if (opt.disabled) return;
        /* div — never <button> (default type=submit refreshes forms). */
        var row=document.createElement('div');
        row.setAttribute('role','option');
        row.setAttribute('tabindex','-1');
        row.textContent=opt.textContent||opt.value||('Option '+(idx+1));
        row.style.cssText=[
          'display:block','width:100%','text-align:left','border:0',
          'background:'+(opt.selected?'#2f3542':'transparent'),
          'color:inherit','padding:8px 12px','cursor:pointer',
          'box-sizing:border-box'
        ].join(';');
        row.onmouseenter=function(){ row.style.background='#2f3542'; };
        row.onmouseleave=function(){ row.style.background=opt.selected?'#2f3542':'transparent'; };
        row.onmousedown=function(ev){
          try{
            ev.preventDefault();
            ev.stopPropagation();
            if (ev.stopImmediatePropagation) ev.stopImmediatePropagation();
          }catch(e0){}
          var prev=sel.value;
          sel.selectedIndex=idx;
          if (String(sel.value)!==String(prev)){
            try{
              sel.dispatchEvent(new Event('input',{bubbles:true}));
              sel.dispatchEvent(new Event('change',{bubbles:true}));
            }catch(e2){}
          }
          closeGw2SelectMenu();
        };
        menu.appendChild(row);
      })(sel.options[i], i);
    }
    (document.body||document.documentElement).appendChild(menu);
    /* If menu would go off-bottom, flip above the control. */
    try{
      var mr=menu.getBoundingClientRect();
      if (mr.bottom>window.innerHeight-8){
        menu.style.top=Math.max(8, rect.top-mr.height-4)+'px';
      }
    }catch(e3){}
  }catch(e){}
}
function polyfillNativeSelects(){
  try{
    if (document.documentElement.getAttribute('data-gw2-select-poly')!=='1'){
      document.documentElement.setAttribute('data-gw2-select-poly','1');
      function inOurMenu(t){
        return !!(t && t.closest && t.closest('#gw2-select-menu'));
      }
      function closestSelect(t){
        return (t && t.closest) ? t.closest('select') : null;
      }
      /* fixed coords are viewport-locked — dismiss on page scroll like native. */
      function dismissOnPageScroll(ev){
        try{
          var m=document.getElementById('gw2-select-menu');
          if (!m) return;
          var t=ev && ev.target;
          if (t===m || (m.contains && t && m.contains(t))) return;
          closeGw2SelectMenu();
        }catch(e){}
      }
      document.addEventListener('mousedown', function(ev){
        try{
          var t=ev.target;
          if (inOurMenu(t)) return;
          var sel=closestSelect(t);
          if (sel){
            ev.preventDefault();
            ev.stopPropagation();
            if (ev.stopImmediatePropagation) ev.stopImmediatePropagation();
            openGw2SelectMenu(sel);
            return;
          }
          closeGw2SelectMenu();
        }catch(e){}
      }, true);
      /* Block the follow-up click so the native widget never opens / submits. */
      document.addEventListener('click', function(ev){
        try{
          var t=ev.target;
          if (inOurMenu(t)) return;
          if (closestSelect(t)){
            ev.preventDefault();
            ev.stopPropagation();
            if (ev.stopImmediatePropagation) ev.stopImmediatePropagation();
          }
        }catch(e){}
      }, true);
      document.addEventListener('keydown', function(ev){
        if (ev.key==='Escape') closeGw2SelectMenu();
      }, true);
      window.addEventListener('scroll', dismissOnPageScroll, true);
      window.addEventListener('resize', function(){ closeGw2SelectMenu(); }, true);
    }
  }catch(e){}
}
function tipSnowcrowsLogin(){}
function tipDiscordOAuth(){}
function wireDiscordAppHandoff(){}
function wireDiscordOpenExt(){}
function markTipPresent(){
  try{ document.documentElement.classList.add('gw2-has-tip'); }catch(e){}
}
function clearTipPresent(){
  try{
    if (!document.querySelector('[data-gw2-open-ext-tip]'))
      document.documentElement.classList.remove('gw2-has-tip');
  }catch(e){}
}
/* Open Ext tips sit at the BOTTOM — top tips covered Snow Crows Profile / Inbox. */
function mountOpenExtTip(id, html){
  try{
    if (document.getElementById(id)) return null;
    var tip=document.createElement('div');
    tip.id=id;
    tip.setAttribute('role','status');
    tip.setAttribute('data-gw2-open-ext-tip','1');
    tip.style.cssText=[
      'position:fixed','z-index:4000','left:12px','right:12px','bottom:12px','top:auto',
      'max-width:560px','margin:0 auto','padding:10px 36px 10px 12px','border-radius:8px',
      'font:13px/1.35 system-ui,sans-serif','color:#1a1a1a','background:#fff8e6',
      'border:1px solid #e0c36a','box-shadow:0 2px 10px rgba(0,0,0,.18)','pointer-events:auto'
    ].join(';');
    tip.innerHTML=html;
    var x=document.createElement('button');
    x.type='button';
    x.textContent='\u00d7';
    x.setAttribute('aria-label','Dismiss');
    x.style.cssText='position:absolute;right:8px;top:6px;border:0;background:transparent;font:18px/1 sans-serif;cursor:pointer;color:#553';
    x.onclick=function(){ tip.remove(); clearTipPresent(); };
    tip.appendChild(x);
    (document.body||document.documentElement).appendChild(tip);
    markTipPresent();
    return tip;
  }catch(e){ return null; }
}
/* Guildjen: Breeze leaves images as empty SVG placeholders (data-breeze) until
   lazy JS runs — that often never fires in CEF, so guides look empty. */
function unlockGuildjenMedia(){
  try{
    if (!/(^|\.)guildjen\.com$/.test(host)) return;
    function hydrate(){
      var imgs=document.querySelectorAll('img.br-lazy[data-breeze],img[data-breeze]');
      for (var i=0;i<imgs.length;i++){
        var img=imgs[i];
        var src=img.getAttribute('data-breeze')||'';
        if (!src) continue;
        try{
          img.setAttribute('src', src);
          var srcset=img.getAttribute('data-brsrcset');
          if (srcset) img.setAttribute('srcset', srcset);
          img.classList.remove('br-lazy');
          img.removeAttribute('data-breeze');
        }catch(e){}
      }
    }
    hydrate();
    try{
      var mo=new MutationObserver(function(){ scheduleWork(hydrate); });
      mo.observe(document.documentElement,{childList:true,subtree:true});
    }catch(e){}
  }catch(e){}
}
/* Google Account sign-in is blocked in embedded CEF. Point users at Open Ext. */
function tipGoogleLogin(){
  try{
    var h=(location.hostname||'').toLowerCase();
    var path=(location.pathname||'').toLowerCase();
    var isAccounts=/(^|\.)accounts\.google\.com$/.test(h);
    var isSignin=isGoogleHost && /signin|ServiceLogin|oauth/i.test(path+location.search);
    if (!isAccounts && !isSignin) return;
    mountOpenExtTip('gw2-google-login-tip',
      'Google often blocks sign-in in this in-game browser. Use <b>Open Ext</b> in the toolbar for Gemini Pro / Google login in your system browser (sessions are separate).');
  }catch(e){}
}
/* gw2.app account login / OAuth may fail in CEF — tip Open Ext; leave cookie banners alone. */
function tipGw2AppLogin(){
  try{
    if (!isGw2App) return;
    var path=(location.pathname||'').toLowerCase();
    if (path.indexOf('/users/login')<0 && path.indexOf('/users/register')<0 &&
        path.indexOf('/users/reset-password')<0 && path.indexOf('/users/account')<0)
      return;
    mountOpenExtTip('gw2-app-login-tip',
      'If GW2.app sign-in or Discord / account linking fails here, use <b>Open Ext</b> in the toolbar (system browser session is separate from in-game tabs). Accept cookies on the page if ads / login need them.');
  }catch(e){}
}
/* Cloudflare / “Just a moment…” interstitial — Open Ext.
   Only real interstitials — Turnstile widgets on normal pages must NOT cover the header. */
function tipCloudflareChallenge(){
  try{
    if (document.getElementById('gw2-cf-challenge-tip')) return;
    var title=(document.title||'').toLowerCase();
    var body=(document.body && (document.body.textContent)||'').slice(0,4000).toLowerCase();
    var hasInterstitial=
      title.indexOf('just a moment')>=0 ||
      title.indexOf('attention required')>=0 ||
      body.indexOf('performing security verification')>=0 ||
      body.indexOf('checking if the site connection is secure')>=0 ||
      body.indexOf('enable javascript and cookies to continue')>=0;
    if (!hasInterstitial) return;
    var loginForm=document.querySelector('form input[type=password], form button[type=submit]');
    if (loginForm && title.indexOf('just a moment')<0 && title.indexOf('attention required')<0)
      return;
    mountOpenExtTip('gw2-cf-challenge-tip',
      'This site’s bot check (Cloudflare) usually cannot finish in the in-game browser. Use <b>Open Ext</b> in the toolbar to sign in with your system browser (that session is separate from in-game tabs).');
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
