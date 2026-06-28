#pragma once

// In-memory data store with O(1) average lookups and JSON file persistence.
//
// Time complexity summary:
//   registerUser / findUserByEmail     → O(1) average (unordered_map)
//   findUserByToken                    → O(1) average
//   addTransaction                     → O(1) amortized (vector push_back)
//   buildDashboard                     → O(n + k log k), n = transactions, k = unique items

#include "json_util.hpp"
#include "models.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ft {

class FinanceStore {
public:
    explicit FinanceStore(std::string dataFilePath)
        : dataFilePath_(std::move(dataFilePath)) {
        loadFromDisk();
    }

    // Demo password hash — NOT cryptographically secure; use bcrypt/argon2 in production.
    static std::string hashPassword(const std::string& password) {
        std::hash<std::string> hasher;
        return std::to_string(hasher(password));
    }

    // Register a new user. Returns empty string on duplicate email.
    std::string registerUser(const std::string& email,
                             const std::string& password,
                             const UserProfile& profile) {
        const std::string normalized = normalizeEmail(email);
        if (normalized.empty()) return "";
        if (emailIndex_.count(normalized)) return "";

        User user;
        user.id = nextUserId_++;
        user.email = normalized;
        user.passwordHash = hashPassword(password);
        user.displayName = displayNameFromEmail(normalized);
        user.profile = profile;
        user.transactions.clear();

        users_[user.id] = user;
        emailIndex_[normalized] = user.id;
        persist();
        return createSession(user.id);
    }

    // Authenticate and return session token, or empty string on failure.
    std::string login(const std::string& email, const std::string& password) {
        const User* user = findUserByEmail(email);
        if (!user) return "";
        if (user->passwordHash != hashPassword(password)) return "";
        return createSession(user->id);
    }

    // Resolve bearer token to user id; O(1) average.
    std::uint64_t userIdFromToken(const std::string& token) const {
        auto it = sessions_.find(token);
        return it == sessions_.end() ? 0 : it->second;
    }

    User* findUserById(std::uint64_t id) {
        auto it = users_.find(id);
        return it == users_.end() ? nullptr : &it->second;
    }

    const User* findUserById(std::uint64_t id) const {
        auto it = users_.find(id);
        return it == users_.end() ? nullptr : &it->second;
    }

    const User* findUserByEmail(const std::string& email) const {
        auto it = emailIndex_.find(normalizeEmail(email));
        if (it == emailIndex_.end()) return nullptr;
        return findUserById(it->second);
    }

    // Append one expense. Returns new transaction id, or 0 on failure.
    std::uint64_t addTransaction(std::uint64_t userId,
                                 const std::string& name,
                                 const std::string& category,
                                 int amount) {
        User* user = findUserById(userId);
        if (!user || name.empty() || amount <= 0 || !isValidCategory(category)) {
            return 0;
        }

        Transaction tx;
        tx.id = nextTransactionId_++;
        tx.name = name;
        tx.category = category;
        tx.amount = amount;
        tx.timestamp = nowMillis();

        user->transactions.push_back(tx); // O(1) amortized
        persist();
        return tx.id;
    }

    // Update onboarding profile after registration.
    bool updateProfile(std::uint64_t userId, const UserProfile& profile) {
        User* user = findUserById(userId);
        if (!user) return false;
        user->profile = profile;
        persist();
        return true;
    }

    // Compute dashboard aggregates from user's transactions.
    DashboardData buildDashboard(const User& user) const {
        DashboardData dash;
        dash.budget = user.profile.budget;
        dash.hasLoan = user.profile.hasLoan;
        dash.loanAmount = user.profile.loanAmount;
        dash.daysLeftInMonth = daysLeftInMonth();

        const std::vector<Transaction>& txs = user.transactions;
        dash.recentTransactions = txs;
        std::reverse(dash.recentTransactions.begin(), dash.recentTransactions.end());
        if (dash.recentTransactions.size() > 10) {
            dash.recentTransactions.resize(10);
        }

        // O(n) — single pass over all transactions.
        std::unordered_map<std::string, int> categoryTotals;
        std::unordered_map<std::string, ItemAggregate> itemMap;

        for (const Transaction& tx : txs) {
            dash.spent += tx.amount;
            categoryTotals[tx.category] += tx.amount;

            // Key combines category + normalized name for O(1) item grouping.
            const std::string itemKey = tx.category + "::" + normalizeItemName(tx.name);
            ItemAggregate& item = itemMap[itemKey];
            if (item.count == 0) {
                item.name = tx.name;
                item.category = tx.category;
            }
            item.count += 1;
            item.totalAmount += tx.amount;
            item.lastUnitPrice = tx.amount;
        }

        dash.remaining = std::max(0, dash.budget - dash.spent);
        dash.budgetUsedPct = dash.budget > 0
            ? static_cast<int>((static_cast<long long>(dash.spent) * 100) / dash.budget)
            : 0;

        // Build category list.
        dash.categories.reserve(categoryTotals.size());
        for (const auto& entry : categoryTotals) {
            dash.categories.push_back({entry.first, entry.second});
        }
        std::sort(dash.categories.begin(), dash.categories.end(),
                  [](const CategorySummary& a, const CategorySummary& b) {
                      return a.total > b.total;
                  });

        if (!dash.categories.empty()) {
            dash.topCategory = dash.categories.front().category;
            dash.topCategoryAmount = dash.categories.front().total;
        }

        // Top items sorted by purchase count, then total spent.
        dash.topItems.reserve(itemMap.size());
        for (const auto& entry : itemMap) {
            dash.topItems.push_back(entry.second);
        }
        std::sort(dash.topItems.begin(), dash.topItems.end(),
                  [](const ItemAggregate& a, const ItemAggregate& b) {
                      if (a.count != b.count) return a.count > b.count;
                      return a.totalAmount > b.totalAmount;
                  });

        if (!dash.topItems.empty()) {
            dash.topItemName = dash.topItems.front().name;
            dash.topItemCount = dash.topItems.front().count;
            dash.topItemLastPrice = dash.topItems.front().lastUnitPrice;
        }

        return dash;
    }

    std::string dashboardToJson(const DashboardData& dash) const {
        std::ostringstream out;
        out << "{";
        out << "\"budget\":" << dash.budget << ",";
        out << "\"spent\":" << dash.spent << ",";
        out << "\"remaining\":" << dash.remaining << ",";
        out << "\"budgetUsedPct\":" << dash.budgetUsedPct << ",";
        out << "\"daysLeftInMonth\":" << dash.daysLeftInMonth << ",";
        out << "\"hasLoan\":" << (dash.hasLoan ? "true" : "false") << ",";
        out << "\"loanAmount\":" << dash.loanAmount << ",";
        out << "\"topCategory\":\"" << jsonEscape(dash.topCategory) << "\",";
        out << "\"topCategoryAmount\":" << dash.topCategoryAmount << ",";
        out << "\"topItemName\":\"" << jsonEscape(dash.topItemName) << "\",";
        out << "\"topItemCount\":" << dash.topItemCount << ",";
        out << "\"topItemLastPrice\":" << dash.topItemLastPrice << ",";

        out << "\"categories\":[";
        for (size_t i = 0; i < dash.categories.size(); ++i) {
            if (i) out << ",";
            out << "{\"category\":\"" << jsonEscape(dash.categories[i].category) << "\",";
            out << "\"total\":" << dash.categories[i].total << "}";
        }
        out << "],";

        out << "\"topItems\":[";
        for (size_t i = 0; i < dash.topItems.size(); ++i) {
            if (i) out << ",";
            const ItemAggregate& item = dash.topItems[i];
            const int avg = item.count > 0 ? item.totalAmount / item.count : 0;
            out << "{\"name\":\"" << jsonEscape(item.name) << "\",";
            out << "\"category\":\"" << jsonEscape(item.category) << "\",";
            out << "\"count\":" << item.count << ",";
            out << "\"totalAmount\":" << item.totalAmount << ",";
            out << "\"lastUnitPrice\":" << item.lastUnitPrice << ",";
            out << "\"avgUnitPrice\":" << avg << "}";
        }
        out << "],";

        out << "\"recentTransactions\":[";
        for (size_t i = 0; i < dash.recentTransactions.size(); ++i) {
            if (i) out << ",";
            out << transactionToJson(dash.recentTransactions[i]);
        }
        out << "]}";
        return out.str();
    }

    std::string userToJson(const User& user) const {
        std::ostringstream out;
        out << "{";
        out << "\"id\":" << user.id << ",";
        out << "\"email\":\"" << jsonEscape(user.email) << "\",";
        out << "\"displayName\":\"" << jsonEscape(user.displayName) << "\",";
        out << "\"profile\":";
        out << profileToJson(user.profile);
        out << "}";
        return out.str();
    }

private:
    std::string dataFilePath_;
    std::uint64_t nextUserId_ = 1;
    std::uint64_t nextTransactionId_ = 1;
    std::uint64_t nextSessionCounter_ = 1;

    std::unordered_map<std::uint64_t, User> users_;          // O(1) by id
    std::unordered_map<std::string, std::uint64_t> emailIndex_; // O(1) by email
    std::unordered_map<std::string, std::uint64_t> sessions_;   // O(1) by token

    static std::string normalizeEmail(const std::string& email) {
        std::string out;
        out.reserve(email.size());
        for (char c : email) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        // Trim spaces
        while (!out.empty() && std::isspace(static_cast<unsigned char>(out.front()))) {
            out.erase(out.begin());
        }
        while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
            out.pop_back();
        }
        return out;
    }

    static std::string normalizeItemName(const std::string& name) {
        std::string out;
        out.reserve(name.size());
        for (char c : name) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return out;
    }

    static std::string displayNameFromEmail(const std::string& email) {
        const size_t at = email.find('@');
        std::string base = at == std::string::npos ? email : email.substr(0, at);
        if (base.empty()) return "User";
        base[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[0])));
        return base;
    }

    static std::int64_t nowMillis() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    // O(1) calendar math — no scan over transactions.
    static int daysLeftInMonth() {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        const int today = local.tm_mday;
        local.tm_mday = 1;
        local.tm_mon += 1;
        std::mktime(&local);          // first day of next month
        local.tm_mday -= 1;
        std::mktime(&local);          // last day of current month
        return std::max(0, local.tm_mday - today);
    }

    std::string createSession(std::uint64_t userId) {
        const std::string token = "ft_" + std::to_string(userId) + "_"
            + std::to_string(nextSessionCounter_++) + "_"
            + std::to_string(nowMillis());
        sessions_[token] = userId;
        return token;
    }

    static std::string profileToJson(const UserProfile& profile) {
        std::ostringstream out;
        out << "{";
        out << "\"isStudent\":" << (profile.isStudent ? "true" : "false") << ",";
        out << "\"inHostel\":" << (profile.inHostel ? "true" : "false") << ",";
        out << "\"maritalStatus\":\"" << jsonEscape(profile.maritalStatus) << "\",";
        out << "\"hasKids\":" << (profile.hasKids ? "true" : "false") << ",";
        out << "\"hasLoan\":" << (profile.hasLoan ? "true" : "false") << ",";
        out << "\"loanAmount\":" << profile.loanAmount << ",";
        out << "\"budget\":" << profile.budget;
        out << "}";
        return out.str();
    }

    static std::string transactionToJson(const Transaction& tx) {
        std::ostringstream out;
        out << "{";
        out << "\"id\":" << tx.id << ",";
        out << "\"name\":\"" << jsonEscape(tx.name) << "\",";
        out << "\"category\":\"" << jsonEscape(tx.category) << "\",";
        out << "\"amount\":" << tx.amount << ",";
        out << "\"timestamp\":" << tx.timestamp;
        out << "}";
        return out.str();
    }

    // --- Persistence (simple JSON lines format for easy debugging) ---

    void persist() {
        std::ofstream out(dataFilePath_, std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Cannot write data file: " + dataFilePath_);
        }
        out << nextUserId_ << "\n";
        out << nextTransactionId_ << "\n";
        out << users_.size() << "\n";
        for (const auto& pair : users_) {
            writeUser(out, pair.second);
        }
    }

    void loadFromDisk() {
        std::ifstream in(dataFilePath_);
        if (!in) return;

        in >> nextUserId_ >> nextTransactionId_;
        size_t count = 0;
        in >> count;
        std::string line;
        std::getline(in, line); // consume endline after count

        for (size_t i = 0; i < count; ++i) {
            User user = readUser(in);
            users_[user.id] = user;
            emailIndex_[user.email] = user.id;
        }
    }

    static void writeUser(std::ofstream& out, const User& user) {
        out << "USER\n";
        out << user.id << "\n";
        out << user.email << "\n";
        out << user.passwordHash << "\n";
        out << user.displayName << "\n";
        // Pipe-delimited profile line avoids parsing issues with spaces.
        out << user.profile.isStudent << "|"
            << user.profile.inHostel << "|"
            << user.profile.maritalStatus << "|"
            << user.profile.hasKids << "|"
            << user.profile.hasLoan << "|"
            << user.profile.loanAmount << "|"
            << user.profile.budget << "\n";
        out << user.transactions.size() << "\n";
        for (const Transaction& tx : user.transactions) {
            out << tx.id << "|" << tx.name << "|" << tx.category << "|"
                << tx.amount << "|" << tx.timestamp << "\n";
        }
        out << "ENDUSER\n";
    }

    static User readUser(std::ifstream& in) {
        User user;
        std::string marker;
        std::getline(in, marker);
        if (marker != "USER") return user;

        in >> user.id;
        in.ignore(1);
        std::getline(in, user.email);
        std::getline(in, user.passwordHash);
        std::getline(in, user.displayName);

        std::string profileLine;
        std::getline(in, profileLine);
        {
            std::istringstream ps(profileLine);
            std::string part;
            int isStudent = 0, inHostel = 0, hasKids = 0, hasLoan = 0;
            std::getline(ps, part, '|'); isStudent = std::stoi(part);
            std::getline(ps, part, '|'); inHostel = std::stoi(part);
            std::getline(ps, user.profile.maritalStatus, '|');
            std::getline(ps, part, '|'); hasKids = std::stoi(part);
            std::getline(ps, part, '|'); hasLoan = std::stoi(part);
            std::getline(ps, part, '|'); user.profile.loanAmount = std::stoi(part);
            std::getline(ps, part, '|'); user.profile.budget = std::stoi(part);
            user.profile.isStudent = isStudent != 0;
            user.profile.inHostel = inHostel != 0;
            user.profile.hasKids = hasKids != 0;
            user.profile.hasLoan = hasLoan != 0;
        }

        size_t txCount = 0;
        in >> txCount;
        in.ignore(1);
        user.transactions.reserve(txCount);
        for (size_t i = 0; i < txCount; ++i) {
            std::string line;
            std::getline(in, line);
            Transaction tx;
            std::istringstream ls(line);
            std::string part;
            std::getline(ls, part, '|'); tx.id = std::stoull(part);
            std::getline(ls, tx.name, '|');
            std::getline(ls, tx.category, '|');
            std::getline(ls, part, '|'); tx.amount = std::stoi(part);
            std::getline(ls, part, '|'); tx.timestamp = std::stoll(part);
            user.transactions.push_back(tx);
        }

        std::getline(in, marker); // ENDUSER
        return user;
    }
};

} // namespace ft
