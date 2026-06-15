/*
 * 网页界面 - 冰箱PID控制系统
 * 嵌入式HTML/JS/CSS字符串
 * 仅显示冷冻室功能（与Wokwi仿真匹配）
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>

const char HTML_PAGE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>冰箱PID控制 - 仿真监控</title>
    <script src="https://cdn.plot.ly/plotly-2.24.1.min.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            color: #fff;
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 900px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            padding: 20px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 10px;
            margin-bottom: 20px;
        }
        
        .header h1 {
            font-size: 28px;
            margin-bottom: 10px;
        }
        
        .status-bar {
            display: flex;
            justify-content: space-between;
            font-size: 14px;
            opacity: 0.9;
        }
        
        .card {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
        }
        
        .card h2 {
            font-size: 20px;
            margin-bottom: 15px;
            border-bottom: 2px solid rgba(255, 255, 255, 0.2);
            padding-bottom: 10px;
        }
        
        .temp-display {
            text-align: center;
            padding: 20px;
        }
        
        .temp-value {
            font-size: 64px;
            font-weight: bold;
            color: #00ff88;
            text-shadow: 0 0 20px rgba(0, 255, 136, 0.5);
        }
        
        .temp-label {
            font-size: 18px;
            opacity: 0.8;
            margin-top: 10px;
        }
        
        .info-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
            margin-top: 20px;
        }
        
        .info-item {
            background: rgba(0, 0, 0, 0.2);
            padding: 15px;
            border-radius: 8px;
        }
        
        .info-label {
            font-size: 14px;
            opacity: 0.7;
            margin-bottom: 5px;
        }
        
        .info-value {
            font-size: 24px;
            font-weight: bold;
        }
        
        .control-group {
            margin-bottom: 20px;
        }
        
        .control-group label {
            display: block;
            margin-bottom: 8px;
            font-size: 16px;
        }
        
        .control-group input {
            width: 100%;
            padding: 10px;
            border: none;
            border-radius: 5px;
            background: rgba(255, 255, 255, 0.9);
            color: #333;
            font-size: 16px;
        }
        
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            cursor: pointer;
            transition: all 0.3s;
            margin: 5px;
        }
        
        .btn-primary {
            background: #007bff;
            color: white;
        }
        
        .btn-primary:hover {
            background: #0056b3;
        }
        
        .btn-mode {
            background: rgba(255, 255, 255, 0.2);
            color: white;
            min-width: 120px;
        }
        
        .btn-mode.active {
            background: #00ff88;
            color: #000;
        }
        
        .btn-mode:hover {
            background: rgba(255, 255, 255, 0.3);
        }
        
        .mode-buttons {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
        }
        
        .pid-inputs {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
            margin-bottom: 15px;
        }
        
        .chart-container {
            width: 100%;
            height: 400px;
        }
        
        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 5px;
        }
        
        .status-on {
            background: #00ff88;
            box-shadow: 0 0 10px #00ff88;
        }
        
        .status-off {
            background: #ff4444;
        }
        
        .message {
            padding: 10px;
            margin: 10px 0;
            border-radius: 5px;
            text-align: center;
            display: none;
        }
        
        .message.success {
            background: rgba(0, 255, 136, 0.2);
            color: #00ff88;
            display: block;
        }
        
        .message.error {
            background: rgba(255, 68, 68, 0.2);
            color: #ff4444;
            display: block;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🧊 冰箱PID控制系统 - 仿真监控</h1>
            <div class="status-bar">
                <span>运行时间: <span id="uptime">0</span>s</span>
                <span>状态: <span id="connection-status">连接中...</span></span>
            </div>
        </div>
        
        <div class="card">
            <h2>📊 实时状态</h2>
            <div class="temp-display">
                <div class="temp-value" id="freezer-temp">-18.0</div>
                <div class="temp-label">冷冻室温度 (°C)</div>
            </div>
            <div class="info-grid">
                <div class="info-item">
                    <div class="info-label">设定温度</div>
                    <div class="info-value" id="setpoint">-18.0 °C</div>
                </div>
                <div class="info-item">
                    <div class="info-label">压缩机</div>
                    <div class="info-value">
                        <span class="status-indicator" id="compressor-status"></span>
                        <span id="compressor-text">OFF</span>
                    </div>
                </div>
                <div class="info-item">
                    <div class="info-label">系统模式</div>
                    <div class="info-value" id="mode-text">COOLING</div>
                </div>
                <div class="info-item">
                    <div class="info-label">PID输出</div>
                    <div class="info-value" id="pid-output">0</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>🎯 温度设置</h2>
            <div class="control-group">
                <label>冷冻室设定温度 (°C)</label>
                <input type="number" id="setpoint-input" step="0.5" value="-18.0">
            </div>
            <button class="btn btn-primary" onclick="setSetpoint()">应用设定</button>
            <div class="message" id="setpoint-message"></div>
        </div>
        
        <div class="card">
            <h2>⚙️ 系统模式</h2>
            <div class="mode-buttons">
                <button class="btn btn-mode" data-mode="0" onclick="setMode(0)">❄️ 制冷</button>
                <button class="btn btn-mode" data-mode="1" onclick="setMode(1)">🔥 除霜</button>
                <button class="btn btn-mode" data-mode="2" onclick="setMode(2)">🌿 节能</button>
                <button class="btn btn-mode" data-mode="3" onclick="setMode(3)">⚠️ 故障</button>
            </div>
            <div class="message" id="mode-message"></div>
        </div>
        
        <div class="card">
            <h2>🔧 PID参数</h2>
            <div class="pid-inputs">
                <div class="control-group">
                    <label>Kp (比例)</label>
                    <input type="number" id="kp-input" step="0.1" value="2.0">
                </div>
                <div class="control-group">
                    <label>Ki (积分)</label>
                    <input type="number" id="ki-input" step="0.1" value="5.0">
                </div>
                <div class="control-group">
                    <label>Kd (微分)</label>
                    <input type="number" id="kd-input" step="0.1" value="1.0">
                </div>
            </div>
            <button class="btn btn-primary" onclick="setPID()">应用PID参数</button>
            <div class="message" id="pid-message"></div>
        </div>
        
        <div class="card">
            <h2>📈 温度曲线</h2>
            <div id="temp-chart" class="chart-container"></div>
        </div>
    </div>
    
    <script>
        // ===== 全局变量 =====
        let updateInterval = null;
        let chartInterval = null;
        let tempHistory = [];
        let timeHistory = [];
        let isUpdating = false;
        let chartReady = false;
        
        let layout = {
            title: '温度变化曲线',
            xaxis: { title: '时间', gridcolor: 'rgba(255,255,255,0.1)', color: '#fff' },
            yaxis: { title: '温度 (°C)', gridcolor: 'rgba(255,255,255,0.1)', color: '#fff' },
            plot_bgcolor: 'rgba(0,0,0,0)', paper_bgcolor: 'rgba(0,0,0,0)',
            font: { color: '#fff' }, margin: { t: 40, r: 20, b: 40, l: 50 }
        };

        // ===== 等待 Plotly 加载后初始化图表 =====
        function waitForPlotly(callback, maxRetries) {
            maxRetries = maxRetries || 50;
            var retries = 0;
            function check() {
                if (typeof Plotly !== 'undefined') {
                    callback();
                } else if (retries < maxRetries) {
                    retries++;
                    setTimeout(check, 200);
                }
            }
            check();
        }

        function initChart() {
            Plotly.newPlot('temp-chart', [{
                x: [], y: [],
                type: 'scatter', name: '冷冻室温度',
                line: { color: '#00ff88', width: 2 },
                mode: 'lines+markers', marker: { size: 3 }
            }], layout, { responsive: true });
            chartReady = true;
        }

        // ===== 更新图表（每 5 秒调用一次，降低频率避免浏览器内存泄漏） =====
        function updateChart() {
            if (!chartReady) return;
            // 限制最多 60 个点
            if (tempHistory.length > 60) {
                tempHistory = tempHistory.slice(-60);
                timeHistory = timeHistory.slice(-60);
            }
            Plotly.react('temp-chart', [{
                x: timeHistory.slice(),
                y: tempHistory.slice(),
                type: 'scatter', name: '冷冻室温度',
                line: { color: '#00ff88', width: 2 },
                mode: 'lines+markers', marker: { size: 3 }
            }], layout);
        }

        // ===== 获取系统状态（每 2 秒调用一次） =====
        async function getStatus() {
            if (isUpdating) return;
            isUpdating = true;
            try {
                var ctrl = new AbortController();
                var tid = setTimeout(function(){ ctrl.abort(); }, 5000);
                var resp = await fetch('/api/status', { signal: ctrl.signal });
                clearTimeout(tid);
                
                if (!resp.ok) throw new Error('HTTP ' + resp.status);
                var data = await resp.json();

                // 更新UI
                document.getElementById('freezer-temp').textContent = data.freezer_temp.toFixed(1);
                document.getElementById('setpoint').textContent = data.freezer_setpoint.toFixed(1) + ' °C';
                document.getElementById('compressor-text').textContent = data.compressor;
                var cs = document.getElementById('compressor-status');
                cs.className = 'status-indicator ' + (data.compressor === 'ON' ? 'status-on' : 'status-off');
                document.getElementById('mode-text').textContent = data.mode;
                document.getElementById('pid-output').textContent = data.pid_output.toFixed(0);
                document.getElementById('uptime').textContent = data.uptime;

                var modeNames = ['COOLING', 'DEFROST', 'ECO', 'ERROR'];
                document.querySelectorAll('.btn-mode').forEach(function(btn){
                    var m = parseInt(btn.dataset.mode);
                    btn.classList.toggle('active', modeNames[m] === data.mode);
                });

                // 记录历史数据
                var now = new Date();
                tempHistory.push(data.freezer_temp);
                timeHistory.push(
                    String(now.getHours()).padStart(2,'0') + ':' +
                    String(now.getMinutes()).padStart(2,'0') + ':' +
                    String(now.getSeconds()).padStart(2,'0'));

                document.getElementById('connection-status').textContent = '已连接';
                document.getElementById('connection-status').style.color = '#00ff88';

            } catch (e) {
                document.getElementById('connection-status').textContent = '连接失败';
                document.getElementById('connection-status').style.color = '#ff4444';
            } finally {
                isUpdating = false;
            }
        }

        // ===== 设置温度 =====
        async function setSetpoint() {
            var v = parseFloat(document.getElementById('setpoint-input').value);
            try {
                var resp = await fetch('/api/setpoint', {
                    method: 'POST', headers: {'Content-Type':'application/json'},
                    body: JSON.stringify({zone:'freezer', value:v})
                });
                var data = await resp.json();
                showMsg('setpoint-message', data.success ? '设置成功' : '设置失败', data.success);
            } catch(e) { showMsg('setpoint-message', '通信失败', false); }
        }

        // ===== 设置模式 =====
        async function setMode(mode) {
            try {
                var resp = await fetch('/api/mode', {
                    method: 'POST', headers: {'Content-Type':'application/json'},
                    body: JSON.stringify({mode:mode})
                });
                var data = await resp.json();
                showMsg('mode-message', data.success ? '设置成功' : '设置失败', data.success);
            } catch(e) { showMsg('mode-message', '通信失败', false); }
        }

        // ===== 设置PID =====
        async function setPID() {
            var kp = parseFloat(document.getElementById('kp-input').value);
            var ki = parseFloat(document.getElementById('ki-input').value);
            var kd = parseFloat(document.getElementById('kd-input').value);
            try {
                var resp = await fetch('/api/pid', {
                    method: 'POST', headers: {'Content-Type':'application/json'},
                    body: JSON.stringify({kp:kp, ki:ki, kd:kd})
                });
                var data = await resp.json();
                showMsg('pid-message', data.success ? '设置成功' : '设置失败', data.success);
            } catch(e) { showMsg('pid-message', '通信失败', false); }
        }

        // ===== 消息提示 =====
        function showMsg(id, text, ok) {
            var el = document.getElementById(id);
            el.textContent = text;
            el.className = 'message ' + (ok ? 'success' : 'error');
            setTimeout(function(){ el.className = 'message'; }, 3000);
        }

        // ===== 统一的页面初始化（window.onload 确保 Plotly CDN 已加载） =====
        window.onload = function() {
            // 等待 Plotly 可用后初始化图表
            waitForPlotly(initChart);

            // 立即获取一次状态
            getStatus();

            // 数据采集：每 2 秒（降低请求频率）
            updateInterval = setInterval(getStatus, 2000);

            // 图表渲染：每 5 秒（大幅降低 GPU 渲染频率）
            chartInterval = setInterval(updateChart, 5000);
        };

        // ===== 页面卸载时清理所有定时器 =====
        window.addEventListener('beforeunload', function() {
            if (updateInterval) { clearInterval(updateInterval); updateInterval = null; }
            if (chartInterval) { clearInterval(chartInterval); chartInterval = null; }
            if (chartReady) {
                try { Plotly.purge('temp-chart'); } catch(e) {}
                chartReady = false;
            }
        });
    </script>
</body>
</html>
)=====";

#endif
