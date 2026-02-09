#include <SFML/Graphics.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <cstdint>

namespace fs = std::filesystem;

// ---------- helpers ----------
static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static bool isImageExt(const fs::path& p) {
    if (!p.has_extension()) return false;
    std::string ext = toLower(p.extension().string());
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

static std::vector<std::string> loadImagePaths(const std::string& folder) {
    std::vector<std::string> paths;
    if (!fs::exists(folder)) return paths;
    for (const auto& e : fs::directory_iterator(folder)) {
        if (e.is_regular_file() && isImageExt(e.path()))
            paths.push_back(e.path().string());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

static std::string baseName(const std::string& fullPath) {
    return fs::path(fullPath).filename().string();
}

// ---------- settings ----------
struct Settings {
    bool darkTheme = true;
    int  fontSizeTitle = 44;
    int  fontSizeMenu  = 22;
    bool showFavoritesOnly = false;
};

static Settings loadSettings(const std::string& path) {
    Settings s;
    std::ifstream in(path);
    if (!in.is_open()) return s;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = trim(line.substr(0, pos));
        std::string val = trim(line.substr(pos + 1));

        if (key == "darkTheme") s.darkTheme = (val == "1");
        if (key == "fontSizeTitle") s.fontSizeTitle = std::stoi(val);
        if (key == "fontSizeMenu")  s.fontSizeMenu  = std::stoi(val);
        if (key == "showFavoritesOnly") s.showFavoritesOnly = (val == "1");
    }
    return s;
}

static void saveSettings(const std::string& path, const Settings& s) {
    std::ofstream out(path, std::ios::trunc);
    out << "darkTheme=" << (s.darkTheme ? "1" : "0") << "\n";
    out << "fontSizeTitle=" << s.fontSizeTitle << "\n";
    out << "fontSizeMenu=" << s.fontSizeMenu << "\n";
    out << "showFavoritesOnly=" << (s.showFavoritesOnly ? "1" : "0") << "\n";
}

// ---------- favorites ----------
static std::vector<std::string> loadLines(const std::string& path) {
    std::vector<std::string> v;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (!line.empty()) v.push_back(line);
    }
    return v;
}

static void saveLines(const std::string& path, const std::vector<std::string>& v) {
    std::ofstream out(path, std::ios::trunc);
    for (auto& s : v) out << s << "\n";
}

static bool inList(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

static void toggleInList(std::vector<std::string>& v, const std::string& x) {
    auto it = std::find(v.begin(), v.end(), x);
    if (it == v.end()) v.push_back(x);
    else v.erase(it);
}

// ---------- viewer layout ----------
static void fitSprite(sf::Sprite& s, const sf::Texture& t,
                      sf::Vector2u win, float bottomBarH) {
    auto sz = t.getSize();
    if (sz.x == 0 || sz.y == 0) return;

    float padding = 30.f;
    float maxW = (float)win.x - padding * 2.f;
    float maxH = (float)win.y - bottomBarH - padding * 2.f;

    float scale = std::min(maxW / (float)sz.x, maxH / (float)sz.y);
    s.setScale({scale, scale});
    s.setPosition({
        ((float)win.x - (float)sz.x * scale) / 2.f,
        (((float)win.y - bottomBarH) - (float)sz.y * scale) / 2.f
    });
}

// ---------- copy ----------
static bool copyToFolderUnique(const std::string& srcPath, const std::string& dstFolder, std::string& outFinalName) {
    fs::path src(srcPath);
    if (!fs::exists(src) || !fs::is_regular_file(src)) return false;

    fs::create_directories(dstFolder);

    fs::path dst = fs::path(dstFolder) / src.filename();
    int n = 1;
    while (fs::exists(dst)) {
        dst = fs::path(dstFolder) / (src.stem().string() + "_" + std::to_string(n++) + src.extension().string());
    }

    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) return false;

    outFinalName = dst.filename().string();
    return true;
}

static void openInDefaultApp(const std::string& path) {
    std::string cmd = "open \"" + path + "\"";
    system(cmd.c_str());
}

// ---------- UI Button ----------
struct UIButton {
    sf::RectangleShape rect;
    sf::Text text;
    bool hovered = false;

    UIButton(const sf::Font& font, const std::string& label, unsigned int size)
        : text(font, label, size) {}

    bool contains(sf::Vector2f p) const { return rect.getGlobalBounds().contains(p); }

    void setLabel(const std::string& s) { text.setString(s); layoutText(); }

    void layoutText() {
        auto pos = rect.getPosition();
        auto size = rect.getSize();
        auto b = text.getLocalBounds();
        text.setPosition({
            pos.x + (size.x - b.size.x) / 2.f,
            pos.y + (size.y - b.size.y) / 2.f - 2.f
        });
    }

    void setHovered(bool h, bool darkTheme) {
        hovered = h;
        rect.setOutlineThickness(1.f);
        if (darkTheme) {
            rect.setFillColor(hovered ? sf::Color(255,255,255,28) : sf::Color(255,255,255,12));
            rect.setOutlineColor(hovered ? sf::Color(255,255,255,70) : sf::Color(255,255,255,30));
            text.setFillColor(sf::Color(245,245,245));
        } else {
            rect.setFillColor(hovered ? sf::Color(0,0,0,16) : sf::Color(0,0,0,8));
            rect.setOutlineColor(hovered ? sf::Color(0,0,0,55) : sf::Color(0,0,0,25));
            text.setFillColor(sf::Color(30,30,35));
        }
    }

    void draw(sf::RenderWindow& w) const { w.draw(rect); w.draw(text); }
};

// ---------- app ----------
enum class Screen { Menu, Photos };

int main() {
    const std::string IMAGES = "assets/images";
    const std::string VIDEOS = "assets/videos";
    const std::string FONT   = "assets/fonts/3966.ttf";

    const std::string SETTINGS_FILE  = "assets/settings.txt";
    const std::string FAVORITES_FILE = "assets/favorites.txt";

    // Desktop Photos source folder
    const std::string SOURCE_PHOTOS = std::string(getenv("HOME")) + "/Desktop/Photos";

    fs::create_directories(IMAGES);
    fs::create_directories(VIDEOS);
    fs::create_directories(SOURCE_PHOTOS);

    Settings settings = loadSettings(SETTINGS_FILE);
    auto favorites = loadLines(FAVORITES_FILE);

    sf::RenderWindow window(sf::VideoMode({1000, 650}), "Media Database");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile(FONT)) {
        std::cout << "Font not found: " << FONT << "\n";
        return 0;
    }

    // menu
    std::vector<std::string> menu = {
        "Photos (view gallery)",
        "Videos (open from Desktop/Photos)",
        "Add Photo (from Desktop/Photos by name)",
        "Exit"
    };
    std::vector<std::string> desc = {
        "View images from assets/images",
        "Open a video if it exists in Desktop/Photos (type name)",
        "Type only filename, e.g. cat.jpg",
        "Close the application"
    };
    int menuIndex = 0;
    std::vector<sf::FloatRect> menuHit(menu.size());

    // viewer state
    std::vector<std::string> photos;
    int photoIdx = 0;

    sf::Texture tex;
    sf::Image dummyImg({1,1}, sf::Color::White);
    (void)tex.loadFromImage(dummyImg);
    sf::Sprite spr(tex);

    // bottom bar
    float barH = 86.f;
    sf::RectangleShape bar({(float)window.getSize().x, barH});
    sf::Text caption(font, "", 20);
    sf::Text counter(font, "", 16);

    // fade
    float fade = 255.f;
    bool  fadingOut = false;
    bool  fadingIn  = false;
    int   pendingIdx = -1;

    // slideshow + info
    bool slideshow = false;
    float slideTimer = 0.f;
    const float SLIDE_DELAY = 2.5f;
    bool showInfo = false;

    sf::Clock dtClock;

    // viewer buttons
    UIButton btnPrev(font, "Prev", 15);
    UIButton btnNext(font, "Next", 15);
    UIButton btnPlay(font, "Play", 15);
    UIButton btnInfo(font, "Info", 15);
    UIButton btnStar(font, "Star", 15);
    UIButton btnFav(font,  "Fav: OFF", 15);
    UIButton btnDel(font,  "Delete", 15);
    UIButton btnBack(font, "Back", 15);

    auto refreshBarColors = [&]() {
        if (settings.darkTheme) {
            bar.setFillColor(sf::Color(0,0,0,190));
            caption.setFillColor(sf::Color(245,245,245));
            counter.setFillColor(sf::Color(200,200,200));
        } else {
            bar.setFillColor(sf::Color(0,0,0,40));
            caption.setFillColor(sf::Color(30,30,35));
            counter.setFillColor(sf::Color(70,70,80));
        }
    };

    auto applyFilters = [&]() {
        auto all = loadImagePaths(IMAGES);

        if (settings.showFavoritesOnly) {
            std::vector<std::string> onlyFav;
            for (auto& p : all) {
                if (inList(favorites, baseName(p))) onlyFav.push_back(p);
            }
            all = std::move(onlyFav);
        }
        return all;
    };

    auto layoutViewer = [&]() {
        auto ws = window.getSize();

        bar.setSize({(float)ws.x, barH});
        bar.setPosition({0.f, (float)ws.y - barH});

        fitSprite(spr, tex, ws, barH);

        caption.setPosition({20.f, (float)ws.y - barH + 10.f});
        counter.setPosition({20.f, (float)ws.y - barH + 40.f});

        float y = (float)ws.y - barH + 18.f;
        float x = (float)ws.x - 20.f;

        auto placeRight = [&](UIButton& b, float w) {
            b.rect.setSize({w, 34.f});
            b.rect.setPosition({x - w, y});
            x -= (w + 10.f);
            b.layoutText();
        };

        placeRight(btnBack, 90.f);
        placeRight(btnDel,  95.f);
        placeRight(btnFav,  110.f);
        placeRight(btnStar, 80.f);
        placeRight(btnInfo, 80.f);
        placeRight(btnPlay, 80.f);
        placeRight(btnNext, 80.f);
        placeRight(btnPrev, 80.f);

        btnPrev.setHovered(false, settings.darkTheme);
        btnNext.setHovered(false, settings.darkTheme);
        btnPlay.setHovered(false, settings.darkTheme);
        btnInfo.setHovered(false, settings.darkTheme);
        btnStar.setHovered(false, settings.darkTheme);
        btnFav .setHovered(false, settings.darkTheme);
        btnDel .setHovered(false, settings.darkTheme);
        btnBack.setHovered(false, settings.darkTheme);
    };

    auto loadCurrentPhoto = [&]() {
        if (photos.empty()) return;
        if (!tex.loadFromFile(photos[photoIdx])) {
            std::cout << "Failed to load: " << photos[photoIdx] << "\n";
            return;
        }
        spr = sf::Sprite(tex);
        layoutViewer();

        std::string file = baseName(photos[photoIdx]);
        bool fav = inList(favorites, file);

        caption.setString((fav ? "★ " : "") + file);
        counter.setString(std::to_string(photoIdx + 1) + " / " + std::to_string(photos.size()));

        btnStar.setLabel(fav ? "Unstar" : "Star");
        btnFav.setLabel(settings.showFavoritesOnly ? "Fav: ON" : "Fav: OFF");
        btnPlay.setLabel(slideshow ? "Pause" : "Play");
        btnInfo.setLabel(showInfo ? "Info: ON" : "Info");
    };

    auto enterPhotos = [&]() {
        photos = applyFilters();

        // safety: if filter hides everything, disable it
        if (photos.empty() && settings.showFavoritesOnly) {
            settings.showFavoritesOnly = false;
            saveSettings(SETTINGS_FILE, settings);
            photos = applyFilters();
        }

        if (photos.empty()) {
            std::cout << "No photos found in assets/images.\n";
            std::cout << "Images dir: " << fs::absolute(IMAGES) << "\n";
            return false;
        }

        photoIdx = 0;
        fade = 255.f;
        fadingOut = false;
        fadingIn = false;
        pendingIdx = -1;

        slideshow = false;
        slideTimer = 0.f;
        showInfo = false;

        refreshBarColors();
        loadCurrentPhoto();
        return true;
    };

    auto requestPhoto = [&](int newIndex) {
        if (photos.empty()) return;
        int n = (newIndex % (int)photos.size() + (int)photos.size()) % (int)photos.size();
        if (n == photoIdx) return;
        pendingIdx = n;
        fadingOut = true;
        fadingIn = false;
    };

    // NEW addPhoto: type filename from Desktop/Photos
    auto addPhotoFromDesktopFolder = [&]() {
        std::cout << "\nSource folder: " << SOURCE_PHOTOS << "\n";
        std::cout << "Enter image filename (example: cat.jpg)\n> ";

        std::string name;
        std::getline(std::cin, name);
        name = trim(name);

        if (name.empty()) {
            std::cout << "Canceled\n";
            return;
        }

        fs::path src = fs::path(SOURCE_PHOTOS) / name;

        if (!fs::exists(src)) {
            std::cout << "File not found: " << src << "\n";
            return;
        }
        if (!isImageExt(src)) {
            std::cout << "Not an image file (allowed: jpg/jpeg/png/bmp)\n";
            return;
        }

        std::string finalName;
        if (copyToFolderUnique(src.string(), IMAGES, finalName)) {
            std::cout << "Added image: " << finalName << "\n";
        } else {
            std::cout << "Failed to copy image\n";
        }
    };

    // Videos: type filename from Desktop/Photos and open
    auto openVideoFromDesktopFolder = [&]() {
        std::cout << "\nSource folder: " << SOURCE_PHOTOS << "\n";
        std::cout << "Enter video filename (example: clip.mp4)\n> ";

        std::string name;
        std::getline(std::cin, name);
        name = trim(name);

        if (name.empty()) {
            std::cout << "Canceled\n";
            return;
        }

        fs::path src = fs::path(SOURCE_PHOTOS) / name;
        if (!fs::exists(src)) {
            std::cout << "File not found: " << src << "\n";
            return;
        }
        openInDefaultApp(src.string());
    };

    auto deleteCurrent = [&]() {
        if (photos.empty()) return;

        std::string file = baseName(photos[photoIdx]);
        std::cout << "\nDelete this photo? (y/n): " << file << "\n";
        std::string ans; std::getline(std::cin, ans);
        ans = toLower(trim(ans));
        if (!(ans == "y" || ans == "yes")) return;

        std::error_code ec;
        fs::remove(fs::path(IMAGES) / file, ec);
        if (ec) std::cout << "Delete error: " << ec.message() << "\n";

        if (inList(favorites, file)) {
            toggleInList(favorites, file);
            saveLines(FAVORITES_FILE, favorites);
        }

        photos = applyFilters();
        if (photos.empty()) return;

        if (photoIdx >= (int)photos.size()) photoIdx = (int)photos.size() - 1;
        loadCurrentPhoto();
    };

    auto runMenuAction = [&](int index, Screen& screen) {
        if (index == 0) {
            if (enterPhotos()) screen = Screen::Photos;
        } else if (index == 1) {
            openVideoFromDesktopFolder();
        } else if (index == 2) {
            addPhotoFromDesktopFolder();
        } else if (index == 3) {
            window.close();
        }
    };

    Screen screen = Screen::Menu;

    // ---------- loop ----------
    while (window.isOpen()) {
        float dt = dtClock.restart().asSeconds();
        sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);

        // slideshow tick
        if (screen == Screen::Photos && slideshow && !photos.empty()) {
            slideTimer += dt;
            if (slideTimer >= SLIDE_DELAY) {
                slideTimer = 0.f;
                requestPhoto(photoIdx + 1);
            }
        }

        // fade tick
        if (screen == Screen::Photos && (fadingOut || fadingIn)) {
            if (fadingOut) {
                fade -= 600.f * dt;
                if (fade <= 0.f) {
                    fade = 0.f;
                    fadingOut = false;

                    if (pendingIdx != -1) {
                        photoIdx = pendingIdx;
                        pendingIdx = -1;
                        loadCurrentPhoto();
                    }
                    fadingIn = true;
                }
            } else if (fadingIn) {
                fade += 600.f * dt;
                if (fade >= 255.f) {
                    fade = 255.f;
                    fadingIn = false;
                }
            }
        }

        // hover buttons
        if (screen == Screen::Photos) {
            btnPrev.setHovered(btnPrev.contains(mouse), settings.darkTheme);
            btnNext.setHovered(btnNext.contains(mouse), settings.darkTheme);
            btnPlay.setHovered(btnPlay.contains(mouse), settings.darkTheme);
            btnInfo.setHovered(btnInfo.contains(mouse), settings.darkTheme);
            btnStar.setHovered(btnStar.contains(mouse), settings.darkTheme);
            btnFav .setHovered(btnFav .contains(mouse), settings.darkTheme);
            btnDel .setHovered(btnDel .contains(mouse), settings.darkTheme);
            btnBack.setHovered(btnBack.contains(mouse), settings.darkTheme);
        }

        while (const auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();

            if (const auto* r = ev->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0.f, 0.f),
                    sf::Vector2f((float)r->size.x, (float)r->size.y)
                )));
                if (screen == Screen::Photos) layoutViewer();
            }

            if (const auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                if (k->code == sf::Keyboard::Key::Escape) {
                    if (screen == Screen::Photos) screen = Screen::Menu;
                    else window.close();
                }

                if (k->code == sf::Keyboard::Key::T) {
                    settings.darkTheme = !settings.darkTheme;
                    saveSettings(SETTINGS_FILE, settings);
                    refreshBarColors();
                    if (screen == Screen::Photos) layoutViewer();
                }

                if (screen == Screen::Menu) {
                    if (k->code == sf::Keyboard::Key::Up) menuIndex = (menuIndex - 1 + 4) % 4;
                    if (k->code == sf::Keyboard::Key::Down) menuIndex = (menuIndex + 1) % 4;
                    if (k->code == sf::Keyboard::Key::Enter) runMenuAction(menuIndex, screen);
                } else {
                    if (k->code == sf::Keyboard::Key::Left)  requestPhoto(photoIdx - 1);
                    if (k->code == sf::Keyboard::Key::Right) requestPhoto(photoIdx + 1);

                    if (k->code == sf::Keyboard::Key::P) {
                        slideshow = !slideshow;
                        slideTimer = 0.f;
                        btnPlay.setLabel(slideshow ? "Pause" : "Play");
                    }

                    if (k->code == sf::Keyboard::Key::I) {
                        showInfo = !showInfo;
                        btnInfo.setLabel(showInfo ? "Info: ON" : "Info");
                    }

                    if (k->code == sf::Keyboard::Key::F) {
                        settings.showFavoritesOnly = !settings.showFavoritesOnly;
                        saveSettings(SETTINGS_FILE, settings);
                        photos = applyFilters();
                        photoIdx = 0;
                        if (!photos.empty()) loadCurrentPhoto();
                        else screen = Screen::Menu;
                    }

                    if (k->code == sf::Keyboard::Key::S) {
                        if (!photos.empty()) {
                            std::string file = baseName(photos[photoIdx]);
                            toggleInList(favorites, file);
                            saveLines(FAVORITES_FILE, favorites);
                            loadCurrentPhoto();
                        }
                    }

                    if (k->code == sf::Keyboard::Key::D) {
                        deleteCurrent();
                        if (photos.empty()) screen = Screen::Menu;
                    }
                }
            }

            // mouse click
            if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    if (screen == Screen::Menu) {
                        for (int i = 0; i < 4; i++) {
                            if (menuHit[i].contains(mouse)) {
                                menuIndex = i;
                                runMenuAction(i, screen);
                                break;
                            }
                        }
                    } else {
                        if (btnPrev.contains(mouse)) requestPhoto(photoIdx - 1);
                        else if (btnNext.contains(mouse)) requestPhoto(photoIdx + 1);
                        else if (btnPlay.contains(mouse)) {
                            slideshow = !slideshow;
                            slideTimer = 0.f;
                            btnPlay.setLabel(slideshow ? "Pause" : "Play");
                        }
                        else if (btnInfo.contains(mouse)) {
                            showInfo = !showInfo;
                            btnInfo.setLabel(showInfo ? "Info: ON" : "Info");
                        }
                        else if (btnStar.contains(mouse)) {
                            if (!photos.empty()) {
                                std::string file = baseName(photos[photoIdx]);
                                toggleInList(favorites, file);
                                saveLines(FAVORITES_FILE, favorites);
                                loadCurrentPhoto();
                            }
                        }
                        else if (btnFav.contains(mouse)) {
                            settings.showFavoritesOnly = !settings.showFavoritesOnly;
                            saveSettings(SETTINGS_FILE, settings);
                            photos = applyFilters();
                            photoIdx = 0;
                            if (!photos.empty()) loadCurrentPhoto();
                            else screen = Screen::Menu;
                        }
                        else if (btnDel.contains(mouse)) {
                            deleteCurrent();
                            if (photos.empty()) screen = Screen::Menu;
                        }
                        else if (btnBack.contains(mouse)) {
                            screen = Screen::Menu;
                        }
                    }
                }
            }
        }

        // ---------- draw ----------
        sf::Color baseBg = settings.darkTheme ? sf::Color(12,12,16) : sf::Color(245,245,250);
        window.clear(baseBg);

        if (screen == Screen::Menu) {
            auto ws = window.getSize();

            if (settings.darkTheme) {
                sf::CircleShape glow1(260.f);
                glow1.setFillColor(sf::Color(120, 160, 255, 35));
                glow1.setPosition({-80.f, -90.f});
                window.draw(glow1);

                sf::CircleShape glow2(320.f);
                glow2.setFillColor(sf::Color(255, 120, 160, 22));
                glow2.setPosition({(float)ws.x - 520.f, (float)ws.y - 520.f});
                window.draw(glow2);
            }

            sf::Text title(font, "Media Database", (unsigned int)settings.fontSizeTitle);
            title.setFillColor(settings.darkTheme ? sf::Color(245,245,245) : sf::Color(30,30,35));

            sf::Text subtitle(font, "Source: Desktop/Photos  |  Library: assets/images", 16);
            subtitle.setFillColor(settings.darkTheme ? sf::Color(180,180,180) : sf::Color(90,90,100));

            sf::Vector2f cardSize(760.f, 360.f);
            sf::Vector2f cardPos(((float)ws.x - cardSize.x) / 2.f,
                                 ((float)ws.y - cardSize.y) / 2.f + 30.f);

            sf::RectangleShape card(cardSize);
            card.setPosition(cardPos);
            card.setFillColor(settings.darkTheme ? sf::Color(255,255,255,16) : sf::Color(0,0,0,10));
            card.setOutlineThickness(1.f);
            card.setOutlineColor(settings.darkTheme ? sf::Color(255,255,255,35) : sf::Color(0,0,0,25));
            window.draw(card);

            title.setPosition({cardPos.x, cardPos.y - 110.f});
            subtitle.setPosition({cardPos.x, cardPos.y - 62.f});
            window.draw(title);
            window.draw(subtitle);

            sf::Text hint(font, "UP/DOWN or mouse - select    ENTER/click - open    ESC - exit", 16);
            hint.setFillColor(settings.darkTheme ? sf::Color(170,170,170) : sf::Color(100,100,110));
            hint.setPosition({cardPos.x, cardPos.y + cardSize.y + 18.f});
            window.draw(hint);

            sf::Text hint2(font, "T - theme | In Photos: P play, I info, S star, F filter, D delete", 15);
            hint2.setFillColor(settings.darkTheme ? sf::Color(160,160,160) : sf::Color(110,110,120));
            hint2.setPosition({cardPos.x, cardPos.y + cardSize.y + 42.f});
            window.draw(hint2);

            float itemX = cardPos.x + 28.f;
            float itemY = cardPos.y + 26.f;
            float itemH = 78.f;

            for (int i = 0; i < 4; i++) {
                sf::FloatRect hit(
                    sf::Vector2f(itemX, itemY + i * itemH),
                    sf::Vector2f(cardSize.x - 56.f, itemH)
                );
                menuHit[i] = hit;

                bool hover = hit.contains(mouse);
                if (hover) menuIndex = i;
                bool active = (i == menuIndex) || hover;

                sf::RectangleShape itemBg({hit.size.x, hit.size.y});
                itemBg.setPosition({hit.position.x, hit.position.y});
                itemBg.setOutlineThickness(1.f);

                if (settings.darkTheme) {
                    itemBg.setFillColor(active ? sf::Color(255,255,255,28) : sf::Color(255,255,255,10));
                    itemBg.setOutlineColor(active ? sf::Color(255,255,255,70) : sf::Color(255,255,255,25));
                } else {
                    itemBg.setFillColor(active ? sf::Color(0,0,0,12) : sf::Color(0,0,0,6));
                    itemBg.setOutlineColor(active ? sf::Color(0,0,0,55) : sf::Color(0,0,0,18));
                }
                window.draw(itemBg);

                sf::RectangleShape strip({6.f, itemH - 16.f});
                strip.setPosition({itemX + 10.f, itemY + i * itemH + 8.f});
                strip.setFillColor(active ? (settings.darkTheme ? sf::Color(160,200,255,200) : sf::Color(40,110,200,200))
                                          : (settings.darkTheme ? sf::Color(255,255,255,25) : sf::Color(0,0,0,18)));
                window.draw(strip);

                sf::Text item(font, menu[i], (unsigned int)settings.fontSizeMenu);
                item.setFillColor(settings.darkTheme ? (active ? sf::Color(250,250,250) : sf::Color(210,210,210))
                                                     : (active ? sf::Color(30,30,35) : sf::Color(70,70,80)));
                item.setPosition({itemX + 30.f, itemY + i * itemH + 12.f});
                window.draw(item);

                sf::Text d(font, desc[i], 15);
                d.setFillColor(settings.darkTheme ? (active ? sf::Color(190,200,215) : sf::Color(160,160,170))
                                                  : (active ? sf::Color(70,90,110) : sf::Color(110,110,120)));
                d.setPosition({itemX + 30.f, itemY + i * itemH + 44.f});
                window.draw(d);

                if (active) {
                    sf::Text arrow(font, ">", 24);
                    arrow.setFillColor(settings.darkTheme ? sf::Color(240,240,240) : sf::Color(60,60,70));
                    arrow.setPosition({itemX + cardSize.x - 86.f, itemY + i * itemH + 18.f});
                    window.draw(arrow);
                }
            }
        } else {
            refreshBarColors();

            spr.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(fade)));
            if (!photos.empty()) window.draw(spr);

            window.draw(bar);
            window.draw(caption);
            window.draw(counter);

            btnPrev.draw(window);
            btnNext.draw(window);
            btnPlay.draw(window);
            btnInfo.draw(window);
            btnStar.draw(window);
            btnFav.draw(window);
            btnDel.draw(window);
            btnBack.draw(window);

            if (showInfo && !photos.empty()) {
                auto imgSize = tex.getSize();
                fs::path p = photos[photoIdx];
                std::error_code ec;
                auto bytes = fs::file_size(p, ec);
                std::uint64_t kb = ec ? 0 : (std::uint64_t)(bytes / 1024);

                std::string file = baseName(photos[photoIdx]);
                bool fav = inList(favorites, file);

                sf::RectangleShape infoBg({460.f, 128.f});
                infoBg.setFillColor(sf::Color(0,0,0,160));
                infoBg.setPosition({20.f, 20.f});
                window.draw(infoBg);

                sf::Text info(font, "", 15);
                info.setFillColor(sf::Color(240,240,240));
                info.setPosition({30.f, 28.f});

                info.setString(
                    std::string("File: ") + file + "\n" +
                    "Resolution: " + std::to_string(imgSize.x) + " x " + std::to_string(imgSize.y) + "\n" +
                    "Size: " + std::to_string(kb) + " KB\n" +
                    "Source folder: Desktop/Photos\n" +
                    (fav ? "★ Favorite" : "")
                );

                window.draw(info);
            }

            sf::Text help(font,
                          "Keys: LEFT/RIGHT | P play | I info | S star | F filter | D delete | ESC menu",
                          13);
            help.setFillColor(settings.darkTheme ? sf::Color(175,175,175) : sf::Color(90,90,100));
            help.setPosition({20.f, 14.f});
            window.draw(help);
        }

        window.display();
    }

    return 0;
}