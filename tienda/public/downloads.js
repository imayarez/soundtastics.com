// Fills the download buttons on pitchy.html from PITCHY_COMMERCE.downloads (commerce-config.js).
(function(){const d=(window.PITCHY_COMMERCE||{}).downloads||{};
function ok(v){if(!v||typeof v!=='string')return null;try{const u=new URL(v,location.href);return (u.protocol==='https:'||u.origin===location.origin)?u.href:null}catch{return null}}
document.querySelectorAll('[data-download]').forEach(a=>{const url=ok(d[a.dataset.download]||'');if(!url)return;a.href=url;a.setAttribute('download','');a.removeAttribute('aria-disabled');a.querySelector('.state').textContent='Descargar'})})();
