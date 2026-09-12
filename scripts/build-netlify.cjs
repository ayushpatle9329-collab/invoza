const fs = require("node:fs");
const path = require("node:path");

const suppliedOrigin = process.env.INVOZA_API_ORIGIN;

if (!suppliedOrigin) {
  throw new Error(
    "Set INVOZA_API_ORIGIN to your https://<service>.onrender.com URL in Netlify.",
  );
}

let parsedOrigin;

try {
  parsedOrigin = new URL(suppliedOrigin);
} catch {
  throw new Error("INVOZA_API_ORIGIN must be a valid http(s) URL.");
}

const isLocalHttp =
  parsedOrigin.protocol === "http:" &&
  ["localhost", "127.0.0.1"].includes(parsedOrigin.hostname);

if (
  !(parsedOrigin.protocol === "https:" || isLocalHttp) ||
  parsedOrigin.pathname !== "/" ||
  parsedOrigin.search ||
  parsedOrigin.hash
) {
  throw new Error(
    "INVOZA_API_ORIGIN must be an HTTPS origin without /api, a path, query, or fragment.",
  );
}

const projectRoot = path.resolve(__dirname, "..");
const publishDir = path.join(projectRoot, "dist");

fs.rmSync(publishDir, { recursive: true, force: true });
fs.mkdirSync(publishDir, { recursive: true });

for (const directory of ["assets", "includes", "modules", "pages"]) {
  fs.cpSync(
    path.join(projectRoot, directory),
    path.join(publishDir, directory),
    { recursive: true },
  );
}

fs.copyFileSync(
  path.join(projectRoot, "index.html"),
  path.join(publishDir, "index.html"),
);

// The browser keeps using /api. Netlify proxies it to Render, avoiding a
// cross-origin login request and keeping the backend URL out of page code.
fs.writeFileSync(
  path.join(publishDir, "_redirects"),
  `/api/* ${parsedOrigin.origin}/api/:splat 200!\n`,
);
