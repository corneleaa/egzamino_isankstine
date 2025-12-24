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
            else if (c == 't') out.push_back('\t');
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
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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

            while (!u.empty() && std::ispunct(static_cast<unsigned char>(u.back())) && u.back() != '/')
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

static std::vector<std::string> extract_urls_from_html(const std::string& html) {
    std::vector<std::string> urls;

    const std::regex href_any(R"(href\s*=\s*["']([^"']+)["'])", std::regex::icase);

    for (std::sregex_iterator it(html.begin(), html.end(), href_any), end; it != end; ++it) {
        std::string u = (*it)[1].str();

        if (u.rfind("//", 0) == 0) u = "https:" + u;

        if (u.rfind("http://", 0) != 0 && u.rfind("https://", 0) != 0) continue;

        size_t p = 0;
        while ((p = u.find("&amp;", p)) != std::string::npos) {
            u.replace(p, 5, "&");
            p += 1;
        }

        while (!u.empty() && std::ispunct(static_cast<unsigned char>(u.back())) && u.back() != '/')
            u.pop_back();

        urls.push_back(u);
    }

    std::sort(urls.begin(), urls.end());
    urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
    return urls;
}

// Wikipedia API URL
static std::string wiki_api_url(const std::string& title) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Nepavyko inicializuoti CURL (escape).");

    char* enc = curl_easy_escape(curl, title.c_str(), static_cast<int>(title.size()));
    if (!enc) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Nepavyko uzkoduoti pavadinimo.");
    }
    std::string et(enc);
    curl_free(enc);
    curl_easy_cleanup(curl);

    return "https://lt.wikipedia.org/w/api.php"
           "?action=query&prop=extracts&explaintext=1"
           "&format=json&formatversion=2&titles=" + et;
}

static std::size_t count_words_in_text_lines(const std::string& text) {
    std::istringstream iss(text);
    std::string line;
    std::size_t total = 0;
    while (std::getline(iss, line)) {
        total += tokenize(line).size();
    }
    return total;
}

int main() {
    try {
        curl_global_init(CURL_GLOBAL_DEFAULT);

        const std::vector<std::string> titles = {
            "Feminizmas",
        };

        std::string text;
        std::size_t total_words = 0;

        for (const auto& t : titles) {
            std::string json = http_get(wiki_api_url(t));
            std::string part = extract_plaintext(json);

            if (part.empty()) continue;

            text += "\n\n=== " + t + " ===\n";
            text += part;

            total_words = count_words_in_text_lines(text);
            if (total_words >= 1000) break;
        }

        if (total_words < 1000) {
            throw std::runtime_error("Nepavyko surinkti 1000 zodziu (bandyti kiti straipsniai arba tikrinti rysi).");
        }

        std::cout << "Bendras zodziu skaicius (be URL skyriaus): " << total_words << "\n";

        const std::string page_url = "https://lt.wikipedia.org/wiki/Feminizmas";
        std::string html = http_get(page_url);
        auto html_urls = extract_urls_from_html(html);

        std::vector<std::string> urls;
        urls.reserve(html_urls.size());
        for (const auto& u : html_urls) {
            if (u.find("wikipedia.org") != std::string::npos) continue;
            if (u.find("wikimedia.org") != std::string::npos) continue;
            urls.push_back(u);
        }
        std::sort(urls.begin(), urls.end());
        urls.erase(std::unique(urls.begin(), urls.end()), urls.end());

        text += "\n\n=== URL (is: " + page_url + ") ===\n";
        text += "URL skaicius: " + std::to_string(urls.size()) + "\n";
        for (const auto& u : urls) text += u + "\n";

        std::ofstream("downloaded_text.txt") << text;

        std::unordered_map<std::string, int> count;
        std::unordered_map<std::string, std::set<int>> lines;

        std::istringstream iss(text);
        std::string line;
        int line_no = 0;

        bool in_url_section = false;
        while (std::getline(iss, line)) {
            if (line.rfind("=== URL (is:", 0) == 0) {
                in_url_section = true;
            }
            if (in_url_section) continue;

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
                  [](const auto& a, const auto& b) {
                      if (a.second != b.second) return a.second > b.second;
                      return a.first < b.first;
                  });

        std::ofstream words("words_report.txt");
        words << "Bendras zodziu skaicius (be URL skyriaus): " << total_words << "\n";
        for (const auto& kv : repeated)
            words << kv.first << " : " << kv.second << "\n";

        std::ofstream cross("crossref_report.txt");
        cross << "Bendras zodziu skaicius (be URL skyriaus): " << total_words << "\n";
        for (const auto& kv : repeated) {
            cross << kv.first << " (" << kv.second << "): ";
            bool first = true;
            for (int ln : lines[kv.first]) {
                if (!first) cross << ", ";
                cross << ln;
                first = false;
            }
            cross << "\n";
        }

        std::ofstream urlf("urls_report.txt");
        urlf << "Saltinis: " << page_url << "\n";
        urlf << "URL skaicius: " << urls.size() << "\n\n";
        for (const auto& u : urls) {
            std::cout << u << "\n";
            urlf << u << "\n";
        }

        curl_global_cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Klaida: " << e.what() << "\n";
        return 1;
    }
}



