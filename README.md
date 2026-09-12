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
