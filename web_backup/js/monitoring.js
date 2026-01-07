// 系统监控管理模块
class MonitoringManager {
    constructor() {
        this.stats = {};
        this.onlineUsers = [];
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadMonitoringData();
        // 每30秒刷新一次监控数据
        setInterval(() => {
            this.loadMonitoringData();
        }, 30000);
    }

    bindEvents() {
        // 刷新按钮
        const refreshBtn = document.getElementById('monitoring-refresh');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.loadMonitoringData();
            });
        }

        // 时间范围选择
        const timeRangeSelect = document.getElementById('monitoring-time-range');
        if (timeRangeSelect) {
            timeRangeSelect.addEventListener('change', () => {
                this.loadMonitoringData(timeRangeSelect.value);
            });
        }
    }

    async loadMonitoringData(timeRange = '24h') {
        this.showLoading('加载监控数据...');

        try {
            const response = await api.getSystemStats(timeRange);

            if (response.success) {
                this.stats = response.stats || {};
                this.onlineUsers = response.onlineUsers || [];
                this.renderStats();
                this.renderOnlineUsers();
                this.renderCharts();
            } else {
                this.showMessage('加载监控数据失败', 'error');
                this.renderEmptyStats();
            }
        } catch (error) {
            console.error('加载监控数据错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
            this.renderEmptyStats();
        } finally {
            this.hideLoading();
        }
    }

    renderStats() {
        const statsContainer = document.getElementById('stats-container');
        if (!statsContainer) return;

        const stats = [
            {
                title: '总用户数',
                value: this.stats.totalUsers || 0,
                icon: '👥',
                color: 'primary'
            },
            {
                title: '在线用户',
                value: this.stats.onlineUsers || 0,
                icon: '🟢',
                color: 'success'
            },
            {
                title: '总文件数',
                value: this.stats.totalFiles || 0,
                icon: '📁',
                color: 'info'
            },
            {
                title: '评审任务',
                value: this.stats.totalReviews || 0,
                icon: '📝',
                color: 'warning'
            },
            {
                title: '待评审',
                value: this.stats.pendingReviews || 0,
                icon: '⏳',
                color: 'warning'
            },
            {
                title: '已完成',
                value: this.stats.completedReviews || 0,
                icon: '✅',
                color: 'success'
            },
            {
                title: '存储使用',
                value: `${this.formatFileSize(this.stats.storageUsed || 0)} / ${this.formatFileSize(this.stats.storageLimit || 0)}`,
                icon: '💾',
                color: 'info'
            },
            {
                title: '系统负载',
                value: `${(this.stats.cpuUsage || 0).toFixed(1)}%`,
                icon: '🖥️',
                color: this.getLoadColor(this.stats.cpuUsage || 0)
            }
        ];

        statsContainer.innerHTML = stats.map(stat => `
            <div class="stat-card stat-${stat.color}">
                <div class="stat-icon">${stat.icon}</div>
                <div class="stat-content">
                    <h3 class="stat-value">${stat.value}</h3>
                    <p class="stat-title">${stat.title}</p>
                </div>
            </div>
        `).join('');
    }

    renderOnlineUsers() {
        const onlineUsersContainer = document.getElementById('online-users-container');
        if (!onlineUsersContainer) return;

        if (this.onlineUsers.length === 0) {
            onlineUsersContainer.innerHTML = `
                <div class="empty-state">
                    <p>当前没有在线用户</p>
                </div>
            `;
            return;
        }

        onlineUsersContainer.innerHTML = `
            <div class="users-list">
                ${this.onlineUsers.map(user => `
                    <div class="user-item">
                        <div class="user-avatar">
                            ${user.username.charAt(0).toUpperCase()}
                        </div>
                        <div class="user-info">
                            <h4>${user.username}</h4>
                            <p>${this.getRoleDisplayName(user.role)} | 在线时长: ${this.formatDuration(user.onlineDuration)}</p>
                            <p class="user-activity">最后活动: ${this.formatDateTime(user.lastActivity)}</p>
                        </div>
                        <div class="user-status ${user.status}">
                            ${this.getStatusDisplayName(user.status)}
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    }

    renderCharts() {
        // 渲染评审状态分布图
        this.renderReviewStatusChart();
        
        // 渲染用户活动趋势图
        this.renderUserActivityChart();
        
        // 渲染文件上传趋势图
        this.renderFileUploadChart();
    }

    renderReviewStatusChart() {
        const chartContainer = document.getElementById('review-status-chart');
        if (!chartContainer) return;

        const data = [
            { status: '待评审', count: this.stats.pendingReviews || 0, color: '#f59e0b' },
            { status: '评审中', count: this.stats.inProgressReviews || 0, color: '#3b82f6' },
            { status: '已完成', count: this.stats.completedReviews || 0, color: '#10b981' },
            { status: '已拒绝', count: this.stats.rejectedReviews || 0, color: '#ef4444' }
        ];

        const total = data.reduce((sum, item) => sum + item.count, 0);
        
        chartContainer.innerHTML = `
            <h3>评审状态分布</h3>
            <div class="chart-container">
                ${data.map(item => {
                    const percentage = total > 0 ? ((item.count / total) * 100).toFixed(1) : 0;
                    return `
                        <div class="chart-item">
                            <div class="chart-bar" style="width: ${percentage}%; background-color: ${item.color};">
                                <span class="chart-label">${item.status}</span>
                                <span class="chart-value">${item.count} (${percentage}%)</span>
                            </div>
                        </div>
                    `;
                }).join('')}
            </div>
        `;
    }

    renderUserActivityChart() {
        const chartContainer = document.getElementById('user-activity-chart');
        if (!chartContainer) return;

        const activityData = this.stats.userActivity || [];
        
        if (activityData.length === 0) {
            chartContainer.innerHTML = `
                <h3>用户活动趋势</h3>
                <div class="empty-state">
                    <p>暂无活动数据</p>
                </div>
            `;
            return;
        }

        chartContainer.innerHTML = `
            <h3>用户活动趋势</h3>
            <div class="chart-container">
                ${activityData.map(item => `
                    <div class="chart-item">
                        <div class="chart-bar" style="width: ${item.percentage}%;">
                            <span class="chart-label">${item.date}</span>
                            <span class="chart-value">${item.count}</span>
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    }

    renderFileUploadChart() {
        const chartContainer = document.getElementById('file-upload-chart');
        if (!chartContainer) return;

        const uploadData = this.stats.fileUploads || [];
        
        if (uploadData.length === 0) {
            chartContainer.innerHTML = `
                <h3>文件上传趋势</h3>
                <div class="empty-state">
                    <p>暂无上传数据</p>
                </div>
            `;
            return;
        }

        chartContainer.innerHTML = `
            <h3>文件上传趋势</h3>
            <div class="chart-container">
                ${uploadData.map(item => `
                    <div class="chart-item">
                        <div class="chart-bar" style="width: ${item.percentage}%;">
                            <span class="chart-label">${item.date}</span>
                            <span class="chart-value">${item.count}</span>
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    }

    renderEmptyStats() {
        const statsContainer = document.getElementById('stats-container');
        const onlineUsersContainer = document.getElementById('online-users-container');
        
        if (statsContainer) {
            statsContainer.innerHTML = `
                <div class="empty-state">
                    <p>暂无统计数据</p>
                    <button class="btn btn-primary" onclick="monitoringManager.loadMonitoringData()">刷新数据</button>
                </div>
            `;
        }
        
        if (onlineUsersContainer) {
            onlineUsersContainer.innerHTML = `
                <div class="empty-state">
                    <p>暂无在线用户数据</p>
                </div>
            `;
        }
    }

    getLoadColor(cpuUsage) {
        if (cpuUsage < 50) return 'success';
        if (cpuUsage < 80) return 'warning';
        return 'error';
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
            'IDLE': '空闲',
            'BUSY': '忙碌'
        };
        return statusNames[status] || status;
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    formatDuration(seconds) {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        
        if (hours > 0) {
            return `${hours}小时${minutes}分钟`;
        } else {
            return `${minutes}分钟`;
        }
    }

    formatDateTime(dateString) {
        const date = new Date(dateString);
        const now = new Date();
        const diff = now - date;
        const minutes = Math.floor(diff / 60000);
        
        if (minutes < 1) return '刚刚';
        if (minutes < 60) return `${minutes}分钟前`;
        
        const hours = Math.floor(minutes / 60);
        if (hours < 24) return `${hours}小时前`;
        
        const days = Math.floor(hours / 24);
        return `${days}天前`;
    }

    formatDate(dateString) {
        const date = new Date(dateString);
        return date.toLocaleDateString('zh-CN');
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
        // 复用其他模块的消息显示逻辑
        if (window.showNotification) {
            window.showNotification(message, type);
        } else {
            console.log(`[${type.toUpperCase()}] ${message}`);
        }
    }
}

// 创建全局监控管理器实例
const monitoringManager = new MonitoringManager();