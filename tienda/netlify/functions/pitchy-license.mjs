#!/usr/bin/env node
// PITCHY licence codes (Ed25519). Used by the Soundtastics web shop after a payment, or by hand.
//
//   node pitchy-license.mjs keygen                      -> creates soundtastics-license-private.pem (SECRET: never in git,
//                                                          never sent to customers) and prints the public key for the plugin
//   node pitchy-license.mjs sign <private.pem> <email> [order-id]
//                                                       -> prints the licence code to send to the customer
//   node pitchy-license.mjs check <code>                -> checks a code against the public key built into PITCHY
//
// Code format:  PITCHY1-<payload base32>-<signature base32>, payload = "PITCHY|1|<email>|<order>|<YYYY-MM-DD>"
// From the website (Node): import { makeLicense } from "./pitchy-license.mjs"; makeLicense(privatePem, email, order)
import { generateKeyPairSync, createPrivateKey, createPublicKey, sign, verify } from "node:crypto";
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";

// Public key built into the plug-in (Source/License.h). Changing it makes every code issued before invalid.
export const PUBLIC_KEY_HEX = "5498609c98751346c6a554dcea4c03ca53b0b38ca1aec99f01825d7fe208ddaa";

const ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no I, O, 0, 1: easy to read and type
export function base32 (bytes)
{
    let out = "", bits = 0, value = 0;
    for (const b of bytes) { value = (value << 8) | b; bits += 8; while (bits >= 5) { out += ALPHABET[(value >>> (bits - 5)) & 31]; bits -= 5; } }
    if (bits > 0) out += ALPHABET[(value << (5 - bits)) & 31];
    return out;
}
export function unbase32 (text)
{
    const out = []; let bits = 0, value = 0;
    for (const ch of text) { const v = ALPHABET.indexOf (ch); if (v < 0) throw new Error ("bad character " + ch); value = (value << 5) | v; bits += 5; if (bits >= 8) { out.push ((value >>> (bits - 8)) & 255); bits -= 8; } }
    return Buffer.from (out);
}
const rawPublic = (hex) => createPublicKey ({ key: Buffer.concat ([Buffer.from ("302a300506032b6570032100", "hex"), Buffer.from (hex, "hex")]), format: "der", type: "spki" });

export function makeLicense (privatePem, email, order = "", date = new Date().toISOString().slice (0, 10))
{
    const clean = (s) => String (s).replace (/[|\r\n]/g, " ").trim();
    const payload = Buffer.from (`PITCHY|1|${clean (email)}|${clean (order)}|${date}`, "utf8");
    const signature = sign (null, payload, createPrivateKey (privatePem));
    return `PITCHY1-${base32 (payload)}-${base32 (signature)}`;
}
export function checkLicense (code, publicHex = PUBLIC_KEY_HEX)
{
    const parts = code.replace (/\s+/g, "").toUpperCase().split ("-");
    if (parts.length !== 3 || parts[0] !== "PITCHY1") return null;
    const payload = unbase32 (parts[1]), signature = unbase32 (parts[2]);
    if (! verify (null, payload, rawPublic (publicHex), signature)) return null;
    const [product, version, email, order, date] = payload.toString ("utf8").split ("|");
    return product === "PITCHY" ? { email, order, date } : null;
}

if (process.argv[1] === fileURLToPath (import.meta.url))
{
    const [cmd, ...args] = process.argv.slice (2);
    if (cmd === "keygen")
    {
        const file = "soundtastics-license-private.pem";
        if (existsSync (file)) { console.error (file + " already exists: not overwritten"); process.exit (1); }
        const { privateKey, publicKey } = generateKeyPairSync ("ed25519");
        writeFileSync (file, privateKey.export ({ format: "pem", type: "pkcs8" }), { mode: 0o600 });
        const hex = publicKey.export ({ format: "der", type: "spki" }).subarray (12).toString ("hex");
        console.log ("Private key (SECRET) written to " + file);
        console.log ("Public key for the plug-in: " + hex);
    }
    else if (cmd === "sign" && args.length >= 2)
        console.log (makeLicense (readFileSync (args[0], "utf8"), args[1], args[2] ?? ""));
    else if (cmd === "check" && args.length >= 1)
    {
        const r = checkLicense (args[0]);
        console.log (r ? `VALID: ${r.email} (order ${r.order || "-"}, ${r.date})` : "NOT VALID");
        process.exit (r ? 0 : 1);
    }
    else { console.log ("usage: keygen | sign <private.pem> <email> [order] | check <code>"); process.exit (2); }
}
