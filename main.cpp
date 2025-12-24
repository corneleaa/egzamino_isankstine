#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <curl/curl.h>
// CURL / HTTP
static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string http_get(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Nepavyko inicializuoti CURL.");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EgzaminoUzdavinys/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP klaida (curl_easy_perform).");
    }
    return response;
}
//JSON istraukimas
static std::string extract_plaintext(const std::string& json) {
    const std::string key = "\"extract\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos)
        throw std::runtime_error("JSON extract nerastas.");

    pos += key.size();
    std::string out;
    bool escape = false;

    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (!escape) {
            if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                break;
            } else {
                out.push_back(c);
            }
        } else {
            if (c == 'n') out.push_back('\n');
            else out.push_back(c);
            escape = false;
        }
    }
    return out;
}
//teksto apdorojimas
static bool is_word_char(unsigned char c) {
    if (c < 128) return std::isalnum(c);
    return true; // UTF-8 baitai – laikome žodžio dalimi
}

static std::string to_lower_ascii(std::string s) {
    for (char& c : s) {
        if ((unsigned char)c < 128)
            c = static_cast<char>(std::tolower(c));
    }
    return s;
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> words;
    std::string cur;

    for (unsigned char c : line) {
        if (is_word_char(c)) {
            cur.push_back(static_cast<char>(c));
        } else if (!cur.empty()) {
            words.push_back(to_lower_ascii(cur));
            cur.clear();
        }
    }
    if (!cur.empty()) words.push_back(to_lower_ascii(cur));
    return words;
}
// URL paieska (be lookbehind)
static std::vector<std::string> extract_urls(const std::string& text) {
    std::vector<std::string> urls;

    const std::regex full_url(R"(\bhttps?://[^\s<>"'\)\]]+)");
    const std::regex short_url(
        R"(\b(?:www\.)?[A-Za-z0-9-]{2,}(?:\.[A-Za-z0-9-]{2,})*\.[A-Za-z]{2,}(?:/[^\s<>"'\)\]]*)?)"
    );

    auto collect = [&](const std::regex& rx, bool email_guard) {
        for (std::sregex_iterator it(text.begin(), text.end(), rx), end; it != end; ++it) {
            std::string u = it->str();
            size_t pos = it->position();

            if (email_guard && pos > 0 && text[pos - 1] == '@')
                continue;

            while (!u.empty() && ispunct(u.back()))
                u.pop_back();

            urls.push_back(u);
        }
    };

    collect(full_url, false);
    collect(short_url, true);

    std::sort(urls.begin(), urls.end());
    urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
    return urls;
}
// Wikipedia API URL
static std::string wiki_api_url() {
    return "https://lt.wikipedia.org/w/api.php"
           "?action=query&prop=extracts&explaintext=1"
           "&format=json&formatversion=2&titles=Feminizmas";
}

int main() {
    try {
        curl_global_init(CURL_GLOBAL_DEFAULT);

        std::string json = http_get(wiki_api_url());
        std::string text = extract_plaintext(json);

        // issaugomas parsisiustas testas
        std::ofstream("downloaded_text.txt") << text;

        std::unordered_map<std::string, int> count;
        std::unordered_map<std::string, std::set<int>> lines;

        std::istringstream iss(text);
        std::string line;
        int line_no = 0;

        while (std::getline(iss, line)) {
            ++line_no;
            for (const auto& w : tokenize(line)) {
                ++count[w];
                lines[w].insert(line_no);
            }
        }
        std::vector<std::pair<std::string, int>> repeated;
        for (const auto& kv : count)
            if (kv.second > 1)
                repeated.push_back(kv);

        std::sort(repeated.begin(), repeated.end(),
                  [](auto& a, auto& b) {
                      if (a.second != b.second) return a.second > b.second;
                      return a.first < b.first;
                  });

