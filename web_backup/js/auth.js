// 认证管理模块
class AuthManager {
    constructor() {
        this.currentUser = null;
        this.init();
    }
    
    init() {
        this.bindEvents();
        this.checkAuthStatus();
    }
    
    bindEvents() {
        // 标签切换
        const tabBtns = document.querySelectorAll('.tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                this.switchTab(btn.dataset.tab);
            });
        });
        
        // 登录表单
        const loginForm = document.getElementById('loginForm');
        if (loginForm) {
            loginForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleLogin();
            });
        }
        
        // 注册表单
        const registerForm = document.getElementById('registerForm');
        console.log('查找注册表单:', registerForm);
        if (registerForm) {
            console.log('绑定注册表单提交事件');
            registerForm.addEventListener('submit', (e) => {
                console.log('注册表单提交事件触发');
                e.preventDefault();
                this.handleRegister();
            });
        } else {
            console.error('未找到注册表单元素');
        }
        
        // 退出登录
        const logoutBtn = document.getElementById('logout-btn');
        if (logoutBtn) {
            logoutBtn.addEventListener('click', () => {
                this.handleLogout();
            });
        }
    }
    
    switchTab(tab) {
        // 更新标签按钮状态
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.remove('active');
        });
        document.querySelector(`[data-tab="${tab}"]`).classList.add('active');
        
        // 更新表单显示
        document.querySelectorAll('.auth-form').forEach(form => {
            form.classList.remove('active');
        });
        document.getElementById(`${tab}-form`).classList.add('active');
        
        // 清除之前的错误信息
        this.clearMessages();
    }
    
    async handleLogin() {
        const username = document.getElementById('login-username').value.trim();
        const password = document.getElementById('login-password').value;
        
        // 验证输入
        if (!this.validateLoginInput(username, password)) {
            return;
        }
        
        this.showLoading('登录中...');
        
        try {
            const response = await api.login(username, password);
            
            if (response.success) {
                this.showMessage(CONFIG.SUCCESS_MESSAGES.LOGIN_SUCCESS, 'success');
                this.currentUser = {
                    userId: response.userId,
                    username: response.username,
                    role: response.role
                };
                
                // 延迟跳转到仪表板
                setTimeout(() => {
                    this.showDashboard();
                }, 1000);
            } else {
                this.showMessage(response.message || '登录失败', 'error');
            }
        } catch (error) {
            console.error('登录错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }
    
    async handleRegister() {
        console.log('=== 开始注册处理 ===');
        const username = document.getElementById('register-username').value.trim();
        const password = document.getElementById('register-password').value;
        const role = document.getElementById('register-role').value;
        
        console.log('表单数据:', { username, password: '***', role });
        
        // 验证输入
        console.log('开始验证输入...');
        if (!this.validateRegisterInput(username, password, role)) {
            console.log('验证失败，返回');
            return;
        }
        console.log('验证通过，继续处理...');
        
        this.showLoading('注册中...');
        
        try {
            const response = await api.register(username, password, role);
            
            if (response.success) {
                this.showMessage(CONFIG.SUCCESS_MESSAGES.REGISTER_SUCCESS, 'success');
                
                // 自动切换到登录页面
                setTimeout(() => {
                    this.switchTab('login');
                    document.getElementById('login-username').value = username;
                }, 1500);
            } else {
                this.showMessage(response.message || '注册失败', 'error');
            }
        } catch (error) {
            console.error('注册错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }
    
    async handleLogout() {
        this.showLoading('退出中...');
        
        try {
            await api.logout();
            this.showMessage(CONFIG.SUCCESS_MESSAGES.LOGOUT_SUCCESS, 'success');
        } catch (error) {
            console.error('退出错误:', error);
        } finally {
            this.hideLoading();
            this.showAuth();
        }
    }
    
    validateLoginInput(username, password) {
        if (!username || !password) {
            this.showMessage('请输入用户名和密码', 'error');
            return false;
        }
        
        if (username.length < CONFIG.VALIDATION.USERNAME_MIN_LENGTH) {
            this.showMessage(`用户名至少需要 ${CONFIG.VALIDATION.USERNAME_MIN_LENGTH} 个字符`, 'error');
            return false;
        }
        
        if (password.length < CONFIG.VALIDATION.PASSWORD_MIN_LENGTH) {
            this.showMessage(`密码至少需要 ${CONFIG.VALIDATION.PASSWORD_MIN_LENGTH} 个字符`, 'error');
            return false;
        }
        
        return true;
    }
    
    validateRegisterInput(username, password, role) {
        console.log('验证输入参数:', { username, passwordLength: password?.length, role });
        console.log('配置检查:', {
            USERNAME_MIN_LENGTH: CONFIG.VALIDATION.USERNAME_MIN_LENGTH,
            USERNAME_MAX_LENGTH: CONFIG.VALIDATION.USERNAME_MAX_LENGTH,
            PASSWORD_MIN_LENGTH: CONFIG.VALIDATION.PASSWORD_MIN_LENGTH,
            PASSWORD_MAX_LENGTH: CONFIG.VALIDATION.PASSWORD_MAX_LENGTH,
            ROLES: CONFIG.ROLES
        });
        
        if (!username || !password || !role) {
            console.log('必填字段验证失败');
            this.showMessage('请填写所有必填字段', 'error');
            return false;
        }
        
        if (username.length < CONFIG.VALIDATION.USERNAME_MIN_LENGTH || 
            username.length > CONFIG.VALIDATION.USERNAME_MAX_LENGTH) {
            console.log('用户名长度验证失败:', username.length);
            this.showMessage(`用户名长度必须在 ${CONFIG.VALIDATION.USERNAME_MIN_LENGTH}-${CONFIG.VALIDATION.USERNAME_MAX_LENGTH} 个字符之间`, 'error');
            return false;
        }
        
        if (password.length < CONFIG.VALIDATION.PASSWORD_MIN_LENGTH || 
            password.length > CONFIG.VALIDATION.PASSWORD_MAX_LENGTH) {
            console.log('密码长度验证失败:', password.length);
            this.showMessage(`密码长度必须在 ${CONFIG.VALIDATION.PASSWORD_MIN_LENGTH}-${CONFIG.VALIDATION.PASSWORD_MAX_LENGTH} 个字符之间`, 'error');
            return false;
        }
        
        const roleValues = Object.values(CONFIG.ROLES);
        console.log('角色验证:', { role, roleValues, includes: roleValues.includes(role) });
        if (!roleValues.includes(role)) {
            console.log('角色验证失败');
            this.showMessage('无效的用户角色', 'error');
            return false;
        }
        
        console.log('验证通过');
        return true;
    }
    
    checkAuthStatus() {
        if (api.isAuthenticated()) {
            this.showDashboard();
        } else {
            this.showAuth();
        }
    }
    
    showDashboard() {
        const authContainer = document.getElementById('auth-container');
        const dashboard = document.getElementById('dashboard');
        
        authContainer.style.display = 'none';
        dashboard.style.display = 'block';
        
        // 更新用户信息显示
        this.updateUserInfo();
        
        // 初始化仪表板
        if (window.dashboardManager) {
            dashboardManager.init();
        }
    }
    
    showAuth() {
        const authContainer = document.getElementById('auth-container');
        const dashboard = document.getElementById('dashboard');
        
        authContainer.style.display = 'flex';
        dashboard.style.display = 'none';
    }
    
    updateUserInfo() {
        const userInfo = document.getElementById('user-info');
        if (userInfo && api.isAuthenticated()) {
            const role = api.role;
            const roleClass = role.toLowerCase();
            const roleName = this.getRoleName(role);
            
            userInfo.innerHTML = `
                欢迎，<strong>${api.username}</strong>
                <span class="role-badge ${roleClass}">${roleName}</span>
            `;
            
            // 根据角色显示/隐藏特定功能
            this.updateRoleBasedUI(role);
        }
    }
    
    updateRoleBasedUI(role) {
        // 隐藏/显示管理员专用功能
        const adminElements = document.querySelectorAll('.admin-only');
        const isAdmin = role === 'ADMIN';
        
        adminElements.forEach(element => {
            element.style.display = isAdmin ? 'block' : 'none';
        });
    }
    
    getRoleName(role) {
        const roleNames = {
            'AUTHOR': '作者',
            'REVIEWER': '评审员',
            'EDITOR': '编辑',
            'ADMIN': '管理员'
        };
        return roleNames[role] || role;
    }
    
    showLoading(message = '加载中...') {
        const overlay = document.getElementById('loading-overlay');
        if (overlay) {
            overlay.style.display = 'flex';
            const text = overlay.querySelector('p');
            if (text) text.textContent = message;
        }
    }
    
    hideLoading() {
        const overlay = document.getElementById('loading-overlay');
        if (overlay) {
            overlay.style.display = 'none';
        }
    }
    
    showMessage(message, type = 'info') {
        const notification = document.getElementById('notification');
        const messageElement = document.getElementById('notification-message');
        
        if (notification && messageElement) {
            messageElement.textContent = message;
            notification.className = `notification ${type}`;
            notification.style.display = 'block';
            
            // 自动隐藏
            setTimeout(() => {
                notification.style.display = 'none';
            }, CONFIG.UI.NOTIFICATION_DURATION);
            
            // 点击关闭
            const closeBtn = document.getElementById('notification-close');
            if (closeBtn) {
                closeBtn.onclick = () => {
                    notification.style.display = 'none';
                };
            }
        }
    }
    
    clearMessages() {
        const notification = document.getElementById('notification');
        if (notification) {
            notification.style.display = 'none';
        }
    }
    
    // 获取当前用户信息
    getCurrentUser() {
        if (api.isAuthenticated()) {
            return {
                userId: api.userId,
                username: api.username,
                role: api.role
            };
        }
        return null;
    }
    
    // 检查权限
    hasPermission(requiredRole) {
        if (!api.isAuthenticated()) {
            return false;
        }
        
        const roleHierarchy = {
            'AUTHOR': 0,
            'REVIEWER': 1,
            'EDITOR': 2,
            'ADMIN': 3
        };
        
        const userRoleLevel = roleHierarchy[api.role] || -1;
        const requiredRoleLevel = roleHierarchy[requiredRole] || -1;
        
        return userRoleLevel >= requiredRoleLevel;
    }
}

// 创建全局认证管理器实例
window.authManager = new AuthManager();

// 导出认证管理器
if (typeof module !== 'undefined' && module.exports) {
    module.exports = AuthManager;
}