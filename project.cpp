/**
 * Touhou Ultimate - Core Game System
 * Fangame created by Brenninho
 * 
 * This file implements the main game loop, entity management,
 * optimized collision system, and bullet patterns.
 * 
 * Suggested compilation: g++ -O2 -lSDL2 -lSDL2_image -lSDL2_ttf project.cpp -o TouhouUltimate
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <random>
#include <chrono>
#include <unordered_map>
#include <functional>

// ============================================================================
// UTILITY CLASSES & HELPERS
// ============================================================================

// 2D Vector mathematics
struct Vec2 {
    float x, y;
    
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
    
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    
    float length() const { return std::sqrt(x * x + y * y); }
    float lengthSquared() const { return x * x + y * y; }
    
    Vec2 normalized() const {
        float len = length();
        return (len > 0) ? Vec2(x / len, y / len) : Vec2(0, 0);
    }
    
    float dot(const Vec2& other) const { return x * other.x + y * other.y; }
    float cross(const Vec2& other) const { return x * other.y - y * other.x; }
    
    // Rotate vector by angle in radians
    Vec2 rotated(float angle) const {
        float cs = std::cos(angle);
        float sn = std::sin(angle);
        return Vec2(x * cs - y * sn, x * sn + y * cs);
    }
    
    // Get angle of vector in radians
    float angle() const { return std::atan2(y, x); }
    
    // Distance to another point
    float distanceTo(const Vec2& other) const { return (*this - other).length(); }
};

// Axis-Aligned Bounding Box for collision
struct AABB {
    Vec2 min, max;
    
    AABB(const Vec2& center, const Vec2& halfSize) {
        min = center - halfSize;
        max = center + halfSize;
    }
    
    bool intersects(const AABB& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y);
    }
    
    bool contains(const Vec2& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y;
    }
};

// Circle collision shape for precise bullet hitboxes
struct Circle {
    Vec2 center;
    float radius;
    
    Circle(const Vec2& center, float radius) : center(center), radius(radius) {}
    
    bool intersects(const Circle& other) const {
        float distSq = center.distanceTo(other.center);
        float radiusSum = radius + other.radius;
        return distSq <= radiusSum * radiusSum;
    }
    
    bool intersects(const AABB& aabb) const {
        // Closest point on AABB to circle center
        float closestX = std::max(aabb.min.x, std::min(center.x, aabb.max.x));
        float closestY = std::max(aabb.min.y, std::min(center.y, aabb.max.y));
        float distSq = (center.x - closestX) * (center.x - closestX) +
                       (center.y - closestY) * (center.y - closestY);
        return distSq <= radius * radius;
    }
};

// Random number generator singleton
class Random {
private:
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist01;
    std::uniform_real_distribution<float> dist11;
    
    Random() : gen(std::chrono::steady_clock::now().time_since_epoch().count()),
               dist01(0.0f, 1.0f), dist11(-1.0f, 1.0f) {}
    
public:
    static Random& instance() {
        static Random instance;
        return instance;
    }
    
    float value() { return dist01(gen); }              // [0, 1]
    float range(float min, float max) {                 // [min, max]
        return min + dist01(gen) * (max - min);
    }
    float signedValue() { return dist11(gen); }         // [-1, 1]
    int rangeInt(int min, int max) {                    // [min, max]
        std::uniform_int_distribution<int> dist(min, max);
        return dist(gen);
    }
    
    // Random angle in radians
    float angle() { return range(0.0f, 2.0f * M_PI); }
    
    // Random point in circle
    Vec2 pointInCircle(float radius) {
        float r = radius * std::sqrt(value());
        float theta = angle();
        return Vec2(r * std::cos(theta), r * std::sin(theta));
    }
};

// ============================================================================
// GAME CONSTANTS
// ============================================================================

constexpr int SCREEN_WIDTH = 640;
constexpr int SCREEN_HEIGHT = 960;
constexpr float PLAYER_SPEED = 5.0f;
constexpr float PLAYER_FOCUS_SPEED = 2.0f;
constexpr float PLAYER_HITBOX_RADIUS = 2.5f;
constexpr float BULLET_HITBOX_RADIUS = 6.0f;
constexpr float ENEMY_BASE_HITBOX_RADIUS = 8.0f;
constexpr int MAX_POWER = 400;
constexpr int POWER_PER_LEVEL = 100;
constexpr int MAX_LIVES = 8;
constexpr int MAX_BOMBS = 8;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class Entity;
class Player;
class Enemy;
class Bullet;
class Item;
class GameState;
class BulletPattern;

// ============================================================================
// ENTITY BASE CLASS
// ============================================================================

class Entity {
protected:
    Vec2 position;
    Vec2 velocity;
    bool active;
    int layer;  // Drawing order
    
public:
    Entity(const Vec2& pos = Vec2()) : position(pos), velocity(0, 0), active(true), layer(0) {}
    virtual ~Entity() = default;
    
    virtual void update(float deltaTime) {
        position += velocity * deltaTime;
    }
    
    virtual void render() = 0;
    virtual Circle getHitbox() const = 0;
    
    bool isActive() const { return active; }
    void deactivate() { active = false; }
    const Vec2& getPosition() const { return position; }
    int getLayer() const { return layer; }
    
    // Check if entity is off-screen (with margin)
    bool isOffscreen(float margin = 50.0f) const {
        return position.x < -margin || position.x > SCREEN_WIDTH + margin ||
               position.y < -margin || position.y > SCREEN_HEIGHT + margin;
    }
};

// ============================================================================
// BULLET CLASS
// ============================================================================

class Bullet : public Entity {
public:
    enum BulletType {
        CIRCLE,         // Standard round bullet
        OVAL,           // Stretched bullet
        ARROW,          // Directional bullet
        LASER,          // Laser beam (handled differently)
        CUSTOM
    };
    
private:
    BulletType type;
    float size;
    uint32_t color;
    float angle;        // Visual rotation
    int grazeFlag;      // Prevents multiple graze counting
    bool grazable;
    
public:
    Bullet(const Vec2& pos, BulletType type = CIRCLE, float size = 8.0f)
        : Entity(pos), type(type), size(size), color(0xFFFFFFFF),
          angle(0.0f), grazeFlag(0), grazable(true) {
        layer = 10; // Bullets render above most things
    }
    
    void update(float deltaTime) override {
        Entity::update(deltaTime);
        
        // Update visual angle based on velocity direction
        if (velocity.lengthSquared() > 0.01f) {
            angle = velocity.angle();
        }
        
        // Deactivate if off-screen
        if (isOffscreen(100.0f)) {
            deactivate();
        }
    }
    
    void render() override {
        // Rendering would use SDL or similar graphics library
        // This is a placeholder for the rendering logic
    }
    
    Circle getHitbox() const override {
        return Circle(position, size * 0.5f);
    }
    
    // Setters for fluent interface
    Bullet& setVelocity(const Vec2& vel) { velocity = vel; return *this; }
    Bullet& setColor(uint32_t c) { color = c; return *this; }
    Bullet& setSize(float s) { size = s; return *this; }
    Bullet& setGrazable(bool g) { grazable = g; return *this; }
    
    bool isGrazable() const { return grazable; }
    int getGrazeFlag() const { return grazeFlag; }
    void setGrazeFlag(int flag) { grazeFlag = flag; }
    float getSize() const { return size; }
    uint32_t getColor() const { return color; }
    BulletType getType() const { return type; }
};

// ============================================================================
// PLAYER CLASS
// ============================================================================

class Player : public Entity {
private:
    // Movement state
    bool movingUp, movingDown, movingLeft, movingRight;
    bool focusMode;     // Slower movement, shows hitbox
    
    // Stats
    int lives;
    int bombs;
    int power;
    int score;
    int graze;
    int pointValue;
    
    // Invulnerability
    float invulnerabilityTimer;
    const float INVULN_DURATION = 2.0f;
    const float INVULN_BLINK_RATE = 0.15f;
    
    // Shot system
    float shotCooldown;
    const float SHOT_INTERVAL = 0.05f;
    int shotPower;
    
public:
    Player()
        : Entity(Vec2(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT * 0.8f)),
          movingUp(false), movingDown(false), movingLeft(false), movingRight(false),
          focusMode(false), lives(3), bombs(3), power(100), score(0), graze(0),
          pointValue(0), invulnerabilityTimer(0.0f), shotCooldown(0.0f), shotPower(1) {
        layer = 100; // Player renders on top
    }
    
    void update(float deltaTime) override {
        // Handle input movement
        Vec2 moveInput(0, 0);
        if (movingUp)    moveInput.y -= 1;
        if (movingDown)  moveInput.y += 1;
        if (movingLeft)  moveInput.x -= 1;
        if (movingRight) moveInput.x += 1;
        
        if (moveInput.lengthSquared() > 0) {
            moveInput = moveInput.normalized();
        }
        
        float speed = focusMode ? PLAYER_FOCUS_SPEED : PLAYER_SPEED;
        velocity = moveInput * speed;
        
        Entity::update(deltaTime);
        
        // Clamp to screen bounds
        position.x = std::max(PLAYER_HITBOX_RADIUS, 
                     std::min(SCREEN_WIDTH - PLAYER_HITBOX_RADIUS, position.x));
        position.y = std::max(PLAYER_HITBOX_RADIUS, 
                     std::min(SCREEN_HEIGHT - PLAYER_HITBOX_RADIUS, position.y));
        
        // Update invulnerability
        if (invulnerabilityTimer > 0) {
            invulnerabilityTimer -= deltaTime;
        }
        
        // Update shooting
        if (shotCooldown > 0) {
            shotCooldown -= deltaTime;
        }
        
        // Auto-shoot while alive
        if (isActive() && shotCooldown <= 0) {
            fire();
            shotCooldown = SHOT_INTERVAL;
        }
    }
    
    void fire() {
        // Will be implemented to spawn player bullets
        // Based on power level (shotPower = power / POWER_PER_LEVEL)
        // Higher power = more bullets / spread
    }
    
    void render() override {
        // Rendering placeholder
        // Player sprite, hitbox when focused or invulnerable
    }
    
    Circle getHitbox() const override {
        return Circle(position, PLAYER_HITBOX_RADIUS);
    }
    
    // Graze detection hitbox (larger than actual hitbox)
    Circle getGrazeHitbox() const {
        return Circle(position, 24.0f);
    }
    
    // Collision response
    void hit() {
        if (invulnerabilityTimer > 0) return;
        
        lives--;
        invulnerabilityTimer = INVULN_DURATION;
        power = std::max(0, power - 50); // Power penalty on death
        
        if (lives <= 0) {
            deactivate();
            // Trigger game over
        }
    }
    
    void bomb() {
        if (bombs <= 0 || invulnerabilityTimer > 0) return;
        
        bombs--;
        invulnerabilityTimer = 3.0f; // Bombs give temporary invulnerability
        // Trigger bomb effect (screen clear, damage to enemies)
    }
    
    // Input handlers
    void setMoveUp(bool pressed) { movingUp = pressed; }
    void setMoveDown(bool pressed) { movingDown = pressed; }
    void setMoveLeft(bool pressed) { movingLeft = pressed; }
    void setMoveRight(bool pressed) { movingRight = pressed; }
    void setFocus(bool pressed) { focusMode = pressed; }
    
    // Stat getters/setters
    int getLives() const { return lives; }
    int getBombs() const { return bombs; }
    int getPower() const { return power; }
    int getScore() const { return score; }
    int getGraze() const { return graze; }
    bool isInvulnerable() const { return invulnerabilityTimer > 0; }
    bool isFocused() const { return focusMode; }
    
    void addScore(int points) { 
        score += points; 
        pointValue += points;
        
        // Extend at score thresholds
        static int nextExtend = 10000000;
        if (score >= nextExtend) {
            lives = std::min(lives + 1, MAX_LIVES);
            nextExtend += 10000000;
        }
    }
    
    void addGraze(int amount = 1) { graze += amount; }
    void addPower(int amount) { 
        power = std::min(power + amount, MAX_POWER);
        shotPower = power / POWER_PER_LEVEL;
    }
};

// ============================================================================
// BULLET PATTERN SYSTEM
// ============================================================================

class BulletPattern {
public:
    using PatternFunction = std::function<void(float time, const Vec2& origin, 
                                                std::vector<Bullet>& bulletList)>;
    
private:
    std::vector<PatternFunction> layers;
    float duration;
    float timer;
    bool looping;
    
public:
    BulletPattern() : duration(0), timer(0), looping(false) {}
    
    BulletPattern& setDuration(float d) { duration = d; return *this; }
    BulletPattern& setLooping(bool l) { looping = l; return *this; }
    
    BulletPattern& addLayer(PatternFunction func) {
        layers.push_back(func);
        return *this;
    }
    
    void update(float deltaTime, const Vec2& origin, std::vector<Bullet>& bullets) {
        if (duration > 0) {
            timer += deltaTime;
            if (timer >= duration) {
                if (looping) {
                    timer = std::fmod(timer, duration);
                } else {
                    return; // Pattern finished
                }
            }
        }
        
        for (auto& layer : layers) {
            layer(timer, origin, bullets);
        }
    }
    
    void reset() { timer = 0; }
    bool isFinished() const { return !looping && timer >= duration; }
};

// Predefined pattern generators
namespace BulletPatterns {
    
    // Spiral pattern
    BulletPattern spiral(int bulletCount, float speed, float rotationSpeed) {
        BulletPattern pattern;
        pattern.setLooping(true);
        
        pattern.addLayer([bulletCount, speed, rotationSpeed](float time, const Vec2& origin, 
                                                               std::vector<Bullet>& bullets) {
            static float lastSpawnTime = -1.0f;
            const float SPAWN_INTERVAL = 0.1f;
            
            if (time - lastSpawnTime >= SPAWN_INTERVAL || lastSpawnTime < 0) {
                lastSpawnTime = time;
                
                float baseAngle = time * rotationSpeed;
                
                for (int i = 0; i < bulletCount; i++) {
                    float angle = baseAngle + (2.0f * M_PI * i / bulletCount);
                    Vec2 direction(std::cos(angle), std::sin(angle));
                    
                    Bullet bullet(origin);
                    bullet.setVelocity(direction * speed)
                          .setColor(0xFF4444FF)
                          .setSize(8.0f);
                    bullets.push_back(bullet);
                }
            }
        });
        
        return pattern;
    }
    
    // Aimed shot at player
    BulletPattern aimedShot(float speed, float angleOffset = 0.0f) {
        BulletPattern pattern;
        
        pattern.addLayer([speed, angleOffset](float time, const Vec2& origin,
                                               std::vector<Bullet>& bullets, Player* player) {
            static float lastSpawnTime = -1.0f;
            const float SPAWN_INTERVAL = 0.15f;
            
            if ((time - lastSpawnTime >= SPAWN_INTERVAL || lastSpawnTime < 0) && player) {
                lastSpawnTime = time;
                
                Vec2 toPlayer = player->getPosition() - origin;
                Vec2 direction = toPlayer.normalized().rotated(angleOffset);
                
                Bullet bullet(origin);
                bullet.setVelocity(direction * speed)
                      .setColor(0xFF66AAFF)
                      .setSize(10.0f);
                bullets.push_back(bullet);
            }
        });
        
        return pattern;
    }
    
    // Circular burst
    BulletPattern circleBurst(int count, float speed, bool aimAtPlayer = false) {
        BulletPattern pattern;
        
        pattern.addLayer([count, speed, aimAtPlayer](float time, const Vec2& origin,
                                                      std::vector<Bullet>& bullets, Player* player) {
            // Only fire once at start
            static bool fired = false;
            if (!fired) {
                fired = true;
                
                float baseAngle = 0;
                if (aimAtPlayer && player) {
                    baseAngle = (player->getPosition() - origin).angle();
                }
                
                for (int i = 0; i < count; i++) {
                    float angle = baseAngle + (2.0f * M_PI * i / count);
                    Vec2 direction(std::cos(angle), std::sin(angle));
                    
                    Bullet bullet(origin);
                    bullet.setVelocity(direction * speed)
                          .setColor(0xFFAA44FF)
                          .setSize(8.0f);
                    bullets.push_back(bullet);
                }
            }
        });
        
        return pattern;
    }
    
    // Streaming bullets (like a machine gun)
    BulletPattern stream(float speed, float angleVariation, float interval) {
        BulletPattern pattern;
        pattern.setLooping(true);
        
        pattern.addLayer([speed, angleVariation, interval](float time, const Vec2& origin,
                                                            std::vector<Bullet>& bullets) {
            static float lastSpawnTime = -1.0f;
            
            if (time - lastSpawnTime >= interval || lastSpawnTime < 0) {
                lastSpawnTime = time;
                
                float angle = -M_PI / 2; // Shoot downward
                angle += Random::instance().range(-angleVariation, angleVariation);
                
                Vec2 direction(std::cos(angle), std::sin(angle));
                
                Bullet bullet(origin);
                bullet.setVelocity(direction * speed)
                      .setColor(0xFFFF8844)
                      .setSize(7.0f);
                bullets.push_back(bullet);
            }
        });
        
        return pattern;
    }
}

// ============================================================================
// ENEMY CLASS
// ============================================================================

class Enemy : public Entity {
protected:
    float health;
    float maxHealth;
    int pointValue;
    float hitboxRadius;
    
    // Movement
    std::function<Vec2(float, const Vec2&)> movementFunc;
    float moveTimer;
    
    // Attack patterns
    std::vector<BulletPattern> patterns;
    int currentPattern;
    
    // Visual
    uint32_t color;
    
public:
    Enemy(const Vec2& pos)
        : Entity(pos), health(100), maxHealth(100), pointValue(1000),
          hitboxRadius(ENEMY_BASE_HITBOX_RADIUS), moveTimer(0), currentPattern(0),
          color(0xFF888888) {
        layer = 50;
    }
    
    void update(float deltaTime) override {
        moveTimer += deltaTime;
        
        // Apply movement function
        if (movementFunc) {
            position = movementFunc(moveTimer, position);
        } else {
            Entity::update(deltaTime);
        }
        
        // Fire bullet patterns
        if (currentPattern < patterns.size()) {
            patterns[currentPattern].update(moveTimer, position, bullets);
            
            if (patterns[currentPattern].isFinished()) {
                currentPattern++;
            }
        }
        
        // Deactivate if off-screen or dead
        if (isOffscreen(100.0f) || health <= 0) {
            if (health <= 0) {
                onDeath();
            }
            deactivate();
        }
    }
    
    virtual void onDeath() {
        // Spawn items, add score, etc.
    }
    
    void render() override {
        // Rendering placeholder
        // Health bar, enemy sprite
    }
    
    Circle getHitbox() const override {
        return Circle(position, hitboxRadius);
    }
    
    void takeDamage(float damage) {
        health -= damage;
    }
    
    // Setters for configuration
    Enemy& setHealth(float hp) { health = maxHealth = hp; return *this; }
    Enemy& setMovement(std::function<Vec2(float, const Vec2&)> func) { movementFunc = func; return *this; }
    Enemy& setHitboxRadius(float r) { hitboxRadius = r; return *this; }
    Enemy& setColor(uint32_t c) { color = c; return *this; }
    Enemy& setPointValue(int pv) { pointValue = pv; return *this; }
    
    Enemy& addPattern(const BulletPattern& pattern) {
        patterns.push_back(pattern);
        return *this;
    }
};

// Predefined movement functions
namespace EnemyMovement {
    
    // Move straight down
    std::function<Vec2(float, const Vec2&)> straightDown(float speed) {
        return [speed](float t, const Vec2& pos) {
            return pos + Vec2(0, speed);
        };
    }
    
    // Sine wave movement
    std::function<Vec2(float, const Vec2&)> sineWave(float speedX, float speedY, 
                                                      float amplitude, float frequency) {
        return [speedX, speedY, amplitude, frequency](float t, const Vec2& pos) {
            float offsetX = amplitude * std::sin(t * frequency);
            return Vec2(pos.x + offsetX, pos.y + speedY);
        };
    }
    
    // Stop at a specific Y position and start firing
    std::function<Vec2(float, const Vec2&)> stopAndShoot(float speedY, float stopY) {
        return [speedY, stopY](float t, const Vec2& pos) {
            if (pos.y >= stopY) {
                return Vec2(pos.x, stopY);
            }
            return pos + Vec2(0, speedY);
        };
    }
    
    // Homing movement (chases player)
    std::function<Vec2(float, const Vec2&)> homing(float speed, Player* player) {
        return [speed, player](float t, const Vec2& pos) {
            if (!player) return pos;
            
            Vec2 toPlayer = player->getPosition() - pos;
            if (toPlayer.length() > 5.0f) {
                return pos + toPlayer.normalized() * speed;
            }
            return pos;
        };
    }
}

// ============================================================================
// ITEM CLASS
// ============================================================================

class Item : public Entity {
public:
    enum ItemType {
        POWER_SMALL,    // +1 power
        POWER_LARGE,    // +10 power
        POINT,          // Score points
        BOMB,           // +1 bomb
        LIFE,           // +1 life (rare)
        FULL_POWER      // Max power
    };
    
private:
    ItemType type;
    float value;
    float attractionSpeed;
    bool autoCollect;
    float fallSpeed;
    
public:
    Item(const Vec2& pos, ItemType type)
        : Entity(pos), type(type), value(0), attractionSpeed(3.0f),
          autoCollect(false), fallSpeed(1.5f) {
        layer = 20;
        
        switch (type) {
            case POWER_SMALL: value = 1; break;
            case POWER_LARGE: value = 10; break;
            case POINT: value = 5000; break;
            case BOMB: value = 1; break;
            case LIFE: value = 1; break;
            case FULL_POWER: value = MAX_POWER; break;
        }
    }
    
    void update(float deltaTime) override {
        if (!autoCollect) {
            // Fall down naturally
            position.y += fallSpeed * deltaTime;
            
            // Auto-collect when player is above collection line
            if (position.y > SCREEN_HEIGHT * 0.75f) {
                autoCollect = true;
            }
        } else {
            // Move toward player
            Player* player = GameState::instance().getPlayer();
            if (player) {
                Vec2 toPlayer = player->getPosition() - position;
                float dist = toPlayer.length();
                
                if (dist < 10.0f) {
                    collect();
                } else {
                    position += toPlayer.normalized() * attractionSpeed * deltaTime;
                }
            }
        }
        
        if (isOffscreen()) {
            deactivate();
        }
    }
    
    void collect() {
        Player* player = GameState::instance().getPlayer();
        if (!player) return;
        
        switch (type) {
            case POWER_SMALL:
            case POWER_LARGE:
                player->addPower(static_cast<int>(value));
                break;
            case POINT:
                player->addScore(static_cast<int>(value));
                break;
            case BOMB:
                player->bombs = std::min(player->bombs + 1, MAX_BOMBS);
                break;
            case LIFE:
                player->lives = std::min(player->lives + 1, MAX_LIVES);
                break;
            case FULL_POWER:
                player->power = MAX_POWER;
                break;
        }
        
        deactivate();
    }
    
    void render() override {
        // Rendering placeholder
    }
    
    Circle getHitbox() const override {
        return Circle(position, 12.0f);
    }
};

// ============================================================================
// GAME STATE MANAGER (Singleton)
// ============================================================================

class GameState {
private:
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<Bullet> bullets;
    std::unique_ptr<Player> player;
    
    // Game state
    bool running;
    bool paused;
    float gameTimer;
    int stage;
    int frameCount;
    
    // Score multipliers
    float grazeMultiplier;
    int consecutiveGraze;
    float grazeTimer;
    
    GameState() : running(false), paused(false), gameTimer(0), stage(1),
                  frameCount(0), grazeMultiplier(1.0f), consecutiveGraze(0),
                  grazeTimer(0) {}
    
public:
    static GameState& instance() {
        static GameState instance;
        return instance;
    }
    
    void initialize() {
        player = std::make_unique<Player>();
        running = true;
        gameTimer = 0;
        stage = 1;
        
        // Start stage 1
        loadStage(stage);
    }
    
    void update(float deltaTime) {
        if (!running || paused) return;
        
        gameTimer += deltaTime;
        frameCount++;
        
        // Update player
        if (player && player->isActive()) {
            player->update(deltaTime);
        }
        
        // Update all entities
        for (auto& entity : entities) {
            if (entity->isActive()) {
                entity->update(deltaTime);
            }
        }
        
        // Update bullets
        for (auto& bullet : bullets) {
            if (bullet.isActive()) {
                bullet.update(deltaTime);
            }
        }
        
        // Collision detection
        checkCollisions();
        
        // Clean up inactive entities and bullets
        cleanup();
        
        // Check stage completion
        if (isStageCleared()) {
            stage++;
            if (stage <= 6) {
                loadStage(stage);
            } else {
                // Game complete!
                running = false;
            }
        }
        
        // Game over check
        if (player && !player->isActive()) {
            running = false;
        }
    }
    
    void checkCollisions() {
        if (!player || !player->isActive()) return;
        
        Circle playerHitbox = player->getHitbox();
        Circle playerGraze = player->getGrazeHitbox();
        bool playerInvuln = player->isInvulnerable();
        
        for (auto& bullet : bullets) {
            if (!bullet.isActive()) continue;
            
            Circle bulletHitbox = bullet.getHitbox();
            
            // Player collision
            if (!playerInvuln && playerHitbox.intersects(bulletHitbox)) {
                player->hit();
                bullet.deactivate();
                
                // Create death effect
                continue;
            }
            
            // Graze detection
            if (bullet.isGrazable() && bullet.getGrazeFlag() == 0) {
                if (playerGraze.intersects(bulletHitbox)) {
                    bullet.setGrazeFlag(1);
                    player->addGraze();
                    consecutiveGraze++;
                    grazeTimer = 0.5f;
                    
                    // Add graze score
                    int grazeScore = 100 * static_cast<int>(grazeMultiplier);
                    player->addScore(grazeScore);
                }
            }
        }
        
        // Update graze multiplier
        if (grazeTimer > 0) {
            grazeTimer -= deltaTime;
            if (grazeTimer <= 0) {
                consecutiveGraze = 0;
                grazeMultiplier = 1.0f;
            } else {
                grazeMultiplier = 1.0f + (consecutiveGraze * 0.1f);
                grazeMultiplier = std::min(grazeMultiplier, 3.0f);
            }
        }
        
        // Enemy collisions (player vs enemies)
        for (auto& entity : entities) {
            Enemy* enemy = dynamic_cast<Enemy*>(entity.get());
            if (enemy && enemy->isActive()) {
                Circle enemyHitbox = enemy->getHitbox();
                
                if (!playerInvuln && playerHitbox.intersects(enemyHitbox)) {
                    player->hit();
                }
            }
        }
        
        // Player bullets vs enemies
        // This would iterate through player bullets and check against enemies
    }
    
    void cleanup() {
        // Remove inactive entities
        entities.erase(
            std::remove_if(entities.begin(), entities.end(),
                [](const std::unique_ptr<Entity>& e) { return !e->isActive(); }),
            entities.end()
        );
        
        // Remove inactive bullets
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(),
                [](const Bullet& b) { return !b.isActive(); }),
            bullets.end()
        );
    }
    
    void loadStage(int stageNum) {
        // Clear previous stage enemies (keep player, bullets, items)
        entities.erase(
            std::remove_if(entities.begin(), entities.end(),
                [](const std::unique_ptr<Entity>& e) {
                    return dynamic_cast<Enemy*>(e.get()) != nullptr;
                }),
            entities.end()
        );
        
        // Load stage-specific enemies and patterns
        switch (stageNum) {
            case 1:
                spawnStage1Enemies();
                break;
            case 2:
                spawnStage2Enemies();
                break;
            // ... up to stage 6
        }
    }
    
    void spawnStage1Enemies() {
        // Spawn some basic enemies
        for (int i = 0; i < 5; i++) {
            auto enemy = std::make_unique<Enemy>(Vec2(100.0f + i * 100.0f, -50.0f));
            enemy->setHealth(50)
                  .setMovement(EnemyMovement::stopAndShoot(2.0f, 200.0f))
                  .addPattern(BulletPatterns::aimedShot(3.0f))
                  .setColor(0xFFCC6666);
            entities.push_back(std::move(enemy));
        }
    }
    
    void spawnStage2Enemies() {
        // Midboss
        auto midboss = std::make_unique<Enemy>(Vec2(SCREEN_WIDTH / 2.0f, -100.0f));
        midboss->setHealth(500)
                .setMovement(EnemyMovement::sineWave(0, 1.5f, 100.0f, 0.02f))
                .setHitboxRadius(20.0f)
                .addPattern(BulletPatterns::spiral(8, 2.5f, 1.5f))
                .addPattern(BulletPatterns::aimedShot(4.0f))
                .setColor(0xFF9966CC);
        entities.push_back(std::move(midboss));
    }
    
    bool isStageCleared() const {
        // Check if all enemies are defeated
        for (const auto& entity : entities) {
            if (dynamic_cast<Enemy*>(entity.get()) != nullptr) {
                return false;
            }
        }
        return true;
    }
    
    void render() {
        // Background rendering
        
        // Render entities by layer
        std::vector<Entity*> renderList;
        for (auto& entity : entities) {
            if (entity->isActive()) renderList.push_back(entity.get());
        }
        for (auto& bullet : bullets) {
            if (bullet.isActive()) renderList.push_back(&bullet);
        }
        if (player && player->isActive()) {
            renderList.push_back(player.get());
        }
        
        // Sort by layer
        std::sort(renderList.begin(), renderList.end(),
            [](Entity* a, Entity* b) { return a->getLayer() < b->getLayer(); });
        
        for (auto* entity : renderList) {
            entity->render();
        }
        
        // UI rendering
        renderUI();
    }
    
    void renderUI() {
        if (!player) return;
        
        // Score, lives, bombs, power, graze
        // This would draw text using SDL_ttf or similar
    }
    
    // Getters
    Player* getPlayer() { return player.get(); }
    std::vector<Bullet>& getBullets() { return bullets; }
    bool isRunning() const { return running; }
    void quit() { running = false; }
    void togglePause() { paused = !paused; }
};

// ============================================================================
// MAIN GAME LOOP
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "Touhou Ultimate - Initializing..." << std::endl;
    
    // Initialize SDL and create window (pseudocode)
    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    // SDL_Window* window = SDL_CreateWindow("Touhou Ultimate", ...);
    // SDL_Renderer* renderer = SDL_CreateRenderer(window, ...);
    
    GameState& game = GameState::instance();
    game.initialize();
    
    auto lastTime = std::chrono::steady_clock::now();
    const float TARGET_FPS = 60.0f;
    const float TARGET_FRAME_TIME = 1.0f / TARGET_FPS;
    
    std::cout << "Touhou Ultimate - Game started!" << std::endl;
    
    while (game.isRunning()) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Cap delta time to avoid spiral of death
        deltaTime = std::min(deltaTime, 0.033f);
        
        // Process input (pseudocode)
        // SDL_Event event;
        // while (SDL_PollEvent(&event)) {
        //     handleInput(event);
        // }
        
        // Update game state
        game.update(deltaTime);
        
        // Render
        // SDL_RenderClear(renderer);
        game.render();
        // SDL_RenderPresent(renderer);
        
        // Frame limiting
        auto frameEnd = std::chrono::steady_clock::now();
        float frameTime = std::chrono::duration<float>(frameEnd - currentTime).count();
        if (frameTime < TARGET_FRAME_TIME) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(TARGET_FRAME_TIME - frameTime)
            );
        }
    }
    
    std::cout << "Touhou Ultimate - Game ended. Final Score: " 
              << (game.getPlayer() ? game.getPlayer()->getScore() : 0) << std::endl;
    
    // Cleanup SDL
    // SDL_DestroyRenderer(renderer);
    // SDL_DestroyWindow(window);
    // SDL_Quit();
    
    return 0;
}
