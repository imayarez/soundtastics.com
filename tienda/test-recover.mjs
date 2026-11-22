// Local test of "Recuperar mi licencia": fake Stripe + fake email server. Run: node test-recover.mjs
let ok = 0, bad = 0;
const check = (c, what) => { console.log ((c ? "  OK    " : "  FALLO ") + what); c ? ok++ : bad++; };
process.env.STRIPE_SECRET_KEY = "rk_test_fake";
process.env.RESEND_API_KEY = "re_fake";

const sessions = [
    { id: "cs_1", email: "fan@example.com", payment_status: "paid", payment_intent: "pi_1" },
    { id: "cs_2", email: "fan@example.com", payment_status: "paid", payment_intent: "pi_2" },
    { id: "cs_3", email: "fan@example.com", payment_status: "unpaid", payment_intent: "pi_3" },
    { id: "cs_4", email: "other@example.com", payment_status: "paid", payment_intent: "pi_4" } ];
const pis = { pi_1: { metadata: { pitchy_license: "PITCHY1-AAA" } }, pi_2: { metadata: { pitchy_license: "PITCHY1-BBB PITCHY1-CCC" } },
              pi_3: { metadata: { pitchy_license: "PITCHY1-NOTPAID" } }, pi_4: { metadata: { pitchy_license: "PITCHY1-OTHER" } } };
const sent = []; let emailDown = false;
globalThis.fetch = async (url, opt = {}) =>
{
    const u = new URL (url), json = (o, status = 200) => ({ ok: status < 300, status, json: async () => o, text: async () => JSON.stringify (o) });
    if (u.host === "api.resend.com") { if (emailDown) return json ({ message: "down" }, 500); sent.push (JSON.parse (opt.body)); return json ({ id: "m" }); }
    if (u.pathname === "/v1/checkout/sessions")
        return json ({ data: sessions.filter ((s) => s.email === u.searchParams.get ("customer_details[email]")) });
    const m = u.pathname.match (/^\/v1\/payment_intents\/(.+)$/);
    if (m) { if (opt.method === "POST") for (const [k, v] of new URLSearchParams (opt.body)) pis[m[1]].metadata[k.slice (9, -1)] = v; return json (pis[m[1]]); }
    return json ({ error: { message: "unknown " + u.pathname } }, 404);
};

const { recover } = await import ("./netlify/functions/recover-license.mjs");
const t0 = 1_800_000_000;

let r = await recover ("fan@example.com", t0);
check (r.status === 200 && sent.length === 1 && sent[0].to[0] === "fan@example.com", "un cliente con compras recibe un email en SU dirección");
const txt = sent[0]?.text ?? "";
check (["AAA", "BBB", "CCC"].every ((c) => txt.includes ("PITCHY1-" + c)), "el email lleva todos sus códigos (2 compras, 3 licencias)");
check (! txt.includes ("NOTPAID") && ! txt.includes ("OTHER"), "no incluye compras sin pagar ni de otros clientes");
check (sent[0].from.includes ("support@soundtastics.com"), "se envía desde support@");
const answer = r.body;

r = await recover ("fan@example.com", t0 + 60);
check (sent.length === 1 && r.body === answer, "pedirlo otra vez al minuto no manda otro email (máx. 1 cada 10 min)");
r = await recover ("fan@example.com", t0 + 601);
check (sent.length === 2, "pasados 10 minutos se puede volver a pedir");

r = await recover ("nadie@example.com", t0);
check (r.status === 200 && r.body === answer && sent.length === 2, "un email sin compras recibe la misma respuesta y no se envía nada");
r = await recover ("  FAN@example.com ", t0 + 2000);
check (sent.length === 3 && sent[2].to[0] === "FAN@example.com", "acepta mayúsculas y espacios en el email");
check ((await recover ("no-es-un-email", t0)).status === 400, "un texto que no es email se rechaza");
check ((await recover (undefined, t0)).status === 400, "sin email se rechaza");

emailDown = true; let threw = false;
try { await recover ("other@example.com", t0 + 5000); } catch { threw = true; }
check (threw && ! pis.pi_4.metadata.pitchy_resent_at, "si el email falla, no se marca como enviado (se puede reintentar)");

console.log (`\n${ok} correctas, ${bad} fallos`); process.exit (bad ? 1 : 0);
