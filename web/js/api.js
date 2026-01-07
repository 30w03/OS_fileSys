// API通信模块
class ApiClient {
    constructor() {
        this.baseUrl = `${CONFIG.SERVER.PROTOCOL}://${CONFIG.SERVER.HOST}:${CONFIG.SERVER.PORT}`;
        this.sessionId = sessionStorage.getItem('sessionId') || null;
        this.userId = sessionStorage.getItem('userId') || null;
        this.username = sessionStorage.getItem('username') || null;
        this.role = sessionStorage.getItem('role') || null;
        
        if (this.isAuthenticated()) {
            this.startHeartbeat();
        }
    }
    
    // 设置认证信息
    setAuth(sessionId, userId, username, role) {
        this.sessionId = sessionId;
        this.userId = userId;
        this.username = username;
        this.role = role;
        
        sessionStorage.setItem('sessionId', sessionId);
        sessionStorage.setItem('userId', userId);
        sessionStorage.setItem('username', username);
        sessionStorage.setItem('role', role);
    }
    
    // 清除认证信息
    clearAuth() {
        this.stopHeartbeat();
        this.sessionId = null;
        this.userId = null;
        this.username = null;
        this.role = null;
        
        sessionStorage.removeItem('sessionId');
        sessionStorage.removeItem('userId');
        sessionStorage.removeItem('username');
        sessionStorage.removeItem('role');
    }
    
    // 检查是否已认证
    isAuthenticated() {
        return this.sessionId !== null && this.userId !== null;
    }
    
    // 通用HTTP请求方法
    async request(method, endpoint, data = null) {
        const url = `${this.baseUrl}${endpoint}`;
        const headers = {
            'Content-Type': 'application/json'
        };
        
        // 添加认证头
        if (this.sessionId) {
            headers['Authorization'] = `Bearer ${this.sessionId}`;
        }
        
        const config = {
            method: method,
            headers: headers,
            credentials: 'include'
        };
        
        if (data) {
            config.body = JSON.stringify(data);
        }
        
        try {
            const response = await fetch(url, config);
            
            if (!response.ok) {
                const errorData = await response.json().catch(() => ({}));
                throw new Error(errorData.message || `HTTP ${response.status}: ${response.statusText}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error('API请求错误:', error);
            throw error;
        }
    }
    
    // 身份验证相关API
    async login(username, password) {
        const response = await this.request('POST', '/api/auth/login', {
            username,
            password
        });
        
        if (response.success) {
            this.setAuth(
                response.sessionId,
                response.userId,
                response.username,
                response.role
            );
            this.startHeartbeat();
        }
        
        return response;
    }

    // 启动心跳
    startHeartbeat() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
        }
        
        // 每30秒发送一次心跳
        this.heartbeatInterval = setInterval(async () => {
            if (this.isAuthenticated()) {
                try {
                    await this.request('GET', '/api/auth/heartbeat');
                } catch (e) {
                    console.warn('Heartbeat failed', e);
                }
            } else {
                this.stopHeartbeat();
            }
        }, 30000);
    }

    stopHeartbeat() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }
    }
    
    async register(username, password, role) {
        return await this.request('POST', '/api/auth/register', {
            username,
            password,
            role
        });
    }
    
    async logout() {
        try {
            await this.request('POST', '/api/auth/logout');
        } catch (error) {
            console.error('登出错误:', error);
        } finally {
            this.clearAuth();
        }
    }
    
    // 文件管理API
    async getFiles(page = 1, limit = CONFIG.PAGINATION.PAGE_SIZE) {
        return await this.request('GET', `/api/files?page=${page}&limit=${limit}`);
    }
    
    async uploadFile(file) {
        const formData = new FormData();
        formData.append('file', file);
        
        const url = `${this.baseUrl}/api/files/upload`;
        const headers = {};
        
        if (this.sessionId) {
            headers['Authorization'] = `Bearer ${this.sessionId}`;
        }
        
        try {
            const response = await fetch(url, {
                method: 'POST',
                headers: headers,
                body: formData,
                credentials: 'include'
            });
            
            if (!response.ok) {
                const errorData = await response.json().catch(() => ({}));
                throw new Error(errorData.message || '文件上传失败');
            }
            
            return await response.json();
        } catch (error) {
            console.error('文件上传错误:', error);
            throw error;
        }
    }
    
    async deleteFile(fileId) {
        return await this.request('DELETE', `/api/files/${fileId}`);
    }
    
    async downloadFile(fileId) {
        try {
            const url = `${this.baseUrl}/api/files/${fileId}/download`;
            const headers = {};
            
            if (this.sessionId) {
                headers['Authorization'] = `Bearer ${this.sessionId}`;
            }
            
            const response = await fetch(url, {
                method: 'GET',
                headers: headers,
                credentials: 'include'
            });
            
            if (!response.ok) {
                throw new Error('文件下载失败');
            }
            
            return response;
        } catch (error) {
            console.error('文件下载错误:', error);
            throw error;
        }
    }
    
    // 评审系统API
    async getReviews(status = null, page = 1, limit = CONFIG.PAGINATION.PAGE_SIZE) {
        const params = new URLSearchParams({ page, limit });
        if (status) {
            params.append('status', status);
        }
        
        return await this.request('GET', `/api/reviews?${params.toString()}`);
    }
    
    async submitReview(reviewData) {
        return await this.request('POST', '/api/reviews', reviewData);
    }
    
    async getReviewDetails(reviewId) {
        return await this.request('GET', `/api/reviews/${reviewId}`);
    }
    
    async assignReview(paperId, reviewerId) {
        return await this.request('POST', `/api/reviews/${paperId}/assign`, {
            reviewerId
        });
    }
    
    // 用户管理API（仅管理员可用）
    async getUsers(page = 1, limit = CONFIG.PAGINATION.PAGE_SIZE, search = '', role = '') {
        let url = `/api/users?page=${page}&limit=${limit}`;
        if (search) url += `&search=${encodeURIComponent(search)}`;
        if (role) url += `&role=${role}`;
        return await this.request('GET', url);
    }
    
    async createUser(userData) {
        return await this.request('POST', '/api/users', userData);
    }
    
    async updateUser(userData) {
        return await this.request('PUT', `/api/users/${userData.userId}`, userData);
    }
    
    async deleteUser(userId) {
        return await this.request('DELETE', `/api/users/${userId}`);
    }
    
    async updateUserRole(userId, role) {
        return await this.request('PUT', `/api/users/${userId}/role`, { role });
    }
    
    async resetPassword(userId) {
        return await this.request('POST', `/api/users/${userId}/reset-password`);
    }
    
    async batchUpdateUsers(data) {
        return await this.request('POST', '/api/users/batch-update', data);
    }
    
    // 系统监控API
    async getSystemStats(timeRange = '24h') {
        return await this.request('GET', `/api/system/stats?timeRange=${timeRange}`);
    }
    
    async getOnlineUsers() {
        return await this.request('GET', '/api/system/online-users');
    }
    
    // 概览数据API
    async getOverview() {
        return await this.request('GET', '/api/overview');
    }
    
    // 用户信息API
    async getCurrentUser() {
        return await this.request('GET', '/api/user/profile');
    }
    
    async updateProfile(profileData) {
        return await this.request('PUT', '/api/user/profile', profileData);
    }
    
    async changePassword(oldPassword, newPassword) {
        return await this.request('PUT', '/api/user/password', {
            oldPassword,
            newPassword
        });
    }
}

// 创建全局API客户端实例
const api = new ApiClient();

// 导出API客户端
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { ApiClient, api };
}