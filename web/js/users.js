// 用户管理模块
class UserManager {
    constructor() {
        this.users = [];
        this.currentPage = 1;
        this.pageSize = CONFIG.PAGINATION.PAGE_SIZE;
        this.totalPages = 0;
        this.currentUser = null;
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadUsers();
    }

    bindEvents() {
        // 用户搜索
        const searchInput = document.getElementById('user-search');
        if (searchInput) {
            let searchTimeout;
            searchInput.addEventListener('input', () => {
                clearTimeout(searchTimeout);
                searchTimeout = setTimeout(() => {
                    this.searchUsers(searchInput.value);
                }, 300);
            });
        }

        // 角色筛选
        const roleFilter = document.getElementById('user-role-filter');
        if (roleFilter) {
            roleFilter.addEventListener('change', () => {
                this.filterUsersByRole(roleFilter.value);
            });
        }

        // 添加用户按钮
        const addUserBtn = document.getElementById('add-user-btn');
        if (addUserBtn) {
            addUserBtn.addEventListener('click', () => {
                this.showAddUserModal();
            });
        }

        // 批量操作
        const batchActionSelect = document.getElementById('batch-action');
        if (batchActionSelect) {
            batchActionSelect.addEventListener('change', () => {
                this.handleBatchAction(batchActionSelect.value);
            });
        }
    }

    async loadUsers(page = 1, search = '', role = '') {
        this.showLoading('加载用户列表...');

        try {
            const response = await api.getUsers(page, this.pageSize, search, role);

            if (response.success) {
                this.users = response.users || [];
                this.currentPage = page;
                this.totalPages = response.totalPages || 0;
                this.renderUserList();
                this.updatePagination();
            } else {
                this.showMessage('加载用户列表失败', 'error');
                this.renderEmptyUsers();
            }
        } catch (error) {
            console.error('加载用户错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
            this.renderEmptyUsers();
        } finally {
            this.hideLoading();
        }
    }

    renderUserList() {
        const userListContainer = document.getElementById('user-list');
        if (!userListContainer) return;

        if (this.users.length === 0) {
            this.renderEmptyUsers();
            return;
        }

        userListContainer.innerHTML = this.users.map(user => `
            <div class="user-item">
                <div class="user-select">
                    <input type="checkbox" class="user-checkbox" value="${user.id}">
                </div>
                <div class="user-avatar">
                    ${user.username.charAt(0).toUpperCase()}
                </div>
                <div class="user-info">
                    <h4>${user.username}</h4>
                    <p class="user-email">${user.email || '未设置邮箱'}</p>
                    <p class="user-role">
                        <span class="role-badge role-${user.role.toLowerCase()}">${this.getRoleDisplayName(user.role)}</span>
                        <span class="user-status ${user.status}">${this.getStatusDisplayName(user.status)}</span>
                    </p>
                    <p class="user-meta">
                        注册时间: ${this.formatDate(user.createdAt)} | 
                        最后登录: ${this.formatDateTime(user.lastLoginAt)}
                    </p>
                </div>
                <div class="user-stats">
                    <div class="stat">
                        <span class="stat-value">${user.fileCount || 0}</span>
                        <span class="stat-label">文件</span>
                    </div>
                    <div class="stat">
                        <span class="stat-value">${user.reviewCount || 0}</span>
                        <span class="stat-label">评审</span>
                    </div>
                </div>
                <div class="user-actions">
                    ${this.canEditUser(user) ? `
                        <button class="btn btn-sm btn-outline" onclick="userManager.editUser('${user.id}')">
                            编辑
                        </button>
                    ` : ''}
                    ${this.canDeleteUser(user) ? `
                        <button class="btn btn-sm btn-error" onclick="userManager.deleteUser('${user.id}', '${user.username}')">
                            删除
                        </button>
                    ` : ''}
                    ${this.canResetPassword(user) ? `
                        <button class="btn btn-sm btn-warning" onclick="userManager.resetPassword('${user.id}', '${user.username}')">
                            重置密码
                        </button>
                    ` : ''}
                </div>
            </div>
        `).join('');
    }

    renderEmptyUsers() {
        const userListContainer = document.getElementById('user-list');
        if (userListContainer) {
            userListContainer.innerHTML = `
                <div class="empty-state">
                    <p>没有找到用户</p>
                    <button class="btn btn-primary" onclick="userManager.loadUsers()">刷新列表</button>
                </div>
            `;
        }
    }

    updatePagination() {
        const paginationContainer = document.getElementById('user-pagination');
        if (!paginationContainer || this.totalPages <= 1) {
            if (paginationContainer) paginationContainer.innerHTML = '';
            return;
        }

        let paginationHTML = '';

        // 上一页
        if (this.currentPage > 1) {
            paginationHTML += `
                <button class="pagination-btn" onclick="userManager.loadUsers(${this.currentPage - 1})">
                    上一页
                </button>
            `;
        }

        // 页码
        const startPage = Math.max(1, this.currentPage - 2);
        const endPage = Math.min(this.totalPages, this.currentPage + 2);

        if (startPage > 1) {
            paginationHTML += `<button class="pagination-btn" onclick="userManager.loadUsers(1)">1</button>`;
            if (startPage > 2) {
                paginationHTML += `<span class="pagination-ellipsis">...</span>`;
            }
        }

        for (let i = startPage; i <= endPage; i++) {
            paginationHTML += `
                <button class="pagination-btn ${i === this.currentPage ? 'active' : ''}" 
                        onclick="userManager.loadUsers(${i})">
                    ${i}
                </button>
            `;
        }

        if (endPage < this.totalPages) {
            if (endPage < this.totalPages - 1) {
                paginationHTML += `<span class="pagination-ellipsis">...</span>`;
            }
            paginationHTML += `<button class="pagination-btn" onclick="userManager.loadUsers(${this.totalPages})">${this.totalPages}</button>`;
        }

        // 下一页
        if (this.currentPage < this.totalPages) {
            paginationHTML += `
                <button class="pagination-btn" onclick="userManager.loadUsers(${this.currentPage + 1})">
                    下一页
                </button>
            `;
        }

        paginationContainer.innerHTML = paginationHTML;
    }

    showAddUserModal() {
        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');

        if (!modal || !modalTitle || !modalBody) return;

        modalTitle.textContent = '添加用户';
        modalBody.innerHTML = `
            <form id="add-user-form">
                <div class="form-row">
                    <div class="form-group">
                        <label for="new-username">用户名 *</label>
                        <input type="text" id="new-username" name="username" required>
                    </div>
                    <div class="form-group">
                        <label for="new-email">邮箱</label>
                        <input type="email" id="new-email" name="email">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="new-password">密码 *</label>
                        <input type="password" id="new-password" name="password" required>
                    </div>
                    <div class="form-group">
                        <label for="new-role">角色 *</label>
                        <select id="new-role" name="role" required>
                            <option value="">请选择角色</option>
                            <option value="AUTHOR">作者</option>
                            <option value="REVIEWER">评审员</option>
                            <option value="EDITOR">编辑</option>
                            <option value="ADMIN">管理员</option>
                        </select>
                    </div>
                </div>
                <div class="form-group">
                    <label for="new-fullname">姓名</label>
                    <input type="text" id="new-fullname" name="fullname">
                </div>
                <div class="form-group">
                    <label for="new-affiliation">所属机构</label>
                    <input type="text" id="new-affiliation" name="affiliation">
                </div>
            </form>
        `;

        modal.style.display = 'block';

        // 绑定表单提交事件
        const form = document.getElementById('add-user-form');
        if (form) {
            form.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleAddUser();
            });
        }
    }

    async handleAddUser() {
        const form = document.getElementById('add-user-form');
        if (!form) return;

        const formData = new FormData(form);
        const userData = {
            username: formData.get('username').trim(),
            email: formData.get('email').trim(),
            password: formData.get('password'),
            role: formData.get('role'),
            fullname: formData.get('fullname').trim(),
            affiliation: formData.get('affiliation').trim()
        };

        // 验证必填字段
        if (!userData.username || !userData.password || !userData.role) {
            this.showMessage('请填写必填字段', 'error');
            return;
        }

        this.showLoading('添加用户...');

        try {
            const response = await api.createUser(userData);

            if (response.success) {
                this.showMessage('用户添加成功', 'success');
                this.closeModal();
                this.loadUsers(this.currentPage);
            } else {
                this.showMessage(response.message || '用户添加失败', 'error');
            }
        } catch (error) {
            console.error('添加用户错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async editUser(userId) {
        const user = this.users.find(u => u.id === userId);
        if (!user) return;

        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');

        if (!modal || !modalTitle || !modalBody) return;

        modalTitle.textContent = `编辑用户: ${user.username}`;
        modalBody.innerHTML = `
            <form id="edit-user-form">
                <div class="form-row">
                    <div class="form-group">
                        <label for="edit-username">用户名 *</label>
                        <input type="text" id="edit-username" name="username" value="${user.username}" required>
                    </div>
                    <div class="form-group">
                        <label for="edit-email">邮箱</label>
                        <input type="email" id="edit-email" name="email" value="${user.email || ''}">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="edit-role">角色 *</label>
                        <select id="edit-role" name="role" required>
                            <option value="AUTHOR" ${user.role === 'AUTHOR' ? 'selected' : ''}>作者</option>
                            <option value="REVIEWER" ${user.role === 'REVIEWER' ? 'selected' : ''}>评审员</option>
                            <option value="EDITOR" ${user.role === 'EDITOR' ? 'selected' : ''}>编辑</option>
                            <option value="ADMIN" ${user.role === 'ADMIN' ? 'selected' : ''}>管理员</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label for="edit-status">状态</label>
                        <select id="edit-status" name="status">
                            <option value="ACTIVE" ${user.status === 'ACTIVE' ? 'selected' : ''}>活跃</option>
                            <option value="INACTIVE" ${user.status === 'INACTIVE' ? 'selected' : ''}>非活跃</option>
                            <option value="SUSPENDED" ${user.status === 'SUSPENDED' ? 'selected' : ''}>暂停</option>
                        </select>
                    </div>
                </div>
                <div class="form-group">
                    <label for="edit-fullname">姓名</label>
                    <input type="text" id="edit-fullname" name="fullname" value="${user.fullname || ''}">
                </div>
                <div class="form-group">
                    <label for="edit-affiliation">所属机构</label>
                    <input type="text" id="edit-affiliation" name="affiliation" value="${user.affiliation || ''}">
                </div>
                <input type="hidden" name="userId" value="${userId}">
            </form>
        `;

        modal.style.display = 'block';

        const form = document.getElementById('edit-user-form');
        if (form) {
            form.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleEditUser();
            });
        }
    }

    async handleEditUser() {
        const form = document.getElementById('edit-user-form');
        if (!form) return;

        const formData = new FormData(form);
        const userData = {
            userId: formData.get('userId'),
            username: formData.get('username').trim(),
            email: formData.get('email').trim(),
            role: formData.get('role'),
            status: formData.get('status'),
            fullname: formData.get('fullname').trim(),
            affiliation: formData.get('affiliation').trim()
        };

        this.showLoading('更新用户信息...');

        try {
            const response = await api.updateUser(userData);

            if (response.success) {
                this.showMessage('用户信息更新成功', 'success');
                this.closeModal();
                this.loadUsers(this.currentPage);
            } else {
                this.showMessage(response.message || '用户信息更新失败', 'error');
            }
        } catch (error) {
            console.error('更新用户错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async deleteUser(userId, username) {
        if (!confirm(`确定要删除用户 "${username}" 吗？此操作不可撤销。`)) {
            return;
        }

        this.showLoading('删除用户...');

        try {
            const response = await api.deleteUser(userId);

            if (response.success) {
                this.showMessage('用户删除成功', 'success');
                this.loadUsers(this.currentPage);
            } else {
                this.showMessage(response.message || '用户删除失败', 'error');
            }
        } catch (error) {
            console.error('删除用户错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async resetPassword(userId, username) {
        if (!confirm(`确定要重置用户 "${username}" 的密码吗？`)) {
            return;
        }

        this.showLoading('重置密码...');

        try {
            const response = await api.resetPassword(userId);

            if (response.success) {
                this.showMessage(`密码重置成功，新密码: ${response.newPassword}`, 'success');
            } else {
                this.showMessage(response.message || '密码重置失败', 'error');
            }
        } catch (error) {
            console.error('重置密码错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async handleBatchAction(action) {
        const selectedUsers = this.getSelectedUsers();
        
        if (selectedUsers.length === 0) {
            this.showMessage('请选择要操作的用户', 'warning');
            return;
        }

        switch (action) {
            case 'activate':
                await this.batchUpdateStatus(selectedUsers, 'ACTIVE');
                break;
            case 'deactivate':
                await this.batchUpdateStatus(selectedUsers, 'INACTIVE');
                break;
            case 'suspend':
                await this.batchUpdateStatus(selectedUsers, 'SUSPENDED');
                break;
            case 'delete':
                if (confirm(`确定要删除选中的 ${selectedUsers.length} 个用户吗？`)) {
                    await this.batchDeleteUsers(selectedUsers);
                }
                break;
        }

        // 重置选择框
        document.getElementById('batch-action').value = '';
    }

    async batchUpdateStatus(userIds, status) {
        this.showLoading('批量更新状态...');

        try {
            const response = await api.batchUpdateUsers({
                userIds,
                action: 'updateStatus',
                status
            });

            if (response.success) {
                this.showMessage(`成功更新 ${userIds.length} 个用户的状态`, 'success');
                this.loadUsers(this.currentPage);
            } else {
                this.showMessage(response.message || '批量更新失败', 'error');
            }
        } catch (error) {
            console.error('批量更新错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async batchDeleteUsers(userIds) {
        this.showLoading('批量删除用户...');

        try {
            const response = await api.batchUpdateUsers({
                userIds,
                action: 'delete'
            });

            if (response.success) {
                this.showMessage(`成功删除 ${userIds.length} 个用户`, 'success');
                this.loadUsers(this.currentPage);
            } else {
                this.showMessage(response.message || '批量删除失败', 'error');
            }
        } catch (error) {
            console.error('批量删除错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }

    getSelectedUsers() {
        const checkboxes = document.querySelectorAll('.user-checkbox:checked');
        return Array.from(checkboxes).map(cb => cb.value);
    }

    searchUsers(query) {
        this.loadUsers(1, query, document.getElementById('user-role-filter')?.value || '');
    }

    filterUsersByRole(role) {
        this.loadUsers(1, document.getElementById('user-search')?.value || '', role);
    }

    canEditUser(user) {
        const currentUser = authManager.getCurrentUser();
        return currentUser && (currentUser.role === 'ADMIN' || currentUser.id === user.id);
    }

    canDeleteUser(user) {
        const currentUser = authManager.getCurrentUser();
        return currentUser && currentUser.role === 'ADMIN' && currentUser.id !== user.id;
    }

    canResetPassword(user) {
        const currentUser = authManager.getCurrentUser();
        return currentUser && currentUser.role === 'ADMIN';
    }

    getRoleDisplayName(role) {
        const roleNames = {
            'AUTHOR': '作者',
            'REVIEWER': '评审员',
            'EDITOR': '编辑',
            'ADMIN': '管理员'
        };
        return roleNames[role] || role;
    }

    getStatusDisplayName(status) {
        const statusNames = {
            'ACTIVE': '活跃',
            'INACTIVE': '非活跃',
            'SUSPENDED': '暂停'
        };
        return statusNames[status] || status;
    }

    formatDate(dateString) {
        const date = new Date(dateString);
        return date.toLocaleDateString('zh-CN');
    }

    formatDateTime(dateString) {
        if (!dateString) return '从未登录';
        const date = new Date(dateString);
        const now = new Date();
        const diff = now - date;
        const days = Math.floor(diff / (1000 * 60 * 60 * 24));
        
        if (days === 0) {
            const hours = Math.floor(diff / (1000 * 60 * 60));
            if (hours === 0) {
                const minutes = Math.floor(diff / (1000 * 60));
                return `${minutes}分钟前`;
            }
            return `${hours}小时前`;
        } else if (days < 7) {
            return `${days}天前`;
        } else {
            return date.toLocaleDateString('zh-CN');
        }
    }

    closeModal() {
        const modal = document.getElementById('modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }

    showLoading(message) {
        const loadingOverlay = document.getElementById('loading-overlay');
        if (loadingOverlay) {
            const loadingText = document.querySelector('.loading-text');
            if (loadingText) loadingText.textContent = message;
            loadingOverlay.style.display = 'flex';
        }
    }

    hideLoading() {
        const loadingOverlay = document.getElementById('loading-overlay');
        if (loadingOverlay) {
            loadingOverlay.style.display = 'none';
        }
    }

    showMessage(message, type = 'info') {
        if (window.showNotification) {
            window.showNotification(message, type);
        } else {
            console.log(`[${type.toUpperCase()}] ${message}`);
        }
    }
}

// 创建全局用户管理器实例
const userManager = new UserManager();