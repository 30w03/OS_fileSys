# 评审系统 (Review System) 实现说明

## 1. 概述
评审系统是 Peer Review System 的核心业务模块，负责处理论文提交、审稿人分配、评审提交以及最终决定的全生命周期管理。该模块构建在底层文件系统和用户管理系统之上，通过自定义网络协议对外提供服务。

## 2. 核心功能

### 2.1 论文提交 (Submission)
*   **功能**: 作者可以提交 PDF 格式的论文，并附带标题、摘要和关键词。
*   **存储**: 论文文件被存储在自定义文件系统的 `/papers/` 目录下，文件名格式为 `paper_{id}_v{version}.pdf`。
*   **权限**: 提交时自动设置文件权限，仅作者和被分配的审稿人/编辑可见。

### 2.2 审稿人分配 (Assignment)
系统支持两种分配模式：
1.  **手动分配**: 编辑指定某位审稿人评审某篇论文。
2.  **自动分配**: 系统根据算法自动推荐并分配最合适的审稿人。

### 2.3 评审流程 (Reviewing)
*   **提交评审**: 审稿人对分配的论文提交评审意见，包括：
    *   **决策**: Accept (接受), Reject (拒绝), Revise (修改)。
    *   **置信度**: 1-5 分。
    *   **评语**: 详细的文本评论。
*   **存储**: 评审内容以文本文件形式存储在 `/reviews/` 目录下。

### 2.4 统计与检索
*   支持查询“我的论文”、“待审论文”和“所有论文”（编辑权限）。
*   提供系统级统计数据（接收率、待审数量等）。

## 3. 自动化分配算法

为了提高审稿效率并保证公平性，我们实现了一套基于约束满足和加权评分的自动分配算法。

### 3.1 硬约束 (Hard Constraints)
满足以下任一条件的候选人将被**直接排除**：
1.  **角色不符**: 用户角色必须是 `REVIEWER`。
2.  **自我评审**: 审稿人不能是论文的作者。
3.  **利益冲突 (CoI)**: 审稿人的所属机构 (`institution`) 与论文任一作者的机构相同。
4.  **负载过载**: 审稿人当前未完成的审稿任务数已达到其设定的 `maxLoad` (默认 3 篇)。
5.  **重复分配**: 已经分配给该论文的审稿人。

### 3.2 软约束与评分 (Soft Constraints & Scoring)
对通过硬约束筛选的候选人进行评分，得分高者优先分配：
*   **话题匹配 (Topic Matching)**: 
    *   计算论文关键词 (`keywords`) 与审稿人研究兴趣 (`researchInterests`) 的交集。
    *   每个匹配的关键词 +10 分。
*   **随机扰动**: 
    *   增加 0.0-1.0 的随机分，避免分数相同时总是分配给同一个人，实现简单的负载均衡。

### 3.3 分配策略
*   系统默认每篇论文需要 **3** 位审稿人。
*   算法会选择得分最高的 Top-N 候选人进行分配。
*   分配成功后，系统会自动更新底层文件系统的 ACL（访问控制列表），授予审稿人读取论文文件的权限。

## 4. 核心数据结构

### 4.1 Paper (论文)
```cpp
struct Paper {
    uint32_t paperId;
    std::string title;
    std::string abstract;
    std::vector<uint32_t> authorIds;
    std::vector<std::string> keywords; // 用于话题匹配
    std::vector<uint32_t> assignedReviewers;
    PaperStatus status; // SUBMITTED, UNDER_REVIEW, ACCEPTED, REJECTED
    // ...
};
```

### 4.2 User Profile (用户画像)
```cpp
struct User {
    // ... 基础字段
    std::string institution;           // 所属机构 (用于 CoI 检测)
    std::vector<std::string> researchInterests; // 研究兴趣 (用于话题匹配)
    int maxLoad;                       // 最大审稿负载
};
```

## 5. 网络协议集成

评审系统的功能通过自定义二进制协议暴露给客户端：

| 消息类型 | ID | 描述 | 关键载荷 |
| :--- | :--- | :--- | :--- |
| `MSG_SUBMIT_PAPER` | 70 | 提交论文 | 标题, 摘要, **关键词列表**, 文件内容 |
| `MSG_AUTO_ASSIGN` | 76 | 触发自动分配 | 论文ID |
| `MSG_UPDATE_PROFILE` | 90 | 更新用户画像 | 机构, **兴趣列表**, 最大负载 |
| `MSG_SUBMIT_REVIEW` | 80 | 提交评审 | 论文ID, 决策, 分数, 评语 |

## 6. 使用示例 (CLI)

### 6.1 更新专家画像
```bash
# 格式: profile "机构名" "兴趣1, 兴趣2"
> profile "MIT" "Artificial Intelligence, Machine Learning"
```

### 6.2 提交带关键词的论文
```bash
# 格式: submit "标题" "摘要" "关键词1, 关键词2" 文件路径
> submit "Deep Learning for NLP" "Abstract..." "DL, NLP" paper.pdf
```

### 6.3 触发自动分配 (仅编辑)
```bash
# 格式: autoassign 论文ID
> autoassign 1
```
