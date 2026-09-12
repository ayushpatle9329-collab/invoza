# INVOZA — Smart Bill & Inventory System

A complete student-friendly shop billing and inventory project using HTML/CSS/JavaScript on the frontend and a dependency-free C++17 HTTP backend.

## Features

- Home page with navbar, Contact Us, Login, Features, Services and footer
- Login and signup with persistent local data
- Dashboard statistics
- Product inventory: add, edit, delete, search and low-stock alerts
- Billing with multiple products, tax and payment mode
- Invoice history
- Sales history
- Purchase/stock-in management
- Reports and business totals
- Contact form saved by the C++ backend
- CSV-based local persistence in `database/`

## Windows setup

1. Install MSYS2 and the UCRT64 GCC compiler.
2. Add `C:\msys64\ucrt64\bin` to Windows PATH.
3. Open a new VS Code terminal inside this `Smart-Bill` folder.
4. Verify:
   `g++ --version`
5. Run:
   `run.bat`
6. Open:
   `http://localhost:8080/pages/index.html`

Demo login: `admin@invoza.com` / `admin123`

The first run automatically creates `database/users.csv`, `products.csv`, `sales.csv`, `purchases.csv`, `invoices.csv` and `contacts.csv`.

## Deploy: Netlify + Render

Netlify hosts only the frontend. The C++ API runs on Render and Netlify proxies every `/api/*` request to it, so login works from the public site.

1. Push this `Smart-Bill` repository to GitHub. Do not commit a database containing real user data: passwords are stored as plain text by this student project.
2. In Render, choose **New > Blueprint**, select the repository, and deploy the included `render.yaml`. Render builds `Dockerfile` and provides an API URL such as `https://invoza-api.onrender.com`.
3. In Netlify, open the existing site settings and set **Base directory** to the folder containing `netlify.toml` (leave it empty when `Smart-Bill` is the repository root). Do not override the build command or publish directory.
4. In Netlify **Environment variables**, add `INVOZA_API_ORIGIN` with the Render URL only, for example `https://invoza-api.onrender.com`. Do not add `/api` or a trailing slash. Its scope must include **Builds**.
5. Trigger a new Netlify deploy. The build creates a minimal `dist/` folder and its `_redirects` file proxies `/api/*` to Render. Backend source and CSV data are intentionally not published.
6. Open `https://your-site.netlify.app/api/health`. It should return `{"success":true,"status":"ok"}`. Then sign in using `admin@invoza.com` / `admin123` or create a new account.

### Data persistence

The backend is configured to save CSV files at `/var/data` on Render. Attach a persistent disk at that exact mount path in Render if your plan supports disks; otherwise data can be lost when Render restarts or redeploys the service. For real production use, replace CSV/plain-text passwords with a managed database and hashed passwords.
