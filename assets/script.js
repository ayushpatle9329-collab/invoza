const API = "/api";
async function api(path, options = {}) {
  const r = await fetch(API + path, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options,
  });
  let d = {};
  try {
    d = await r.json();
  } catch {}
  if (!r.ok) throw new Error(d.message || "Request failed");
  return d;
}
function showAlert(id, msg, type = "error") {
  const e = document.getElementById(id);
  if (!e) return;
  e.className = "alert " + type;
  e.textContent = msg;
  e.style.display = "block";
}
function toast(msg) {
  const e = document.getElementById("toast");
  if (!e) return;
  e.textContent = msg;
  e.style.display = "block";
  setTimeout(() => (e.style.display = "none"), 2500);
}
function auth() {
  try {
    return JSON.parse(localStorage.getItem("invozaUser"));
  } catch {
    return null;
  }
}
function requireAuth() {
  if (!auth()) location.href = "/pages/login2.html";
}
function logout() {
  localStorage.removeItem("invozaUser");
  location.href = "/pages/login2.html";
}
function money(n) {
  return (
    "₹" +
    Number(n || 0).toLocaleString("en-IN", {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    })
  );
}
function esc(s) {
  return String(s ?? "").replace(
    /[&<>'"]/g,
    (c) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;" })[
        c
      ],
  );
}
async function loginSubmit(e) {
  e.preventDefault();
  try {
    const email = document.getElementById("email").value.trim(),
      password = document.getElementById("password").value;
    const d = await api("/login", {
      method: "POST",
      body: JSON.stringify({ email, password }),
    });
    localStorage.setItem("invozaUser", JSON.stringify(d.user));
    location.href = "/pages/dashboard.html";
  } catch (x) {
    showAlert("alert", x.message);
  }
}
async function signupSubmit(e) {
  e.preventDefault();
  try {
    const body = {
      name: document.getElementById("name").value.trim(),
      email: document.getElementById("email").value.trim(),
      password: document.getElementById("password").value,
    };
    if (body.password !== document.getElementById("confirm").value)
      throw new Error("Passwords do not match");
    await api("/signup", { method: "POST", body: JSON.stringify(body) });
    showAlert("alert", "Account created. You can now login.", "success");
    setTimeout(() => (location.href = "/pages/login2.html"), 900);
  } catch (x) {
    showAlert("alert", x.message);
  }
}
async function contactSubmit(e) {
  e.preventDefault();
  try {
    await api("/contact", {
      method: "POST",
      body: JSON.stringify(Object.fromEntries(new FormData(e.target))),
    });
    showAlert(
      "alert",
      "Message sent successfully! We will contact you soon.",
      "success",
    );
    e.target.reset();
  } catch (x) {
    showAlert("alert", x.message);
  }
}
async function loadDashboard() {
  requireAuth();
  const d = await api("/dashboard");
  document.querySelector("#products").textContent = d.products;
  document.querySelector("#stock").textContent = d.stock;
  document.querySelector("#sales").textContent = d.sales_count;
  document.querySelector("#revenue").textContent = money(d.revenue);
  document.querySelector("#low").textContent = d.low_stock;
}
let products = [];
async function loadProducts() {
  requireAuth();
  products = await api("/products");
  renderProducts(products);
}
function renderProducts(list) {
  const tb = document.querySelector("#productRows");
  if (!tb) return;
  tb.innerHTML =
    list
      .map(
        (p) =>
          `<tr><td>${esc(p.id)}</td><td><b>${esc(p.name)}</b><br><small>${esc(p.category)}</small></td><td>${esc(p.sku)}</td><td>${money(p.price)}</td><td>${p.stock <= 10 ? `<span class="badge low">${p.stock} Low</span>` : `<span class="badge ok">${p.stock}</span>`}</td><td>${esc(p.supplier)}</td><td><button class="btn btn-primary" onclick="editProduct('${p.id}')">Edit</button> <button class="btn" onclick="deleteProduct('${p.id}')">Delete</button></td></tr>`,
      )
      .join("") || '<tr><td colspan="7">No products found.</td></tr>';
}
function filterProducts(v) {
  renderProducts(
    products.filter((p) =>
      (p.name + " " + p.category + " " + p.sku + " " + p.supplier)
        .toLowerCase()
        .includes(v.toLowerCase()),
    ),
  );
}
async function productSubmit(e) {
  e.preventDefault();
  try {
    const body = Object.fromEntries(new FormData(e.target));
    body.price = Number(body.price);
    body.stock = Number(body.stock);
    if (body.id) {
      await api("/products", { method: "PUT", body: JSON.stringify(body) });
    } else {
      await api("/products", { method: "POST", body: JSON.stringify(body) });
    }
    e.target.reset();
    document.getElementById("pid").value = "";
    document.getElementById("formTitle").textContent = "Add Product";
    toast("Product saved successfully");
    loadProducts();
  } catch (x) {
    showAlert("formAlert", x.message);
  }
}
function editProduct(id) {
  const p = products.find((x) => x.id === id);
  if (!p) return;
  ["id", "name", "category", "sku", "price", "stock", "supplier"].forEach(
    (k) => {
      const e = document.getElementById(k);
      if (e) e.value = p[k];
    },
  );
  document.getElementById("formTitle").textContent = "Edit Product";
  window.scrollTo({ top: 0, behavior: "smooth" });
}
async function deleteProduct(id) {
  if (!confirm("Delete this product?")) return;
  try {
    await api("/products", { method: "DELETE", body: JSON.stringify({ id }) });
    toast("Product deleted");
    loadProducts();
  } catch (x) {
    alert(x.message);
  }
}
function addBillRow() {
  const sel = document.querySelector("#billProduct").value;
  if (!sel) return;
  const p = products.find((x) => x.id === sel);
  if (!p) return;
  const q = Math.max(1, Number(document.querySelector("#billQty").value || 1));
  if (q > p.stock) return alert("Not enough stock");
  const tr = document.querySelector("#billRows");
  const existing = [...tr.querySelectorAll("tr")].find(
    (x) => x.dataset.id === p.id,
  );
  if (existing) {
    existing.querySelector(".qty").value =
      Number(existing.querySelector(".qty").value) + q;
  } else {
    const r = document.createElement("tr");
    r.dataset.id = p.id;
    r.innerHTML = `<td>${esc(p.name)}</td><td>${money(p.price)}</td><td><input class="qty" type="number" min="1" max="${p.stock}" value="${q}" style="width:80px" oninput="calcBill()"></td><td class="line">${money(p.price * q)}</td><td><button type="button" onclick="this.closest('tr').remove();calcBill()">×</button></td>`;
    tr.appendChild(r);
  }
  calcBill();
}
function calcBill() {
  let sub = 0;
  document.querySelectorAll("#billRows tr").forEach((r) => {
    const p = products.find((x) => x.id === r.dataset.id),
      q = Number(r.querySelector(".qty").value || 0),
      line = (p?.price || 0) * q;
    r.querySelector(".line").textContent = money(line);
    sub += line;
  });
  const tax =
    (sub * Number(document.querySelector("#taxRate").value || 0)) / 100;
  document.querySelector("#subtotal").textContent = money(sub);
  document.querySelector("#tax").textContent = money(tax);
  document.querySelector("#total").textContent = money(sub + tax);
}
async function invoiceSubmit(e) {
  e.preventDefault();
  try {
    const rows = [...document.querySelectorAll("#billRows tr")];
    if (!rows.length) throw new Error("Add at least one product");
    const items = rows.map((r) => ({
      id: r.dataset.id,
      qty: Number(r.querySelector(".qty").value),
    }));
    const subtotal = rows.reduce(
      (a, r) =>
        a +
        (products.find((p) => p.id === r.dataset.id)?.price || 0) *
          Number(r.querySelector(".qty").value),
      0,
    );
    const tax =
      (subtotal * Number(document.querySelector("#taxRate").value || 0)) / 100;
    const total = subtotal + tax;
    const d = await api("/invoice", {
      method: "POST",
      body: JSON.stringify({
        customer:
          document.querySelector("#customer").value || "Walk-in Customer",
        payment: document.querySelector("#payment").value,
        items: JSON.stringify(items),
        subtotal,
        tax,
        total,
      }),
    });
    showAlert(
      "billAlert",
      `Invoice ${d.invoice} created successfully`,
      "success",
    );
    e.target.reset();
    document.querySelector("#billRows").innerHTML = "";
    calcBill();
    loadProducts();
  } catch (x) {
    showAlert("billAlert", x.message);
  }
}
async function loadTable(endpoint, target, kind) {
  requireAuth();
  const data = await api(endpoint),
    tb = document.querySelector(target);
  if (!tb) return;
  if (kind === "sales")
    tb.innerHTML =
      data
        .reverse()
        .map(
          (x) =>
            `<tr><td>${esc(x.invoice)}</td><td>${esc(x.date)}</td><td>${esc(x.customer)}</td><td>${x.items}</td><td>${money(x.amount)}</td><td>${esc(x.payment)}</td><td><span class="badge ok">Paid</span></td></tr>`,
        )
        .join("") || '<tr><td colspan="7">No sales yet.</td></tr>';
  if (kind === "purchases")
    tb.innerHTML =
      data
        .reverse()
        .map(
          (x) =>
            `<tr><td>${esc(x.id)}</td><td>${esc(x.date)}</td><td>${esc(x.product)}</td><td>${x.quantity}</td><td>${money(x.amount)}</td><td>${esc(x.supplier)}</td></tr>`,
        )
        .join("") || '<tr><td colspan="6">No purchases yet.</td></tr>';
  if (kind === "invoices")
    tb.innerHTML =
      data
        .reverse()
        .map(
          (x) =>
            `<tr><td>${esc(x.invoice)}</td><td>${esc(x.date)}</td><td>${esc(x.customer)}</td><td>${money(x.subtotal)}</td><td>${money(x.tax)}</td><td><b>${money(x.total)}</b></td><td>${esc(x.payment)}</td></tr>`,
        )
        .join("") || '<tr><td colspan="7">No invoices yet.</td></tr>';
}
async function purchaseSubmit(e) {
  e.preventDefault();
  try {
    const body = Object.fromEntries(new FormData(e.target));
    body.quantity = Number(body.quantity);
    body.amount = Number(body.amount || 0);
    await api("/purchase", { method: "POST", body: JSON.stringify(body) });
    showAlert("alert", "Stock added successfully.", "success");
    e.target.reset();
    loadProducts();
  } catch (x) {
    showAlert("alert", x.message);
  }
}
function fillProductOptions() {
  const s =
    document.querySelector("#product_id") ||
    document.querySelector("#billProduct");
  if (!s) return;
  s.innerHTML =
    '<option value="">Select product</option>' +
    products
      .map(
        (p) =>
          `<option value="${p.id}">${esc(p.name)} — ${money(p.price)} (stock ${p.stock})</option>`,
      )
      .join("");
}
