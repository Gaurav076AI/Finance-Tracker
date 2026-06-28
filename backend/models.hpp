#pragma once

// Core data models for the finance tracker backend.

#include <cstdint>
#include <string>
#include <vector>

namespace ft {

// Valid expense categories (fixed set → O(1) validation via string compare).
inline bool isValidCategory(const std::string& category) {
    return category == "food" || category == "transport" || category == "clothes"
        || category == "study" || category == "other";
}

// User profile collected during onboarding.
struct UserProfile {
    bool isStudent = true;
    bool inHostel = true;
    std::string maritalStatus; // "single" | "married" | ""
    bool hasKids = false;
    bool hasLoan = false;
    int loanAmount = 0;
    int budget = 0;
};

// Single expense transaction — append-only; aggregation done on read.
struct Transaction {
    std::uint64_t id = 0;
    std::string name;
    std::string category;
    int amount = 0;
    std::int64_t timestamp = 0; // Unix epoch milliseconds
};

// Registered user account.
struct User {
    std::uint64_t id = 0;
    std::string email;
    std::string passwordHash;
    std::string displayName;
    UserProfile profile;
    std::vector<Transaction> transactions;
};

// Aggregated item stats for "top purchased" dashboard section.
struct ItemAggregate {
    std::string name;
    std::string category;
    int count = 0;
    int totalAmount = 0;
    int lastUnitPrice = 0;
};

// Category spending summary.
struct CategorySummary {
    std::string category;
    int total = 0;
};

// Full dashboard payload returned by GET /api/dashboard.
struct DashboardData {
    int budget = 0;
    int spent = 0;
    int remaining = 0;
    int budgetUsedPct = 0;
    int daysLeftInMonth = 0;
    bool hasLoan = false;
    int loanAmount = 0;
    std::string topCategory;
    int topCategoryAmount = 0;
    std::string topItemName;
    int topItemCount = 0;
    int topItemLastPrice = 0;
    std::vector<CategorySummary> categories;
    std::vector<ItemAggregate> topItems;
    std::vector<Transaction> recentTransactions;
};

} // namespace ft
