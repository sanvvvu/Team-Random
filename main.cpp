
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;

struct Recipe {
    string title;
    string ingredients;
};

vector<Recipe> load_recipes(const string& filename) {
    vector<Recipe> recipes;
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        size_t pos = line.find('|');
        if (pos != string::npos) {
            recipes.push_back({line.substr(0, pos), line.substr(pos + 1)});
        }
    }
    return recipes;
}

void save_recipe(const string& filename, const Recipe& recipe) {
    ofstream file(filename, ios::app);
    file << recipe.title << "|" << recipe.ingredients << endl;
}

string generate_recipe_list_html(const vector<Recipe>& recipes) {
    string html = "<!DOCTYPE html><html><head><title>Список рецептов</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body>";
    html += "<h1>Список рецептов</h1><ul>";
    for (const auto& r : recipes) {
        html += "<li><strong>" + r.title + ":</strong> " + r.ingredients + "</li>";
    }
    html += "</ul><a href=\"/\">← Назад</a></body></html>";
    return html;
}

bool endsWith(const string& str, const string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

string get_mime_type(const string& path) {
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".html")) return "text/html";
    return "text/plain";
}

string url_decode(string s) {
    string result;
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == '+') result += ' ';
        else if (s[i] == '%' && i + 2 < s.length()) {
            string hex = s.substr(i + 1, 2);
            result += static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            i += 2;
        } else result += s[i];
    }
    return result;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, 5);

    cout << "Сервер запущен на http://localhost:8080\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        char buffer[8192] = {0};
        recv(client, buffer, sizeof(buffer) - 1, 0);
        string request(buffer);

        string method, path;
        istringstream iss(request);
        iss >> method >> path;

        if (method == "GET") {
            if (path == "/") path = "/index.html";
            if (path == "/recipes") {
                auto recipes = load_recipes("recipes.txt");
                sort(recipes.begin(), recipes.end(), [](auto& a, auto& b) {
                    return a.title < b.title;
                });
                string body = generate_recipe_list_html(recipes);
                string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + body;
                send(client, response.c_str(), response.size(), 0);
            } else {
                string file_path = path.substr(1);
                ifstream file(file_path, ios::binary);
                if (!file) {
                    string not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
                    send(client, not_found.c_str(), not_found.size(), 0);
                } else {
                    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
                    string response = "HTTP/1.1 200 OK\r\nContent-Type: " + get_mime_type(file_path) + "; charset=utf-8\r\n\r\n" + content;
                    send(client, response.c_str(), response.size(), 0);
                }
            }
        } else if (method == "POST" && path == "/submit") {
            size_t pos = request.find("\r\n\r\n");
            if (pos != string::npos) {
                string body = request.substr(pos + 4);
                string title, ingredients;
                size_t p1 = body.find("title="), p2 = body.find("&ingredients=");
                if (p1 != string::npos && p2 != string::npos) {
                    title = url_decode(body.substr(p1 + 6, p2 - (p1 + 6)));
                    ingredients = url_decode(body.substr(p2 + 13));
                    save_recipe("recipes.txt", {title, ingredients});
                }
            }
            string redirect = "HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n";
            send(client, redirect.c_str(), redirect.size(), 0);
        }

        closesocket(client);
    }

    WSACleanup();
    return 0;
}
