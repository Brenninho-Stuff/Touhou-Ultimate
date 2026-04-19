/**
 * Touhou Ultimate - Game Manager
 * Conecta o frontend HTML com o backend C++ (WebAssembly)
 */

class GameManager {
    constructor() {
        this.module = null;
        this.gameState = null;
        this.canvas = document.getElementById('gameCanvas');
        this.ctx = this.canvas.getContext('2d');
        
        // Estado dos inputs
        this.keys = {
            up: false, down: false, left: false, right: false,
            shoot: false, focus: false
        };
        
        // Bindings
        this.bindEvents();
        this.initializeWebAssembly();
    }
    
    async initializeWebAssembly() {
        try {
            // Carrega o módulo WebAssembly compilado do C++
            const response = await fetch('touhou_ultimate.wasm');
            const buffer = await response.arrayBuffer();
            
            // Configura o ambiente
            const module = {
                canvas: this.canvas,
                onRuntimeInitialized: () => {
                    console.log('✅ Touhou Ultimate WASM loaded!');
                    this.module = Module;
                    this.gameState = new Module.GameState();
                    this.startGameLoop();
                    document.querySelector('.loading-screen')?.remove();
                },
                print: (text) => console.log('[C++]', text),
                printErr: (text) => console.error('[C++]', text)
            };
            
            // Inicializa o módulo
            window.Module = module;
            
            // Script de inicialização (Emscripten)
            const script = document.createElement('script');
            script.src = 'touhou_ultimate.js';
            document.body.appendChild(script);
            
        } catch (error) {
            console.error('Failed to load WebAssembly:', error);
            this.fallbackToWebSocket();
        }
    }
    
    fallbackToWebSocket() {
        console.log('🔄 Falling back to WebSocket native connection...');
        this.ws = new WebSocket('ws://localhost:8080');
        
        this.ws.onopen = () => {
            console.log('✅ Connected to native C++ server');
            this.connected = true;
            this.startGameLoop();
        };
        
        this.ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            this.handleServerMessage(data);
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
    }
    
    handleServerMessage(data) {
        switch (data.type) {
            case 'frame':
                // Recebe frame renderizado do servidor
                this.renderFrame(data.frameData);
                break;
            case 'state':
                // Atualiza UI
                this.updateUI(data);
                break;
            case 'gameover':
                this.showGameOver(data.finalScore);
                break;
        }
    }
    
    renderFrame(frameData) {
        // Se receber dados binários do canvas
        if (frameData) {
            const img = new Image();
            img.onload = () => {
                this.ctx.drawImage(img, 0, 0);
            };
            img.src = frameData;
        }
    }
    
    startGameLoop() {
        const loop = () => {
            this.update();
            requestAnimationFrame(loop);
        };
        loop();
        
        // Loop de inputs (60 FPS)
        setInterval(() => {
            this.sendInputState();
        }, 16);
    }
    
    update() {
        if (this.module && this.gameState) {
            // Chama update do C++ via WebAssembly
            this.gameState.update(16.0 / 1000.0);
            
            // Render
            this.gameState.render();
            
            // Atualiza UI
            this.updateUIFromWASM();
        }
    }
    
    updateUIFromWASM() {
        if (!this.module) return;
        
        // Lê dados do jogador do C++
        const player = this.gameState.getPlayer();
        if (player) {
            document.getElementById('score').textContent = player.getScore();
            document.getElementById('lives').textContent = player.getLives();
            document.getElementById('bombs').textContent = player.getBombs();
            document.getElementById('power').textContent = (player.getPower() / 100).toFixed(2);
            document.getElementById('graze').textContent = player.getGraze();
            
            // Barra de graze
            const grazePercent = (player.getGraze() % 100);
            document.getElementById('grazeFill').style.width = grazePercent + '%';
        }
    }
    
    updateUI(data) {
        document.getElementById('score').textContent = data.score || 0;
        document.getElementById('lives').textContent = data.lives || 3;
        document.getElementById('bombs').textContent = data.bombs || 3;
        document.getElementById('power').textContent = data.power || '1.00';
        document.getElementById('graze').textContent = data.graze || 0;
    }
    
    sendInputState() {
        const inputState = {
            up: this.keys.up,
            down: this.keys.down,
            left: this.keys.left,
            right: this.keys.right,
            shoot: this.keys.shoot,
            focus: this.keys.focus,
            bomb: this.keys.bomb
        };
        
        if (this.module && this.gameState) {
            // Via WebAssembly
            this.gameState.setInput(JSON.stringify(inputState));
        } else if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            // Via WebSocket
            this.ws.send(JSON.stringify({
                type: 'input',
                state: inputState
            }));
        }
        
        // Reset bomb flag (one-shot)
        this.keys.bomb = false;
    }
    
    bindEvents() {
        // Keyboard controls
        document.addEventListener('keydown', (e) => {
            this.handleKeyDown(e);
        });
        
        document.addEventListener('keyup', (e) => {
            this.handleKeyUp(e);
        });
        
        // Prevent page scroll with arrow keys
        window.addEventListener('keydown', (e) => {
            if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' '].includes(e.key)) {
                e.preventDefault();
            }
        }, false);
        
        // Pause on Escape
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') {
                this.togglePause();
            }
        });
    }
    
    handleKeyDown(e) {
        switch (e.key) {
            case 'ArrowUp': this.keys.up = true; break;
            case 'ArrowDown': this.keys.down = true; break;
            case 'ArrowLeft': this.keys.left = true; break;
            case 'ArrowRight': this.keys.right = true; break;
            case 'z':
            case 'Z': this.keys.shoot = true; break;
            case 'Shift': this.keys.focus = true; break;
            case 'x':
            case 'X': this.bomb(); break;
        }
    }
    
    handleKeyUp(e) {
        switch (e.key) {
            case 'ArrowUp': this.keys.up = false; break;
            case 'ArrowDown': this.keys.down = false; break;
            case 'ArrowLeft': this.keys.left = false; break;
            case 'ArrowRight': this.keys.right = false; break;
            case 'z':
            case 'Z': this.keys.shoot = false; break;
            case 'Shift': this.keys.focus = false; break;
        }
    }
    
    // Mobile controls
    moveStart(direction) {
        this.keys[direction] = true;
    }
    
    moveEnd(direction) {
        this.keys[direction] = false;
    }
    
    shoot(pressed) {
        this.keys.shoot = pressed;
    }
    
    focus(pressed) {
        this.keys.focus = pressed;
    }
    
    bomb() {
        this.keys.bomb = true;
    }
    
    togglePause() {
        if (this.module && this.gameState) {
            this.gameState.togglePause();
            const paused = this.gameState.isPaused();
            document.getElementById('pauseMenu').style.display = paused ? 'block' : 'none';
        }
    }
    
    resume() {
        if (this.module && this.gameState) {
            this.gameState.togglePause();
            document.getElementById('pauseMenu').style.display = 'none';
        }
    }
    
    restart() {
        if (this.module && this.gameState) {
            this.gameState.initialize();
        } else if (this.ws) {
            this.ws.send(JSON.stringify({ type: 'restart' }));
        }
        document.getElementById('pauseMenu').style.display = 'none';
        document.getElementById('gameoverScreen').style.display = 'none';
    }
    
    quit() {
        window.location.reload();
    }
    
    showGameOver(finalScore) {
        document.getElementById('finalScore').textContent = finalScore;
        document.getElementById('gameoverScreen').style.display = 'block';
    }
    
    submitScore() {
        const name = document.getElementById('playerNameInput').value || 'Player';
        const score = document.getElementById('finalScore').textContent;
        
        // Envia para leaderboard
        fetch('/api/scores', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, score: parseInt(score) })
        }).then(() => {
            alert('Score submitted!');
            this.restart();
        });
    }
}

// Inicializa quando a página carregar
window.addEventListener('load', () => {
    window.gameManager = new GameManager();
});
