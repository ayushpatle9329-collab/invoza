// Smart-Bill / INVOZA
// Dependency-free C++17 HTTP backend for Windows and Linux
//
// Build:
// g++ -std=c++17 backend/main.cpp -o backend/smartbill_server.exe -lws2_32

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;

int closesocket(SOCKET socket) {
    return close(socket);
}
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

namespace fs = std::filesystem;
using namespace std;

int serverPort() {
    const char* value = getenv("PORT");

    if (!value || !*value)
        return 8080;

    char* end = nullptr;
    long port = strtol(value, &end, 10);

    return end && *end == '\0' &&
           port > 0 && port <= 65535
        ? static_cast<int>(port)
        : 8080;
}

const int PORT = serverPort();

const string DB_DIR = [] {
    const char* value = getenv("SMARTBILL_DB_DIR");

    return value && *value
        ? string(value)
        : string("database");
}();

mutex dbMutex;

// ------------------------------------------------------------
// Utility
// ------------------------------------------------------------

string nowISO() {
    time_t t = time(nullptr);
    tm lt{};

#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif

    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &lt);

    return b;
}

string jsonEscape(const string& s) {
    string o;

    for (char c : s) {
        if (c == '\\')
            o += "\\\\";
        else if (c == '"')
            o += "\\\"";
        else if (c == '\n')
            o += "\\n";
        else if (c == '\r')
            o += "\\r";
        else if (c == '\t')
            o += "\\t";
        else
            o += c;
    }

    return o;
}

string bodyValue(const string& body, const string& key) {
    string k = "\"" + key + "\"";

    size_t p = body.find(k);

    if (p == string::npos)
        return "";

    p = body.find(':', p);

    if (p == string::npos)
        return "";

    p++;

    while (p < body.size() &&
           isspace(static_cast<unsigned char>(body[p]))) {
        p++;
    }

    // String value
    if (p < body.size() && body[p] == '"') {
        p++;

        string o;
        bool escaped = false;

        for (; p < body.size(); p++) {
            char c = body[p];

            if (escaped) {
                if (c == 'n')
                    o += '\n';
                else if (c == 'r')
                    o += '\r';
                else if (c == 't')
                    o += '\t';
                else
                    o += c;

                escaped = false;
            }
            else if (c == '\\') {
                escaped = true;
            }
            else if (c == '"') {
                break;
            }
            else {
                o += c;
            }
        }

        return o;
    }

    // Number / boolean / simple JSON value
    size_t e = p;

    while (e < body.size() &&
           body[e] != ',' &&
           body[e] != '}' &&
           !isspace(static_cast<unsigned char>(body[e]))) {
        e++;
    }

    return body.substr(p, e - p);
}

int intVal(const string& s) {
    try {
        return stoi(s);
    }
    catch (...) {
        return 0;
    }
}

double dblVal(const string& s) {
    try {
        return stod(s);
    }
    catch (...) {
        return 0;
    }
}

string lower(string s) {
    transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        }
    );

    return s;
}

string trim(string s) {
    while (!s.empty() &&
           isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }

    while (!s.empty() &&
           isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }

    return s;
}

vector<string> split(const string& s, char d) {
    vector<string> v;
    string x;

    stringstream ss(s);

    while (getline(ss, x, d))
        v.push_back(x);

    return v;
}

string makeId(const string& prefix) {
    static mt19937 rng(
        static_cast<unsigned>(
            chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count()
        )
    );

    return prefix + to_string(100000 + rng() % 900000);
}

// ------------------------------------------------------------
// Database
// ------------------------------------------------------------

void ensureDB() {

    fs::create_directories(DB_DIR);

    if (!fs::exists(DB_DIR + "/users.csv")) {

        ofstream f(DB_DIR + "/users.csv");

        f << "id|name|email|password|created_at\n";

        f << "1|Admin|admin@invoza.com|admin123|"
          << nowISO()
          << "\n";
    }

    if (!fs::exists(DB_DIR + "/products.csv")) {

        ofstream f(DB_DIR + "/products.csv");

        f << "id|name|category|price|stock|sku|supplier|updated_at\n";

        f << "P1001|Paracetamol 500mg|Medicine|25|100|"
          << "PARA500|ABC Pharma|"
          << nowISO()
          << "\n";

        f << "P1002|Notebook A4|Stationery|80|50|"
          << "NOTE-A4|Local Supplier|"
          << nowISO()
          << "\n";

        f << "P1003|Ball Pen|Stationery|10|200|"
          << "PEN-BLUE|Local Supplier|"
          << nowISO()
          << "\n";
    }

    if (!fs::exists(DB_DIR + "/sales.csv")) {

        ofstream f(DB_DIR + "/sales.csv");

        f << "id|invoice|customer|items|amount|payment|date\n";
    }

    if (!fs::exists(DB_DIR + "/purchases.csv")) {

        ofstream f(DB_DIR + "/purchases.csv");

        f << "id|product|quantity|amount|supplier|date\n";
    }

    if (!fs::exists(DB_DIR + "/invoices.csv")) {

        ofstream f(DB_DIR + "/invoices.csv");

        f << "id|invoice|customer|items_json|"
             "subtotal|tax|total|payment|date\n";
    }

    if (!fs::exists(DB_DIR + "/contacts.csv")) {

        ofstream f(DB_DIR + "/contacts.csv");

        f << "id|name|email|phone|subject|message|date\n";
    }
}

vector<vector<string>> readRows(const string& file) {

    lock_guard<mutex> g(dbMutex);

    vector<vector<string>> rows;

    ifstream f(DB_DIR + "/" + file);

    if (!f)
        return rows;

    string line;

    bool first = true;

    while (getline(f, line)) {

        if (first) {
            first = false;
            continue;
        }

        if (!line.empty())
            rows.push_back(split(line, '|'));
    }

    return rows;
}

void appendRow(
    const string& file,
    const vector<string>& row
) {

    lock_guard<mutex> g(dbMutex);

    ofstream f(
        DB_DIR + "/" + file,
        ios::app
    );

    for (size_t i = 0; i < row.size(); i++) {

        if (i)
            f << '|';

        string x = row[i];

        replace(
            x.begin(),
            x.end(),
            '|',
            '/'
        );

        replace(
            x.begin(),
            x.end(),
            '\n',
            ' '
        );

        replace(
            x.begin(),
            x.end(),
            '\r',
            ' '
        );

        f << x;
    }

    f << '\n';
}

void rewriteRows(
    const string& file,
    const vector<vector<string>>& rows
) {

    lock_guard<mutex> g(dbMutex);

    ifstream old(DB_DIR + "/" + file);

    string header;

    getline(old, header);

    old.close();

    ofstream f(DB_DIR + "/" + file);

    f << header << '\n';

    for (const auto& row : rows) {

        for (size_t i = 0; i < row.size(); i++) {

            if (i)
                f << '|';

            f << row[i];
        }

        f << '\n';
    }
}

// ------------------------------------------------------------
// JSON APIs
// ------------------------------------------------------------

string productsJson() {

    auto rows = readRows("products.csv");

    string o = "[";

    for (auto& r : rows) {

        if (r.size() < 8)
            continue;

        if (o.size() > 1)
            o += ",";

        o +=
            "{\"id\":\"" + jsonEscape(r[0]) +
            "\",\"name\":\"" + jsonEscape(r[1]) +
            "\",\"category\":\"" + jsonEscape(r[2]) +
            "\",\"price\":" + to_string(dblVal(r[3])) +
            ",\"stock\":" + to_string(intVal(r[4])) +
            ",\"sku\":\"" + jsonEscape(r[5]) +
            "\",\"supplier\":\"" + jsonEscape(r[6]) +
            "\",\"updated_at\":\"" + jsonEscape(r[7]) +
            "\"}";
    }

    return o + "]";
}

string salesJson() {

    auto rows = readRows("sales.csv");

    string o = "[";

    for (auto& r : rows) {

        if (r.size() < 7)
            continue;

        if (o.size() > 1)
            o += ",";

        o +=
            "{\"id\":\"" + jsonEscape(r[0]) +
            "\",\"invoice\":\"" + jsonEscape(r[1]) +
            "\",\"customer\":\"" + jsonEscape(r[2]) +
            "\",\"items\":" + to_string(intVal(r[3])) +
            ",\"amount\":" + to_string(dblVal(r[4])) +
            ",\"payment\":\"" + jsonEscape(r[5]) +
            "\",\"date\":\"" + jsonEscape(r[6]) +
            "\"}";
    }

    return o + "]";
}

string purchasesJson() {

    auto rows = readRows("purchases.csv");

    string o = "[";

    for (auto& r : rows) {

        if (r.size() < 6)
            continue;

        if (o.size() > 1)
            o += ",";

        o +=
            "{\"id\":\"" + jsonEscape(r[0]) +
            "\",\"product\":\"" + jsonEscape(r[1]) +
            "\",\"quantity\":" + to_string(intVal(r[2])) +
            ",\"amount\":" + to_string(dblVal(r[3])) +
            ",\"supplier\":\"" + jsonEscape(r[4]) +
            "\",\"date\":\"" + jsonEscape(r[5]) +
            "\"}";
    }

    return o + "]";
}

string invoicesJson() {

    auto rows = readRows("invoices.csv");

    string o = "[";

    for (auto& r : rows) {

        if (r.size() < 9)
            continue;

        if (o.size() > 1)
            o += ",";

        o +=
            "{\"id\":\"" + jsonEscape(r[0]) +
            "\",\"invoice\":\"" + jsonEscape(r[1]) +
            "\",\"customer\":\"" + jsonEscape(r[2]) +
            "\",\"items\":" + r[3] +
            ",\"subtotal\":" + r[4] +
            ",\"tax\":" + r[5] +
            ",\"total\":" + r[6] +
            ",\"payment\":\"" + jsonEscape(r[7]) +
            "\",\"date\":\"" + jsonEscape(r[8]) +
            "\"}";
    }

    return o + "]";
}

string dashboardJson() {

    auto p = readRows("products.csv");
    auto s = readRows("sales.csv");
    auto pu = readRows("purchases.csv");

    double revenue = 0;
    double purchase = 0;

    int low = 0;
    int stock = 0;

    for (auto& r : p) {

        if (r.size() >= 5) {

            stock += intVal(r[4]);

            if (intVal(r[4]) <= 10)
                low++;
        }
    }

    for (auto& r : s) {

        if (r.size() >= 5)
            revenue += dblVal(r[4]);
    }

    for (auto& r : pu) {

        if (r.size() >= 4)
            purchase += dblVal(r[3]);
    }

    return
        "{\"products\":" +
        to_string(static_cast<int>(p.size())) +

        ",\"stock\":" +
        to_string(stock) +

        ",\"low_stock\":" +
        to_string(low) +

        ",\"sales_count\":" +
        to_string(static_cast<int>(s.size())) +

        ",\"revenue\":" +
        to_string(revenue) +

        ",\"purchase_total\":" +
        to_string(purchase) +

        "}";
}

// ------------------------------------------------------------
// HTTP REQUEST
// ------------------------------------------------------------

struct Request {
    string method;
    string path;
    string body;
};

bool receiveMore(
    SOCKET client,
    string& data,
    size_t wanted
) {

    char buffer[8192];

    while (data.size() < wanted) {

        int n = recv(
            client,
            buffer,
            sizeof(buffer),
            0
        );

        if (n <= 0)
            return false;

        data.append(
            buffer,
            n
        );
    }

    return true;
}

Request parseRequest(
    SOCKET client,
    string raw
) {

    Request q;

    size_t headerEnd =
        raw.find("\r\n\r\n");

    if (headerEnd == string::npos)
        return q;

    string header =
        raw.substr(0, headerEnd);

    string body =
        raw.substr(headerEnd + 4);

    string firstLine =
        header.substr(
            0,
            header.find("\r\n")
        );

    stringstream ss(firstLine);

    ss >> q.method >> q.path;

    // Find Content-Length safely
    size_t clPos =
        header.find("Content-Length:");

    if (clPos != string::npos) {

        clPos += strlen("Content-Length:");

        size_t end =
            header.find("\r\n", clPos);

        string value =
            header.substr(
                clPos,
                end == string::npos
                    ? string::npos
                    : end - clPos
            );

        int contentLength =
            intVal(trim(value));

        if (contentLength > 0 &&
            body.size() < static_cast<size_t>(contentLength)) {

            receiveMore(
                client,
                body,
                static_cast<size_t>(contentLength)
            );
        }

        if (body.size() >
            static_cast<size_t>(contentLength)) {

            body.resize(
                static_cast<size_t>(contentLength)
            );
        }
    }

    q.body = body;

    return q;
}

string recvRequest(SOCKET client) {

    string data;

    char buffer[8192];

    size_t headerEnd =
        string::npos;

    // Receive headers first
    while (headerEnd == string::npos) {

        int n = recv(
            client,
            buffer,
            sizeof(buffer),
            0
        );

        if (n <= 0)
            return "";

        data.append(
            buffer,
            n
        );

        headerEnd =
            data.find("\r\n\r\n");

        // Protection against absurd headers
        if (data.size() > 1024 * 1024)
            return "";
    }

    return data;
}

// ------------------------------------------------------------
// HTTP RESPONSE
// ------------------------------------------------------------

string response(
    int code,
    const string& type,
    const string& body
) {

    string msg;

    if (code == 200)
        msg = "OK";
    else if (code == 201)
        msg = "Created";
    else if (code == 400)
        msg = "Bad Request";
    else if (code == 401)
        msg = "Unauthorized";
    else if (code == 404)
        msg = "Not Found";
    else
        msg = "Internal Server Error";

    return
        "HTTP/1.1 " +
        to_string(code) +
        " " +
        msg +
        "\r\n"

        "Content-Type: " +
        type +
        "\r\n"

        "Access-Control-Allow-Origin: *\r\n"

        "Access-Control-Allow-Headers: "
        "Content-Type\r\n"

        "Access-Control-Allow-Methods: "
        "GET,POST,PUT,DELETE,OPTIONS\r\n"

        "Content-Length: " +
        to_string(body.size()) +
        "\r\n"

        "Connection: close\r\n"

        "\r\n" +

        body;
}

// ------------------------------------------------------------
// STATIC FILE SERVER
// ------------------------------------------------------------

string mime(const string& p) {

    size_t dot =
        p.find_last_of('.');

    if (dot == string::npos)
        return "text/plain";

    string e =
        lower(p.substr(dot + 1));

    if (e == "html")
        return "text/html; charset=utf-8";

    if (e == "css")
        return "text/css; charset=utf-8";

    if (e == "js")
        return "application/javascript; charset=utf-8";

    if (e == "json")
        return "application/json";

    if (e == "png")
        return "image/png";

    if (e == "jpg" || e == "jpeg")
        return "image/jpeg";

    if (e == "svg")
        return "image/svg+xml";

    if (e == "ico")
        return "image/x-icon";

    return "text/plain; charset=utf-8";
}

string staticFile(string path) {

    if (path == "/")
        path = "/pages/index.html";

    // Basic path traversal protection
    if (path.find("..") != string::npos)
        return "";

    string fp = "." + path;

    ifstream f(
        fp,
        ios::binary
    );

    if (!f)
        return "";

    return string(
        (istreambuf_iterator<char>(f)),
        istreambuf_iterator<char>()
    );
}

// ------------------------------------------------------------
// API
// ------------------------------------------------------------

string api(
    const Request& q,
    int& code,
    string& ctype
) {

    ctype = "application/json; charset=utf-8";

    string p = q.path;

    // CORS preflight
    if (q.method == "OPTIONS")
        return "{}";

    if (p == "/api/health" &&
        q.method == "GET") {

        return "{\"success\":true,\"status\":\"ok\"}";
    }

    // --------------------------------------------------------
    // LOGIN
    // --------------------------------------------------------

    if (p == "/api/login" &&
        q.method == "POST") {

        string email =
            lower(
                trim(
                    bodyValue(
                        q.body,
                        "email"
                    )
                )
            );

        string pass =
            bodyValue(
                q.body,
                "password"
            );

        for (auto& r : readRows("users.csv")) {

            if (r.size() >= 4 &&
                lower(r[2]) == email &&
                r[3] == pass) {

                return
                    "{\"success\":true,"
                    "\"user\":{"
                    "\"id\":\"" +
                    jsonEscape(r[0]) +

                    "\",\"name\":\"" +
                    jsonEscape(r[1]) +

                    "\",\"email\":\"" +
                    jsonEscape(r[2]) +

                    "\"}}";
            }
        }

        code = 401;

        return
            "{\"success\":false,"
            "\"message\":\"Invalid email or password\"}";
    }

    // --------------------------------------------------------
    // SIGNUP
    // --------------------------------------------------------

    if (p == "/api/signup" &&
        q.method == "POST") {

        string name =
            bodyValue(q.body, "name");

        string email =
            lower(
                trim(
                    bodyValue(
                        q.body,
                        "email"
                    )
                )
            );

        string pass =
            bodyValue(
                q.body,
                "password"
            );

        if (name.empty() ||
            email.empty() ||
            pass.size() < 6) {

            code = 400;

            return
                "{\"success\":false,"
                "\"message\":\"Name, email and "
                "password (6+ characters) "
                "are required\"}";
        }

        for (auto& r :
             readRows("users.csv")) {

            if (r.size() >= 4 &&
                lower(r[2]) == email) {

                code = 400;

                return
                    "{\"success\":false,"
                    "\"message\":\"Email already "
                    "registered\"}";
            }
        }

        string id =
            makeId("U");

        appendRow(
            "users.csv",
            {
                id,
                name,
                email,
                pass,
                nowISO()
            }
        );

        return
            "{\"success\":true,"
            "\"message\":\"Account created "
            "successfully\"}";
    }

    // --------------------------------------------------------
    // PRODUCTS - GET
    // --------------------------------------------------------

    if (p == "/api/products" &&
        q.method == "GET") {

        return productsJson();
    }

    // --------------------------------------------------------
    // PRODUCTS - POST
    // --------------------------------------------------------

    if (p == "/api/products" &&
        q.method == "POST") {

        string name =
            bodyValue(q.body, "name");

        string cat =
            bodyValue(q.body, "category");

        string sku =
            bodyValue(q.body, "sku");

        string sup =
            bodyValue(q.body, "supplier");

        double price =
            dblVal(
                bodyValue(
                    q.body,
                    "price"
                )
            );

        int stock =
            intVal(
                bodyValue(
                    q.body,
                    "stock"
                )
            );

        if (name.empty() ||
            price < 0 ||
            stock < 0) {

            code = 400;

            return
                "{\"success\":false,"
                "\"message\":\"Valid product "
                "details are required\"}";
        }

        string id =
            makeId("P");

        appendRow(
            "products.csv",
            {
                id,
                name,
                cat,
                to_string(price),
                to_string(stock),
                sku,
                sup,
                nowISO()
            }
        );

        code = 201;

        return
            "{\"success\":true,"
            "\"id\":\"" +
            jsonEscape(id) +
            "\"}";
    }

    // --------------------------------------------------------
    // PRODUCTS - PUT
    // --------------------------------------------------------

    if (p == "/api/products" &&
        q.method == "PUT") {

        string id =
            bodyValue(q.body, "id");

        auto rows =
            readRows("products.csv");

        bool found = false;

        for (auto& r : rows) {

            if (r.size() >= 8 &&
                r[0] == id) {

                string v;

                v = bodyValue(q.body, "name");
                if (!v.empty())
                    r[1] = v;

                v = bodyValue(q.body, "category");
                if (!v.empty())
                    r[2] = v;

                v = bodyValue(q.body, "price");
                if (!v.empty())
                    r[3] = v;

                v = bodyValue(q.body, "stock");
                if (!v.empty())
                    r[4] = v;

                v = bodyValue(q.body, "sku");
                if (!v.empty())
                    r[5] = v;

                v = bodyValue(q.body, "supplier");
                if (!v.empty())
                    r[6] = v;

                r[7] = nowISO();

                found = true;

                break;
            }
        }

        if (!found) {

            code = 404;

            return
                "{\"success\":false,"
                "\"message\":\"Product not found\"}";
        }

        rewriteRows(
            "products.csv",
            rows
        );

        return
            "{\"success\":true}";
    }

    // --------------------------------------------------------
    // PRODUCTS - DELETE
    // --------------------------------------------------------

    if (p == "/api/products" &&
        q.method == "DELETE") {

        string id =
            bodyValue(q.body, "id");

        auto rows =
            readRows("products.csv");

        auto it =
            remove_if(
                rows.begin(),
                rows.end(),
                [&](const auto& r) {
                    return !r.empty() &&
                           r[0] == id;
                }
            );

        if (it == rows.end()) {

            code = 404;

            return
                "{\"success\":false,"
                "\"message\":\"Product not found\"}";
        }

        rows.erase(
            it,
            rows.end()
        );

        rewriteRows(
            "products.csv",
            rows
        );

        return
            "{\"success\":true}";
    }

    // --------------------------------------------------------
    // DASHBOARD
    // --------------------------------------------------------

    if (p == "/api/dashboard" &&
        q.method == "GET") {

        return dashboardJson();
    }

    // --------------------------------------------------------
    // SALES
    // --------------------------------------------------------

    if (p == "/api/sales" &&
        q.method == "GET") {

        return salesJson();
    }

    // --------------------------------------------------------
    // PURCHASES
    // --------------------------------------------------------

    if (p == "/api/purchases" &&
        q.method == "GET") {

        return purchasesJson();
    }

    // --------------------------------------------------------
    // INVOICES
    // --------------------------------------------------------

    if (p == "/api/invoices" &&
        q.method == "GET") {

        return invoicesJson();
    }

    // --------------------------------------------------------
    // PURCHASE
    // --------------------------------------------------------

    if (p == "/api/purchase" &&
        q.method == "POST") {

        string pid =
            bodyValue(
                q.body,
                "product_id"
            );

        string sup =
            bodyValue(
                q.body,
                "supplier"
            );

        int qty =
            intVal(
                bodyValue(
                    q.body,
                    "quantity"
                )
            );

        double amount =
            dblVal(
                bodyValue(
                    q.body,
                    "amount"
                )
            );

        if (pid.empty() ||
            qty <= 0) {

            code = 400;

            return
                "{\"success\":false,"
                "\"message\":\"Product and "
                "positive quantity are required\"}";
        }

        auto rows =
            readRows("products.csv");

        bool found = false;

        string productName;

        for (auto& r : rows) {

            if (r.size() >= 8 &&
                r[0] == pid) {

                int currentStock =
                    intVal(r[4]);

                r[4] =
                    to_string(
                        currentStock + qty
                    );

                r[7] = nowISO();

                productName = r[1];

                found = true;

                break;
            }
        }

        if (!found) {

            code = 404;

            return
                "{\"success\":false,"
                "\"message\":\"Product not found\"}";
        }

        rewriteRows(
            "products.csv",
            rows
        );

        appendRow(
            "purchases.csv",
            {
                makeId("PUR"),
                productName,
                to_string(qty),
                to_string(amount),
                sup,
                nowISO()
            }
        );

        return
            "{\"success\":true}";
    }

    // --------------------------------------------------------
    // INVOICE
    // --------------------------------------------------------

    if (p == "/api/invoice" &&
        q.method == "POST") {

        string customer =
            bodyValue(
                q.body,
                "customer"
            );

        string payment =
            bodyValue(
                q.body,
                "payment"
            );

        string items =
            bodyValue(
                q.body,
                "items"
            );

        double subtotal =
            dblVal(
                bodyValue(
                    q.body,
                    "subtotal"
                )
            );

        double tax =
            dblVal(
                bodyValue(
                    q.body,
                    "tax"
                )
            );

        double total =
            dblVal(
                bodyValue(
                    q.body,
                    "total"
                )
            );

        if (items.empty() ||
            total < 0) {

            code = 400;

            return
                "{\"success\":false,"
                "\"message\":\"Invoice items "
                "are required\"}";
        }

        string invoiceNumber =
            makeId("INV-");

        appendRow(
            "invoices.csv",
            {
                makeId("I"),
                invoiceNumber,
                customer,
                items,
                to_string(subtotal),
                to_string(tax),
                to_string(total),
                payment,
                nowISO()
            }
        );

        // Update stock
        int totalItems = 0;

        auto rows =
            readRows("products.csv");

        size_t pos = 0;

        while (
            (pos = items.find(
                "\"id\"",
                pos
            )) != string::npos
        ) {

            string part =
                items.substr(pos);

            string id =
                bodyValue(
                    part,
                    "id"
                );

            int qty = 0;

            size_t qpos =
                items.find(
                    "\"qty\"",
                    pos
                );

            if (qpos != string::npos) {

                string qtyPart =
                    items.substr(qpos);

                qty =
                    intVal(
                        bodyValue(
                            qtyPart,
                            "qty"
                        )
                    );
            }

            if (!id.empty() &&
                qty > 0) {

                for (auto& r : rows) {

                    if (r.size() >= 8 &&
                        r[0] == id) {

                        int current =
                            intVal(r[4]);

                        r[4] =
                            to_string(
                                max(
                                    0,
                                    current - qty
                                )
                            );

                        r[7] = nowISO();

                        totalItems += qty;

                        break;
                    }
                }
            }

            pos += 5;
        }

        rewriteRows(
            "products.csv",
            rows
        );

        appendRow(
            "sales.csv",
            {
                makeId("S"),
                invoiceNumber,
                customer,
                to_string(totalItems),
                to_string(total),
                payment,
                nowISO()
            }
        );

        return
            "{\"success\":true,"
            "\"invoice\":\"" +
            jsonEscape(invoiceNumber) +
            "\"}";
    }

    // --------------------------------------------------------
    // CONTACT
    // --------------------------------------------------------

    if (p == "/api/contact" &&
        q.method == "POST") {

        appendRow(
            "contacts.csv",
            {
                makeId("C"),
                bodyValue(q.body, "name"),
                bodyValue(q.body, "email"),
                bodyValue(q.body, "phone"),
                bodyValue(q.body, "subject"),
                bodyValue(q.body, "message"),
                nowISO()
            }
        );

        return
            "{\"success\":true,"
            "\"message\":\"Thanks! Your message "
            "has been received.\"}";
    }

    // Unknown route
    code = 404;

    return
        "{\"success\":false,"
        "\"message\":\"API route not found\"}";
}

// ------------------------------------------------------------
// CLIENT HANDLER
// ------------------------------------------------------------

void handle(SOCKET client) {

    string initial =
        recvRequest(client);

    if (initial.empty()) {

        closesocket(client);

        return;
    }

    Request q =
        parseRequest(
            client,
            initial
        );

    if (q.method.empty()) {

        closesocket(client);

        return;
    }

    int code = 200;

    string type =
        "application/json; charset=utf-8";

    string body;

    if (
        q.path.rfind(
            "/api/",
            0
        ) == 0 ||
        q.method == "OPTIONS"
    ) {

        body =
            api(
                q,
                code,
                type
            );
    }
    else {

        body =
            staticFile(
                q.path
            );

        if (body.empty()) {

            code = 404;

            type =
                "text/html; charset=utf-8";

            body =
                "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "<title>404</title>"
                "</head>"
                "<body>"
                "<h1>404 Not Found</h1>"
                "</body>"
                "</html>";
        }
        else {

            type = mime(q.path == "/" ? "/pages/index.html" : q.path);
        }
    }

    string out =
        response(
            code,
            type,
            body
        );

    size_t sent = 0;

    while (sent < out.size()) {

        int n =
            send(
                client,
                out.c_str() + sent,
                static_cast<int>(
                    out.size() - sent
                ),
                0
            );

        if (n <= 0)
            break;

        sent += n;
    }

    closesocket(client);
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------

int main() {

    ensureDB();

#ifdef _WIN32
    WSADATA wsa;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa
        ) != 0
    ) {

        cerr
            << "WSAStartup failed\n";

        return 1;
    }
#endif

    SOCKET server =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

    if (server == INVALID_SOCKET) {

        cerr
            << "Socket creation failed\n";

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    sockaddr_in addr{};

    addr.sin_family =
        AF_INET;

    addr.sin_addr.s_addr =
        INADDR_ANY;

    addr.sin_port =
        htons(PORT);

    int opt = 1;

    setsockopt(
        server,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<char*>(&opt),
        sizeof(opt)
    );

    if (
        bind(
            server,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        ) == SOCKET_ERROR
    ) {

        cerr
            << "Bind failed on port "
            << PORT
            << "\n";

        closesocket(server);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    if (
        listen(
            server,
            16
        ) == SOCKET_ERROR
    ) {

        cerr
            << "Listen failed\n";

        closesocket(server);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    cout
        << "====================================\n"
        << "      INVOZA Smart-Bill Server\n"
        << "====================================\n"
        << "Server running at:\n"
        << "http://localhost:"
        << PORT
        << "\n\n"
        << "Press Ctrl+C to stop.\n"
        << "====================================\n";

    while (true) {

        SOCKET client =
            accept(
                server,
                nullptr,
                nullptr
            );

        if (
            client != INVALID_SOCKET
        ) {

            handle(client);
        }
    }

    closesocket(server);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
