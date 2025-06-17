#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <chrono>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

struct Recipe {
    string title;
    string ingredients;
    string allergens;
    string category;
    string image;
    string steps;
    string date;
};

string url_decode(const string& value) {
    string result;
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '+') result += ' ';
        else if (value[i] == '%' && i + 2 < value.length()) {
            string hex = value.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            result += ch;
            i += 2;
        } else result += value[i];
    }
    return result;
}

vector<Recipe> load_recipes(const string& filename) {
    vector<Recipe> recipes;
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        Recipe r;
        getline(ss, r.title, '|');
        getline(ss, r.ingredients, '|');
        getline(ss, r.allergens, '|');
        getline(ss, r.category, '|');
        getline(ss, r.image, '|');
        getline(ss, r.steps, '|');
        getline(ss, r.date, '|');
        recipes.push_back(r);
    }
    return recipes;
}

string generate_recipe_list_html(const vector<Recipe>& recipes) {
    stringstream html;
    html << "<div class='card-list'>";
    for (size_t i = 0; i < recipes.size(); i++) {
        const auto& r = recipes[i];
        string imgSrc = r.image.empty() ?
            "https://via.placeholder.com/300x200?text=No+Image" :
            r.image;

        html << "<div class='card'>"
            << "<div class='card-image'><img src='" << imgSrc << "' alt='Фото рецепта' style='width:100%; border-radius: 12px;'></div>"
            << "<div class='card-overlay'>"
            << "<h3><a href='/recipe?id=" << i << "' class='recipe-link'>" << r.title << "</a></h3>"
            << "<p>" << r.ingredients << "</p>"
            << "<p><em>Категория: " << r.category << "</em></p>"
            << "<small>Дата: " << r.date << "</small>"
            << "</div></div>";
    }
    html << "</div>";
    return html.str();
}

string generate_stats_json(const vector<Recipe>& recipes) {
    int total = recipes.size();
    int cakes = 0;
    int desserts = 0;

    for (const auto& r : recipes) {
        if (r.category == "Торт") cakes++;
        if (r.category == "Десерт") desserts++;
    }

    stringstream json;
    json << "{"
         << "\"total_recipes\":" << total << ","
         << "\"cakes_count\":" << cakes << ","
         << "\"desserts_count\":" << desserts
         << "}";
    return json.str();
}

string replace_newlines(const string& input) {
    string result = input;
    size_t pos = 0;
    while ((pos = result.find('\n', pos)) != string::npos) {
        result.replace(pos, 1, "<br>");
        pos += 4; // длина "<br>"
    }
    return result;
}

string generate_recipe_detail_html(const Recipe& recipe) {
    stringstream html;
    html << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>" << recipe.title << "</title>"
        << "<link rel='stylesheet' href='/style.css'>"
        << "<link href='https://fonts.googleapis.com/css2?family=Pacifico&family=Open+Sans:wght@400;700&display=swap' rel='stylesheet'>"
        << "</head><body><div class='container'>"
        << "<h1>" << recipe.title << "</h1>";

    if (!recipe.image.empty()) {
        html << "<div class='recipe-image'><img src='" << recipe.image << "' alt='" << recipe.title << "'></div>";
    }

    html << "<div class='recipe-details'>"
        << "<div class='detail-section'><h2>Ингредиенты</h2><p>" << replace_newlines(recipe.ingredients) << "</p></div>"
        << "<div class='detail-section'><h2>Инструкции</h2><p>"
        << replace_newlines(recipe.steps)
        << "</p></div>"
        << "<div class='meta-info'>"
        << "<p><strong>Категория:</strong> " << recipe.category << "</p>"
        << "<p><strong>Дата добавления:</strong> " << recipe.date << "</p>";

    if (!recipe.allergens.empty()) {
        html << "<p><strong>Аллергены:</strong> " << recipe.allergens << "</p>";
    }

    html << "</div></div>"
        << "<a class='button' href='/recipes' style='margin-top: 20px;'>← Вернуться к списку рецептов</a>"
        << "</div></body></html>";

    return html.str();
}

string generate_recipe_page_html(const vector<Recipe>& recipes) {
    stringstream html;
    html << "<!DOCTYPE html><html lang='ru'><head><meta charset='UTF-8'><title>Рецепты</title>"
         << "<link rel='stylesheet' href='/style.css'>"
         << "<link href='https://fonts.googleapis.com/css2?family=Pacifico&family=Open+Sans:wght@400;700&family=Caveat:wght@700&family=Comfortaa:wght@500&display=swap' rel='stylesheet'>"
         << "</head><body><div class='container'>"
         << "<h1>📖 Все рецепты</h1>"
         << "<form action='/recipes' method='GET' class='filter-form'>"
         << "<div class='filter-row'>"
         << "<input type='text' name='search' placeholder='Поиск по названию'>"
         << "<input type='text' name='ingredient' placeholder='Поиск по ингредиенту'>"
         << "<select name='category'><option value=''>Все категории</option>"
         << "<option value='Торт'>Торт</option><option value='Печенье'>Печенье</option>"
         << "<option value='Десерт'>Десерт</option><option value='Выпечка'>Выпечка</option></select>"
         << "</div><div class='filter-row'>"
         << "<select name='allergen_filter' class='allergen-select'>"
         << "<option value=''>Все аллергены</option>"
         << "<option value='no_allergens'>Без аллергенов</option>"
         << "<option value='глютен'>Без глютена</option>"
         << "<option value='орехи'>Без орехов</option>"
         << "<option value='молоко'>Без молока</option>"
         << "<option value='яйца'>Без яиц</option>"
         << "</select>"
         << "<select name='sort'><option value=''>Сортировка</option>"
         << "<option value='date_new'>Сначала новые</option><option value='date_old'>Сначала старые</option></select>"
         << "<button type='submit' class='filter-button'>🔍 Применить фильтры</button>"
         << "</div></form>"
         << generate_recipe_list_html(recipes)
         << "<p style='text-align:center; margin-top:20px;'><a class='button' href='/'>← Назад</a></p>"
         << "</div></body></html>";
    return html.str();
}

string get_form_field(const string& body, const string& name) {
    string key = name + "=";
    size_t start = body.find(key);
    if (start == string::npos) return "";
    start += key.length();
    size_t end = body.find("&", start);
    string value = body.substr(start, (end == string::npos ? body.size() : end) - start);
    return url_decode(value);
}

void handle_request(SOCKET clientSocket, const string& request) {
    istringstream requestStream(request);
    string method, path;
    requestStream >> method >> path;

    if (method == "GET") {
        string content, contentType = "text/html";

        if (path == "/" || path == "/index.html") {
            ifstream file("index.html");
            content.assign(istreambuf_iterator<char>(file), {});
        } else if (path == "/add.html") {
            ifstream file("add.html");
            content.assign(istreambuf_iterator<char>(file), {});
        } else if (path == "/style.css") {
            contentType = "text/css";
            ifstream file("style.css");
            content.assign(istreambuf_iterator<char>(file), {});
        } else if (path == "/stats") {
            contentType = "application/json";
            vector<Recipe> recipes = load_recipes("recipes.txt");
            content = generate_stats_json(recipes);
        } else if (path.find("/recipe?id=") == 0) {
            size_t id_pos = path.find("=") + 1;
            string id_str = path.substr(id_pos);
            int id = stoi(id_str);

            auto recipes = load_recipes("recipes.txt");
            if (id >= 0 && id < recipes.size()) {
                content = generate_recipe_detail_html(recipes[id]);
            }
            else {
                content = "404 Рецепт не найден";
            }///////////////
        } else if (path.find("/recipes") == 0) {
            vector<Recipe> recipes = load_recipes("recipes.txt");

            string search, ingredient, category, sort, allergen_filter;

            size_t queryPos = path.find("?");
            if (queryPos != string::npos) {
                string query = path.substr(queryPos + 1);
                istringstream queryStream(query);
                string param;
                while (getline(queryStream, param, '&')) {
                    size_t equalPos = param.find("=");
                    if (equalPos != string::npos) {
                        string key = param.substr(0, equalPos);
                        string value = param.substr(equalPos + 1);
                        value = url_decode(value);
                        
                        if (key == "search") {
                            search = value;
                        } else if (key == "ingredient") {
                            ingredient = value;
                        } else if (key == "category") {
                            category = value;
                        } else if (key == "allergen_filter") {
                            allergen_filter = value;
                        } else if (key == "sort") {
                            sort = value;
                        }
                    }
                }
            }

            // Фильтрация по названию (исправленный вариант)
            if (!search.empty()) {
                string searchLower = search;
                transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                
                recipes.erase(remove_if(recipes.begin(), recipes.end(), [&](const Recipe& r) {
                    string titleLower = r.title;
                    transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
                    return titleLower.find(searchLower) == string::npos;
                }), recipes.end());
            }

            if (!ingredient.empty()) {
                recipes.erase(remove_if(recipes.begin(), recipes.end(), [&](const Recipe& r) {
                    string ingredientsLower = r.ingredients;
                    transform(ingredientsLower.begin(), ingredientsLower.end(), ingredientsLower.begin(), ::tolower);
                    string ingredientLower = ingredient;
                    transform(ingredientLower.begin(), ingredientLower.end(), ingredientLower.begin(), ::tolower);
                    return ingredientsLower.find(ingredientLower) == string::npos;
                }), recipes.end());
            }

            if (!category.empty()) {
                recipes.erase(remove_if(recipes.begin(), recipes.end(), [&](const Recipe& r) {
                    return r.category != category;
                }), recipes.end());
            }

            if (!allergen_filter.empty()) {
                if (allergen_filter == "no_allergens") {
                    recipes.erase(remove_if(recipes.begin(), recipes.end(), [&](const Recipe& r) {
                        return !r.allergens.empty() && r.allergens != "нет";
                    }), recipes.end());
                } else {
                    recipes.erase(remove_if(recipes.begin(), recipes.end(), [&](const Recipe& r) {
                        string allergensLower = r.allergens;
                        transform(allergensLower.begin(), allergensLower.end(), allergensLower.begin(), ::tolower);
                        string filterLower = allergen_filter;
                        transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                        return allergensLower.find(filterLower) != string::npos;
                    }), recipes.end());
                }
            }

            // Сортировка
            if (sort == "date_new") {
               std::sort(recipes.begin(), recipes.end(), [](const Recipe& a, const Recipe& b) {
                    return a.date > b.date;
                });
            } else if (sort == "date_old") {
                std::sort(recipes.begin(), recipes.end(), [](const Recipe& a, const Recipe& b) {
                    return a.date < b.date;
                });
            }

            content = generate_recipe_page_html(recipes);
        } else {
            content = "404 Not Found";
        }

        ostringstream response;
        response << "HTTP/1.1 200 OK\r\nContent-Type: " << contentType << "; charset=UTF-8\r\n\r\n" << content;
        send(clientSocket, response.str().c_str(), response.str().length(), 0);
    }
    else if (method == "POST" && path == "/submit") {
        size_t pos = request.find("\r\n\r\n");
        if (pos != string::npos) {
            string body = request.substr(pos + 4);

            Recipe r;
            r.title = get_form_field(body, "title");
            r.ingredients = get_form_field(body, "ingredients");
            r.allergens = get_form_field(body, "allergens");
            r.category = get_form_field(body, "category");
            r.image = get_form_field(body, "image");
            r.date = get_form_field(body, "date");

            // Собираем все шаги в одну строку
            for (int i = 1; i <= 50; i++) {
                string step = get_form_field(body, "step" + to_string(i));
                if (!step.empty()) {
                    // Удаляем символы новой строки и заменяем разделители
                    step.erase(remove(step.begin(), step.end(), '\n'), step.end());
                    replace(step.begin(), step.end(), '|', ' '); // Заменяем | на пробел
                    replace(step.begin(), step.end(), ';', ',');  // Заменяем ; на запятую

                    if (!r.steps.empty()) {
                        r.steps += "; ";
                    }
                    r.steps += step;
                }
            }

            ofstream out("recipes.txt", ios::app);
            if (out.is_open()) {
                auto clean = [](string s) {
                replace(s.begin(), s.end(), '|', ' ');  // удалим символ-разделитель
                return s;
            };

            out << clean(r.title) << "|"
                << clean(r.ingredients) << "|"
                << clean(r.allergens) << "|"
                << clean(r.category) << "|"
                << clean(r.image) << "|"
                << clean(r.steps) << "|"
                << clean(r.date) << "\n";
                out.close();
            }

            string redirect = "HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n";
            send(clientSocket, redirect.c_str(), redirect.length(), 0);
        }
    } else {
        string response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        send(clientSocket, response.c_str(), response.length(), 0);
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, SOMAXCONN);

    cout << "Сервер работает на http://localhost:8080\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        string request;
        char buffer[4096];
        int bytes;

        do {
            bytes = recv(client, buffer, sizeof(buffer), 0);
            if (bytes > 0) {
                request.append(buffer, bytes);
                if (request.find("\r\n\r\n") != string::npos) {
                    size_t header_end = request.find("\r\n\r\n") + 4;
                    size_t content_length_pos = request.find("Content-Length:");
                    if (content_length_pos != string::npos) {
                        size_t len_start = content_length_pos + 15;
                        while (len_start < request.size() && isspace(request[len_start])) ++len_start;
                        size_t len_end = request.find("\r\n", len_start);
                        int content_length = stoi(request.substr(len_start, len_end - len_start));
                        while (request.size() - header_end < static_cast<size_t>(content_length)) {
                            bytes = recv(client, buffer, sizeof(buffer), 0);
                            if (bytes > 0) {
                                request.append(buffer, bytes);
                            } else break;
                        }
                    }
                    break;
                }
            }
        } while (bytes > 0);

        if (!request.empty()) {
            handle_request(client, request);
        }
        this_thread::sleep_for(chrono::milliseconds(1));
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
