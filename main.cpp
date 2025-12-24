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

