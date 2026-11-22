// "Recuperar mi licencia": the customer types the email they paid with; if there are PITCHY purchases with that
// email, their licence code(s) are emailed again to THAT address (never shown on the page).
//
//   soundtastics.com/recuperar  --POST {email}-->  /api/recover-license  --> Stripe (codes on the payment) --> email
//
// - The answer is always the same, so nobody can find out which emails are customers.
// - Each purchase is re-sent at most once every 10 minutes (time kept on the payment in Stripe: pitchy_resent_at).
// - Uses the same settings as stripe-webhook.mjs (STRIPE_SECRET_KEY, RESEND_API_KEY, MAIL_FROM, DOWNLOAD_URL).
//   The restricted Stripe key needs: Checkout Sessions read, PaymentIntents write.
import { licenceEmail } from "./stripe-webhook.mjs";

const env = (name, fallback = "") => (globalThis.Netlify?.env?.get (name) ?? process.env[name] ?? fallback);
const STRIPE = "https://api.stripe.com/v1";
const COOLDOWN_SEC = 600;
const ANSWER = "Si hay una compra de PITCHY con ese email, te hemos enviado tu código. Revisa tu bandeja de entrada y la carpeta de spam.";

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

export const validEmail = (e) => typeof e === "string" && e.length <= 254 && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test (e);

// All PITCHY codes bought with this email: [{ pi, codes, resentAt }]
async function purchases (email)
{
    const found = new Map();
    for (const e of new Set ([email, email.toLowerCase()]))
    {
        const list = await stripe (`/checkout/sessions?limit=100&customer_details[email]=${encodeURIComponent (e)}`);
        for (const s of list.data ?? [])
        {
            if (s.payment_status !== "paid") continue;
            const pi = typeof s.payment_intent === "string" ? s.payment_intent : s.payment_intent?.id;
            if (! pi || found.has (pi)) continue;
            const meta = (await stripe (`/payment_intents/${pi}`)).metadata ?? {};
            const codes = (meta.pitchy_license ?? "").split (" ").filter (Boolean);
            if (codes.length) found.set (pi, { pi, codes, resentAt: Number (meta.pitchy_resent_at ?? 0) });
        }
    }
    return [...found.values()];
}

export async function recover (email, nowSec = Math.floor (Date.now() / 1000))
{
    email = String (email ?? "").trim();
    if (! validEmail (email)) return { status: 400, body: "Escribe un email válido." };

    const due = (await purchases (email)).filter ((p) => nowSec - p.resentAt >= COOLDOWN_SEC);
    if (due.length === 0) return { status: 200, body: ANSWER };

    const codes = due.flatMap ((p) => p.codes);
    const mail = licenceEmail (codes, env ("DOWNLOAD_URL", "https://www.soundtastics.com/pitchy"));
    const res = await fetch ("https://api.resend.com/emails", {
        method: "POST",
        headers: { Authorization: `Bearer ${env ("RESEND_API_KEY")}`, "Content-Type": "application/json" },
        body: JSON.stringify ({ from: env ("MAIL_FROM", "Soundtastics <support@soundtastics.com>"), to: [email],
                                reply_to: "support@soundtastics.com", subject: mail.subject, html: mail.html, text: mail.text }) });
    if (! res.ok) throw new Error (`Email to ${email} failed: ${res.status} ${await res.text()}`);
    for (const p of due) await stripe (`/payment_intents/${p.pi}`, { method: "POST", form: { "metadata[pitchy_resent_at]": String (nowSec) } });
    return { status: 200, body: ANSWER };
}

export default async (req) =>
{
    if (req.method !== "POST") return new Response ("PITCHY licence recovery", { status: 200 });
    try
    {
        let email = "";
        const type = req.headers.get ("content-type") ?? "";
        if (type.includes ("application/json")) email = (await req.json())?.email;
        else email = new URLSearchParams (await req.text()).get ("email");
        const r = await recover (email);
        return new Response (r.body, { status: r.status, headers: { "Content-Type": "text/plain; charset=utf-8" } });
    }
    catch (e)
    {
        console.error (e);   // Netlify -> Logs
        return new Response ("No hemos podido enviarlo ahora. Inténtalo en unos minutos o escribe a support@soundtastics.com.", { status: 500 });
    }
};

export const config = { path: "/api/recover-license" };
