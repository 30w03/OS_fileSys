// 文件管理模块
class FileManager {
    constructor() {
        this.currentPage = 1;
        this.pageSize = CONFIG.PAGINATION.PAGE_SIZE;
        this.files = [];
        this.init();
    }
    
    init() {
        this.bindEvents();
        this.loadFiles();
    }
    
    bindEvents() {
        // 上传文件按钮
        const uploadBtn = document.getElementById('upload-file-btn');
        if (uploadBtn) {
            uploadBtn.addEventListener('click', () => {
                this.showUploadModal();
            });
        }
        
        // 文件上传表单
        const uploadForm = document.getElementById('file-upload-form');
        if (uploadForm) {
            uploadForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleFileUpload();
            });
        }
        
        // 文件类型筛选
        const fileFilter = document.getElementById('file-filter');
        if (fileFilter) {
            fileFilter.addEventListener('change', () => {
                this.filterFiles();
            });
        }
        
        // 文件搜索
        const fileSearch = document.getElementById('file-search');
        if (fileSearch) {
            fileSearch.addEventListener('input', () => {
                this.searchFiles();
            });
        }
    }
    
    async loadFiles() {
        this.showLoading('加载文件列表...');
        
        try {
            const response = await api.getFiles(this.currentPage, this.pageSize);
            
            if (response.success) {
                this.files = response.files || [];
                this.renderFileList();
                this.updatePagination(response.totalCount, response.page, response.totalPages);
            } else {
                this.showMessage('加载文件列表失败', 'error');
                this.renderEmptyState();
            }
        } catch (error) {
            console.error('加载文件错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
            this.renderEmptyState();
        } finally {
            this.hideLoading();
        }
    }
    
    renderFileList() {
        const fileList = document.getElementById('file-list');
        if (!fileList) return;
        
        if (this.files.length === 0) {
            this.renderEmptyState();
            return;
        }
        
        fileList.innerHTML = this.files.map(file => `
            <div class="file-item">
                <div class="file-info">
                    <div class="file-icon">${this.getFileIcon(file.type)}</div>
                    <div class="file-details">
                        <h4>${file.name}</h4>
                        <p>
                            大小: ${this.formatFileSize(file.size)} | 
                            上传时间: ${this.formatDate(file.uploadTime)} |
                            作者: ${file.author}
                        </p>
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn btn-sm btn-outline" onclick="fileManager.downloadFile('${file.id}')">
                        下载
                    </button>
                    ${this.canDeleteFile(file) ? `
                        <button class="btn btn-sm btn-error" onclick="fileManager.deleteFile('${file.id}', '${file.name}')">
                            删除
                        </button>
                    ` : ''}
                    <button class="btn btn-sm btn-outline" onclick="fileManager.viewFileDetails('${file.id}')">
                        详情
                    </button>
                </div>
            </div>
        `).join('');
    }
    
    renderEmptyState() {
        const fileList = document.getElementById('file-list');
        if (!fileList) return;
        
        fileList.innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">📁</div>
                <h3>暂无文件</h3>
                <p>您还没有上传任何文件，点击上方按钮开始上传。</p>
                <button class="btn btn-primary" onclick="fileManager.showUploadModal()">
                    上传文件
                </button>
            </div>
        `;
    }
    
    showUploadModal() {
        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');
        
        if (!modal || !modalTitle || !modalBody) return;
        
        modalTitle.textContent = '上传文件';
        modalBody.innerHTML = `
            <form id="file-upload-form">
                <div class="form-group">
                    <label for="file-input">选择文件</label>
                    <input type="file" id="file-input" name="file" accept="${CONFIG.UPLOAD.ALLOWED_TYPES.map(ext => '.' + ext).join(',')}" required>
                    <small class="text-sm text-secondary">
                        支持的文件类型: ${CONFIG.UPLOAD.ALLOWED_TYPES.join(', ')}<br>
                        最大文件大小: ${this.formatFileSize(CONFIG.UPLOAD.MAX_SIZE)}
                    </small>
                </div>
                <div class="form-group">
                    <label for="file-description">文件描述 (可选)</label>
                    <textarea id="file-description" name="description" placeholder="请输入文件描述..."></textarea>
                </div>
            </form>
        `;
        
        modal.style.display = 'block';
        
        // 绑定表单提交事件
        const uploadForm = document.getElementById('file-upload-form');
        if (uploadForm) {
            uploadForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleFileUpload();
            });
        }
    }
    
    async handleFileUpload() {
        const fileInput = document.getElementById('file-input');
        const file = fileInput.files[0];
        
        if (!file) {
            this.showMessage('请选择要上传的文件', 'error');
            return;
        }
        
        // 验证文件
        if (!this.validateFile(file)) {
            return;
        }
        
        this.showLoading('上传文件中...');
        
        try {
            const response = await api.uploadFile(file);
            
            if (response.success) {
                this.showMessage(CONFIG.SUCCESS_MESSAGES.FILE_UPLOAD_SUCCESS, 'success');
                this.closeModal();
                this.loadFiles(); // 重新加载文件列表
            } else {
                this.showMessage(response.message || '文件上传失败', 'error');
            }
        } catch (error) {
            console.error('文件上传错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }
    
    validateFile(file) {
        // 检查文件大小
        if (file.size > CONFIG.UPLOAD.MAX_SIZE) {
            this.showMessage(CONFIG.ERROR_MESSAGES.FILE_TOO_LARGE, 'error');
            return false;
        }
        
        // 检查文件类型
        const fileExtension = file.name.split('.').pop().toLowerCase();
        if (!CONFIG.UPLOAD.ALLOWED_TYPES.includes(fileExtension)) {
            this.showMessage(CONFIG.ERROR_MESSAGES.INVALID_FILE_TYPE, 'error');
            return false;
        }
        
        return true;
    }
    
    async downloadFile(fileId) {
        try {
            const response = await api.downloadFile(fileId);
            
            // 创建下载链接
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = response.headers.get('content-disposition')?.split('filename=')[1] || 'download';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
        } catch (error) {
            console.error('文件下载错误:', error);
            this.showMessage('文件下载失败', 'error');
        }
    }
    
    async deleteFile(fileId, fileName) {
        if (!confirm(`确定要删除文件 "${fileName}" 吗？此操作不可撤销。`)) {
            return;
        }
        
        this.showLoading('删除文件中...');
        
        try {
            const response = await api.deleteFile(fileId);
            
            if (response.success) {
                this.showMessage(CONFIG.SUCCESS_MESSAGES.FILE_DELETE_SUCCESS, 'success');
                this.loadFiles(); // 重新加载文件列表
            } else {
                this.showMessage(response.message || '文件删除失败', 'error');
            }
        } catch (error) {
            console.error('文件删除错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }
    
    viewFileDetails(fileId) {
        const file = this.files.find(f => f.id === fileId);
        if (!file) return;
        
        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');
        
        if (!modal || !modalTitle || !modalBody) return;
        
        modalTitle.textContent = '文件详情';
        modalBody.innerHTML = `
            <div class="file-details-view">
                <div class="detail-item">
                    <strong>文件名:</strong> ${file.name}
                </div>
                <div class="detail-item">
                    <strong>文件大小:</strong> ${this.formatFileSize(file.size)}
                </div>
                <div class="detail-item">
                    <strong>文件类型:</strong> ${file.type}
                </div>
                <div class="detail-item">
                    <strong>上传时间:</strong> ${this.formatDate(file.uploadTime)}
                </div>
                <div class="detail-item">
                    <strong>作者:</strong> ${file.author}
                </div>
                <div class="detail-item">
                    <strong>文件描述:</strong> ${file.description || '无'}
                </div>
            </div>
        `;
        
        modal.style.display = 'block';
    }
    
    canDeleteFile(file) {
        const currentUser = authManager.getCurrentUser();
        if (!currentUser) return false;
        
        // 管理员可以删除所有文件
        if (currentUser.role === 'ADMIN') return true;
        
        // 文件作者可以删除自己的文件
        if (currentUser.userId === file.authorId) return true;
        
        return false;
    }
    
    filterFiles() {
        const filter = document.getElementById('file-filter').value;
        // 实现文件类型筛选逻辑
        this.renderFileList();
    }
    
    searchFiles() {
        const query = document.getElementById('file-search').value.toLowerCase();
        // 实现文件搜索逻辑
        this.renderFileList();
    }
    
    updatePagination(totalCount, currentPage, totalPages) {
        // 实现分页逻辑
        const pagination = document.getElementById('pagination');
        if (!pagination) return;
        
        // 根据后端返回的分页信息更新UI
        // 这里需要根据实际API返回的分页格式进行调整
    }
    
    getFileIcon(fileType) {
        const iconMap = {
            'pdf': '📄',
            'doc': '📝',
            'docx': '📝',
            'txt': '📃',
            'tex': '📐',
            'bib': '📚',
            'jpg': '🖼️',
            'jpeg': '🖼️',
            'png': '🖼️',
            'gif': '🖼️'
        };
        return iconMap[fileType.toLowerCase()] || '📄';
    }
    
    formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
    
    formatDate(dateString) {
        const date = new Date(dateString);
        return date.toLocaleString('zh-CN', {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit'
        });
    }
    
    closeModal() {
        const modal = document.getElementById('modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }
    
    showLoading(message = '加载中...') {
        if (window.authManager) {
            authManager.showLoading(message);
        }
    }
    
    hideLoading() {
        if (window.authManager) {
            authManager.hideLoading();
        }
    }
    
    showMessage(message, type = 'info') {
        if (window.authManager) {
            authManager.showMessage(message, type);
        }
    }
}

// 创建全局文件管理器实例
const fileManager = new FileManager();

// 导出文件管理器
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { FileManager, fileManager };
}