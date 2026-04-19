/**
 * Servidor WebSocket para Touhou Ultimate
 * Conecta o navegador com o executável nativo C++
 */

const WebSocket = require('ws');
const { spawn } = require('child_process');
const express = require('express');
const path = require('path');

const app = express();
const PORT = 8080;

// Serve arquivos estáticos
app.use(express.static(__dirname));
app.use(express.json());

// Leaderboard API
let scores = [];

app.post('/api/scores', (req, res) => {
    const { name, score } = req.body;
    scores.push({ name, score, date: new Date() });
    scores.sort((a, b) => b.score - a.score);
    scores = scores.slice(0, 10); // Top 10
    res.json({ success: true });
});

app.get('/api/scores', (req, res) => {
    res.json(scores);
});

const server = app.listen(PORT, () => {
    console.log(`🌐 Server running on http://localhost:${PORT}`);
});

// WebSocket server
const wss = new WebSocket.Server({ server });

wss.on('connection', (ws) => {
    console.log('🎮 Player connected');
    
    // Spawn C++ game process
    const gameProcess = spawn('./TouhouUltimate', ['--headless', '--websocket']);
    
    gameProcess.stdout.on('data', (data) => {
        // Envia dados do jogo para o cliente
        ws.send(data.toString());
    });
    
    ws.on('message', (message) => {
        const data = JSON.parse(message);
        
        if (data.type === 'input') {
            // Envia input para o processo C++
            gameProcess.stdin.write(JSON.stringify(data.state) + '\n');
        } else if (data.type === 'restart') {
            gameProcess.kill();
            // Respawn...
        }
    });
    
    ws.on('close', () => {
        console.log('👋 Player disconnected');
        gameProcess.kill();
    });
});

console.log('🎮 Touhou Ultimate Server Ready!');
