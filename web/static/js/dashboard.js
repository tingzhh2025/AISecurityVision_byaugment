// AI Security Vision Dashboard JavaScript

class Dashboard {
    constructor() {
        this.apiBase = window.location.origin;
        this.updateInterval = 2000; // 2 seconds
        this.maxDataPoints = 50;
        this.isPaused = false;
        this.charts = {};
        this.data = {
            performance: {
                labels: [],
                cpu: [],
                gpu: [],
                memory: []
            },
            pipelines: {
                labels: [],
                frameRates: [],
                dropRates: []
            }
        };
        
        this.init();
    }

    async init() {
        console.log('Initializing AI Security Vision Dashboard...');
        
        // Initialize charts
        this.initializeCharts();
        
        // Set up event listeners
        this.setupEventListeners();
        
        // Start data updates
        this.startUpdates();
        
        // Hide loading overlay
        setTimeout(() => {
            document.getElementById('loading-overlay').classList.add('hidden');
        }, 1000);
    }

    initializeCharts() {
        // Performance Chart
        const performanceCtx = document.getElementById('performance-chart').getContext('2d');
        this.charts.performance = new Chart(performanceCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'CPU Usage (%)',
                        data: [],
                        borderColor: '#3498db',
                        backgroundColor: 'rgba(52, 152, 219, 0.1)',
                        tension: 0.4,
                        fill: true
                    },
                    {
                        label: 'GPU Usage (%)',
                        data: [],
                        borderColor: '#e74c3c',
                        backgroundColor: 'rgba(231, 76, 60, 0.1)',
                        tension: 0.4,
                        fill: true
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        max: 100,
                        ticks: {
                            callback: function(value) {
                                return value + '%';
                            }
                        }
                    },
                    x: {
                        display: false
                    }
                },
                plugins: {
                    legend: {
                        position: 'top'
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false
                    }
                },
                interaction: {
                    mode: 'nearest',
                    axis: 'x',
                    intersect: false
                }
            }
        });

        // Pipeline Chart
        const pipelineCtx = document.getElementById('pipeline-chart').getContext('2d');
        this.charts.pipeline = new Chart(pipelineCtx, {
            type: 'doughnut',
            data: {
                labels: ['Running', 'Stopped', 'Unhealthy'],
                datasets: [{
                    data: [0, 0, 0],
                    backgroundColor: [
                        '#27ae60',
                        '#95a5a6',
                        '#e74c3c'
                    ],
                    borderWidth: 2,
                    borderColor: '#fff'
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'bottom'
                    },
                    tooltip: {
                        callbacks: {
                            label: function(context) {
                                return context.label + ': ' + context.parsed + ' pipelines';
                            }
                        }
                    }
                }
            }
        });
    }

    setupEventListeners() {
        // Pause/Resume button
        document.getElementById('pause-btn').addEventListener('click', () => {
            this.togglePause();
        });

        // Reset charts button
        document.getElementById('reset-btn').addEventListener('click', () => {
            this.resetCharts();
        });

        // Refresh pipelines button
        document.getElementById('refresh-pipelines').addEventListener('click', () => {
            this.updatePipelineTable();
        });

        // Auto-refresh on window focus
        window.addEventListener('focus', () => {
            if (!this.isPaused) {
                this.updateDashboard();
            }
        });
    }

    startUpdates() {
        // Initial update
        this.updateDashboard();
        
        // Set up interval
        this.updateTimer = setInterval(() => {
            if (!this.isPaused) {
                this.updateDashboard();
            }
        }, this.updateInterval);
    }

    async updateDashboard() {
        try {
            // Update system status
            this.updateSystemStatus('online', 'System Online');
            
            // Fetch and update system stats
            await this.updateSystemStats();
            
            // Fetch and update pipeline stats
            await this.updatePipelineStats();
            
            // Update pipeline table
            await this.updatePipelineTable();
            
            // Update last update time
            document.getElementById('last-update').textContent = new Date().toLocaleTimeString();
            
        } catch (error) {
            console.error('Dashboard update error:', error);
            this.updateSystemStatus('offline', 'Connection Error');
        }
    }

    async updateSystemStats() {
        try {
            const response = await fetch(`${this.apiBase}/api/system/stats`);
            if (!response.ok) throw new Error('Failed to fetch system stats');
            
            const data = await response.json();
            
            // Update overview cards
            this.updateOverviewCards(data);
            
            // Update performance chart
            this.updatePerformanceChart(data);
            
            // Update system info
            this.updateSystemInfo(data);
            
        } catch (error) {
            console.error('Error updating system stats:', error);
        }
    }

    updateOverviewCards(data) {
        const system = data.system || {};
        const resources = data.resources || {};
        const performance = data.performance || {};

        // Active Pipelines
        document.getElementById('active-pipelines').textContent = 
            `${system.running_pipelines || 0} / ${system.total_pipelines || 0}`;
        document.getElementById('pipeline-health').textContent = 
            `${(performance.health_ratio || 0).toFixed(1)}% Healthy`;

        // CPU Usage
        const cpuUsage = resources.cpu_usage || 0;
        document.getElementById('cpu-usage').textContent = `${cpuUsage.toFixed(1)}%`;
        document.getElementById('cpu-progress').style.width = `${cpuUsage}%`;

        // GPU Usage
        const gpuUsage = resources.gpu_utilization || 0;
        document.getElementById('gpu-usage').textContent = `${gpuUsage.toFixed(1)}%`;
        document.getElementById('gpu-memory').textContent = resources.gpu_memory || 'N/A';

        // Frame Rate
        const totalFps = system.total_frame_rate || 0;
        const avgFps = performance.avg_frame_rate || 0;
        document.getElementById('total-fps').textContent = totalFps.toFixed(1);
        document.getElementById('avg-fps').textContent = `Avg: ${avgFps.toFixed(1)} FPS`;
    }

    updatePerformanceChart(data) {
        const resources = data.resources || {};
        const now = new Date().toLocaleTimeString();

        // Add new data point
        this.data.performance.labels.push(now);
        this.data.performance.cpu.push(resources.cpu_usage || 0);
        this.data.performance.gpu.push(resources.gpu_utilization || 0);

        // Limit data points
        if (this.data.performance.labels.length > this.maxDataPoints) {
            this.data.performance.labels.shift();
            this.data.performance.cpu.shift();
            this.data.performance.gpu.shift();
        }

        // Update chart
        this.charts.performance.data.labels = this.data.performance.labels;
        this.charts.performance.data.datasets[0].data = this.data.performance.cpu;
        this.charts.performance.data.datasets[1].data = this.data.performance.gpu;
        this.charts.performance.update('none');
    }

    async updatePipelineStats() {
        try {
            const response = await fetch(`${this.apiBase}/api/system/pipeline-stats`);
            if (!response.ok) throw new Error('Failed to fetch pipeline stats');
            
            const data = await response.json();
            const pipelines = data.pipelines || [];

            // Count pipeline states
            let running = 0, stopped = 0, unhealthy = 0;
            
            pipelines.forEach(pipeline => {
                if (pipeline.is_running) {
                    if (pipeline.is_healthy) {
                        running++;
                    } else {
                        unhealthy++;
                    }
                } else {
                    stopped++;
                }
            });

            // Update pipeline chart
            this.charts.pipeline.data.datasets[0].data = [running, stopped, unhealthy];
            this.charts.pipeline.update();
            
        } catch (error) {
            console.error('Error updating pipeline stats:', error);
        }
    }

    async updatePipelineTable() {
        try {
            const response = await fetch(`${this.apiBase}/api/system/pipeline-stats`);
            if (!response.ok) throw new Error('Failed to fetch pipeline stats');
            
            const data = await response.json();
            const pipelines = data.pipelines || [];
            const tbody = document.getElementById('pipeline-table-body');

            if (pipelines.length === 0) {
                tbody.innerHTML = '<tr><td colspan="9" class="no-data">No active pipelines</td></tr>';
                return;
            }

            tbody.innerHTML = pipelines.map(pipeline => `
                <tr>
                    <td>${pipeline.source_id}</td>
                    <td>${pipeline.protocol.toUpperCase()}</td>
                    <td><span class="status-badge ${pipeline.is_running ? 'status-running' : 'status-stopped'}">
                        ${pipeline.is_running ? 'Running' : 'Stopped'}
                    </span></td>
                    <td><span class="status-badge ${pipeline.is_healthy ? 'status-healthy' : 'status-unhealthy'}">
                        ${pipeline.is_healthy ? 'Healthy' : 'Unhealthy'}
                    </span></td>
                    <td>${pipeline.frame_rate.toFixed(1)} fps</td>
                    <td>${pipeline.processed_frames.toLocaleString()}</td>
                    <td>${pipeline.dropped_frames.toLocaleString()}</td>
                    <td>${this.formatUptime(pipeline.uptime_seconds)}</td>
                    <td>
                        <button class="control-btn" onclick="dashboard.viewPipeline('${pipeline.source_id}')">
                            👁️ View
                        </button>
                    </td>
                </tr>
            `).join('');
            
        } catch (error) {
            console.error('Error updating pipeline table:', error);
        }
    }

    updateSystemInfo(data) {
        const system = data.system || {};
        const performance = data.performance || {};
        const resources = data.resources || {};

        document.getElementById('system-uptime').textContent = 
            this.formatUptime(system.uptime_seconds || 0);
        document.getElementById('total-processed').textContent = 
            `${(system.total_processed_frames || 0).toLocaleString()} frames`;
        document.getElementById('total-dropped').textContent = 
            `${(system.total_dropped_frames || 0).toLocaleString()} frames`;
        document.getElementById('drop-rate').textContent = 
            `${(performance.drop_rate || 0).toFixed(2)}%`;
        document.getElementById('gpu-temperature').textContent = 
            `${(resources.gpu_temperature || 0).toFixed(1)}°C`;
    }

    updateSystemStatus(status, text) {
        const indicator = document.getElementById('system-status');
        const statusText = document.getElementById('system-status-text');
        
        indicator.className = `status-indicator ${status}`;
        statusText.textContent = text;
    }

    togglePause() {
        this.isPaused = !this.isPaused;
        const btn = document.getElementById('pause-btn');
        btn.textContent = this.isPaused ? '▶️ Resume' : '⏸️ Pause';
        btn.style.background = this.isPaused ? '#27ae60' : '#3498db';
    }

    resetCharts() {
        // Clear data
        this.data.performance.labels = [];
        this.data.performance.cpu = [];
        this.data.performance.gpu = [];

        // Update charts
        this.charts.performance.data.labels = [];
        this.charts.performance.data.datasets[0].data = [];
        this.charts.performance.data.datasets[1].data = [];
        this.charts.performance.update();

        console.log('Charts reset');
    }

    viewPipeline(sourceId) {
        // TODO: Implement pipeline detail view
        alert(`Pipeline details for: ${sourceId}\n\nThis feature will be implemented in a future update.`);
    }

    formatUptime(seconds) {
        if (seconds < 60) return `${seconds.toFixed(0)}s`;
        if (seconds < 3600) return `${(seconds / 60).toFixed(1)}m`;
        if (seconds < 86400) return `${(seconds / 3600).toFixed(1)}h`;
        return `${(seconds / 86400).toFixed(1)}d`;
    }
}

// Initialize dashboard when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.dashboard = new Dashboard();
});

// Handle page visibility changes
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        console.log('Dashboard hidden - pausing updates');
    } else {
        console.log('Dashboard visible - resuming updates');
        if (window.dashboard && !window.dashboard.isPaused) {
            window.dashboard.updateDashboard();
        }
    }
});
