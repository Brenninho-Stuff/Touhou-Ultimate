// Adicione no início do project.cpp
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/bind.h>

// Emscripten bindings para expor classes C++ ao JavaScript
EMSCRIPTEN_BINDINGS(touhou_ultimate) {
    emscripten::class_<GameState>("GameState")
        .constructor<>()
        .function("initialize", &GameState::initialize)
        .function("update", &GameState::update)
        .function("render", &GameState::render)
        .function("setInput", &GameState::setInput)
        .function("togglePause", &GameState::togglePause)
        .function("isPaused", &GameState::isPaused)
        .function("getPlayer", &GameState::getPlayer, emscripten::allow_raw_pointers());
    
    emscripten::class_<Player>("Player")
        .function("getScore", &Player::getScore)
        .function("getLives", &Player::getLives)
        .function("getBombs", &Player::getBombs)
        .function("getPower", &Player::getPower)
        .function("getGraze", &Player::getGraze)
        .function("getPosition", &Player::getPosition);
    
    emscripten::value_object<Vec2>("Vec2")
        .field("x", &Vec2::x)
        .field("y", &Vec2::y);
}

// Função principal para WebAssembly
int main() {
    GameState& game = GameState::instance();
    game.initialize();
    
    // Loop principal gerenciado pelo navegador
    emscripten_set_main_loop([]() {
        GameState::instance().update(16.0f / 1000.0f);
    }, 0, 1);
    
    return 0;
}

// Adicione este método na classe GameState:
void GameState::setInput(const std::string& inputJson) {
    // Parse JSON e atualiza estado de input
    // Usar uma biblioteca JSON ou parsing manual
}
#endif
