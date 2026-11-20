# Soundtastics website + shop

`public/` is the live website (soundtastics.com), exactly as deployed on Netlify by drag-and-drop on 19 Nov 2026.
Sales are switched on in `public/commerce-config.js` (Stripe Payment Link + terms/privacy/refund URLs).

## PITCHY licence codes

Netlify function `netlify/functions/stripe-webhook.mjs`, at `https://<site>/api/stripe-webhook`:
Stripe payment -> licence code (`pitchy-license.mjs`, the same file the plug-in's codes come from) -> email from
support@soundtastics.com (Resend). The code is kept on the payment in Stripe (metadata `pitchy_license`).

Netlify: base directory `tienda`. Environment variables (secret): `STRIPE_SECRET_KEY`, `STRIPE_WEBHOOK_SECRET`,
`PITCHY_LICENSE_PRIVATE_KEY` (whole .pem text), `RESEND_API_KEY`; optional `PITCHY_PRODUCT_IDS`, `MAIL_FROM`, `DOWNLOAD_URL`.
Stripe webhook events: `checkout.session.completed`, `checkout.session.async_payment_succeeded`.

Test without internet or keys: `node test-webhook.mjs`.
