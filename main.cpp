#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
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
    return true; 
}

static std::string to_lower_ascii(std::string s) {
    for (char& c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
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
static std::size_t count_words_in_text(const std::string& text) {
    std::istringstream iss(text);
    std::string line;
    std::size_t total = 0;
    while (std::getline(iss, line)) {
        total += tokenize(line).size();
    }
    return total;
}
//url encode
static std::string url_encode(CURL* curl, const std::string& s) {
    char* enc = curl_easy_escape(curl, s.c_str(), static_cast<int>(s.size()));
    if (!enc) throw std::runtime_error("Nepavyko URL-encode.");
    std::string out(enc);
    curl_free(enc);
    return out;
}
// MediaWiki API URL tekstui
static std::string wiki_extract_url(CURL* curl, const std::string& title) {
    return "https://lt.wikipedia.org/w/api.php"
           "?action=query&prop=extracts&explaintext=1"
           "&format=json&formatversion=2&titles=" + url_encode(curl, title);
}
// URL paieška iš HTML 
static std::string html_unescape_amp(std::string s) {
    size_t pos = 0;
    while ((pos = s.find("&amp;", pos)) != std::string::npos) {
        s.replace(pos, 5, "&");
        pos += 1;
    }
    return s;
}

static bool is_punct_to_trim(unsigned char c) {
    return std::ispunct(c) && c != '/';
}

static std::vector<std::string> extract_urls_from_html(const std::string& html, bool external_only = true) {
    std::vector<std::string> urls;

    // href="..." arba href='...'
    const std::regex href_any(R"(href\s*=\s*["']([^"']+)["'])", std::regex::icase);

    for (std::sregex_iterator it(html.begin(), html.end(), href_any), end; it != end; ++it) {
        std::string u = (*it)[1].str();

        // Protokolo-nepriklausomos nuorodos: //example.com -> https://example.com
        if (u.rfind("//", 0) == 0) u = "https:" + u;

        // Paliekame tik pilnas http/https nuorodas
        if (u.rfind("http://", 0) != 0 && u.rfind("https://", 0) != 0) continue;

        u = html_unescape_amp(u);

        while (!u.empty() && is_punct_to_trim(static_cast<unsigned char>(u.back()))) {
            u.pop_back();
        }

        if (external_only) {
            if (u.find("wikipedia.org") != std::string::npos) continue;
            if (u.find("wikimedia.org") != std::string::npos) continue;
        }

        urls.push_back(u);
    }

    std::sort(urls.begin(), urls.end());
    urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
    return urls;
}
int main() {
    try {
        curl_global_init(CURL_GLOBAL_DEFAULT);
 if (!curl) throw std::runtime_error("Nepavyko inicializuoti CURL (handle).");
        const std::vector<std::string> titles = {
            "Feminizmas",
        };

        std::string text;
        std::size_t total_words = 0;

        for (const auto& t : titles) {
            std::string json = http_get(wiki_extract_url(curl, t));
            std::string part = extract_plaintext(json);
            if (part.empty()) continue;
            text += "\n\n=== " + t + " ===\n";
            text += part;
            total_words = count_words_in_text(text);
            if (total_words >= 1000) break;
        }
        if (total_words < 1000) {
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            throw std::runtime_error("Nepavyko surinkti 1000 žodžių (bandyti kiti straipsniai arba tikrinti ryšį).");
        }

        // URL paieska 
        const std::string page_url = "https://lt.wikipedia.org/wiki/Feminizmas";
        std::string html = http_get(page_url);

        // svarus url sarasas
        auto urls = extract_urls_from_html(html, /*external_only=*/true);

        // URL sarasas tekste
        text += "\n\n=== URL (is: " + page_url + ") ===\n";
        text += "URL skaicius: " + std::to_string(urls.size()) + "\n";
        for (const auto& u : urls) {
            text += u + "\n";
        }
        //Issaugome galutini teksta
        std::ofstream("downloaded_text.txt") << text;

        // zodziu statistika + crossref 
        std::string text_for_analysis = text;
        {
            const std::string marker = "\n\n=== URL (is:";
            size_t cut = text_for_analysis.find(marker);
            if (cut != std::string::npos) text_for_analysis = text_for_analysis.substr(0, cut);
        }

        std::unordered_map<std::string, int> count;
        std::unordered_map<std::string, std::set<int>> lines;

        std::istringstream iss(text_for_analysis);
        std::string line;
        int line_no = 0;
        std::size_t recomputed_words = 0;

        while (std::getline(iss, line)) {
            ++line_no;
            auto toks = tokenize(line);
            recomputed_words += toks.size();

            for (const auto& w : toks) {
                ++count[w];
                lines[w].insert(line_no);
            }
        }
        // zodziu skaicius ekrane 
        std::cout << "Bendras zodziu skaicius tekste (be URL skyriaus): " << recomputed_words << "\n";

        std::vector<std::pair<std::string, int>> repeated;
        repeated.reserve(count.size());
        for (const auto& kv : count) {
            if (kv.second > 1) repeated.push_back(kv);
        }
        std::sort(repeated.begin(), repeated.end(),
                  [](const auto& a, const auto& b) {
                      if (a.second != b.second) return a.second > b.second;
                      return a.first < b.first;
                  });
        // words_report.txt
        std::ofstream words("words_report.txt");
        words << "Bendras zodziu skaicius (be URL skyriaus): " << recomputed_words << "\n";
        words << "Zodziai, pasikartojantys > 1:\n\n";
        for (const auto& kv : repeated) {
            words << kv.first << " : " << kv.second << "\n";
        }
        // crossref_report.txt
        std::ofstream cross("crossref_report.txt");
        cross << "Bendras zodziu skaicius (be URL skyriaus): " << recomputed_words << "\n";
        cross << "Cross-reference (zodis (kiekis): eilutes):\n\n";
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
        // urls_report.txt
        std::ofstream urlf("urls_report.txt");
        urlf << "Saltinis: " << page_url << "\n";
        urlf << "URL skaicius (is puslapio HTML, tik isoriniai http/https): " << urls.size() << "\n\n";
        for (const auto& u : urls) {
            urlf << u << "\n";
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Klaida: " << e.what() << "\n";
        return 1;
    }
}

