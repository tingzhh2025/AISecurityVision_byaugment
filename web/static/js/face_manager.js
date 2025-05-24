// Face Manager JavaScript

class FaceManager {
    constructor() {
        this.apiBase = window.location.origin;
        this.faces = [];
        this.selectedFile = null;
        this.deleteTargetId = null;
        
        this.init();
    }

    async init() {
        console.log('Initializing Face Manager...');
        
        // Set up event listeners
        this.setupEventListeners();
        
        // Load initial data
        await this.loadFaces();
        
        // Update system status
        this.updateSystemStatus('online', 'System Online');
    }

    setupEventListeners() {
        // Form submission
        document.getElementById('face-upload-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.handleFormSubmit();
        });

        // File input change
        document.getElementById('face-image').addEventListener('change', (e) => {
            this.handleFileSelect(e.target.files[0]);
        });

        // Drag and drop
        const uploadArea = document.getElementById('file-upload-area');
        uploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadArea.classList.add('dragover');
        });

        uploadArea.addEventListener('dragleave', () => {
            uploadArea.classList.remove('dragover');
        });

        uploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadArea.classList.remove('dragover');
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                this.handleFileSelect(files[0]);
            }
        });

        // Clear form button
        document.getElementById('clear-form').addEventListener('click', () => {
            this.clearForm();
        });

        // Refresh faces button
        document.getElementById('refresh-faces').addEventListener('click', () => {
            this.loadFaces();
        });

        // Modal controls
        document.getElementById('modal-close').addEventListener('click', () => {
            this.hideDeleteModal();
        });

        document.getElementById('cancel-delete').addEventListener('click', () => {
            this.hideDeleteModal();
        });

        document.getElementById('confirm-delete').addEventListener('click', () => {
            this.confirmDelete();
        });

        // Close modal on background click
        document.getElementById('delete-modal').addEventListener('click', (e) => {
            if (e.target.id === 'delete-modal') {
                this.hideDeleteModal();
            }
        });
    }

    handleFileSelect(file) {
        if (!file) return;

        // Validate file type
        const validTypes = ['image/jpeg', 'image/jpg', 'image/png', 'image/bmp'];
        if (!validTypes.includes(file.type)) {
            this.showNotification('Please select a valid image file (JPG, PNG, BMP)', 'error');
            return;
        }

        // Validate file size (10MB max)
        const maxSize = 10 * 1024 * 1024;
        if (file.size > maxSize) {
            this.showNotification('File size must be less than 10MB', 'error');
            return;
        }

        this.selectedFile = file;
        this.updateFileInput();
        this.showImagePreview(file);
        this.validateForm();
    }

    updateFileInput() {
        const fileInput = document.getElementById('face-image');
        const uploadArea = document.getElementById('file-upload-area');
        
        if (this.selectedFile) {
            uploadArea.innerHTML = `
                <div class="upload-prompt">
                    <div class="upload-icon">✅</div>
                    <div class="upload-text">
                        <strong>${this.selectedFile.name}</strong>
                    </div>
                    <div class="upload-hint">Click to change image</div>
                </div>
            `;
        }
    }

    showImagePreview(file) {
        const preview = document.getElementById('image-preview');
        const previewInfo = document.getElementById('preview-info');
        
        const reader = new FileReader();
        reader.onload = (e) => {
            preview.innerHTML = `<img src="${e.target.result}" alt="Preview">`;
            
            // Show file info
            const sizeKB = Math.round(file.size / 1024);
            previewInfo.textContent = `${file.name} (${sizeKB} KB)`;
        };
        reader.readAsDataURL(file);
    }

    validateForm() {
        const nameInput = document.getElementById('face-name');
        const uploadBtn = document.getElementById('upload-btn');
        
        const isValid = nameInput.value.trim() && this.selectedFile;
        uploadBtn.disabled = !isValid;
    }

    async handleFormSubmit() {
        const nameInput = document.getElementById('face-name');
        const uploadBtn = document.getElementById('upload-btn');
        const btnText = uploadBtn.querySelector('.btn-text');
        const btnSpinner = uploadBtn.querySelector('.btn-spinner');
        
        if (!this.selectedFile || !nameInput.value.trim()) {
            this.showNotification('Please fill in all fields', 'error');
            return;
        }

        // Show loading state
        uploadBtn.disabled = true;
        btnText.style.display = 'none';
        btnSpinner.style.display = 'inline';
        this.updateUploadStatus('Uploading...');

        try {
            const formData = new FormData();
            formData.append('name', nameInput.value.trim());
            formData.append('image', this.selectedFile);

            const response = await fetch(`${this.apiBase}/api/faces/add`, {
                method: 'POST',
                body: formData
            });

            const result = await response.json();

            if (response.ok) {
                this.showNotification(`Face added successfully: ${result.name}`, 'success');
                this.clearForm();
                await this.loadFaces();
            } else {
                throw new Error(result.error || 'Upload failed');
            }

        } catch (error) {
            console.error('Upload error:', error);
            this.showNotification(`Upload failed: ${error.message}`, 'error');
        } finally {
            // Reset button state
            uploadBtn.disabled = false;
            btnText.style.display = 'inline';
            btnSpinner.style.display = 'none';
            this.updateUploadStatus('Ready to upload');
        }
    }

    clearForm() {
        document.getElementById('face-upload-form').reset();
        this.selectedFile = null;
        
        // Reset file upload area
        const uploadArea = document.getElementById('file-upload-area');
        uploadArea.innerHTML = `
            <input type="file" id="face-image" name="image" accept="image/*" required>
            <div class="upload-prompt">
                <div class="upload-icon">📁</div>
                <div class="upload-text">
                    <strong>Click to select image</strong> or drag and drop
                </div>
                <div class="upload-hint">Supports JPG, PNG, BMP (max 10MB)</div>
            </div>
        `;
        
        // Reset preview
        const preview = document.getElementById('image-preview');
        preview.innerHTML = `
            <div class="preview-placeholder">
                <div class="placeholder-icon">🖼️</div>
                <div class="placeholder-text">No image selected</div>
            </div>
        `;
        
        document.getElementById('preview-info').textContent = '';
        
        // Re-attach file input listener
        document.getElementById('face-image').addEventListener('change', (e) => {
            this.handleFileSelect(e.target.files[0]);
        });
        
        this.validateForm();
    }

    async loadFaces() {
        const loadingEl = document.getElementById('faces-loading');
        const gridEl = document.getElementById('faces-grid');
        const emptyEl = document.getElementById('faces-empty');
        const countEl = document.getElementById('faces-count');
        
        // Show loading state
        loadingEl.style.display = 'block';
        gridEl.style.display = 'none';
        emptyEl.style.display = 'none';

        try {
            const response = await fetch(`${this.apiBase}/api/faces`);
            if (!response.ok) throw new Error('Failed to load faces');
            
            const data = await response.json();
            this.faces = data.faces || [];
            
            // Update count
            countEl.textContent = `${this.faces.length} face${this.faces.length !== 1 ? 's' : ''}`;
            
            // Render faces
            this.renderFaces();
            
        } catch (error) {
            console.error('Error loading faces:', error);
            this.showNotification('Failed to load faces', 'error');
        } finally {
            loadingEl.style.display = 'none';
        }
    }

    renderFaces() {
        const gridEl = document.getElementById('faces-grid');
        const emptyEl = document.getElementById('faces-empty');
        
        if (this.faces.length === 0) {
            gridEl.style.display = 'none';
            emptyEl.style.display = 'block';
            return;
        }
        
        gridEl.style.display = 'grid';
        emptyEl.style.display = 'none';
        
        gridEl.innerHTML = this.faces.map(face => this.createFaceCard(face)).join('');
        
        // Attach delete event listeners
        gridEl.querySelectorAll('.btn-delete').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const faceId = e.target.dataset.faceId;
                this.showDeleteModal(faceId);
            });
        });
    }

    createFaceCard(face) {
        const date = new Date(face.created_at).toLocaleDateString();
        const imagePath = face.image_path ? `/${face.image_path}` : '';
        
        return `
            <div class="face-card">
                <div class="face-image">
                    ${imagePath ? 
                        `<img src="${imagePath}" alt="${face.name}" onerror="this.parentElement.innerHTML='<div class=\\"placeholder\\">👤</div>'">` :
                        '<div class="placeholder">👤</div>'
                    }
                </div>
                <div class="face-info">
                    <div class="face-name">${this.escapeHtml(face.name)}</div>
                    <div class="face-date">Added ${date}</div>
                    <div class="face-actions">
                        <button class="btn-icon btn-delete" data-face-id="${face.id}" title="Delete face">
                            🗑️
                        </button>
                    </div>
                </div>
            </div>
        `;
    }

    showDeleteModal(faceId) {
        const face = this.faces.find(f => f.id == faceId);
        if (!face) return;
        
        this.deleteTargetId = faceId;
        
        // Populate modal
        document.getElementById('delete-face-name').textContent = face.name;
        document.getElementById('delete-face-date').textContent = `Added ${new Date(face.created_at).toLocaleDateString()}`;
        
        const imageEl = document.getElementById('delete-face-image');
        if (face.image_path) {
            imageEl.src = `/${face.image_path}`;
            imageEl.style.display = 'block';
        } else {
            imageEl.style.display = 'none';
        }
        
        // Show modal
        document.getElementById('delete-modal').classList.add('show');
    }

    hideDeleteModal() {
        document.getElementById('delete-modal').classList.remove('show');
        this.deleteTargetId = null;
    }

    async confirmDelete() {
        if (!this.deleteTargetId) return;
        
        const confirmBtn = document.getElementById('confirm-delete');
        const originalText = confirmBtn.textContent;
        
        try {
            confirmBtn.textContent = 'Deleting...';
            confirmBtn.disabled = true;
            
            const response = await fetch(`${this.apiBase}/api/faces/${this.deleteTargetId}`, {
                method: 'DELETE'
            });
            
            if (response.ok) {
                this.showNotification('Face deleted successfully', 'success');
                await this.loadFaces();
                this.hideDeleteModal();
            } else {
                const error = await response.json();
                throw new Error(error.error || 'Delete failed');
            }
            
        } catch (error) {
            console.error('Delete error:', error);
            this.showNotification(`Delete failed: ${error.message}`, 'error');
        } finally {
            confirmBtn.textContent = originalText;
            confirmBtn.disabled = false;
        }
    }

    updateUploadStatus(status) {
        document.getElementById('upload-status').textContent = status;
    }

    updateSystemStatus(status, text) {
        const indicator = document.getElementById('system-status');
        const statusText = document.getElementById('system-status-text');
        
        indicator.className = `status-indicator ${status}`;
        statusText.textContent = text;
    }

    showNotification(message, type = 'info') {
        const container = document.getElementById('notification-container');
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.textContent = message;
        
        container.appendChild(notification);
        
        // Auto remove after 5 seconds
        setTimeout(() => {
            if (notification.parentElement) {
                notification.remove();
            }
        }, 5000);
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    new FaceManager();
});

// Add form validation on input
document.addEventListener('DOMContentLoaded', () => {
    const nameInput = document.getElementById('face-name');
    if (nameInput) {
        nameInput.addEventListener('input', () => {
            if (window.faceManager) {
                window.faceManager.validateForm();
            }
        });
    }
});
