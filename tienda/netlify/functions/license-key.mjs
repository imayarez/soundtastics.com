// Reads PITCHY_LICENSE_PRIVATE_KEY as pasted in Netlify and returns a proper PEM, whatever happened to its line breaks:
// real line breaks, "\n" written by hand, everything glued on one line, extra spaces or stray text around it.
export function normalizePem (raw)
{
    const text = String (raw ?? "").replace (/\\n/g, "\n");
    const m = text.match (/-----BEGIN ([A-Z ]+)-----([\s\S]*?)-----END \1-----/);
    if (! m) return text.trim();
    const body = m[2].replace (/[^A-Za-z0-9+/=]/g, "");
    return `-----BEGIN ${m[1]}-----\n${(body.match (/.{1,64}/g) ?? []).join ("\n")}\n-----END ${m[1]}-----\n`;
}
