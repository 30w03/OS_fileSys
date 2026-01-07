// 主应用程序入口文件
class App {
    constructor() {
        this.isInitialized = false;
        this.init();
    }

    async init() {
        try {
            // 等待DOM完全加载
            if (document.readyState === 'loading') {
                document.addEventListener('DOMContentLoaded', () => {
                    this.initializeApp();
                });
            } else {
                this.initializeApp();
            }
        } catch (error) {
            console.error('应用初始化错误:', error);
            this.showError('应用初始化失败，请刷新页面重试');
        }
    }

    initializeApp() {
        try {
            // 初始化通知系统
            this.initNotificationSystem();
            
            // 初始化模态框系统
            this.initModalSystem();
            
            // 初始化全局键盘快捷键
            this.initKeyboardShortcuts();
            
            // 初始化全局错误处理
            this.initGlobalErrorHandler();
            
            // 初始化工具函数
            this.initUtilityFunctions();
            
            this.isInitialized = true;
            console.log('应用初始化完成');
            
            // 触发应用就绪事件
            document.dispatchEvent(new CustomEvent('appReady', {
                detail: { timestamp: Date.now() }
            }));
            
        } catch (error) {
            console.error('应用初始化过程中发生错误:', error);
            this.showError('应用初始化失败');
        }
    }

    initNotificationSystem() {
        // 创建全局通知函数
        window.showNotification = (message, type = 'info', duration = 3000) => {
            this.showNotification(message, type, duration);
        };

        // 通知关闭按钮事件
        const notificationClose = document.getElementById('notification-close');
        if (notificationClose) {
            notificationClose.addEventListener('click', () => {
                this.hideNotification();
            });
        }

        // 自动隐藏通知
        this.notificationTimeout = null;
    }

    showNotification(message, type = 'info', duration = 3000) {
        const notification = document.getElementById('notification');
        const messageEl = document.getElementById('notification-message');
        
        if (!notification || !messageEl) return;

        // 设置消息内容
        messageEl.textContent = message;
        
        // 设置通知类型样式
        notification.className = `notification notification-${type}`;
        notification.classList.add('show');
        
        // 自动隐藏
        clearTimeout(this.notificationTimeout);
        this.notificationTimeout = setTimeout(() => {
            this.hideNotification();
        }, duration);
    }

    hideNotification() {
        const notification = document.getElementById('notification');
        if (notification) {
            notification.classList.remove('show');
        }
        clearTimeout(this.notificationTimeout);
    }

    initModalSystem() {
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

    closeModal() {
        const modal = document.getElementById('modal');
        if (modal) {
            modal.style.display = 'none';
            // 清空模态框内容
            const modalBody = document.getElementById('modal-body');
            if (modalBody) {
                modalBody.innerHTML = '';
            }
        }
    }

    initKeyboardShortcuts() {
        document.addEventListener('keydown', (e) => {
            // Ctrl/Cmd + K 打开搜索（如果实现）
            if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
                e.preventDefault();
                // 可以在这里实现全局搜索功能
            }
            
            // Ctrl/Cmd + R 刷新当前页面数据
            if ((e.ctrlKey || e.metaKey) && e.key === 'r') {
                e.preventDefault();
                this.refreshCurrentPage();
            }
        });
    }

    initGlobalErrorHandler() {
        // 全局错误处理
        window.addEventListener('error', (e) => {
            console.error('全局错误:', e.error);
            this.showError('系统发生错误，请稍后重试');
        });

        // 未捕获的Promise错误
        window.addEventListener('unhandledrejection', (e) => {
            console.error('未处理的Promise错误:', e.reason);
            this.showError('网络请求失败，请检查网络连接');
        });
    }

    initUtilityFunctions() {
        // 格式化文件大小
        window.formatFileSize = (bytes) => {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        };

        // 格式化日期
        window.formatDate = (dateString) => {
            const date = new Date(dateString);
            return date.toLocaleDateString('zh-CN');
        };

        // 格式化日期时间
        window.formatDateTime = (dateString) => {
            const date = new Date(dateString);
            return date.toLocaleString('zh-CN');
        };

        // 获取相对时间
        window.getRelativeTime = (dateString) => {
            const date = new Date(dateString);
            const now = new Date();
            const diff = now - date;
            
            const seconds = Math.floor(diff / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);
            const days = Math.floor(hours / 24);
            
            if (days > 0) return `${days}天前`;
            if (hours > 0) return `${hours}小时前`;
            if (minutes > 0) return `${minutes}分钟前`;
            return '刚刚';
        };

        // 防抖函数
        window.debounce = (func, wait) => {
            let timeout;
            return function executedFunction(...args) {
                const later = () => {
                    clearTimeout(timeout);
                    func(...args);
                };
                clearTimeout(timeout);
                timeout = setTimeout(later, wait);
            };
        };

        // 节流函数
        window.throttle = (func, limit) => {
            let inThrottle;
            return function executedFunction(...args) {
                if (!inThrottle) {
                    func.apply(this, args);
                    inThrottle = true;
                    setTimeout(() => inThrottle = false, limit);
                }
            };
        };
    }

    refreshCurrentPage() {
        // 根据当前活跃的页面刷新数据
        const activePage = document.querySelector('.page.active');
        if (!activePage) return;

        const pageId = activePage.id;
        
        switch (pageId) {
            case 'files-page':
                if (window.fileManager) {
                    window.fileManager.loadFiles();
                }
                break;
            case 'reviews-page':
                if (window.reviewManager) {
                    window.reviewManager.loadReviews();
                }
                break;
            case 'monitoring-page':
                if (window.monitoringManager) {
                    window.monitoringManager.loadMonitoringData();
                }
                break;
            case 'users-page':
                if (window.userManager) {
                    window.userManager.loadUsers();
                }
                break;
            default:
                // 概览页面或其他页面
                this.loadOverviewData();
                break;
        }
    }

    async loadOverviewData() {
        try {
            // 加载概览数据
            const response = await api.getOverview();
            if (response.success) {
                this.renderQuickStats(response.data);
            }
        } catch (error) {
            console.error('加载概览数据失败:', error);
        }
    }

    renderQuickStats(data) {
        const container = document.getElementById('quick-stats');
        if (!container || !data) return;

        const stats = [
            { label: '总文件数', value: data.totalFiles || 0, icon: '📁' },
            { label: '待评审', value: data.pendingReviews || 0, icon: '⏳' },
            { label: '已完成', value: data.completedReviews || 0, icon: '✅' },
            { label: '在线用户', value: data.onlineUsers || 0, icon: '👥' }
        ];

        container.innerHTML = stats.map(stat => `
            <div class="stat-card">
                <span class="stat-icon">${stat.icon}</span>
                <div class="stat-info">
                    <span class="stat-value">${stat.value}</span>
                    <span class="stat-label">${stat.label}</span>
                </div>
            </div>
        `).join('');
    }

    showError(message) {
        this.showNotification(message, 'error');
    }

    showSuccess(message) {
        this.showNotification(message, 'success');
    }

    showWarning(message) {
        this.showNotification(message, 'warning');
    }

    showInfo(message) {
        this.showNotification(message, 'info');
    }
}

// 应用配置
const APP_CONFIG = {
    VERSION: '1.0.0',
    DEBUG: false,
    AUTO_SAVE_INTERVAL: 30000, // 30秒
    NOTIFICATION_DURATION: 3000,
    MAX_FILE_SIZE: 50 * 1024 * 1024, // 50MB
    SUPPORTED_FILE_TYPES: ['.pdf', '.doc', '.docx', '.txt', '.rtf']
};

// 将配置暴露到全局
window.APP_CONFIG = APP_CONFIG;

// 创建应用实例
const app = new App();

// 导出应用实例供其他模块使用
window.app = app;

// 开发模式下的调试信息
if (APP_CONFIG.DEBUG) {
    console.log('🚀 同行评审系统启动');
    console.log('版本:', APP_CONFIG.VERSION);
    console.log('调试模式已启用');
}