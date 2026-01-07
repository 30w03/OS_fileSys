// 评审系统模块
class ReviewManager {
    constructor() {
        this.currentTab = 'pending';
        this.reviews = [];
        this.init();
    }
    
    init() {
        this.bindEvents();
    }
    
    bindEvents() {
        // 评审提交表单
        const submitReviewForm = document.getElementById('submit-review-form');
        if (submitReviewForm) {
            submitReviewForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleReviewSubmission();
            });
        }
        
        // 评审分配表单
        const assignReviewForm = document.getElementById('assign-review-form');
        if (assignReviewForm) {
            assignReviewForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleReviewAssignment();
            });
        }
    }
    
    async loadReviews(status = null) {
        this.showLoading('加载评审列表...');
        
        try {
            const response = await api.getReviews(status);
            
            if (response.success) {
                this.reviews = response.reviews || [];
                this.renderReviews();
            } else {
                this.showMessage('加载评审列表失败', 'error');
                this.renderEmptyReviews();
            }
        } catch (error) {
            console.error('加载评审错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
            this.renderEmptyReviews();
        } finally {
            this.hideLoading();
        }
    }
    
    renderReviews() {
        const currentUser = authManager.getCurrentUser();
        
        if (this.reviews.length === 0) {
            this.renderEmptyReviews();
            return;
        }
        
        // 根据用户角色和当前标签筛选评审
        const filteredReviews = this.getFilteredReviews();
        
        // 渲染待评审
        const pendingContainer = document.getElementById('pending-reviews');
        if (pendingContainer) {
            const pendingReviews = filteredReviews.filter(review => 
                review.status === CONFIG.REVIEW_STATUS.PENDING || 
                review.status === CONFIG.REVIEW_STATUS.IN_PROGRESS
            );
            
            pendingContainer.innerHTML = pendingReviews.map(review => 
                this.renderReviewItem(review, 'pending')
            ).join('');
        }
        
        // 渲染已完成
        const completedContainer = document.getElementById('completed-reviews');
        if (completedContainer) {
            const completedReviews = filteredReviews.filter(review => 
                review.status === CONFIG.REVIEW_STATUS.COMPLETED || 
                review.status === CONFIG.REVIEW_STATUS.REJECTED
            );
            
            completedContainer.innerHTML = completedReviews.map(review => 
                this.renderReviewItem(review, 'completed')
            ).join('');
        }
    }
    
    renderReviewItem(review, type) {
        const currentUser = authManager.getCurrentUser();
        const isPending = type === 'pending';
        
        let actionsHtml = '';
        
        if (isPending) {
            // 待评审操作
            if (currentUser.role === 'REVIEWER' && review.assignedTo === currentUser.userId) {
                actionsHtml = `
                    <button class="btn btn-primary btn-sm" onclick="reviewManager.submitReview('${review.id}')">
                        提交评审
                    </button>
                    <button class="btn btn-outline btn-sm" onclick="reviewManager.viewReviewDetails('${review.id}')">
                        查看详情
                    </button>
                `;
            } else if (currentUser.role === 'EDITOR' || currentUser.role === 'ADMIN') {
                actionsHtml = `
                    <button class="btn btn-outline btn-sm" onclick="reviewManager.assignReviewer('${review.id}')">
                        分配评审员
                    </button>
                    <button class="btn btn-outline btn-sm" onclick="reviewManager.viewReviewDetails('${review.id}')">
                        查看详情
                    </button>
                `;
            }
        } else {
            // 已完成评审操作
            actionsHtml = `
                <button class="btn btn-outline btn-sm" onclick="reviewManager.viewReviewDetails('${review.id}')">
                    查看详情
                </button>
                ${this.canEditReview(review) ? `
                    <button class="btn btn-outline btn-sm" onclick="reviewManager.editReview('${review.id}')">
                        编辑
                    </button>
                ` : ''}
            `;
        }
        
        return `
            <div class="review-item fade-in">
                <div class="review-header">
                    <div class="review-title">${review.paperTitle}</div>
                    <div class="review-meta">
                        <span class="status status-${review.status.toLowerCase()}">
                            ${this.getStatusName(review.status)}
                        </span>
                        <span class="review-date">${this.formatDate(review.createdAt)}</span>
                    </div>
                </div>
                <div class="review-content">
                    <p><strong>作者:</strong> ${review.author}</p>
                    <p><strong>摘要:</strong> ${review.abstract}</p>
                    <p><strong>评审员:</strong> ${review.reviewerName || '未分配'}</p>
                </div>
                <div class="review-actions">
                    ${actionsHtml}
                </div>
            </div>
        `;
    }
    
    renderEmptyReviews() {
        const pendingContainer = document.getElementById('pending-reviews');
        const completedContainer = document.getElementById('completed-reviews');
        
        const emptyStateHtml = `
            <div class="empty-state">
                <div class="empty-state-icon">📝</div>
                <h3>暂无评审任务</h3>
                <p>当前没有需要处理的评审任务。</p>
            </div>
        `;
        
        if (pendingContainer) {
            pendingContainer.innerHTML = emptyStateHtml;
        }
        
        if (completedContainer) {
            completedContainer.innerHTML = emptyStateHtml;
        }
    }
    
    getFilteredReviews() {
        const currentUser = authManager.getCurrentUser();
        
        // 根据用户角色过滤评审
        switch (currentUser.role) {
            case 'AUTHOR':
                // 作者只能看到自己的论文评审
                return this.reviews.filter(review => review.authorId === currentUser.userId);
                
            case 'REVIEWER':
                // 评审员只能看到分配给自己的评审
                return this.reviews.filter(review => 
                    review.assignedTo === currentUser.userId ||
                    review.reviewerId === currentUser.userId
                );
                
            case 'EDITOR':
                // 编辑可以看到所有评审
                return this.reviews;
                
            case 'ADMIN':
                // 管理员可以看到所有评审
                return this.reviews;
                
            default:
                return [];
        }
    }
    
    submitReview(reviewId) {
        const review = this.reviews.find(r => r.id === reviewId);
        if (!review) return;
        
        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');
        
        if (!modal || !modalTitle || !modalBody) return;
        
        modalTitle.textContent = '提交评审';
        modalBody.innerHTML = `
            <form id="submit-review-form">
                <div class="form-group">
                    <label>论文标题:</label>
                    <p>${review.paperTitle}</p>
                </div>
                <div class="form-group">
                    <label for="review-score">评分 (1-10):</label>
                    <input type="number" id="review-score" name="score" min="1" max="10" required>
                </div>
                <div class="form-group">
                    <label for="review-comments">评审意见:</label>
                    <textarea id="review-comments" name="comments" required placeholder="请输入详细的评审意见..."></textarea>
                </div>
                <div class="form-group">
                    <label for="review-recommendation">推荐意见:</label>
                    <select id="review-recommendation" name="recommendation" required>
                        <option value="">请选择</option>
                        <option value="ACCEPT">接受发表</option>
                        <option value="MINOR_REVISION">小修后接受</option>
                        <option value="MAJOR_REVISION">大修后重审</option>
                        <option value="REJECT">拒绝发表</option>
                    </select>
                </div>
                <input type="hidden" name="reviewId" value="${reviewId}">
            </form>
        `;
        
        modal.style.display = 'block';
    }
    
    async handleReviewSubmission() {
        const form = document.getElementById('submit-review-form');
        const formData = new FormData(form);
        
        const reviewData = {
            reviewId: formData.get('reviewId'),
            score: parseInt(formData.get('score')),
            comments: formData.get('comments'),
            recommendation: formData.get('recommendation')
        };
        
        this.showLoading('提交评审中...');
        
        try {
            const response = await api.submitReview(reviewData);
            
            if (response.success) {
                this.showMessage(CONFIG.SUCCESS_MESSAGES.REVIEW_SUBMIT_SUCCESS, 'success');
                this.closeModal();
                this.loadReviews();
            } else {
                this.showMessage(response.message || '评审提交失败', 'error');
            }
        } catch (error) {
            console.error('评审提交错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }
    
    assignReviewer(paperId) {
        if (!authManager.hasPermission('EDITOR')) {
            this.showMessage('权限不足，无法分配评审员', 'error');
            return;
        }
        
        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');
        
        if (!modal || !modalTitle || !modalBody) return;
        
        modalTitle.textContent = '分配评审员';
        modalBody.innerHTML = `
            <form id="assign-review-form">
                <div class="form-group">
                    <label for="reviewer-select">选择评审员:</label>
                    <select id="reviewer-select" name="reviewerId" required>
                        <option value="">请选择评审员</option>
                        <!-- 动态加载评审员列表 -->
                    </select>
                </div>
                <input type="hidden" name="paperId" value="${paperId}">
            </form>
        `;
        
        // 加载评审员列表
        this.loadReviewers();
        
        modal.style.display = 'block';
    }
    
    async loadReviewers() {
        try {
            // 这里需要后端API支持获取评审员列表
            // const response = await api.getReviewers();
            // 暂时使用模拟数据
            const reviewers = [
                { id: 1, name: '评审员A', role: 'REVIEWER' },
                { id: 2, name: '评审员B', role: 'REVIEWER' }
            ];
            
            const select = document.getElementById('reviewer-select');
            if (select) {
                select.innerHTML = '<option value="">请选择评审员</option>' +
                    reviewers.map(reviewer => 
                        `<option value="${reviewer.id}">${reviewer.name}</option>`
                    ).join('');
            }
        } catch (error) {
            console.error('加载评审员列表错误:', error);
        }
    }
    
    async handleReviewAssignment() {
        const form = document.getElementById('assign-review-form');
        const formData = new FormData(form);
        
        const paperId = formData.get('paperId');
        const reviewerId = parseInt(formData.get('reviewerId'));
        
        this.showLoading('分配评审员中...');
        
        try {
            const response = await api.assignReview(paperId, reviewerId);
            
            if (response.success) {
                this.showMessage('评审员分配成功', 'success');
                this.closeModal();
                this.loadReviews();
            } else {
                this.showMessage(response.message || '评审员分配失败', 'error');
            }
        } catch (error) {
            console.error('评审员分配错误:', error);
            this.showMessage(CONFIG.ERROR_MESSAGES.NETWORK_ERROR, 'error');
        } finally {
            this.hideLoading();
        }
    }
    
    viewReviewDetails(reviewId) {
        const review = this.reviews.find(r => r.id === reviewId);
        if (!review) return;
        
        const modal = document.getElementById('modal');
        const modalTitle = document.getElementById('modal-title');
        const modalBody = document.getElementById('modal-body');
        
        if (!modal || !modalTitle || !modalBody) return;
        
        modalTitle.textContent = '评审详情';
        modalBody.innerHTML = `
            <div class="review-details-view">
                <div class="detail-section">
                    <h4>论文信息</h4>
                    <p><strong>标题:</strong> ${review.paperTitle}</p>
                    <p><strong>作者:</strong> ${review.author}</p>
                    <p><strong>摘要:</strong> ${review.abstract}</p>
                    <p><strong>关键词:</strong> ${review.keywords || '无'}</p>
                </div>
                <div class="detail-section">
                    <h4>评审信息</h4>
                    <p><strong>状态:</strong> <span class="status status-${review.status.toLowerCase()}">${this.getStatusName(review.status)}</span></p>
                    <p><strong>评审员:</strong> ${review.reviewerName || '未分配'}</p>
                    <p><strong>创建时间:</strong> ${this.formatDate(review.createdAt)}</p>
                    <p><strong>更新时间:</strong> ${this.formatDate(review.updatedAt)}</p>
                </div>
                ${review.score ? `
                    <div class="detail-section">
                        <h4>评审结果</h4>
                        <p><strong>评分:</strong> ${review.score}/10</p>
                        <p><strong>推荐意见:</strong> ${this.getRecommendationName(review.recommendation)}</p>
                        <p><strong>评审意见:</strong></p>
                        <div class="review-comments">${review.comments}</div>
                    </div>
                ` : ''}
            </div>
        `;
        
        modal.style.display = 'block';
    }
    
    editReview(reviewId) {
        // 实现编辑评审逻辑
        this.showMessage('编辑功能正在开发中', 'info');
    }
    
    canEditReview(review) {
        const currentUser = authManager.getCurrentUser();
        
        // 管理员和编辑可以编辑任何评审
        if (currentUser.role === 'ADMIN' || currentUser.role === 'EDITOR') {
            return true;
        }
        
        // 评审员可以编辑自己的评审（在一定时间范围内）
        if (currentUser.role === 'REVIEWER' && 
            review.reviewerId === currentUser.userId &&
            this.canEditWithinTimeLimit(review.updatedAt)) {
            return true;
        }
        
        return false;
    }
    
    canEditWithinTimeLimit(updatedAt) {
        const updated = new Date(updatedAt);
        const now = new Date();
        const diffHours = (now - updated) / (1000 * 60 * 60);
        
        // 允许在24小时内编辑
        return diffHours <= 24;
    }
    
    getStatusName(status) {
        const statusNames = {
            'PENDING': '待分配',
            'IN_PROGRESS': '评审中',
            'COMPLETED': '已完成',
            'REJECTED': '已拒绝'
        };
        return statusNames[status] || status;
    }
    
    getRecommendationName(recommendation) {
        const recommendationNames = {
            'ACCEPT': '接受发表',
            'MINOR_REVISION': '小修后接受',
            'MAJOR_REVISION': '大修后重审',
            'REJECT': '拒绝发表'
        };
        return recommendationNames[recommendation] || recommendation;
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
        if (window.dashboardManager) {
            dashboardManager.closeModal();
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

// 创建全局评审管理器实例
const reviewManager = new ReviewManager();

// 导出评审管理器
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { ReviewManager, reviewManager };
}