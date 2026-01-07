// 仪表板管理模块
class DashboardManager {
    constructor() {
        this.currentPage = 'overview';
        this.init();
    }
    
    init() {
        this.bindEvents();
        this.loadCurrentPage();
    }
    
    bindEvents() {
        // 导航链接
        const navLinks = document.querySelectorAll('.nav-link');
        navLinks.forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const page = link.dataset.page;
                if (page) {
                    this.navigateToPage(page);
                }
            });
        });
        
        // 评审标签切换
        const reviewTabs = document.querySelectorAll('[data-review-tab]');
        reviewTabs.forEach(tab => {
            tab.addEventListener('click', () => {
                const reviewTab = tab.dataset.reviewTab;
                if (reviewTab) {
                    this.switchReviewTab(reviewTab);
                }
            });
        });
        
        // 模态框关闭
        this.bindModalEvents();
    }
    
    bindModalEvents() {
        // 模态框关闭按钮
        const modalCloseBtns = document.querySelectorAll('.modal-close, .modal-cancel');
        modalCloseBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                this.closeModal();
            });
        });
        
        // 点击模态框背景关闭
        const modal = document.getElementById('modal');
        if (modal) {
            modal.addEventListener('click', (e) => {
                if (e.target === modal) {
                    this.closeModal();
                }
            });
        }
        
        // ESC键关闭模态框
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') {
                this.closeModal();
            }
        });
    }
    
    navigateToPage(page) {
        // 更新导航状态
        document.querySelectorAll('.nav-link').forEach(link => {
            link.classList.remove('active');
        });
        document.querySelector(`[data-page="${page}"]`).classList.add('active');
        
        // 更新页面显示
        document.querySelectorAll('.page').forEach(pageEl => {
            pageEl.classList.remove('active');
        });
        document.getElementById(`${page}-page`).classList.add('active');
        
        this.currentPage = page;
        this.loadCurrentPage();
    }
    
    loadCurrentPage() {
        switch (this.currentPage) {
            case 'overview':
                this.loadOverview();
                break;
            case 'files':
                this.loadFilesPage();
                break;
            case 'reviews':
                this.loadReviewsPage();
                break;
            case 'monitoring':
                this.loadMonitoringPage();
                break;
            case 'users':
                this.loadUsersPage();
                break;
        }
    }
    
    async loadOverview() {
        this.showLoading('加载概览信息...');
        
        try {
            // 获取用户信息
            const userResponse = await api.getCurrentUser();
            
            // 获取快速统计
            const statsData = await this.getQuickStats();
            
            this.renderOverview(userResponse, statsData);
        } catch (error) {
            console.error('加载概览错误:', error);
            this.renderOverviewError();
        } finally {
            this.hideLoading();
        }
    }
    
    renderOverview(userResponse, statsData) {
        const quickStats = document.getElementById('quick-stats');
        if (!quickStats) return;
        
        // 根据用户角色显示不同的统计信息
        const roleStats = this.getRoleBasedStats(statsData);
        
        quickStats.innerHTML = roleStats.map(stat => `
            <div class="stat-card fade-in">
                <div class="stat-icon">${stat.icon}</div>
                <div class="stat-value">${stat.value}</div>
                <div class="stat-label">${stat.label}</div>
            </div>
        `).join('');
        
        // 更新欢迎信息
        this.updateWelcomeMessage(userResponse);
    }
    
    renderOverviewError() {
        const quickStats = document.getElementById('quick-stats');
        if (!quickStats) return;
        
        quickStats.innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">⚠️</div>
                <h3>加载失败</h3>
                <p>无法加载概览信息，请刷新页面重试。</p>
                <button class="btn btn-primary" onclick="dashboardManager.loadOverview()">
                    重新加载
                </button>
            </div>
        `;
    }
    
    updateWelcomeMessage(userResponse) {
        const welcomeCard = document.querySelector('.welcome-card');
        if (!welcomeCard) return;
        
        const currentUser = authManager.getCurrentUser();
        const roleName = authManager.getRoleName(currentUser.role);
        
        welcomeCard.innerHTML = `
            <h3>欢迎，${currentUser.username}！</h3>
            <p>您以 ${roleName} 身份登录，请从左侧导航菜单选择功能模块开始工作。</p>
            <div class="quick-actions" style="margin-top: 1rem;">
                <button class="btn btn-light" onclick="dashboardManager.navigateToPage('files')">
                    📁 我的文件
                </button>
                <button class="btn btn-light" onclick="dashboardManager.navigateToPage('reviews')">
                    📝 评审任务
                </button>
            </div>
        `;
    }
    
    getRoleBasedStats(statsData) {
        const currentUser = authManager.getCurrentUser();
        const baseStats = [
            {
                icon: '👤',
                value: '1',
                label: '当前用户'
            },
            {
                icon: '📁',
                value: statsData.totalFiles || '0',
                label: '总文件数'
            },
            {
                icon: '📝',
                value: statsData.totalReviews || '0',
                label: '总评审数'
            }
        ];
        
        // 根据用户角色添加特定统计
        switch (currentUser.role) {
            case 'AUTHOR':
                baseStats.push(
                    {
                        icon: '📤',
                        value: statsData.myFiles || '0',
                        label: '我的文件'
                    },
                    {
                        icon: '⏳',
                        value: statsData.pendingReviews || '0',
                        label: '待评审'
                    }
                );
                break;
                
            case 'REVIEWER':
                baseStats.push(
                    {
                        icon: '📋',
                        value: statsData.assignedReviews || '0',
                        label: '分配评审'
                    },
                    {
                        icon: '✅',
                        value: statsData.completedReviews || '0',
                        label: '已完成'
                    }
                );
                break;
                
            case 'EDITOR':
                baseStats.push(
                    {
                        icon: '📊',
                        value: statsData.totalSubmissions || '0',
                        label: '总投稿'
                    },
                    {
                        icon: '🔄',
                        value: statsData.inReview || '0',
                        label: '评审中'
                    }
                );
                break;
                
            case 'ADMIN':
                baseStats.push(
                    {
                        icon: '👥',
                        value: statsData.totalUsers || '0',
                        label: '总用户数'
                    },
                    {
                        icon: '🟢',
                        value: statsData.onlineUsers || '0',
                        label: '在线用户'
                    }
                );
                break;
        }
        
        return baseStats;
    }
    
    async getQuickStats() {
        try {
            const currentUser = authManager.getCurrentUser();
            
            // 基础统计
            const stats = {
                totalFiles: '0',
                totalReviews: '0'
            };
            
            // 根据角色获取特定统计
            if (currentUser.role === 'ADMIN') {
                const systemStats = await api.getSystemStats();
                const onlineUsers = await api.getOnlineUsers();
                
                stats.totalUsers = systemStats.totalUsers || '0';
                stats.onlineUsers = onlineUsers.users?.length || '0';
            }
            
            return stats;
        } catch (error) {
            console.error('获取快速统计错误:', error);
            return {};
        }
    }
    
    loadFilesPage() {
        // 文件管理页面由FileManager模块处理
        if (window.fileManager) {
            fileManager.loadFiles();
        }
    }
    
    loadReviewsPage() {
        // 评审页面由ReviewManager模块处理
        if (window.reviewManager) {
            reviewManager.loadReviews();
        }
    }
    
    async loadMonitoringPage() {
        if (!authManager.hasPermission('ADMIN')) {
            this.showMessage('权限不足，无法访问系统监控', 'error');
            this.navigateToPage('overview');
            return;
        }
        
        this.showLoading('加载监控信息...');
        
        try {
            const systemStats = await api.getSystemStats();
            const onlineUsers = await api.getOnlineUsers();
            
            this.renderMonitoring(systemStats, onlineUsers);
        } catch (error) {
            console.error('加载监控信息错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
            this.renderMonitoringError();
        } finally {
            this.hideLoading();
        }
    }
    
    renderMonitoring(systemStats, onlineUsers) {
        // 渲染系统统计
        const statsGrid = document.getElementById('stats-grid');
        if (statsGrid) {
            statsGrid.innerHTML = `
                <div class="monitoring-stat">
                    <div class="stat-value">${this.formatUptime(systemStats.uptime)}</div>
                    <div class="stat-label">运行时间</div>
                </div>
                <div class="monitoring-stat">
                    <div class="stat-value">${systemStats.totalConnections || '0'}</div>
                    <div class="stat-label">总连接数</div>
                </div>
                <div class="monitoring-stat">
                    <div class="stat-value">${systemStats.activeConnections || '0'}</div>
                    <div class="stat-label">活跃连接</div>
                </div>
                <div class="monitoring-stat">
                    <div class="stat-value">${systemStats.memoryUsageMB || '0'}MB</div>
                    <div class="stat-label">内存使用</div>
                </div>
                <div class="monitoring-stat">
                    <div class="stat-value">${systemStats.diskUsageMB || '0'}MB</div>
                    <div class="stat-label">磁盘使用</div>
                </div>
                <div class="monitoring-stat">
                    <div class="stat-value">${systemStats.cacheHitRate || '0'}%</div>
                    <div class="stat-label">缓存命中率</div>
                </div>
            `;
        }
        
        // 渲染在线用户
        const onlineUsersEl = document.getElementById('online-users');
        if (onlineUsersEl && onlineUsers.users) {
            onlineUsersEl.innerHTML = `
                <h3>在线用户 (${onlineUsers.users.length})</h3>
                <div class="online-users-list">
                    ${onlineUsers.users.map(user => `
                        <div class="user-item">
                            <div class="user-status">
                                <div class="status-dot"></div>
                                <span>${user.username}</span>
                            </div>
                            <div class="user-role">${authManager.getRoleName(user.role)}</div>
                            <div class="user-time">${this.formatDate(user.loginTime)}</div>
                        </div>
                    `).join('')}
                </div>
            `;
        }
    }
    
    renderMonitoringError() {
        const statsGrid = document.getElementById('stats-grid');
        const onlineUsersEl = document.getElementById('online-users');
        
        if (statsGrid) {
            statsGrid.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">⚠️</div>
                    <h3>加载失败</h3>
                    <p>无法加载监控信息，请稍后重试。</p>
                    <button class="btn btn-primary" onclick="dashboardManager.loadMonitoringPage()">
                        重新加载
                    </button>
                </div>
            `;
        }
        
        if (onlineUsersEl) {
            onlineUsersEl.innerHTML = '';
        }
    }
    
    loadUsersPage() {
        if (!authManager.hasPermission('ADMIN')) {
            this.showMessage('权限不足，无法访问用户管理', 'error');
            this.navigateToPage('overview');
            return;
        }
        
        // 用户管理页面由UserManager模块处理
        if (window.userManager) {
            userManager.loadUsers();
        }
    }
    
    switchReviewTab(tab) {
        // 更新标签状态
        document.querySelectorAll('[data-review-tab]').forEach(tabEl => {
            tabEl.classList.remove('active');
        });
        document.querySelector(`[data-review-tab="${tab}"]`).classList.add('active');
        
        // 更新内容显示
        document.querySelectorAll('.review-section').forEach(section => {
            section.classList.remove('active');
        });
        document.getElementById(`${tab}-reviews`).classList.add('active');
        
        // 加载对应内容
        if (window.reviewManager) {
            reviewManager.loadReviews(tab);
        }
    }
    
    formatUptime(seconds) {
        if (!seconds) return '0秒';
        
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        
        if (days > 0) {
            return `${days}天${hours}小时`;
        } else if (hours > 0) {
            return `${hours}小时${minutes}分钟`;
        } else {
            return `${minutes}分钟`;
        }
    }
    
    formatDate(dateString) {
        const date = new Date(dateString);
        return date.toLocaleString('zh-CN', {
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

// 创建全局仪表板管理器实例
const dashboardManager = new DashboardManager();

// 导出仪表板管理器
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { DashboardManager, dashboardManager };
}