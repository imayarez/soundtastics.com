// Local test of the shop function: fake Stripe + fake email server, a throwaway licence key, real signature check.
// Run: node test-webhook.mjs   (no internet, no real keys)
import { generateKeyPairSync, createHmac } from "node:crypto";
let ok = 0, bad = 0;
const check = (c, what) => { console.log ((c ? "  OK    " : "  FALLO ") + what); c ? ok++ : bad++; };

const { privateKey, publicKey } = generateKeyPairSync ("ed25519");
const pubHex = publicKey.export ({ format: "der", type: "spki" }).subarray (12).toString ("hex");
process.env.STRIPE_SECRET_KEY = "sk_test_fake";
process.env.STRIPE_WEBHOOK_SECRET = "whsec_fake";
process.env.PITCHY_LICENSE_PRIVATE_KEY = privateKey.export ({ format: "pem", type: "pkcs8" }).replace (/\n/g, "\\n");   // as pasted in one line
process.env.RESEND_API_KEY = "re_fake";
process.env.PITCHY_PRODUCT_IDS = "prod_PITCHY";

// fake Stripe and Resend
const db = { sessions: {}, pis: {}, items: {} }; const sent = [];
globalThis.fetch = async (url, opt = {}) =>
{
    const u = new URL (url), body = opt.body ?? "";
    const json = (o, status = 200) => ({ ok: status < 300, status, json: async () => o, text: async () => JSON.stringify (o) });
    if (u.host === "api.resend.com") { sent.push (JSON.parse (body)); return json ({ id: "mail_" + sent.length }); }
    let m;
    if ((m = u.pathname.match (/^\/v1\/checkout\/sessions\/([^/]+)\/line_items$/))) return json ({ data: db.items[m[1]] });
    if ((m = u.pathname.match (/^\/v1\/checkout\/sessions\/([^/]+)$/))) return json (db.sessions[m[1]]);
    if ((m = u.pathname.match (/^\/v1\/payment_intents\/([^/]+)$/)))
    {
        if (opt.method === "POST") for (const [k, v] of new URLSearchParams (body)) db.pis[m[1]].metadata[k.slice (9, -1)] = v;
        return json (db.pis[m[1]]);
    }
    return json ({ error: { message: "unknown " + u.pathname } }, 404);
};

const { handle, verifyStripeSignature } = await import ("./netlify/functions/stripe-webhook.mjs");
const { checkLicense } = await import ("./netlify/functions/pitchy-license.mjs");
const sign = (body, secret = "whsec_fake", t = Math.floor (Date.now() / 1000)) => `t=${t},v1=${createHmac ("sha256", secret).update (`${t}.${body}`).digest ("hex")}`;
const event = (id, type = "checkout.session.completed") => JSON.stringify ({ id: "evt_" + id, type, data: { object: { id } } });
const order = (id, { paid = true, email = "fan@example.com", items = [{ quantity: 1, price: { product: "prod_PITCHY" } }] } = {}) =>
{
    db.sessions[id] = { id, payment_status: paid ? "paid" : "unpaid", customer_details: { email }, payment_intent: "pi_" + id, metadata: {} };
    db.pis["pi_" + id] = { id: "pi_" + id, metadata: {} }; db.items[id] = items;
};

// 1. a normal sale
order ("cs_1");
let r = await handle (event ("cs_1"), sign (event ("cs_1")));
check (r.status === 200 && sent.length === 1, "una compra envia un email (" + r.body + ")");
const code = db.pis.pi_cs_1.metadata.pitchy_license;
check (/^PITCHY1-/.test (code ?? ""), "el codigo queda guardado en el pago de Stripe");
check (checkLicense (code, pubHex)?.email === "fan@example.com", "el codigo es valido para PITCHY y lleva el email del cliente");
check (sent[0].to[0] === "fan@example.com" && sent[0].from.includes ("support@soundtastics.com") && sent[0].text.includes (code), "el email va al cliente, desde support@, con el codigo dentro");
// 2. Stripe repeats the event
r = await handle (event ("cs_1"), sign (event ("cs_1")));
check (sent.length === 1 && db.pis.pi_cs_1.metadata.pitchy_license === code, "si Stripe repite el aviso, no se manda otro email ni otro codigo");
// 3. forged / old / wrong-secret calls
check ((await handle (event ("cs_1"), "t=1,v1=abc")).status === 400, "una llamada falsa (firma mala) se rechaza");
check ((await handle (event ("cs_1"), sign (event ("cs_1"), "whsec_other"))).status === 400, "firmada con otro secreto: rechazada");
check (! verifyStripeSignature (event ("cs_1"), sign (event ("cs_1"), "whsec_fake", Math.floor (Date.now() / 1000) - 3600), "whsec_fake"), "un aviso de hace una hora (repetido por un atacante) se rechaza");
// 4. not paid yet, then paid
order ("cs_2", { paid: false });
r = await handle (event ("cs_2"), sign (event ("cs_2")));
check (sent.length === 1 && r.body === "not paid yet", "pago pendiente (transferencia): todavia no se manda nada");
db.sessions.cs_2.payment_status = "paid";
r = await handle (event ("cs_2", "checkout.session.async_payment_succeeded"), sign (event ("cs_2", "checkout.session.async_payment_succeeded")));
check (sent.length === 2, "cuando el pago se completa, se manda el codigo");
// 5. another product, two PITCHY
order ("cs_3", { items: [{ quantity: 1, price: { product: "prod_OTHER" } }] });
r = await handle (event ("cs_3"), sign (event ("cs_3")));
check (sent.length === 2 && r.body === "no PITCHY in this order", "una compra de otro producto no genera codigo de PITCHY");
order ("cs_4", { email: "studio@example.com", items: [{ quantity: 2, price: { product: "prod_PITCHY" } }] });
await handle (event ("cs_4"), sign (event ("cs_4")));
const two = db.pis.pi_cs_4.metadata.pitchy_license.split (" ");
check (two.length === 2 && two[0] !== two[1] && two.every ((c) => checkLicense (c, pubHex)), "comprar 2 licencias da 2 codigos distintos y validos");
// 6. other events
check ((await handle (JSON.stringify ({ type: "invoice.paid", data: { object: {} } }), sign (JSON.stringify ({ type: "invoice.paid", data: { object: {} } })))).body === "ignored", "otros avisos de Stripe se ignoran");
// 7. the email failing: Stripe retries, the same code is reused
order ("cs_5");
const realFetch = globalThis.fetch; let fail = true;
globalThis.fetch = async (url, opt) => (fail && String (url).includes ("resend")) ? { ok: false, status: 500, text: async () => "down" } : realFetch (url, opt);
let threw = false; try { await handle (event ("cs_5"), sign (event ("cs_5"))); } catch { threw = true; }
const first = db.pis.pi_cs_5.metadata.pitchy_license;
fail = false; await handle (event ("cs_5"), sign (event ("cs_5")));
check (threw && sent.at (-1).text.includes (first), "si el email falla, Stripe reintenta y llega el mismo codigo");
console.log (`\n${ok} correctas, ${bad} fallos`); process.exit (bad ? 1 : 0);
