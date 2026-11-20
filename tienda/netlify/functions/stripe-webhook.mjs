// Soundtastics shop: Stripe tells this function about each payment; it makes the PITCHY licence code and emails it.
//
//   Stripe (payment done) --> POST /api/stripe-webhook --> code (pitchy-license.mjs) --> email from support@soundtastics.com
//
// No database: the code is saved on the payment itself in Stripe (the payment's metadata "pitchy_license"), so
//   - if Stripe sends the same event twice, the customer gets the same code, not a second one;
//   - to resend a code by hand, open the payment in the Stripe dashboard and copy it from "Metadata".
//
// Settings (Netlify -> Site configuration -> Environment variables). All secret; none of them goes in the code:
//   STRIPE_SECRET_KEY            sk_live_... (sk_test_... for tests)
//   STRIPE_WEBHOOK_SECRET        whsec_... (shown by Stripe when the webhook is added)
//   PITCHY_LICENSE_PRIVATE_KEY   the whole text of soundtastics-license-private.pem
//   RESEND_API_KEY               re_... (resend.com, sends the email from support@soundtastics.com)
// Optional:
//   PITCHY_PRODUCT_IDS           prod_... (comma separated): only these products get a PITCHY code; empty = every payment
//   MAIL_FROM                    default "Soundtastics <support@soundtastics.com>"
//   DOWNLOAD_URL                 default https://www.soundtastics.com/pitchy
import { createHmac, timingSafeEqual } from "node:crypto";
import { makeLicense } from "./pitchy-license.mjs";

const env = (name, fallback = "") => (globalThis.Netlify?.env?.get (name) ?? process.env[name] ?? fallback);
const STRIPE = "https://api.stripe.com/v1";

// ---- Stripe ----------------------------------------------------------------------------------------------------
export function verifyStripeSignature (rawBody, header, secret, toleranceSec = 300, nowSec = Math.floor (Date.now() / 1000))
{
    if (! header || ! secret) return false;
    const parts = Object.fromEntries (header.split (",").map ((kv) => { const i = kv.indexOf ("="); return [kv.slice (0, i).trim(), kv.slice (i + 1)]; }));
    const t = Number (parts.t);
    if (! Number.isFinite (t) || Math.abs (nowSec - t) > toleranceSec) return false;
    const expected = createHmac ("sha256", secret).update (`${t}.${rawBody}`).digest ("hex");
    const sigs = header.split (",").filter ((kv) => kv.trim().startsWith ("v1=")).map ((kv) => kv.trim().slice (3));
    return sigs.some ((s) => s.length === expected.length && timingSafeEqual (Buffer.from (s), Buffer.from (expected)));
}

async function stripe (path, { method = "GET", form = null } = {})
{
    const res = await fetch (STRIPE + path, {
        method,
        headers: { Authorization: `Bearer ${env ("STRIPE_SECRET_KEY")}`, ...(form ? { "Content-Type": "application/x-www-form-urlencoded" } : {}) },
        body: form ? new URLSearchParams (form).toString() : undefined });
    const json = await res.json();
    if (! res.ok) throw new Error (`Stripe ${method} ${path}: ${json?.error?.message ?? res.status}`);
    return json;
}

// How many PITCHY licences this checkout bought (0 = none)
async function pitchyQuantity (sessionId)
{
    const ids = env ("PITCHY_PRODUCT_IDS").split (",").map ((s) => s.trim()).filter (Boolean);
    const items = await stripe (`/checkout/sessions/${sessionId}/line_items?limit=100`);
    let n = 0;
    for (const it of items.data ?? [])
        if (ids.length === 0 || ids.includes (typeof it.price?.product === "string" ? it.price.product : it.price?.product?.id))
            n += it.quantity ?? 1;
    return Math.min (n, 20);
}

// ---- email -----------------------------------------------------------------------------------------------------
export function licenceEmail (codes, downloadUrl)
{
    const many = codes.length > 1;
    const list = codes.map ((c) => `<p style="font-family:monospace;font-size:14px;background:#f2f2f2;padding:10px;border-radius:6px;word-break:break-all">${c}</p>`).join ("");
    const html = `<div style="font-family:Arial,sans-serif;font-size:15px;color:#111;max-width:560px">
<h2 style="margin:0 0 12px">Thank you for buying PITCHY!</h2>
<p>Here is your licence code${many ? "s" : ""}:</p>${list}
<p><b>How to activate it</b></p>
<ol><li>Install PITCHY from <a href="${downloadUrl}">${downloadUrl}</a> (if you have not yet).</li>
<li>Copy the whole code above.</li>
<li>Open PITCHY, click the gear (settings) &rarr; <b>LICENSE</b> &rarr; <b>PASTE CODE</b>.</li></ol>
<p>The DEMO silences go away at once. The code works offline and on all your own computers. Keep this email.</p>
<p>Any problem? Just reply to this email.</p><p>Soundtastics</p></div>`;
    const text = `Thank you for buying PITCHY!\n\nYour licence code${many ? "s" : ""}:\n\n${codes.join ("\n\n")}\n\n`
               + `How to activate it:\n1. Install PITCHY from ${downloadUrl}\n2. Copy the whole code.\n3. Open PITCHY, click the gear -> LICENSE -> PASTE CODE.\n\n`
               + `The DEMO silences go away at once. The code works offline and on all your own computers. Keep this email.\n\nSoundtastics`;
    return { subject: many ? "Your PITCHY licence codes" : "Your PITCHY licence code", html, text };
}

async function sendEmail (to, mail)
{
    const res = await fetch ("https://api.resend.com/emails", {
        method: "POST",
        headers: { Authorization: `Bearer ${env ("RESEND_API_KEY")}`, "Content-Type": "application/json" },
        body: JSON.stringify ({ from: env ("MAIL_FROM", "Soundtastics <support@soundtastics.com>"), to: [to],
                                reply_to: "support@soundtastics.com", subject: mail.subject, html: mail.html, text: mail.text }) });
    if (! res.ok) throw new Error (`Email to ${to} failed: ${res.status} ${await res.text()}`);
}

// ---- the webhook -----------------------------------------------------------------------------------------------
export async function handle (rawBody, signature)
{
    if (! verifyStripeSignature (rawBody, signature, env ("STRIPE_WEBHOOK_SECRET")))
        return { status: 400, body: "bad signature" };

    const event = JSON.parse (rawBody);
    if (! ["checkout.session.completed", "checkout.session.async_payment_succeeded"].includes (event.type))
        return { status: 200, body: "ignored" };

    const session = await stripe (`/checkout/sessions/${event.data.object.id}`);   // fresh copy, not the event's
    if (session.payment_status !== "paid") return { status: 200, body: "not paid yet" };   // async payments: wait for the second event
    const email = session.customer_details?.email ?? session.customer_email;
    if (! email) throw new Error (`session ${session.id} has no email`);

    // the code(s) live on the payment's metadata in Stripe: a repeated event reuses them
    const pi = typeof session.payment_intent === "string" ? session.payment_intent : session.payment_intent?.id;
    const metaPath = pi ? `/payment_intents/${pi}` : `/checkout/sessions/${session.id}`;
    const meta = pi ? (await stripe (metaPath)).metadata ?? {} : session.metadata ?? {};
    let codes = (meta.pitchy_license ?? "").split (" ").filter (Boolean);
    if (codes.length === 0)
    {
        const n = await pitchyQuantity (session.id);
        if (n === 0) return { status: 200, body: "no PITCHY in this order" };
        const key = env ("PITCHY_LICENSE_PRIVATE_KEY").replace (/\\n/g, "\n");
        codes = Array.from ({ length: n }, (_, i) => makeLicense (key, email, n > 1 ? `${session.id}#${i + 1}` : session.id));
        await stripe (metaPath, { method: "POST", form: { "metadata[pitchy_license]": codes.join (" ") } });
    }
    if (meta.pitchy_emailed === "yes") return { status: 200, body: "already sent" };

    await sendEmail (email, licenceEmail (codes, env ("DOWNLOAD_URL", "https://www.soundtastics.com/pitchy")));
    await stripe (metaPath, { method: "POST", form: { "metadata[pitchy_emailed]": "yes" } });
    return { status: 200, body: `sent ${codes.length} code(s)` };
}

export default async (req) =>
{
    if (req.method !== "POST") return new Response ("PITCHY licence webhook", { status: 200 });
    try
    {
        const r = await handle (await req.text(), req.headers.get ("stripe-signature"));
        return new Response (r.body, { status: r.status });
    }
    catch (e)
    {
        console.error (e);   // visible in Netlify -> Logs; Stripe retries the event for up to 3 days
        return new Response ("error", { status: 500 });
    }
};

export const config = { path: "/api/stripe-webhook" };
