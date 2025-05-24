// ROI Manager JavaScript - HTML5 Canvas Polygon Drawing
class ROIManager {
    constructor() {
        this.canvas = null;
        this.ctx = null;
        this.isDrawing = false;
        this.currentPolygon = [];
        this.savedROIs = [];
        this.selectedROI = null;
        this.drawMode = false;

        // Canvas settings
        this.pointRadius = 6;
        this.lineWidth = 2;
        this.colors = {
            point: '#FF4444',
            line: '#4444FF',
            polygon: 'rgba(68, 68, 255, 0.2)',
            selected: '#FF8800',
            grid: 'rgba(255, 255, 255, 0.1)'
        };

        this.init();
    }

    init() {
        this.setupCanvas();
        this.setupEventListeners();
        this.loadCameras();
        this.loadROIs();
        this.updateSystemStatus();

        // Hide loading overlay
        setTimeout(() => {
            document.getElementById('loading-overlay').style.display = 'none';
        }, 1000);
    }

    setupCanvas() {
        this.canvas = document.getElementById('roi-canvas');
        this.ctx = this.canvas.getContext('2d');

        // Set canvas size
        this.canvas.width = 1280;
        this.canvas.height = 720;

        this.drawGrid();
        this.updateCanvasInfo();
    }

    setupEventListeners() {
        // Canvas events
        this.canvas.addEventListener('click', (e) => this.handleCanvasClick(e));
        this.canvas.addEventListener('mousemove', (e) => this.handleCanvasMouseMove(e));
        this.canvas.addEventListener('contextmenu', (e) => e.preventDefault());

        // Control buttons
        document.getElementById('draw-mode-btn').addEventListener('click', () => this.toggleDrawMode());
        document.getElementById('clear-canvas').addEventListener('click', () => this.clearCanvas());
        document.getElementById('undo-point').addEventListener('click', () => this.undoLastPoint());
        document.getElementById('complete-polygon').addEventListener('click', () => this.completePolygon());

        // Camera controls
        document.getElementById('refresh-cameras').addEventListener('click', () => this.loadCameras());
        document.getElementById('load-stream').addEventListener('click', () => this.loadVideoStream());

        // ROI form
        document.getElementById('save-roi').addEventListener('click', () => this.saveROI());
        document.getElementById('cancel-roi').addEventListener('click', () => this.cancelROI());
        document.getElementById('refresh-rois').addEventListener('click', () => this.loadROIs());

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => this.handleKeyboard(e));
    }

    handleCanvasClick(e) {
        if (!this.drawMode) return;

        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        // Scale coordinates to canvas size
        const canvasX = (x / rect.width) * this.canvas.width;
        const canvasY = (y / rect.height) * this.canvas.height;

        // Check if clicking near the first point to close polygon
        if (this.currentPolygon.length > 2) {
            const firstPoint = this.currentPolygon[0];
            const distance = Math.sqrt(
                Math.pow(canvasX - firstPoint.x, 2) + Math.pow(canvasY - firstPoint.y, 2)
            );

            if (distance < this.pointRadius * 2) {
                this.completePolygon();
                return;
            }
        }

        // Add new point
        this.currentPolygon.push({ x: canvasX, y: canvasY });
        this.redrawCanvas();
        this.updateCanvasInfo();
    }

    handleCanvasMouseMove(e) {
        if (!this.drawMode || this.currentPolygon.length === 0) return;

        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        const canvasX = (x / rect.width) * this.canvas.width;
        const canvasY = (y / rect.height) * this.canvas.height;

        // Redraw with preview line
        this.redrawCanvas();

        // Draw preview line from last point to mouse
        if (this.currentPolygon.length > 0) {
            const lastPoint = this.currentPolygon[this.currentPolygon.length - 1];
            this.ctx.strokeStyle = this.colors.line;
            this.ctx.lineWidth = 1;
            this.ctx.setLineDash([5, 5]);
            this.ctx.beginPath();
            this.ctx.moveTo(lastPoint.x, lastPoint.y);
            this.ctx.lineTo(canvasX, canvasY);
            this.ctx.stroke();
            this.ctx.setLineDash([]);
        }
    }

    toggleDrawMode() {
        this.drawMode = !this.drawMode;
        const btn = document.getElementById('draw-mode-btn');
        const modeSpan = document.getElementById('canvas-mode');

        if (this.drawMode) {
            btn.textContent = '👁️ View Mode';
            btn.classList.add('primary');
            this.canvas.classList.add('draw-mode');
            this.canvas.classList.remove('view-mode');
            modeSpan.textContent = 'Draw';
        } else {
            btn.textContent = '✏️ Draw Mode';
            btn.classList.remove('primary');
            this.canvas.classList.add('view-mode');
            this.canvas.classList.remove('draw-mode');
            modeSpan.textContent = 'View';
            this.currentPolygon = [];
            this.redrawCanvas();
        }

        this.updateCanvasInfo();
    }

    clearCanvas() {
        this.currentPolygon = [];
        this.selectedROI = null;
        this.redrawCanvas();
        this.updateCanvasInfo();
    }

    undoLastPoint() {
        if (this.currentPolygon.length > 0) {
            this.currentPolygon.pop();
            this.redrawCanvas();
            this.updateCanvasInfo();
        }
    }

    completePolygon() {
        if (this.currentPolygon.length < 3) {
            alert('A polygon must have at least 3 points');
            return;
        }

        // Enable form for configuration
        this.enableROIForm();
        this.drawMode = false;
        this.toggleDrawMode();
    }

    drawGrid() {
        this.ctx.strokeStyle = this.colors.grid;
        this.ctx.lineWidth = 1;

        // Draw grid lines
        const gridSize = 40;
        for (let x = 0; x <= this.canvas.width; x += gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, this.canvas.height);
            this.ctx.stroke();
        }

        for (let y = 0; y <= this.canvas.height; y += gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(0, y);
            this.ctx.lineTo(this.canvas.width, y);
            this.ctx.stroke();
        }
    }

    redrawCanvas() {
        // Clear canvas
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        // Draw grid
        this.drawGrid();

        // Draw saved ROIs
        this.savedROIs.forEach((roi, index) => {
            this.drawPolygon(roi.polygon, roi === this.selectedROI, roi.name);
        });

        // Draw current polygon being drawn
        if (this.currentPolygon.length > 0) {
            this.drawCurrentPolygon();
        }
    }

    drawPolygon(points, isSelected = false, label = '') {
        if (points.length < 2) return;

        const color = isSelected ? this.colors.selected : this.colors.line;

        // Draw polygon fill
        if (points.length > 2) {
            this.ctx.fillStyle = isSelected ?
                'rgba(255, 136, 0, 0.2)' : this.colors.polygon;
            this.ctx.beginPath();
            this.ctx.moveTo(points[0].x, points[0].y);
            for (let i = 1; i < points.length; i++) {
                this.ctx.lineTo(points[i].x, points[i].y);
            }
            this.ctx.closePath();
            this.ctx.fill();
        }

        // Draw polygon outline
        this.ctx.strokeStyle = color;
        this.ctx.lineWidth = isSelected ? 3 : this.lineWidth;
        this.ctx.beginPath();
        this.ctx.moveTo(points[0].x, points[0].y);
        for (let i = 1; i < points.length; i++) {
            this.ctx.lineTo(points[i].x, points[i].y);
        }
        if (points.length > 2) {
            this.ctx.closePath();
        }
        this.ctx.stroke();

        // Draw points
        this.ctx.fillStyle = color;
        points.forEach((point, index) => {
            this.ctx.beginPath();
            this.ctx.arc(point.x, point.y, this.pointRadius, 0, 2 * Math.PI);
            this.ctx.fill();

            // Draw point number
            this.ctx.fillStyle = 'white';
            this.ctx.font = '12px Arial';
            this.ctx.textAlign = 'center';
            this.ctx.fillText(index + 1, point.x, point.y + 4);
            this.ctx.fillStyle = color;
        });

        // Draw label
        if (label && points.length > 0) {
            this.ctx.fillStyle = color;
            this.ctx.font = '14px Arial';
            this.ctx.textAlign = 'left';
            this.ctx.fillText(label, points[0].x + 10, points[0].y - 10);
        }
    }

    drawCurrentPolygon() {
        if (this.currentPolygon.length === 0) return;

        // Draw lines between points
        this.ctx.strokeStyle = this.colors.line;
        this.ctx.lineWidth = this.lineWidth;
        this.ctx.beginPath();
        this.ctx.moveTo(this.currentPolygon[0].x, this.currentPolygon[0].y);
        for (let i = 1; i < this.currentPolygon.length; i++) {
            this.ctx.lineTo(this.currentPolygon[i].x, this.currentPolygon[i].y);
        }
        this.ctx.stroke();

        // Draw points
        this.ctx.fillStyle = this.colors.point;
        this.currentPolygon.forEach((point, index) => {
            this.ctx.beginPath();
            this.ctx.arc(point.x, point.y, this.pointRadius, 0, 2 * Math.PI);
            this.ctx.fill();

            // Draw point number
            this.ctx.fillStyle = 'white';
            this.ctx.font = '12px Arial';
            this.ctx.textAlign = 'center';
            this.ctx.fillText(index + 1, point.x, point.y + 4);
            this.ctx.fillStyle = this.colors.point;
        });

        // Highlight first point if we can close the polygon
        if (this.currentPolygon.length > 2) {
            this.ctx.strokeStyle = this.colors.point;
            this.ctx.lineWidth = 3;
            this.ctx.beginPath();
            this.ctx.arc(this.currentPolygon[0].x, this.currentPolygon[0].y,
                        this.pointRadius + 3, 0, 2 * Math.PI);
            this.ctx.stroke();
        }
    }

    updateCanvasInfo() {
        document.getElementById('canvas-resolution').textContent =
            `${this.canvas.width}x${this.canvas.height}`;
        document.getElementById('point-count').textContent = this.currentPolygon.length;
    }

    enableROIForm() {
        const form = document.getElementById('roi-form');
        form.style.display = 'grid';

        // Generate default name
        const roiCount = this.savedROIs.length + 1;
        document.getElementById('roi-name').value = `ROI_${roiCount}`;
    }

    async loadCameras() {
        try {
            const response = await fetch('/api/sources');
            const data = await response.json();

            const select = document.getElementById('camera-select');
            select.innerHTML = '<option value="">Select a camera...</option>';

            if (data.sources && data.sources.length > 0) {
                data.sources.forEach(source => {
                    const option = document.createElement('option');
                    option.value = source.id;
                    option.textContent = `${source.id} (${source.protocol})`;
                    select.appendChild(option);
                });
            }
        } catch (error) {
            console.error('Failed to load cameras:', error);
            this.showNotification('Failed to load cameras', 'error');
        }
    }

    async loadVideoStream() {
        const cameraId = document.getElementById('camera-select').value;
        if (!cameraId) {
            alert('Please select a camera first');
            return;
        }

        try {
            // Get stream configuration
            const response = await fetch(`/api/stream/config?camera_id=${cameraId}`);
            const data = await response.json();

            if (data.stream_url) {
                // Load stream as background (for reference)
                const img = document.getElementById('video-stream');
                img.src = data.stream_url;
                img.style.display = 'block';

                // Position canvas over the image
                this.canvas.style.position = 'absolute';
                this.canvas.style.top = '0';
                this.canvas.style.left = '0';

                this.showNotification('Video stream loaded successfully', 'success');
            }
        } catch (error) {
            console.error('Failed to load video stream:', error);
            this.showNotification('Failed to load video stream', 'error');
        }
    }

    async saveROI() {
        if (this.currentPolygon.length < 3) {
            alert('Please draw a polygon with at least 3 points');
            return;
        }

        const cameraId = document.getElementById('camera-select').value;
        if (!cameraId) {
            alert('Please select a camera first');
            return;
        }

        const roiData = {
            id: `roi_${Date.now()}`,
            name: document.getElementById('roi-name').value,
            type: document.getElementById('roi-type').value,
            priority: parseInt(document.getElementById('roi-priority').value),
            min_duration: parseFloat(document.getElementById('min-duration').value),
            confidence: parseFloat(document.getElementById('confidence-threshold').value),
            enabled: document.getElementById('roi-enabled').checked,
            polygon: this.currentPolygon.map(point => ({
                x: Math.round(point.x),
                y: Math.round(point.y)
            })),
            camera_id: cameraId
        };

        try {
            // Create intrusion rule with ROI
            const ruleData = {
                id: `rule_${Date.now()}`,
                roi: {
                    id: roiData.id,
                    name: roiData.name,
                    polygon: roiData.polygon,
                    enabled: roiData.enabled,
                    priority: roiData.priority
                },
                min_duration: roiData.min_duration,
                confidence: roiData.confidence,
                enabled: roiData.enabled
            };

            const response = await fetch('/api/rules', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(ruleData)
            });

            if (response.ok) {
                // Add to saved ROIs
                this.savedROIs.push({
                    ...roiData,
                    polygon: this.currentPolygon.slice()
                });

                // Clear current polygon
                this.currentPolygon = [];
                this.redrawCanvas();
                this.updateCanvasInfo();

                // Reset form
                this.resetROIForm();

                // Refresh ROI list
                this.loadROIs();

                this.showNotification('ROI saved successfully', 'success');
            } else {
                throw new Error('Failed to save ROI');
            }
        } catch (error) {
            console.error('Failed to save ROI:', error);
            this.showNotification('Failed to save ROI', 'error');
        }
    }

    cancelROI() {
        this.currentPolygon = [];
        this.redrawCanvas();
        this.updateCanvasInfo();
        this.resetROIForm();
    }

    resetROIForm() {
        document.getElementById('roi-form').reset();
        document.getElementById('roi-enabled').checked = true;
        document.getElementById('roi-priority').value = '1';
        document.getElementById('min-duration').value = '5.0';
        document.getElementById('confidence-threshold').value = '0.7';
    }

    async loadROIs() {
        try {
            const response = await fetch('/api/rules');
            const data = await response.json();

            this.savedROIs = [];
            const roiList = document.getElementById('roi-list');
            roiList.innerHTML = '';

            if (data.rules && data.rules.length > 0) {
                data.rules.forEach(rule => {
                    if (rule.roi && rule.roi.polygon) {
                        const roi = {
                            id: rule.roi.id,
                            name: rule.roi.name,
                            type: 'intrusion',
                            priority: rule.roi.priority || 1,
                            enabled: rule.roi.enabled,
                            polygon: rule.roi.polygon,
                            rule_id: rule.id
                        };

                        this.savedROIs.push(roi);
                        this.createROIListItem(roi);
                    }
                });

                this.redrawCanvas();
            } else {
                roiList.innerHTML = '<div class="no-rois">No ROIs configured</div>';
            }
        } catch (error) {
            console.error('Failed to load ROIs:', error);
            this.showNotification('Failed to load ROIs', 'error');
        }
    }

    createROIListItem(roi) {
        const roiList = document.getElementById('roi-list');
        const item = document.createElement('div');
        item.className = `roi-item ${roi.enabled ? 'active' : ''}`;
        item.innerHTML = `
            <div class="roi-header">
                <span class="roi-name">${roi.name}</span>
                <span class="roi-type">${roi.type}</span>
            </div>
            <div class="roi-details">
                <div class="roi-detail">
                    <span>Priority:</span>
                    <span>${roi.priority}</span>
                </div>
                <div class="roi-detail">
                    <span>Points:</span>
                    <span>${roi.polygon.length}</span>
                </div>
                <div class="roi-detail">
                    <span>Status:</span>
                    <span>${roi.enabled ? 'Enabled' : 'Disabled'}</span>
                </div>
            </div>
            <div class="roi-actions">
                <button class="control-btn" onclick="roiManager.selectROI('${roi.id}')">👁️ View</button>
                <button class="control-btn danger" onclick="roiManager.deleteROI('${roi.id}')">🗑️ Delete</button>
            </div>
        `;

        roiList.appendChild(item);
    }

    selectROI(roiId) {
        this.selectedROI = this.savedROIs.find(roi => roi.id === roiId);
        this.redrawCanvas();
    }

    async deleteROI(roiId) {
        if (!confirm('Are you sure you want to delete this ROI?')) {
            return;
        }

        const roi = this.savedROIs.find(r => r.id === roiId);
        if (!roi) return;

        try {
            const response = await fetch(`/api/rules/${roi.rule_id}`, {
                method: 'DELETE'
            });

            if (response.ok) {
                this.savedROIs = this.savedROIs.filter(r => r.id !== roiId);
                this.selectedROI = null;
                this.redrawCanvas();
                this.loadROIs();
                this.showNotification('ROI deleted successfully', 'success');
            } else {
                throw new Error('Failed to delete ROI');
            }
        } catch (error) {
            console.error('Failed to delete ROI:', error);
            this.showNotification('Failed to delete ROI', 'error');
        }
    }

    handleKeyboard(e) {
        if (e.key === 'Escape') {
            if (this.drawMode) {
                this.toggleDrawMode();
            }
        } else if (e.key === 'Enter' && this.drawMode) {
            this.completePolygon();
        } else if (e.key === 'Backspace' && this.drawMode) {
            e.preventDefault();
            this.undoLastPoint();
        }
    }

    async updateSystemStatus() {
        try {
            const response = await fetch('/api/system/status');
            const data = await response.json();

            const statusElement = document.getElementById('system-status');
            const statusText = document.getElementById('system-status-text');

            if (data.status === 'running') {
                statusElement.style.color = '#4CAF50';
                statusText.textContent = 'System Online';
            } else {
                statusElement.style.color = '#F44336';
                statusText.textContent = 'System Offline';
            }
        } catch (error) {
            const statusElement = document.getElementById('system-status');
            const statusText = document.getElementById('system-status-text');
            statusElement.style.color = '#FF9800';
            statusText.textContent = 'Connection Error';
        }
    }

    showNotification(message, type = 'info') {
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.textContent = message;
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 15px 20px;
            border-radius: 8px;
            color: white;
            font-weight: 500;
            z-index: 1000;
            transition: all 0.3s ease;
            background: ${type === 'success' ? '#4CAF50' : type === 'error' ? '#F44336' : '#2196F3'};
        `;

        document.body.appendChild(notification);

        // Remove after 3 seconds
        setTimeout(() => {
            notification.style.opacity = '0';
            setTimeout(() => {
                document.body.removeChild(notification);
            }, 300);
        }, 3000);
    }
}

// Initialize ROI Manager when page loads
let roiManager;
document.addEventListener('DOMContentLoaded', () => {
    roiManager = new ROIManager();
});