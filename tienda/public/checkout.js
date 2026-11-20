function secureCommerceUrl(value){try{const u=new URL(value);return u.protocol==='https:'&&!u.username&&!u.password?u.href:null}catch{return null}}
function commerceSettingsReady(settings){return settings.salesEnabled===true&&['checkoutUrl','termsUrl','privacyUrl','refundUrl'].every(key=>secureCommerceUrl(settings[key]))&&/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(settings.supportEmail||'')}
const commerceSettings=window.PITCHY_COMMERCE||{};
const commerceReady=commerceSettingsReady(commerceSettings);
const checkoutButton=document.getElementById('checkout');
checkoutButton.textContent=commerceReady?'Ir al pago seguro':'Compra disponible próximamente';
checkoutButton.onclick=()=>{
  const message=document.getElementById('cartMessage');
  if(!commerceReady){message.textContent='La venta todavía no está activada. No se realizará ningún cobro.';return}
  if(document.getElementById('cartProduct').hidden){message.textContent='Añade Pitchy a la cesta antes de continuar.';return}
  const waiver=document.getElementById('waiver');if(waiver&&!waiver.checked){message.textContent='Para continuar, marca la casilla de aceptación de las condiciones y de entrega inmediata.';waiver.focus();return}
  checkoutButton.disabled=true;message.textContent='Abriendo el pago seguro…';
  window.location.assign(secureCommerceUrl(commerceSettings.checkoutUrl));
};
window.addEventListener('pageshow',()=>{if(commerceReady)checkoutButton.disabled=document.getElementById('cartProduct').hidden});
for(const [id,key] of [['termsLink','termsUrl'],['privacyLink','privacyUrl'],['refundLink','refundUrl']]){const link=document.getElementById(id);const url=secureCommerceUrl(commerceSettings[key]);if(link&&url){link.href=url;link.hidden=false}}
const contact=document.getElementById('supportLink');if(contact&&/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(commerceSettings.supportEmail||'')){contact.href='mailto:'+commerceSettings.supportEmail;contact.hidden=false}
if(commerceReady){document.querySelectorAll('[data-launch-note]').forEach(node=>node.textContent='El precio, los impuestos aplicables y las condiciones se muestran antes de confirmar el pago.');if(document.getElementById('purchaseAnswer'))document.getElementById('purchaseAnswer').textContent='Añade Pitchy a la cesta y continúa al pago seguro. Al terminar recibirás tu código de licencia por email.';if(document.getElementById('deliveryAnswer'))document.getElementById('deliveryAnswer').textContent='Descarga el instalador desde esta página: funciona en modo demo. Al comprar recibirás por email, desde support@soundtastics.com, tu código de licencia para desbloquearlo. Conserva ese correo.'}

if(commerceReady&&document.getElementById('waiverBox'))document.getElementById('waiverBox').hidden=false;
