// Comprobación del sello: ¿la clave privada guardada en Netlify (PITCHY_LICENSE_PRIVATE_KEY) es la pareja de la clave
// pública que lleva PITCHY dentro? Responde solo OK o FALLO; nunca muestra la clave.   GET /api/license-check
import { createPrivateKey, createPublicKey } from "node:crypto";
import { PUBLIC_KEY_HEX, makeLicense, checkLicense } from "./pitchy-license.mjs";
import { normalizePem } from "./license-key.mjs";

const env = (name) => (globalThis.Netlify?.env?.get (name) ?? process.env[name] ?? "");

export function check (raw)
{
    if (! raw) return { ok: false, msg: "FALLO: no hay ninguna clave PITCHY_LICENSE_PRIVATE_KEY en Netlify (o falta volver a publicar la web)." };
    const pem = normalizePem (raw);
    let pub;
    try { pub = createPublicKey (createPrivateKey (pem)).export ({ format: "der", type: "spki" }).subarray (12).toString ("hex"); }
    catch { return { ok: false, msg: "FALLO: la clave no se puede leer. Revisa que esté completa, con las líneas BEGIN y END." }; }
    if (pub !== PUBLIC_KEY_HEX) return { ok: false, msg: "FALLO: la clave se lee bien, pero NO es la del plugin PITCHY. Los códigos no funcionarían." };
    const code = makeLicense (pem, "prueba@soundtastics.com", "comprobacion");
    if (! checkLicense (code)) return { ok: false, msg: "FALLO: no se ha podido crear un código válido." };
    return { ok: true, msg: "OK: la clave es la correcta. Los códigos que se generen funcionarán en PITCHY." };
}

export default async () =>
{
    const r = check (env ("PITCHY_LICENSE_PRIVATE_KEY"));
    return new Response (r.msg, { status: r.ok ? 200 : 500, headers: { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" } });
};

export const config = { path: "/api/license-check" };
