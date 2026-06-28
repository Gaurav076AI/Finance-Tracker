// Finance Tracker — C++ Backend Server
// -------------------------------------
// Provides a REST JSON API and serves the existing frontend static files.
// Frontend source files are NOT modified; open http://localhost:8080 after starting.
//
// Build:  make
// Run:    ./finance_server
//
// API base URL: http://localhost:8080/api

#include "http_server.hpp"
#include "json_util.hpp"
#include "models.hpp"
#include "store.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace {

struct AppContext {
    ft::FinanceStore* store = nullptr;
};

// Extract Bearer token from Authorization header.
std::string getBearerToken(const ft::HttpRequest& req) {
    auto it = req.headers.find("authorization");
    if (it == req.headers.end()) return "";
    const std::string& value = it->second;
    const std::string prefix = "Bearer ";
    if (value.rfind(prefix, 0) == 0) {
        return value.substr(prefix.size());
    }
    return "";
}

std::uint64_t userFromRequest(const ft::HttpRequest& req, AppContext* ctx) {
    const std::string token = getBearerToken(req);
    if (token.empty()) return 0;
    return ctx->store->userIdFromToken(token);
}

ft::UserProfile profileFromFields(const std::unordered_map<std::string, std::string>& fields) {
    ft::UserProfile profile;
    profile.isStudent = ft::fieldBool(fields, "isStudent", true);
    profile.inHostel = ft::fieldBool(fields, "inHostel", true);
    profile.maritalStatus = ft::fieldStr(fields, "maritalStatus");
    profile.hasKids = ft::fieldBool(fields, "hasKids", false);
    profile.hasLoan = ft::fieldBool(fields, "hasLoan", false);
    profile.loanAmount = ft::fieldInt(fields, "loanAmount", 0);
    profile.budget = ft::fieldInt(fields, "budget", 0);
    return profile;
}

// GET /api/health — quick check that backend is running.
ft::HttpResponse handleHealth(const ft::HttpRequest&, void*) {
    return ft::HttpResponse::json(200,
        "{\"success\":true,\"service\":\"finance-tracker\",\"status\":\"ok\"}");
}

// POST /api/auth/register
// Creates a user with onboarding profile. Starts with zero transactions.
ft::HttpResponse handleRegister(const ft::HttpRequest& req, void* context) {
    auto* ctx = static_cast<AppContext*>(context);
    const auto fields = ft::parseFlatObject(req.body);

    const std::string email = ft::fieldStr(fields, "email");
    const std::string password = ft::fieldStr(fields, "password");
    if (email.empty() || password.empty()) {
        return ft::HttpResponse::json(400,
            "{\"success\":false,\"error\":\"email and password are required\"}");
    }

    const ft::UserProfile profile = profileFromFields(fields);
    const std::string token = ctx->store->registerUser(email, password, profile);
    if (token.empty()) {
        return ft::HttpResponse::json(409,
            "{\"success\":false,\"error\":\"email already registered\"}");
    }

    const ft::User* user = ctx->store->findUserByEmail(email);
    const ft::DashboardData dash = ctx->store->buildDashboard(*user);

    std::ostringstream out;
    out << "{\"success\":true,";
    out << "\"token\":\"" << ft::jsonEscape(token) << "\",";
    out << "\"user\":" << ctx->store->userToJson(*user) << ",";
    out << "\"dashboard\":" << ctx->store->dashboardToJson(dash) << "}";
    return ft::HttpResponse::json(201, out.str());
}

// POST /api/auth/login
ft::HttpResponse handleLogin(const ft::HttpRequest& req, void* context) {
    auto* ctx = static_cast<AppContext*>(context);
    const auto fields = ft::parseFlatObject(req.body);

    const std::string email = ft::fieldStr(fields, "email");
    const std::string password = ft::fieldStr(fields, "password");
    if (email.empty() || password.empty()) {
        return ft::HttpResponse::json(400,
            "{\"success\":false,\"error\":\"email and password are required\"}");
    }

    const std::string token = ctx->store->login(email, password);
    if (token.empty()) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"invalid email or password\"}");
    }

    const ft::User* user = ctx->store->findUserByEmail(email);
    const ft::DashboardData dash = ctx->store->buildDashboard(*user);

    std::ostringstream out;
    out << "{\"success\":true,";
    out << "\"token\":\"" << ft::jsonEscape(token) << "\",";
    out << "\"user\":" << ctx->store->userToJson(*user) << ",";
    out << "\"dashboard\":" << ctx->store->dashboardToJson(dash) << "}";
    return ft::HttpResponse::json(200, out.str());
}

// GET /api/dashboard — requires Authorization: Bearer <token>
ft::HttpResponse handleDashboard(const ft::HttpRequest& req, void* context) {
    auto* ctx = static_cast<AppContext*>(context);
    const std::uint64_t userId = userFromRequest(req, ctx);
    if (userId == 0) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"missing or invalid token\"}");
    }

    const ft::User* user = ctx->store->findUserById(userId);
    if (!user) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"user not found\"}");
    }

    const ft::DashboardData dash = ctx->store->buildDashboard(*user);
    std::ostringstream out;
    out << "{\"success\":true,";
    out << "\"user\":" << ctx->store->userToJson(*user) << ",";
    out << "\"dashboard\":" << ctx->store->dashboardToJson(dash) << "}";
    return ft::HttpResponse::json(200, out.str());
}

// POST /api/expenses — add a new expense for authenticated user.
ft::HttpResponse handleAddExpense(const ft::HttpRequest& req, void* context) {
    auto* ctx = static_cast<AppContext*>(context);
    const std::uint64_t userId = userFromRequest(req, ctx);
    if (userId == 0) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"missing or invalid token\"}");
    }

    const auto fields = ft::parseFlatObject(req.body);
    const std::string name = ft::fieldStr(fields, "name");
    const std::string category = ft::fieldStr(fields, "category");
    const int amount = ft::fieldInt(fields, "amount", 0);

    const std::uint64_t txId = ctx->store->addTransaction(userId, name, category, amount);
    if (txId == 0) {
        return ft::HttpResponse::json(400,
            "{\"success\":false,\"error\":\"invalid expense data\"}");
    }

    const ft::User* user = ctx->store->findUserById(userId);
    const ft::DashboardData dash = ctx->store->buildDashboard(*user);

    std::ostringstream out;
    out << "{\"success\":true,";
    out << "\"transactionId\":" << txId << ",";
    out << "\"dashboard\":" << ctx->store->dashboardToJson(dash) << "}";
    return ft::HttpResponse::json(201, out.str());
}

// PUT /api/profile — update onboarding profile fields.
ft::HttpResponse handleUpdateProfile(const ft::HttpRequest& req, void* context) {
    auto* ctx = static_cast<AppContext*>(context);
    const std::uint64_t userId = userFromRequest(req, ctx);
    if (userId == 0) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"missing or invalid token\"}");
    }

    const ft::User* user = ctx->store->findUserById(userId);
    if (!user) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"user not found\"}");
    }

    ft::UserProfile profile = user->profile;
    const auto fields = ft::parseFlatObject(req.body);
    if (fields.count("isStudent")) profile.isStudent = ft::fieldBool(fields, "isStudent");
    if (fields.count("inHostel")) profile.inHostel = ft::fieldBool(fields, "inHostel");
    if (fields.count("maritalStatus")) profile.maritalStatus = ft::fieldStr(fields, "maritalStatus");
    if (fields.count("hasKids")) profile.hasKids = ft::fieldBool(fields, "hasKids");
    if (fields.count("hasLoan")) profile.hasLoan = ft::fieldBool(fields, "hasLoan");
    if (fields.count("loanAmount")) profile.loanAmount = ft::fieldInt(fields, "loanAmount");
    if (fields.count("budget")) profile.budget = ft::fieldInt(fields, "budget");

    ctx->store->updateProfile(userId, profile);
    user = ctx->store->findUserById(userId);

    std::ostringstream out;
    out << "{\"success\":true,";
    out << "\"user\":" << ctx->store->userToJson(*user) << "}";
    return ft::HttpResponse::json(200, out.str());
}

// GET /api/expenses — list all transactions for authenticated user.
ft::HttpResponse handleListExpenses(const ft::HttpRequest& req, void* context) {
    auto* ctx = static_cast<AppContext*>(context);
    const std::uint64_t userId = userFromRequest(req, ctx);
    if (userId == 0) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"missing or invalid token\"}");
    }

    const ft::User* user = ctx->store->findUserById(userId);
    if (!user) {
        return ft::HttpResponse::json(401,
            "{\"success\":false,\"error\":\"user not found\"}");
    }

    std::ostringstream out;
    out << "{\"success\":true,\"expenses\":[";
    for (size_t i = 0; i < user->transactions.size(); ++i) {
        if (i) out << ",";
        const ft::Transaction& tx = user->transactions[i];
        out << "{";
        out << "\"id\":" << tx.id << ",";
        out << "\"name\":\"" << ft::jsonEscape(tx.name) << "\",";
        out << "\"category\":\"" << ft::jsonEscape(tx.category) << "\",";
        out << "\"amount\":" << tx.amount << ",";
        out << "\"timestamp\":" << tx.timestamp;
        out << "}";
    }
    out << "]}";
    return ft::HttpResponse::json(200, out.str());
}

} // namespace

int main() {
    try {
        // Resolve paths relative to backend/ directory.
        const std::string dataFile = "data/store.dat";
        const std::string frontendRoot = "../frontend";

        ft::FinanceStore store(dataFile);
        AppContext context;
        context.store = &store;

        ft::HttpServer server("0.0.0.0", 8080, frontendRoot);
        server.setContext(&context);

        // Register API routes — O(1) hash lookup per request.
        server.addRoute("GET", "/api/health", handleHealth);
        server.addRoute("POST", "/api/auth/register", handleRegister);
        server.addRoute("POST", "/api/auth/login", handleLogin);
        server.addRoute("GET", "/api/dashboard", handleDashboard);
        server.addRoute("POST", "/api/expenses", handleAddExpense);
        server.addRoute("PUT", "/api/profile", handleUpdateProfile);
        server.addRoute("GET", "/api/expenses", handleListExpenses);
        server.addRoute("OPTIONS", "/api/health", handleHealth);
        server.addRoute("OPTIONS", "/api/auth/register", handleRegister);
        server.addRoute("OPTIONS", "/api/auth/login", handleLogin);
        server.addRoute("OPTIONS", "/api/dashboard", handleDashboard);
        server.addRoute("OPTIONS", "/api/expenses", handleAddExpense);
        server.addRoute("OPTIONS", "/api/profile", handleUpdateProfile);

        std::cout << "============================================\n";
        std::cout << " Finance Tracker Backend (C++)\n";
        std::cout << " Frontend : http://localhost:8080\n";
        std::cout << " API      : http://localhost:8080/api/health\n";
        std::cout << " Data file: " << dataFile << "\n";
        std::cout << "============================================\n";

        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
