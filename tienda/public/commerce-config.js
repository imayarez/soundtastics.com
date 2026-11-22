// Public storefront settings. Never put API keys or other secrets here.
// The hosted checkout must own the price, taxes, order confirmation and delivery.
window.PITCHY_COMMERCE = Object.freeze({
  salesEnabled: false,
  checkoutUrl: 'https://buy.stripe.com/5kQ6oH7mXe4T3Uy2LjdUY01',
  supportEmail: 'hello@soundtastics.com',
  termsUrl: 'https://soundtastics.com/condiciones.html',
  privacyUrl: 'https://soundtastics.com/privacidad.html',
  // Installer links for /pitchy#descargar. Empty = button shows "Próximamente".
  downloads: Object.freeze({ windows: '', windowsZip: '', mac: '', linux: '' }),
  refundUrl: 'https://soundtastics.com/devoluciones.html'
});
