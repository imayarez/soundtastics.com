// Lupa sobre la imagen de la interfaz de PITCHY (pitchy.html, sección "PITCHY, de cerca").
// Ratón: la lupa sigue al cursor. Táctil: toca la imagen para activar la lupa, arrastra para moverla, toca fuera para cerrarla.
(function(){
  const img=document.getElementById('pitchyImage');if(!img)return;
  const stage=img.closest('.imageStage');stage.classList.add('magnifyStage');
  const lens=document.createElement('div');lens.className='magnifier';lens.setAttribute('aria-hidden','true');stage.appendChild(lens);
  const hint=document.createElement('p');hint.className='magnifyHint';stage.after(hint);
  const touchy=matchMedia('(hover: none)').matches;
  hint.textContent=touchy?'Toca la interfaz para verla de cerca con la lupa.':'Pasa el ratón por la interfaz para verla de cerca.';
  const ZOOM=2.6;let active=false,touchMode=false;
  // the picture inside the <img> box (object-fit: contain can leave empty bands)
  function picture(){const r=img.getBoundingClientRect(),nw=img.naturalWidth||1,nh=img.naturalHeight||1,s=Math.min(r.width/nw,r.height/nh),w=nw*s,h=nh*s;return{left:r.left+(r.width-w)/2,top:r.top+(r.height-h)/2,w,h}}
  function move(x,y){const p=picture();const inside=x>=p.left&&x<=p.left+p.w&&y>=p.top&&y<=p.top+p.h;
    if(!inside){lens.classList.remove('on');return}
    const s=stage.getBoundingClientRect(),size=lens.offsetWidth||170;
    lens.style.left=(x-s.left-size/2)+'px';lens.style.top=(y-s.top-size/2-(touchMode?size*0.6:0))+'px';
    lens.style.backgroundImage='url("'+img.currentSrc+'")';
    lens.style.backgroundSize=(p.w*ZOOM)+'px '+(p.h*ZOOM)+'px';
    lens.style.backgroundPosition=(-((x-p.left)*ZOOM-size/2))+'px '+(-((y-p.top)*ZOOM-size/2))+'px';
    lens.classList.add('on')}
  function stop(){active=false;touchMode=false;lens.classList.remove('on');stage.classList.remove('magnifyOn')}
  img.addEventListener('pointermove',e=>{if(e.pointerType==='mouse'||active)move(e.clientX,e.clientY)});
  img.addEventListener('pointerleave',e=>{if(e.pointerType==='mouse')lens.classList.remove('on')});
  // touch: tap to start, drag to move, tap outside to stop
  img.addEventListener('pointerdown',e=>{if(e.pointerType==='mouse')return;if(!active){active=true;touchMode=true;stage.classList.add('magnifyOn')}move(e.clientX,e.clientY)});
  document.addEventListener('pointerdown',e=>{if(active&&e.target!==img)stop()});
  img.addEventListener('contextmenu',e=>{if(active)e.preventDefault()});
  img.addEventListener('load',()=>lens.classList.remove('on'));
  addEventListener('scroll',()=>{if(!touchMode)lens.classList.remove('on')},{passive:true});
})();
