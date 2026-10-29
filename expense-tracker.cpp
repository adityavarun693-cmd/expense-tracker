// ============================================================
//  Smart Expense Tracker (Backend) - C++
//  Bank/UPI SMS padhta hai -> amount nikalta hai -> category lagata hai
//  -> summary + saving tips deta hai
//
//  Compile : g++ main.cpp -o tracker -std=c++17
//  Run     : ./tracker            (Windows: tracker.exe)
//            ./tracker sms.txt    (apni SMS file ke saath)
//            ./tracker sms.txt json   (JSON output, frontend ke liye)
//
//  sms.txt mein HAR LINE par EK SMS likho.
// ============================================================

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>
using namespace std;

// ---------- Ek transaction ka data ----------
struct Txn {
    double amount = 0;
    bool isDebit = true;      // true = paisa gaya, false = paisa aaya
    string merchant = "unknown";
    string category = "Others";
};

// ---------- Categories aur unke keywords ----------
// Yahan naye keywords add kar sakte ho. ORDER important hai:
// upar wali category pehle match hoti hai.
vector<pair<string, vector<string>>> CATEGORIES = {
    {"Fees",     {"fee", "fees", "charges", "charge", "penalty", "gst",
                  "annual", "maintenance", "college", "tuition", "exam"}},
    {"Food",     {"swiggy", "zomato", "dominos", "mcdonalds", "kfc", "cafe",
                  "restaurant", "pizza", "burger", "bakery", "chai", "canteen",
                  "food", "bigbasket", "blinkit", "zepto", "dmart", "grocery"}},
    {"Travel",   {"uber", "ola", "rapido", "irctc", "metro", "redbus", "petrol",
                  "fuel", "fastag", "railway", "bus", "indigo", "makemytrip"}},
    {"Shopping", {"amazon", "flipkart", "myntra", "ajio", "meesho", "mall", "store"}},
    {"Bills",    {"recharge", "jio", "airtel", "vi", "electricity", "bill",
                  "netflix", "spotify", "hotstar", "broadband", "dth"}},
};

// ---------- Chhote helper functions ----------
string toLower(string s) {
    for (auto &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Poora word match karta hai ("coffee" mein "fee" match nahi hoga)
bool hasWord(const string &text, const string &word) {
    size_t pos = 0;
    while ((pos = text.find(word, pos)) != string::npos) {
        bool leftOk = (pos == 0) || !isalnum((unsigned char)text[pos - 1]);
        size_t end = pos + word.size();
        bool rightOk = (end >= text.size()) || !isalnum((unsigned char)text[end]);
        if (leftOk && rightOk) return true;
        pos += 1;
    }
    return false;
}

string categorize(const string &lowerSms) {
    for (const auto &cat : CATEGORIES)
        for (const auto &kw : cat.second)
            if (hasWord(lowerSms, kw)) return cat.first;
    return "Others";
}

// ---------- SMS ko samajhna (parse) ----------
bool parseSms(const string &sms, Txn &t) {
    string lower = toLower(sms);

    // 1) Amount: "Rs 250", "Rs.250.00", "INR 1,499.00"
    regex amtRe(R"(\b(?:rs\.?|inr)\s*([0-9,]+(?:\.[0-9]+)?))");
    smatch m;
    if (!regex_search(lower, m, amtRe)) return false;
    string num = m[1];
    num.erase(remove(num.begin(), num.end(), ','), num.end());
    try { t.amount = stod(num); } catch (...) { return false; }

    // 2) Debit ya credit?
    bool debit = hasWord(lower, "debited") || hasWord(lower, "spent") ||
                 hasWord(lower, "paid") || hasWord(lower, "sent") ||
                 hasWord(lower, "withdrawn") || hasWord(lower, "purchase");
    bool credit = hasWord(lower, "credited") || hasWord(lower, "received") ||
                  hasWord(lower, "deposited");
    if (!debit && !credit) return false;   // transaction SMS nahi hai (OTP etc.)
    t.isDebit = debit;                     // dono ho to debit maano

    // 3) Merchant: "to swiggy", "at amazon"
    regex merRe(R"(\b(?:to|at)\s+([a-z0-9@._&-]+))");
    if (regex_search(lower, m, merRe)) t.merchant = m[1];

    // 4) Category
    t.category = t.isDebit ? categorize(lower) : "Income";
    return true;
}

// ---------- Saving tips ----------
vector<string> makeTips(const map<string, double> &spend, double totalSpent,
                        double totalIncome) {
    vector<string> tips;
    if (totalSpent <= 0) {
        tips.push_back("Koi kharcha nahi mila. Pehle kuch SMS add karo.");
        return tips;
    }
    auto pct = [&](const string &c) {
        auto it = spend.find(c);
        return it == spend.end() ? 0.0 : it->second / totalSpent * 100.0;
    };
    if (pct("Food") > 35)
        tips.push_back("Food pe 35% se zyada ja raha hai. Hafte mein 2 baar bahar ka khana kam karo.");
    if (pct("Travel") > 25)
        tips.push_back("Travel kharcha zyada hai. Metro/bus ya friends ke saath ride share try karo.");
    if (pct("Shopping") > 25)
        tips.push_back("Shopping zyada hai. Kharidne se pehle 24 ghante ruko (24-hour rule).");
    if (pct("Fees") > 0)
        tips.push_back("Fees/charges dikh rahe hain. Bank charges aur late fees check karo, bachaye ja sakte hain.");
    if (pct("Bills") > 0)
        tips.push_back("Subscriptions/recharge dekho. Jo use nahi hota use band karo.");
    if (totalIncome > 0) {
        double saved = totalIncome - totalSpent;
        if (saved < 0.2 * totalIncome)
            tips.push_back("Bachat 20% se kam hai. Income aate hi 20% alag rakh do (pehle bachat, phir kharcha).");
        else
            tips.push_back("Badhiya! Tumne income ka 20% se zyada bachaya hai.");
    }
    tips.push_back("50-30-20 rule: 50% zaroorat, 30% shauk, 20% bachat.");
    return tips;
}

// ---------- Demo ke liye sample SMS ----------
vector<string> sampleSms() {
    return {
        "Rs.250.00 debited from A/c XX1234 on 25-10-26 to swiggy@ybl. UPI Ref 123456.",
        "Sent Rs.120.00 From Bank A/c XX1234 To ZOMATO on 26-10-26",
        "Rs 85 paid to uber@axis via UPI",
        "INR 1,499.00 spent on Amazon using card XX5678",
        "Rs.35 debited towards ATM charges + GST",
        "Rs 599 debited for Jio recharge",
        "Rs 60 paid to chai wala",
        "Rs 450 debited at Indian Oil petrol pump",
        "Rs.2200 paid to college exam fee",
        "Rs.5000.00 credited to A/c XX1234 from Papa",
        "Your OTP is 482913. Do not share.",
    };
}

// ---------- main ----------
int main(int argc, char *argv[]) {
    string fileName = (argc > 1) ? argv[1] : "sms.txt";
    bool jsonMode = (argc > 2 && string(argv[2]) == "json");

    // SMS lines padho
    vector<string> lines;
    ifstream in(fileName);
    if (in.is_open()) {
        string line;
        while (getline(in, line))
            if (!line.empty()) lines.push_back(line);
    } else {
        lines = sampleSms();
        if (!jsonMode)
            cout << "[Note] '" << fileName << "' file nahi mili, sample SMS use ho rahe hain.\n\n";
    }

    // Har SMS parse karo
    vector<Txn> txns;
    int skipped = 0;
    for (const auto &sms : lines) {
        Txn t;
        if (parseSms(sms, t)) txns.push_back(t);
        else skipped++;
    }

    // Totals nikalo
    map<string, double> spend;
    double totalSpent = 0, totalIncome = 0;
    const Txn *biggest = nullptr;
    for (const auto &t : txns) {
        if (t.isDebit) {
            spend[t.category] += t.amount;
            totalSpent += t.amount;
            if (!biggest || t.amount > biggest->amount) biggest = &t;
        } else {
            totalIncome += t.amount;
        }
    }

    // Category ko bade kharche se chhote ke order mein lagao
    vector<pair<string, double>> sorted(spend.begin(), spend.end());
    sort(sorted.begin(), sorted.end(),
         [](const auto &a, const auto &b) { return a.second > b.second; });

    vector<string> tips = makeTips(spend, totalSpent, totalIncome);

    // ----- JSON output (frontend ke liye) -----
    if (jsonMode) {
        cout << fixed << setprecision(2);
        cout << "{\n  \"total_spent\": " << totalSpent
             << ",\n  \"total_income\": " << totalIncome
             << ",\n  \"categories\": {";
        for (size_t i = 0; i < sorted.size(); i++) {
            cout << (i ? ", " : "") << "\"" << sorted[i].first << "\": " << sorted[i].second;
        }
        cout << "},\n  \"tips\": [";
        for (size_t i = 0; i < tips.size(); i++)
            cout << (i ? ", " : "") << "\"" << tips[i] << "\"";
        cout << "]\n}\n";
        return 0;
    }

    // ----- Normal (padhne wala) output -----
    cout << fixed << setprecision(2);
    cout << "=== TRANSACTIONS ===\n";
    for (const auto &t : txns) {
        cout << (t.isDebit ? "- " : "+ ") << "Rs " << setw(9) << t.amount
             << " | " << setw(8) << left << t.category << right
             << " | " << t.merchant << "\n";
    }
    cout << "(" << skipped << " SMS skip hue: transaction nahi the)\n";

    cout << "\n=== SUMMARY ===\n";
    cout << "Total kharcha : Rs " << totalSpent << "\n";
    cout << "Total income  : Rs " << totalIncome << "\n";
    if (biggest)
        cout << "Sabse bada    : Rs " << biggest->amount << " (" << biggest->category << ")\n";

    cout << "\n=== CATEGORY-WISE ===\n";
    for (const auto &p : sorted) {
        double pc = totalSpent > 0 ? p.second / totalSpent * 100.0 : 0;
        cout << setw(9) << left << p.first << right << " Rs " << setw(9) << p.second
             << " (" << setw(5) << setprecision(1) << pc << "%) "
             << string((size_t)(pc / 5), '#') << "\n" << setprecision(2);
    }

    cout << "\n=== SAVING TIPS ===\n";
    int n = 1;
    for (const auto &tip : tips) cout << n++ << ". " << tip << "\n";

    return 0;
}