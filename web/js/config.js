// 前端配置
const CONFIG = {
    // 服务器配置
    SERVER: {
        HOST: 'localhost',
        PORT: 8080,
        PROTOCOL: 'http'
    },
    
    // API配置
    API: {
        TIMEOUT: 10000,
        RETRY_ATTEMPTS: 3,
        RETRY_DELAY: 1000
    },
    
    // UI配置
    UI: {
        NOTIFICATION_DURATION: 5000,
        LOADING_DELAY: 300,
        ANIMATION_DURATION: 300
    },
    
    // 分页配置
    PAGINATION: {
        PAGE_SIZE: 10,
        MAX_PAGE_SIZE: 100
    },
    
    // 文件上传配置
    UPLOAD: {
        MAX_SIZE: 50 * 1024 * 1024, // 50MB
        ALLOWED_TYPES: [
            'pdf', 'doc', 'docx', 'txt', 'rtf',
            'tex', 'bib', 'jpg', 'jpeg', 'png', 'gif'
        ]
    },
    
    // 用户角色
    ROLES: {
        AUTHOR: 'AUTHOR',
        REVIEWER: 'REVIEWER', 
        EDITOR: 'EDITOR',
        ADMIN: 'ADMIN'
    },
    
    // 评审状态
    REVIEW_STATUS: {
        PENDING: 'PENDING',
        IN_PROGRESS: 'IN_PROGRESS',
        COMPLETED: 'COMPLETED',
        REJECTED: 'REJECTED'
    },
    
    // 消息类型
    MESSAGE_TYPES: {
        LOGIN: 'LOGIN',
        REGISTER: 'REGISTER',
        LOGOUT: 'LOGOUT',
        FILE_UPLOAD: 'FILE_UPLOAD',
        FILE_DOWNLOAD: 'FILE_DOWNLOAD',
        FILE_DELETE: 'FILE_DELETE',
        REVIEW_SUBMIT: 'REVIEW_SUBMIT',
        REVIEW_ASSIGN: 'REVIEW_ASSIGN',
        SYSTEM_STATS: 'SYSTEM_STATS',
        ONLINE_USERS: 'ONLINE_USERS'
    },
    
    // 错误消息
    ERROR_MESSAGES: {
        NETWORK_ERROR: '网络连接错误，请检查网络设置',
        TIMEOUT_ERROR: '请求超时，请稍后重试',
        AUTHENTICATION_ERROR: '身份验证失败，请重新登录',
        PERMISSION_DENIED: '权限不足，无法执行此操作',
        INVALID_INPUT: '输入数据无效，请检查后重试',
        SERVER_ERROR: '服务器内部错误，请稍后重试',
        FILE_TOO_LARGE: '文件大小超出限制',
        INVALID_FILE_TYPE: '不支持的文件类型',
        UNKNOWN_ERROR: '发生未知错误，请联系管理员'
    },
    
    // 成功消息
    SUCCESS_MESSAGES: {
        LOGIN_SUCCESS: '登录成功',
        REGISTER_SUCCESS: '注册成功',
        LOGOUT_SUCCESS: '退出成功',
        FILE_UPLOAD_SUCCESS: '文件上传成功',
        FILE_DELETE_SUCCESS: '文件删除成功',
        REVIEW_SUBMIT_SUCCESS: '评审提交成功',
        PROFILE_UPDATE_SUCCESS: '个人信息更新成功'
    },
    
    // 验证规则
    VALIDATION: {
        USERNAME_MIN_LENGTH: 3,
        USERNAME_MAX_LENGTH: 50,
        PASSWORD_MIN_LENGTH: 6,
        PASSWORD_MAX_LENGTH: 100,
        FILENAME_MAX_LENGTH: 255
    }
};

// 导出配置
if (typeof module !== 'undefined' && module.exports) {
    module.exports = CONFIG;
}